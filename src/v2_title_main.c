/*
 * v2_title_main.c -- clean-rebuild starting point for Jerb's Game.
 *
 * This intentionally links no legacy rooms, collision, casino, or game
 * loop. It keeps the established title presentation alive while v2 scenes
 * are rebuilt against the 512 KiB MMC3 cartridge contract.
 */

#include <nes.h>

#include "font.h"
#include "mmc3.h"
#include "music.h"
#include "rng.h"
#include "scene_casino_data.h"
#include "v2_casino.h"
#include "v2_game_state.h"
#include "v2_runtime.h"
#include "v2_vram_queue.h"

#define CHR_TILE_FLOOR 0
#define CHR_TILE_PATH 3
#define FONT_TILE_BASE 8
#define TILE_BORDER_H 52
#define TILE_BORDER_V 53
#define DOORKNOB_TILE 51

/* These tile IDs are stable because build.bat preserves the original CHR
 * asset ordering while the scenes themselves are rebuilt independently. */
#define TITLE_PORTRAIT_TILE_BASE 63
#define TITLE_CLOSED_MOUTH_TILE_BASE 183
#define TITLE_PORTRAIT_TOP 8
#define TITLE_PORTRAIT_LEFT 2
#define TITLE_PORTRAIT_WIDTH 10
#define TITLE_PORTRAIT_HEIGHT 12
#define TITLE_MOUTH_TILE_ROW 7
#define TITLE_MOUTH_TILE_COL 4
#define TITLE_MOUTH_WIDTH 3
#define TITLE_MOUTH_HEIGHT 3
#define TITLE_TONGUE_TILE 246
#define TITLE_TONGUE_TILE_RIGHT 247
#define TITLE_UVULA_TILE 248

/* Neon Avenue is the first rebuilt gameplay scene. These IDs come from the
 * explicit CHR order in build.bat, checked against game_core.map. */
#define NEON_TILE_ASPHALT 192
#define NEON_TILE_SIDEWALK 193
#define NEON_TILE_BRICK 194
#define NEON_TILE_AWNING 195
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199
#define NEON_TILE_SIGN_TL 200
#define NEON_TILE_SIGN_T 201
#define NEON_TILE_SIGN_TR 202
#define NEON_TILE_SIGN_L 203
#define NEON_TILE_SIGN_FILL 204
#define NEON_TILE_SIGN_R 205
#define NEON_TILE_SIGN_BL 206
#define NEON_TILE_SIGN_B 207
#define NEON_TILE_SIGN_BR 208
#define NEON_TILE_WINDOW_H 209
#define NEON_TILE_WINDOW_V 210
#define NEON_TILE_WINDOW_FILL 211
#define PLAYER_TILE_BASE 212
#define TAXI_TILE_BASE 220
#define TAXI_PARTICLE_TILE 232
#define TAXI_PARTICLE_SMALL_TILE 233
#define TAXI_PARTICLE_DOT_TILE 234

/* traffic_bones.s -- linked into CHR all along but never referenced by any
 * tile constant until now, so the bone scatter it was authored for never
 * actually appeared after a hit. Six tiles: bone-left, bone-right,
 * wishbone, femur, club-bone, skull-and-crossbones. */
#define TRAFFIC_BONE_TILE_BASE 249

/* furniture.s, linked right before the title portrait -- two tiles
 * (authored as the left half of a 16x16 area, OAM-mirrored for the
 * right) forming Jimbles himself: a sentient pint standing behind his
 * own bar counter. */
#define JIMBLES_TILE_TOP 61
#define JIMBLES_TILE_BOTTOM 62
#define JIMBLES_TILE_BOTTOM_RIGHT 1
#define PISS_FACE_FEATURE_TILE 2
#define PISS_FACE_OAM_BASE 20

/* critter.s is retired from the active v2 scenes. Its former one-tile
 * wildlife slot now gives the All-Nite Mart checkout a readable register. */
#define MART_TILE_REGISTER 58

/* portal.s is retired from the active v2 scenes. Its two stable slots now
 * provide the upper/lower halves of a back-facing bar regular. */
#define BAR_PATRON_BACK_TOP 59
#define BAR_PATRON_BACK_BOTTOM 60

/* Retired combat tiles, retained at their stable CHR IDs, now furnish the
 * lower-right Piss Corner in Jimbles's Bar. */
#define PISS_TILE_STREAK_A 55
#define PISS_TILE_STREAK_B 56
#define PISS_TILE_PUDDLE 57

/* bar_interior.s -- Jimbles's Bar, rebuilt as its own scene. Eleven
 * tiles: floor, wall, counter, bottle shelf, two back-room planks, a
 * two-tile jukebox, a stool, and a two-tile door (jamb + fill,
 * used on both sides of the doorway gap). */
#define BAR_TILE_FLOOR        235
#define BAR_TILE_WALL         236
#define BAR_TILE_COUNTER      237
#define BAR_TILE_SHELF        238
#define BAR_TILE_PLANK_R      239
#define BAR_TILE_JUKEBOX_TOP  240
#define BAR_TILE_JUKEBOX_BOT  241
#define BAR_TILE_PLANK_L      242
#define BAR_TILE_STOOL        243
#define BAR_TILE_DOOR_JAMB    244
#define BAR_TILE_DOOR_FILL    245

/* token.s -- the very last tile slot before the 256-tile boundary both
 * sprites and backgrounds currently share (PPU.control's pattern-table
 * bits never select the far CHR half, so tile indices above 255 aren't
 * reachable without a per-scene PPU.control change nothing here needs
 * yet). A round coin with a punched hole, spent at the jukebox. */
#define TOKEN_TILE 255

/* run_bar_interior()'s return value: which of its two doors the player
 * left through, so main()'s dispatcher knows whether to head back to
 * Neon Avenue or on into the casino. */
#define BAR_EXIT_AVENUE 0
#define BAR_EXIT_CASINO 1
#define AVENUE_EXIT_BAR 0
#define AVENUE_EXIT_SEWER 1
#define AVENUE_EXIT_MART 2
#define MART_EXIT_AVENUE 0
#define MART_EXIT_INTERIOR 1

#define PARTICLE_OAM_BASE 48

/* A low, 32x8 football-shaped manhole on the sidewalk and its sewer ladder. */
#define MANHOLE_LEFT 4
#define MANHOLE_TOP 25
#define MANHOLE_X 48
#define MANHOLE_Y 204
#define MANHOLE_TILE_L 6
#define MANHOLE_TILE_R 7
#define MANHOLE_TILE_M 54

#define TITLE_MESSAGE_COUNT 3
#define TITLE_MESSAGE_HIDDEN TITLE_MESSAGE_COUNT

volatile unsigned char title_mouth_dirty;
volatile unsigned char title_mouth_open;
volatile unsigned char title_message_dirty;
volatile unsigned char title_message_index;

/* Defined with the other interior UI helpers below; declared here because
 * the Mart scene appears earlier in this translation unit. */
static void show_message_box(const char *line1, const char *line2, const char *line3);
static void show_mart_message(const char *line1, const char *line2, const char *line3);
static void run_inventory(void);

/* Keep the established scene code readable while making its persistent data
 * explicitly owned by the fixed-core game state. */
#define token_taken game_state.token_taken
#define has_token game_state.has_token
#define door_unlocked game_state.door_unlocked
#define has_beer game_state.has_beer
#define has_paper_clip game_state.has_paper_clip
#define jimbles_x game_state.jimbles_x
#define casino_credits game_state.casino.credits
#define casino_bet game_state.casino.bet
#define casino_reel game_state.casino.reel

static void reset_scroll(void)
{
    /* $2005 and $2006 share a write latch. Every scene tile update uses
     * $2006, so reset that latch before restoring the camera or the next
     * frame can scroll into the adjacent, empty nametable. */
    (void)PPU.status;
    PPU.scroll = 0;
    PPU.scroll = 0;
}

static void set_tile(unsigned char row, unsigned char col, unsigned char tile)
{
    PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
    PPU.vram.address = (unsigned char)((row << 5) + col);
    PPU.vram.data = tile;
}

static void fill_title_rect(unsigned char first_row, unsigned char first_col,
                            unsigned char width, unsigned char height,
                            unsigned char tile)
{
    unsigned char row, col;

    for (row = first_row; row < first_row + height; ++row) {
        PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
        PPU.vram.address = (unsigned char)((row << 5) + first_col);
        for (col = 0; col < width; ++col) PPU.vram.data = tile;
    }
}

static void set_attribute_block(unsigned char row, unsigned char col,
                                unsigned char palette)
{
    PPU.vram.address = 0x23;
    PPU.vram.address = (unsigned char)(0xC0 + row * 8 + col);
    PPU.vram.data = (unsigned char)(palette * 0x55);
}

static void draw_text(unsigned char row, unsigned char col, const char *text)
{
    PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
    PPU.vram.address = (unsigned char)((row << 5) + col);
    while (*text) PPU.vram.data = (unsigned char)(FONT_TILE_BASE + font_glyph_index(*text++));
}

static void draw_title_plate(unsigned char top_row, unsigned char left_col,
                             unsigned char width, unsigned char height)
{
    unsigned char row;

    fill_title_rect(top_row, left_col, width, height, FONT_TILE_BASE);
    fill_title_rect(top_row, left_col, width, 1, TILE_BORDER_H);
    fill_title_rect(top_row + height - 1, left_col, width, 1, TILE_BORDER_H);
    for (row = top_row + 1; row < top_row + height - 1; ++row) {
        set_tile(row, left_col, TILE_BORDER_V);
        set_tile(row, left_col + width - 1, TILE_BORDER_V);
    }
}

static void draw_title_portrait(void)
{
    unsigned char row, col;
    unsigned char tile = TITLE_PORTRAIT_TILE_BASE;

    for (row = TITLE_PORTRAIT_TOP; row < TITLE_PORTRAIT_TOP + TITLE_PORTRAIT_HEIGHT; ++row) {
        PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
        PPU.vram.address = (unsigned char)((row << 5) + TITLE_PORTRAIT_LEFT);
        for (col = 0; col < TITLE_PORTRAIT_WIDTH; ++col) PPU.vram.data = tile++;
    }
}

static void draw_title_mouth_closed(void)
{
    unsigned char row, col;
    unsigned char tile = TITLE_CLOSED_MOUTH_TILE_BASE;

    for (row = 0; row < TITLE_MOUTH_HEIGHT; ++row) {
        PPU.vram.address = (unsigned char)(0x20 + ((TITLE_PORTRAIT_TOP + TITLE_MOUTH_TILE_ROW + row) >> 3));
        PPU.vram.address = (unsigned char)(((TITLE_PORTRAIT_TOP + TITLE_MOUTH_TILE_ROW + row) << 5) +
                                            TITLE_PORTRAIT_LEFT + TITLE_MOUTH_TILE_COL);
        for (col = 0; col < TITLE_MOUTH_WIDTH; ++col) PPU.vram.data = tile++;
    }
}

static void set_title_tongue(unsigned char open)
{
    if (open) {
        set_oam_sprite(0, 56, 132, TITLE_TONGUE_TILE, 0);
        set_oam_sprite(1, 64, 132, TITLE_TONGUE_TILE_RIGHT, 0);
        set_oam_sprite(2, 56, 129, TITLE_UVULA_TILE, 0);
    } else {
        hide_oam_sprite(0);
        hide_oam_sprite(1);
        hide_oam_sprite(2);
    }
}

static void queue_title_mouth(unsigned char open)
{
    title_mouth_open = open;
    title_mouth_dirty = 1;
    set_title_tongue(open);
}

static void queue_title_message(unsigned char index)
{
    title_message_index = index;
    title_message_dirty = 1;
}

static void show_title_screen(void)
{
    static const unsigned char title_palette[32] = {
        0x0F, 0x11, 0x01, 0x01, 0x0F, 0x16, 0x30, 0x01,
        0x0F, 0x27, 0x28, 0x01, 0x0F, 0x27, 0x20, 0x01,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_title_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = title_palette[i];
    fill_title_rect(0, 0, 32, 30, CHR_TILE_FLOOR);

    fill_title_rect(3, 4, 26, 5, CHR_TILE_PATH);
    draw_title_plate(2, 3, 26, 5);
    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 1);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 1);
    draw_text(3, 10, "JERB'S GAME");

    for (i = 0; i < 3; ++i) set_attribute_block(2, i, 2);
    for (i = 0; i < 3; ++i) set_attribute_block(3, i, 2);
    for (i = 0; i < 3; ++i) set_attribute_block(4, i, 3);
    draw_title_portrait();
    draw_title_mouth_closed();

    fill_title_rect(24, 6, 22, 5, CHR_TILE_PATH);
    draw_title_plate(23, 5, 22, 5);
    for (i = 1; i < 7; ++i) set_attribute_block(5, i, 1);
    for (i = 1; i < 7; ++i) set_attribute_block(6, i, 1);
    draw_text(25, 8, "PRESS A TO START");

    title_message_dirty = 0;
    title_mouth_dirty = 0;
    title_mouth_open = 0;
    clear_sprites();
    reset_scroll();
    oam_flush();
    PPU.mask = 0x1E;
}

/* The title remains a complete, self-contained presentation. Its only
 * outward transition is a debounced New Game press into the rebuilt avenue. */
static void wait_for_title_start(void)
{
    static const unsigned char talk_frames[] = {7, 5, 6, 5, 8, 6};
    unsigned char message_index = TITLE_MESSAGE_COUNT - 1;
    unsigned char message_visible = 0;
    unsigned char message_timer = 240;
    unsigned char mouth_open = 0;
    unsigned char talk_index = 0;
    unsigned char talk_timer = 0;
    unsigned char released = 0;

    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        if (--message_timer == 0) {
            if (message_visible) {
                queue_title_message(TITLE_MESSAGE_HIDDEN);
                message_visible = 0;
                if (mouth_open) {
                    mouth_open = 0;
                    queue_title_mouth(0);
                }
                message_timer = 120;
            } else {
                message_index = (unsigned char)((message_index + 1) % TITLE_MESSAGE_COUNT);
                queue_title_message(message_index);
                message_visible = 1;
                talk_index = 0;
                talk_timer = 1;
                message_timer = 180;
            }
        }
        if (message_visible && --talk_timer == 0) {
            mouth_open ^= 1;
            queue_title_mouth(mouth_open);
            talk_timer = talk_frames[talk_index++];
            if (talk_index >= sizeof(talk_frames)) talk_index = 0;
        }
        if (!(read_pad() & (PAD_A | PAD_START))) {
            if (released < 4) ++released;
        } else if (released >= 4) {
            return;
        }
    }
}

static void fill_row(unsigned char row, unsigned char tile)
{
    fill_title_rect(row, 0, 32, 1, tile);
}

/* Rows 8-11 exactly -- one whole attribute block (row-block 2), cols
 * 8-23 (column-blocks 2-5) -- so every tile the neon sign palette
 * touches is actually sign art, none of it bare brick. Copy belongs to
 * the owning storefront, not this shared frame helper. */
static void draw_sign(void)
{
    set_tile(8, 8, NEON_TILE_SIGN_TL);
    fill_title_rect(8, 9, 14, 1, NEON_TILE_SIGN_T);
    set_tile(8, 23, NEON_TILE_SIGN_TR);
    set_tile(9, 8, NEON_TILE_SIGN_L);
    fill_title_rect(9, 9, 14, 1, NEON_TILE_SIGN_FILL);
    set_tile(9, 23, NEON_TILE_SIGN_R);
    set_tile(10, 8, NEON_TILE_SIGN_L);
    fill_title_rect(10, 9, 14, 1, NEON_TILE_SIGN_FILL);
    set_tile(10, 23, NEON_TILE_SIGN_R);
    set_tile(11, 8, NEON_TILE_SIGN_BL);
    fill_title_rect(11, 9, 14, 1, NEON_TILE_SIGN_B);
    set_tile(11, 23, NEON_TILE_SIGN_BR);
}

/* 4 tiles square -- exactly one attribute block -- so the blue window
 * palette never touches anything but window art either. The 2x2
 * interior reuses one NEON_TILE_WINDOW_FILL tile with a dark edge mark
 * on its own right and bottom side (see that tile's redesign in
 * neon_avenue.s): placed 2x2, those edges line up into one continuous
 * cross through the middle of the glass, no separate mullion tile
 * needed. Now sits at row-block 4 (see show_neon_avenue()) -- a
 * different row-block than the sign, which is what frees it to move in
 * from the building's own edges without needing to dodge the sign's
 * column span the way it used to. */
static void draw_window(unsigned char top_row, unsigned char left_col)
{
    unsigned char row;

    fill_title_rect(top_row, left_col, 4, 1, NEON_TILE_WINDOW_H);
    fill_title_rect(top_row + 3, left_col, 4, 1, NEON_TILE_WINDOW_H);
    for (row = top_row + 1; row < top_row + 3; ++row) {
        set_tile(row, left_col, NEON_TILE_WINDOW_V);
        fill_title_rect(row, left_col + 1, 2, 1, NEON_TILE_WINDOW_FILL);
        set_tile(row, left_col + 3, NEON_TILE_WINDOW_V);
    }
}

static void set_oam_sprite_offset(unsigned char slot, unsigned char x, int dx,
                                   unsigned char y, unsigned char tile, unsigned char attr);

static void draw_traffic_particles(unsigned char x, unsigned char moving_left)
{
    int direction = moving_left ? -1 : 1;

    /* Three near-impact flecks plus four lighter droplets thrown out toward
     * the taxi's far travel edge make the road scatter feel directional. */
    set_oam_sprite_offset(PARTICLE_OAM_BASE, x, direction * 8, 220, TAXI_PARTICLE_TILE, 2);
    set_oam_sprite_offset(PARTICLE_OAM_BASE + 1, x, direction * 24, 216, TAXI_PARTICLE_SMALL_TILE, 2);
    set_oam_sprite_offset(PARTICLE_OAM_BASE + 2, x, direction * 38, 228, TAXI_PARTICLE_DOT_TILE, 2);
    set_oam_sprite_offset(59, x, direction * 52, 212, TAXI_PARTICLE_SMALL_TILE, 2);
    set_oam_sprite_offset(60, x, direction * 66, 222, TAXI_PARTICLE_DOT_TILE, 2);
    set_oam_sprite_offset(61, x, direction * 80, 230, TAXI_PARTICLE_SMALL_TILE, 2);
    set_oam_sprite_offset(62, x, direction * 92, 218, TAXI_PARTICLE_DOT_TILE, 2);
}

/* x is unsigned char and the player's own on-screen range (12-244) puts it
 * well within reach of underflowing a naive `x + dx` for negative dx --
 * the same wraparound risk draw_taxi_sprite() had, just via subtraction
 * this time. Clamping through a signed int here is cheap and this only
 * ever runs a handful of times per crash, not per frame at scale. */
static void set_oam_sprite_offset(unsigned char slot, unsigned char x, int dx,
                                   unsigned char y, unsigned char tile, unsigned char attr)
{
    int px = (int)x + dx;
    if (px < 0) px = 0;
    if (px > 247) px = 247;
    set_oam_sprite(slot, (unsigned char)px, y, tile, attr);
}

/* Bones traffic_bones.s was authored for, plus the player's own head
 * tiles reused as a "landed nearby" gag -- both existed in CHR all along
 * but were never actually drawn during the crash sequence. */
static void draw_crash_tableau(unsigned char x, unsigned char moving_left)
{
    /* Debris follows the taxi's travel direction instead of forming a
     * centered tableau. The left/right layouts are intentionally different
     * rather than a mirrored stamp, so each impact reads like a real scatter. */
    if (moving_left) {
        set_oam_sprite_offset(51, x, -66, 220, TRAFFIC_BONE_TILE_BASE, 3);
        set_oam_sprite_offset(52, x, -58, 220, TRAFFIC_BONE_TILE_BASE + 1, 3);
        set_oam_sprite_offset(53, x, -18, 216, TRAFFIC_BONE_TILE_BASE + 2, 3);
        set_oam_sprite_offset(54, x, -38, 228, TRAFFIC_BONE_TILE_BASE + 3, 3);
        set_oam_sprite_offset(55, x,  -8, 224, TRAFFIC_BONE_TILE_BASE + 4, 3);
        set_oam_sprite_offset(56, x, -45, 210, TRAFFIC_BONE_TILE_BASE + 5, 3);
        set_oam_sprite_offset(57, x, -34, 208, PLAYER_TILE_BASE, 1);
        set_oam_sprite_offset(58, x, -26, 208, PLAYER_TILE_BASE + 1, 1);
    } else {
        set_oam_sprite_offset(51, x,  34, 220, TRAFFIC_BONE_TILE_BASE, 3);
        set_oam_sprite_offset(52, x,  42, 220, TRAFFIC_BONE_TILE_BASE + 1, 3);
        set_oam_sprite_offset(53, x,  70, 216, TRAFFIC_BONE_TILE_BASE + 2, 3);
        set_oam_sprite_offset(54, x,  46, 228, TRAFFIC_BONE_TILE_BASE + 3, 3);
        set_oam_sprite_offset(55, x,  76, 224, TRAFFIC_BONE_TILE_BASE + 4, 3);
        set_oam_sprite_offset(56, x,  30, 210, TRAFFIC_BONE_TILE_BASE + 5, 3);
        set_oam_sprite_offset(57, x,  50, 208, PLAYER_TILE_BASE, 1);
        set_oam_sprite_offset(58, x,  58, 208, PLAYER_TILE_BASE + 1, 1);
    }
}

static void draw_player_sprite(unsigned char x, unsigned char y, unsigned char walking)
{
    unsigned char left = x - 8;
    unsigned char top = y - 24;
    unsigned char legs = (unsigned char)(PLAYER_TILE_BASE + (walking ? 6 : 4));

    set_oam_sprite(24, left, top, PLAYER_TILE_BASE, 1);
    set_oam_sprite(25, left + 8, top, PLAYER_TILE_BASE + 1, 1);
    set_oam_sprite(26, left, top + 8, PLAYER_TILE_BASE + 2, 1);
    set_oam_sprite(27, left + 8, top + 8, PLAYER_TILE_BASE + 3, 1);
    set_oam_sprite(28, left, top + 16, legs, 1);
    set_oam_sprite(29, left + 8, top + 16, legs + 1, 1);
}

static void hide_player_sprite(void)
{
    unsigned char i;
    for (i = 24; i < 30; ++i) oam_buf[(unsigned int)i * 4] = 0xFF;
}

static void draw_taxi_sprite(int x, unsigned char moving_left)
{
    unsigned char row, col;
    unsigned char slot = 32;
    unsigned char attr = moving_left ? 3 : 0x43;
    /* Matches the player's own sprite top (y - 24) at the y >= 214 crash
     * threshold in run_neon_avenue(), so the 24px-tall taxi lines up with
     * the 24px-tall player instead of only clipping its feet. The crash
     * check itself is still X-only; this is purely so the two actually
     * look like they collide. */
    unsigned char base_y = 190;

    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 4; ++col) {
            /* x is a signed int specifically so the taxi can drive
             * partway off either screen edge; each 8px column has to be
             * bounds-checked (and hidden individually once it's past
             * the edge) before narrowing to the unsigned char OAM byte
             * expects. Skipping this check let a negative column value
             * wrap around to a large positive one instead -- the taxi
             * visibly jumping to the opposite edge instead of just
             * driving off this one. */
            int col_x = x + col * 8;
            if (col_x < 0 || col_x > 247) {
                oam_buf[(unsigned int)slot * 4] = 0xFF;
                ++slot;
            } else {
                set_oam_sprite(slot++, (unsigned char)col_x,
                               (unsigned char)(base_y + row * 8),
                               /* Alternate the two checker-trim phases on
                                * the bottom row, with no tiny tire sprites. */
                               (unsigned char)(row == 2 ? TAXI_TILE_BASE + 8 + (col & 1)
                                                        : TAXI_TILE_BASE + row * 4 + col),
                               attr);
            }
        }
    }
}

static void hide_taxi_sprite(void)
{
    unsigned char i;
    for (i = 32; i < 44; ++i) oam_buf[(unsigned int)i * 4] = 0xFF;
}

static void show_traffic_death(void)
{
    unsigned char i;

    PPU.mask = 0;
    /* Palette 1 belongs to the street facade, so changing it recolors the
     * Mart. Palette 3 only draws the window areas, all of which this panel
     * covers, making it safe to borrow for one consistent death-box style. */
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x0C;
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x16;
    PPU.vram.data = 0x06;
    PPU.vram.data = 0x30;
    /* A 24x8 plate at cols 4-27 / rows 12-19 exactly fills attribute
     * blocks 1-6 and rows 3-4. Keeping both axes aligned prevents the old
     * white/yellow street palettes from leaking into either side. */
    draw_title_plate(12, 4, 24, 8);
    for (i = 1; i < 7; ++i) set_attribute_block(3, i, 3);
    for (i = 1; i < 7; ++i) set_attribute_block(4, i, 3);
    draw_text(14, 6, "YOU CAN'T JUST WALK");
    draw_text(16, 9, "INTO TRAFFIC.");
    reset_scroll();
    PPU.mask = 0x1E;
}

static void show_neon_avenue(void)
{
    /* pal1 (bytes 4-7) is the plain brick wall. pal2 (bytes 8-11) is the
     * sign's own palette -- neon_avenue.s's sign-tile comment specifies
     * "index 1 is pink trim, index 2 is black fill, index 3 is the
     * font's white ink," so that's exactly what bytes 9-11 are now (the
     * previous pass set index 2 to a second pink instead of black,
     * which is why the sign read as solid pink rather than a black
     * plate with pink trim). pal3 (bytes 12-15) is the window glass --
     * already a genuine blue, already labeled "window light" in that
     * file's header comment, just never actually painted onto the fill
     * tile itself before now (see NEON_TILE_WINDOW_FILL's redesign).
     * pal0's index 3 (byte 3) is CHR_TILE_PATH's color -- CHR_TILE_PATH
     * is solid index 3, so it was rendering the sky strip and the side
     * margins solid white; now it's black, reading as open night sky
     * on either side of the building instead. */
    static const unsigned char avenue_palette[32] = {
        0x0F, 0x01, 0x28, 0x0F, 0x0F, 0x16, 0x06, 0x30,
        0x0F, 0x14, 0x0F, 0x30, 0x0F, 0x16, 0x21, 0x30,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x30, 0x27, 0x16,
        0x0F, 0x16, 0x06, 0x30, 0x0F, 0x0F, 0x28, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    /* The title NMI owns its queued PPU writes. Clear the queue before this
     * nametable becomes visible so it cannot patch a mouth into the street. */
    title_message_dirty = 0;
    title_mouth_dirty = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = avenue_palette[i];

    for (i = 0; i < 3; ++i) fill_row(i, CHR_TILE_PATH);
    /* The building is narrower than the screen now -- cols 4-27 (col-
     * blocks 1-6), not the full 0-31 -- leaving a margin on each side
     * (col-blocks 0 and 7) for whatever goes there later. The margin
     * keeps showing CHR_TILE_PATH (the sky tile) all the way down to
     * street level, like open lot on either side of the building,
     * rather than an untextured gap. Sidewalk/asphalt stay full width
     * below, since the street itself isn't narrower, just the facade. */
    for (i = 3; i < 24; ++i) {
        fill_title_rect(i, 0, 4, 1, CHR_TILE_PATH);
        fill_title_rect(i, 4, 24, 1, NEON_TILE_BRICK);
        fill_title_rect(i, 28, 4, 1, CHR_TILE_PATH);
    }
    for (i = 24; i < 26; ++i) fill_row(i, NEON_TILE_SIDEWALK);
    for (i = 26; i < 30; ++i) fill_row(i, NEON_TILE_ASPHALT);
    fill_title_rect(3, 4, 24, 1, NEON_TILE_AWNING);
    draw_sign();
    draw_text(9, 10, "JIMBLES'S BAR");
    /* The one brick row beneath the plate keeps the sign from crowding the
     * windows. Windows cross two attribute blocks here; palette 3's index-1
     * is intentionally the same red as brick, so the surrounding wall stays
     * red while each window's index-2 glass remains blue. */
    draw_window(13, 8);
    draw_window(13, 20);
    for (i = 17; i < 24; ++i) {
        set_tile(i, 14, NEON_TILE_DOOR_L);
        set_tile(i, 15, NEON_TILE_DOOR_C);
        set_tile(i, 16, NEON_TILE_DOOR_R);
    }
    /* A dedicated background tile retains the door's red field outside the
     * white circle, unlike a transparent sprite or coin tile over black. */
    set_tile(20, 16, DOORKNOB_TILE);
    set_tile(MANHOLE_TOP, MANHOLE_LEFT, MANHOLE_TILE_L);
    set_tile(MANHOLE_TOP, MANHOLE_LEFT + 1, MANHOLE_TILE_M);
    set_tile(MANHOLE_TOP, MANHOLE_LEFT + 2, MANHOLE_TILE_M);
    set_tile(MANHOLE_TOP, MANHOLE_LEFT + 3, MANHOLE_TILE_R);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    /* Margin columns (block 0 and 7) stay the sky palette through the
     * full wall height, matching the CHR_TILE_PATH filling them. */
    for (i = 1; i <= 5; ++i) {
        set_attribute_block(i, 0, 0);
        set_attribute_block(i, 7, 0);
    }
    /* Plain wall (pal1) across the building's own column-blocks (1-6)... */
    for (i = 1; i < 7; ++i) {
        set_attribute_block(1, i, 1);
        set_attribute_block(2, i, 1);
        set_attribute_block(3, i, 1);
        set_attribute_block(4, i, 1);
        set_attribute_block(5, i, 1);
    }
    /* ...except the sign's exact span (row-block 2, column-blocks 2-5)
     * gets the pink sign palette... */
    for (i = 2; i < 6; ++i) set_attribute_block(2, i, 2);
    /* ...and each window's exact span (row-block 3, column-block 2 or
     * 5) gets the blue window palette. Every block any of these three
     * touch is now either 100% sign, 100% window, or 100% plain brick --
     * never a mix, so none of them can bleed into each other or into
     * the bare wall the way the sign's halo used to. */
    set_attribute_block(3, 2, 3);
    set_attribute_block(3, 5, 3);
    set_attribute_block(4, 2, 3);
    set_attribute_block(4, 5, 3);
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);
    clear_sprites();
    /* clear_sprites() only wipes the RAM shadow buffer -- without this,
     * the *real* PPU OAM still holds whatever the previous scene last
     * DMA'd (Jimbles, the crash-tableau bones, the taxi...) until the
     * next natural oam_flush() catches up, which briefly shows the old
     * scene's sprites stacked on this one's background. show_title_screen()
     * already gets this right; this call was missing it. */
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

static unsigned char run_neon_avenue(unsigned char arrival)
{
    unsigned char x = arrival == 1 ? MANHOLE_X : (arrival == 2 ? 24 : 128);
    /* Return with the player's feet below the facade and just north of the
     * cover. This leaves a deliberate step before the sewer trigger. */
    unsigned char y = arrival == 1 ? 196 : 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;
    unsigned char traffic_state = 0;
    unsigned char taxi_moving_left = 0;
    unsigned char crash_timer = 0;
    unsigned char death_released = 0;
    int taxi_x = 0;

    show_neon_avenue();
    for (;;) {
        waitvsync();
        /* Flush whatever the *previous* iteration computed first, before
         * any of this frame's input/movement/drawing work -- so the
         * DMA's timing never depends on how much work this particular
         * frame happens to need (a few pixels of movement vs. the
         * taxi's 12-sprite redraw vs. a state transition). Doing this
         * at the end of the loop instead (the original order here) only
         * stays inside vblank if that frame's work happens to be short
         * enough, which is exactly why the player/taxi sprites never
         * appeared during normal play while the traffic-death screen
         * (comparatively little per-frame work) rendered fine -- same
         * root cause as oam_flush()'s own comment already documents,
         * just triggered by ordering instead of a redundant write. */
        reset_scroll();
        oam_flush();

        pad = read_pad();
        if (traffic_state == 0) {
            if (pad & PAD_START) {
                run_inventory();
                show_neon_avenue();
                continue;
            }
            if (pad & PAD_LEFT) {
                if (x > 12) --x;
                else return AVENUE_EXIT_MART;
            } else if (pad & PAD_RIGHT) {
                if (x < 244) ++x;
            }
            if (pad & PAD_UP) {
                /* The sprite's feet can approach the facade, but never walk
                 * through it -- except through the doorway gap (cols 14-16,
                 * pixels 112-135 -- see show_neon_avenue()'s door tiles),
                 * where walking further in now transitions into the bar
                 * instead of just stopping at the wall. */
                if (y > 190) {
                    --y;
                } else if (x >= 104 && x <= 144) {
                    return AVENUE_EXIT_BAR;
                }
            } else if (pad & PAD_DOWN) {
                if (y < 216) ++y;
            }
            if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
                if (++walk_timer == 8) {
                    walk_timer = 0;
                    walk_frame ^= 1;
                }
            } else {
                walk_timer = 0;
                walk_frame = 0;
            }
            if (y >= 214) {
                traffic_state = 1;
                taxi_moving_left = x < 128;
                taxi_x = taxi_moving_left ? 224 : 0;
            }
            /* The player coordinate is their feet, so require those feet to
             * be on the visible center of the cover, not merely near it. */
            if (x >= MANHOLE_X - 12 && x <= MANHOLE_X + 12 &&
                y >= MANHOLE_Y && y <= MANHOLE_Y + 8) {
                return AVENUE_EXIT_SEWER;
            }
            draw_player_sprite(x, y, walk_frame);
        } else if (traffic_state == 1 || traffic_state == 2) {
            taxi_x += taxi_moving_left ? -3 : 3;
            if (taxi_x > -32 && taxi_x < 256) draw_taxi_sprite(taxi_x, taxi_moving_left);
            else hide_taxi_sprite();
            if (traffic_state == 1 && taxi_x <= (int)x + 8 && taxi_x + 32 >= (int)x - 8) {
                traffic_state = 2;
                crash_timer = 90;
                hide_player_sprite();
                draw_traffic_particles(x, taxi_moving_left);
                draw_crash_tableau(x, taxi_moving_left);
                music_sfx_hurt();
            }
            if (traffic_state == 2) {
                draw_traffic_particles(x, taxi_moving_left);
                draw_crash_tableau(x, taxi_moving_left);
                if (--crash_timer == 0) {
                    traffic_state = 3;
                    clear_sprites();
                    draw_traffic_particles(x, taxi_moving_left);
                    draw_crash_tableau(x, taxi_moving_left);
                    show_traffic_death();
                }
            }
        } else {
            if (!(pad & PAD_A)) {
                if (death_released < 4) ++death_released;
            } else if (death_released >= 4) {
                x = 128;
                y = 200;
                traffic_state = 0;
                death_released = 0;
                show_neon_avenue();
            }
        }
    }
}

/* All-Nite Mart sits one screen west of Jimbles's Bar. It shares the avenue
 * tile set, but uses the same full-height storefront treatment as the bar so
 * the two screens feel like parts of one intentional street. */
static void show_mart_exterior(void)
{
    static const unsigned char mart_palette[32] = {
        0x0F, 0x01, 0x28, 0x0F,
        /* The shared running-bond tile becomes gray cinder block here;
         * its awning uses palette entries 2 and 3 as a steel/white stripe. */
        0x0F, 0x00, 0x10, 0x30,
        /* The plate art maps index 1 to trim and index 2 to its black
         * center, so this makes the Mart's sign a green neon plate. */
        0x0F, 0x2A, 0x0F, 0x30,
        /* Window blocks also contain a little surrounding wall. Keep
         * index 1 the same gray as the cinder block, leaving only the
         * glass itself blue instead of creating blue masonry artifacts. */
        0x0F, 0x00, 0x21, 0x30,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x30, 0x27, 0x16,
        0x0F, 0x16, 0x06, 0x30, 0x0F, 0x0F, 0x28, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = mart_palette[i];
    for (i = 0; i < 3; ++i) fill_row(i, CHR_TILE_PATH);
    for (i = 3; i < 24; ++i) {
        fill_title_rect(i, 0, 4, 1, CHR_TILE_PATH);
        fill_title_rect(i, 4, 24, 1, NEON_TILE_BRICK);
        fill_title_rect(i, 28, 4, 1, CHR_TILE_PATH);
    }
    for (i = 24; i < 26; ++i) fill_row(i, NEON_TILE_SIDEWALK);
    for (i = 26; i < 30; ++i) fill_row(i, NEON_TILE_ASPHALT);
    fill_title_rect(3, 4, 24, 1, NEON_TILE_AWNING);
    /* Use the avenue's rounded neon storefront sign rather than the broad
     * title-menu plate so the Mart and bar share a consistent sign language. */
    draw_sign();
    draw_text(9, 9, "ALL-NITE MART");
    draw_window(13, 8);
    draw_window(13, 20);
    for (i = 17; i < 24; ++i) {
        set_tile(i, 14, NEON_TILE_DOOR_L);
        set_tile(i, 15, NEON_TILE_DOOR_C);
        set_tile(i, 16, NEON_TILE_DOOR_R);
    }
    set_tile(20, 16, DOORKNOB_TILE);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 1; i <= 5; ++i) {
        set_attribute_block(i, 0, 0);
        set_attribute_block(i, 7, 0);
    }
    for (i = 1; i < 7; ++i) {
        set_attribute_block(1, i, 1);
        set_attribute_block(2, i, 1);
        set_attribute_block(3, i, 1);
        set_attribute_block(4, i, 1);
        set_attribute_block(5, i, 1);
    }
    for (i = 2; i < 6; ++i) set_attribute_block(2, i, 2);
    set_attribute_block(3, 2, 3);
    set_attribute_block(3, 5, 3);
    set_attribute_block(4, 2, 3);
    set_attribute_block(4, 5, 3);
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

static unsigned char run_mart_exterior(unsigned char from_mart)
{
    /* Street travel arrives at the east edge; leaving the store returns
     * directly beneath its centered front door. */
    unsigned char x = from_mart ? 128 : 232;
    unsigned char y = from_mart ? 196 : 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;
    unsigned char traffic_state = 0;
    unsigned char taxi_moving_left = 0;
    unsigned char crash_timer = 0;
    unsigned char death_released = 0;
    int taxi_x = 0;

    show_mart_exterior();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if (traffic_state == 0) {
            if (pad & PAD_START) {
                run_inventory();
                show_mart_exterior();
                continue;
            }
            if (pad & PAD_LEFT) {
                if (x > 12) --x;
            } else if (pad & PAD_RIGHT) {
                if (x < 244) ++x;
                else return MART_EXIT_AVENUE;
            }
            if (pad & PAD_UP) {
                if (y > 190) --y;
                else if (x >= 104 && x <= 144) return MART_EXIT_INTERIOR;
            } else if (pad & PAD_DOWN) {
                if (y < 216) ++y;
            }
            if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
                if (++walk_timer == 8) {
                    walk_timer = 0;
                    walk_frame ^= 1;
                }
            } else {
                walk_timer = 0;
                walk_frame = 0;
            }
            if (y >= 214) {
                traffic_state = 1;
                taxi_moving_left = x < 128;
                taxi_x = taxi_moving_left ? 224 : 0;
            }
            draw_player_sprite(x, y, walk_frame);
        } else if (traffic_state == 1 || traffic_state == 2) {
            taxi_x += taxi_moving_left ? -3 : 3;
            if (taxi_x > -32 && taxi_x < 256) draw_taxi_sprite(taxi_x, taxi_moving_left);
            else hide_taxi_sprite();
            if (traffic_state == 1 && taxi_x <= (int)x + 8 && taxi_x + 32 >= (int)x - 8) {
                traffic_state = 2;
                crash_timer = 90;
                hide_player_sprite();
                draw_traffic_particles(x, taxi_moving_left);
                draw_crash_tableau(x, taxi_moving_left);
                music_sfx_hurt();
            }
            if (traffic_state == 2) {
                draw_traffic_particles(x, taxi_moving_left);
                draw_crash_tableau(x, taxi_moving_left);
                if (--crash_timer == 0) {
                    traffic_state = 3;
                    clear_sprites();
                    draw_traffic_particles(x, taxi_moving_left);
                    draw_crash_tableau(x, taxi_moving_left);
                    show_traffic_death();
                }
            }
        } else {
            if (!(pad & PAD_A)) {
                if (death_released < 4) ++death_released;
            } else if (death_released >= 4) {
                /* A street death at the mart sends the player back to the
                 * bar-side avenue entry, just like leaving the mart west. */
                return MART_EXIT_AVENUE;
            }
        }
    }
}

/* A compact store floor: dense stock walls frame a clear center aisle, while
 * a clerk and register make the north counter read as an actual checkout. */
static void show_mart_interior(void)
{
    static const unsigned char mart_interior_palette[32] = {
        0x0F, 0x16, 0x27, 0x30,
        /* BG palette 1 is reserved for the temporary navy dialogue box. */
        0x0F, 0x00, 0x10, 0x30,
        /* The header gets its own stable copy of the cinder-block gray so
         * it never changes color while palette 1 is repainted for dialogue. */
        0x0F, 0x00, 0x10, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x28, 0x21, 0x30,
        0x0F, 0x16, 0x06, 0x30, 0x0F, 0x0F, 0x0F, 0x0F
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = mart_interior_palette[i];
    for (i = 4; i < 27; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 4, BAR_TILE_WALL);
    fill_title_rect(27, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(0, 0, 2, 30, BAR_TILE_WALL);
    fill_title_rect(0, 30, 2, 30, BAR_TILE_WALL);
    draw_title_plate(0, 4, 24, 4);
    draw_text(1, 9, "ALL-NITE MART");
    /* Nine short shelf runs on each wall suggest stocked aisles without
     * crowding the player out of the broad middle path. */
    for (i = 7; i <= 23; i += 2) {
        fill_title_rect(i, 3, 6, 1, BAR_TILE_SHELF);
        fill_title_rect(i, 23, 6, 1, BAR_TILE_SHELF);
    }
    fill_title_rect(5, 8, 16, 1, BAR_TILE_COUNTER);
    /* Reuse the large player portrait tiles as the clerk's face above the
     * counter; the counter hides the rest of the body naturally. */
    set_tile(4, 14, PLAYER_TILE_BASE);
    set_tile(4, 15, (unsigned char)(PLAYER_TILE_BASE + 1));
    set_tile(4, 18, MART_TILE_REGISTER);
    for (i = 27; i < 30; ++i) {
        set_tile(i, 14, BAR_TILE_DOOR_JAMB);
        set_tile(i, 15, BAR_TILE_DOOR_FILL);
        set_tile(i, 16, BAR_TILE_DOOR_JAMB);
    }
    for (i = 0; i < 8; ++i) {
        set_attribute_block(0, i, 2);
        set_attribute_block(1, i, 0);
        set_attribute_block(2, i, 0);
        set_attribute_block(3, i, 0);
        set_attribute_block(4, i, 0);
        set_attribute_block(5, i, 0);
        set_attribute_block(6, i, 0);
        set_attribute_block(7, i, 0);
    }
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The player position is the sprite's feet. Keep its full 16x24 body out of
 * each one-tile-high stock run, while leaving the gaps between runs walkable. */
static unsigned char mart_shelf_blocks(unsigned char x, unsigned char y)
{
    unsigned char shelf_row;
    unsigned char left = (unsigned char)(x - 8);
    unsigned char right = (unsigned char)(x + 7);
    unsigned char top = (unsigned char)(y - 24);
    unsigned char bottom = (unsigned char)(y - 1);

    for (shelf_row = 7; shelf_row <= 23; shelf_row += 2) {
        unsigned char shelf_top = (unsigned char)(shelf_row << 3);
        unsigned char shelf_bottom = (unsigned char)(shelf_top + 7);

        if (bottom >= shelf_top && top <= shelf_bottom &&
            ((left <= 71) || (right >= 184))) return 1;
    }
    return 0;
}

static void run_mart_interior(void)
{
    unsigned char x = 128;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;
    unsigned char cashier_prompted = 0;
    unsigned char register_prompted = 0;
    unsigned char shelf_prompted = 0;
    unsigned char shelf_dialogue = 0;

    show_mart_interior();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if (pad & PAD_START) {
            run_inventory();
            show_mart_interior();
            continue;
        }
        /* Releasing A re-arms an interaction in place. This keeps the
         * one-press latch needed for dialogue dismissal, without forcing the
         * player to walk away and back before speaking again. */
        if (!(pad & PAD_A)) {
            cashier_prompted = 0;
            register_prompted = 0;
            shelf_prompted = 0;
        }
        if (pad & PAD_LEFT) {
            if (x > 20 && !mart_shelf_blocks((unsigned char)(x - 1), y)) --x;
        } else if (pad & PAD_RIGHT) {
            if (x < 236 && !mart_shelf_blocks((unsigned char)(x + 1), y)) ++x;
        }
        if (pad & PAD_UP) {
            if (y > 72 && !mart_shelf_blocks(x, (unsigned char)(y - 1))) --y;
        } else if (pad & PAD_DOWN) {
            if (y < 216 && !mart_shelf_blocks(x, (unsigned char)(y + 1))) ++y;
            else if (x >= 104 && x <= 144) return;
        }
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if (++walk_timer == 8) {
                walk_timer = 0;
                walk_frame ^= 1;
            }
        } else {
            walk_timer = 0;
            walk_frame = 0;
        }
        /* The clerk sits behind the north checkout. Like the bar's
         * interactions, this fires once per approach rather than once per
         * frame while A is held. */
        if (x >= 96 && x <= 135 && y >= 72 && y <= 96) {
            if ((pad & PAD_A) && !cashier_prompted) {
                cashier_prompted = 1;
                show_mart_message("CASHIER: YOU BUYIN'?", "OR JUST MAKING EYES?", 0);
                show_mart_interior();
            }
        } else {
            cashier_prompted = 0;
        }
        /* The register is intentionally its own hotspot on the checkout's
         * right side, so the paper clip pickup cannot be confused with the
         * cashier conversation. */
        if (x >= 136 && x <= 176 && y >= 72 && y <= 96) {
            if ((pad & PAD_A) && !register_prompted) {
                register_prompted = 1;
                if (!has_paper_clip) {
                    has_paper_clip = 1;
                    show_mart_message("YOU SPOT A PAPER CLIP",
                                      "NEAR THE REGISTER AND", "TAKE IT.");
                    show_mart_interior();
                }
            }
        } else {
            register_prompted = 0;
        }
        /* The side-wall stock runs are flavor-only hotspots. Keep them clear
         * of the checkout range and rotate a few observations so browsing
         * the shelves does not become a one-line dead end. */
        /* Shelf collision stops the player's body just outside the stock
         * runs, so the interaction halo reaches those valid approach spots. */
        if ((x <= 88 || x >= 168) && y >= 48 && y <= 208) {
            if ((pad & PAD_A) && !shelf_prompted) {
                shelf_prompted = 1;
                if (shelf_dialogue == 0) {
                    show_mart_message("SOME KIND OF", "MYSTERY PUDDING.", 0);
                } else if (shelf_dialogue == 1) {
                    show_mart_message("THE CHIPS ARE ALL", "SUSPICIOUSLY WARM.", 0);
                } else {
                    show_mart_message("A MAGAZINE PROMISES", "TO FIX YOUR LIFE.", 0);
                }
                shelf_dialogue = (unsigned char)((shelf_dialogue + 1) % 3);
                show_mart_interior();
            }
        } else {
            shelf_prompted = 0;
        }
        draw_player_sprite(x, y, walk_frame);
    }
}

/* A compact first sewer chamber. The shared floor/wall/asphalt/sidewalk and
 * window-edge tiles become damp stone, black water, and a ladder under a new
 * palette; dedicated sewer art can replace these once scene CHR banks exist. */
static void show_sewer(void)
{
    static const unsigned char sewer_palette[32] = {
        0x0F, 0x01, 0x21, 0x30, 0x0F, 0x00, 0x10, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x28, 0x21, 0x30,
        0x0F, 0x16, 0x06, 0x30, 0x0F, 0x0F, 0x0F, 0x0F
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = sewer_palette[i];

    for (i = 3; i < 24; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(24, 0, 32, 6, NEON_TILE_ASPHALT);
    fill_title_rect(0, 0, 2, 30, BAR_TILE_WALL);
    fill_title_rect(0, 30, 2, 30, BAR_TILE_WALL);
    for (i = 3; i < 24; ++i) {
        set_tile(i, 14, NEON_TILE_WINDOW_V);
        set_tile(i, 17, NEON_TILE_WINDOW_V);
        if ((i & 1) == 0) {
            set_tile(i, 15, NEON_TILE_SIDEWALK);
            set_tile(i, 16, NEON_TILE_SIDEWALK);
        }
    }
    draw_text(1, 11, "SEWER");
    for (i = 0; i < 8; ++i) {
        set_attribute_block(0, i, 1);
        set_attribute_block(1, i, 0);
        set_attribute_block(2, i, 0);
        set_attribute_block(3, i, 0);
        set_attribute_block(4, i, 0);
        set_attribute_block(5, i, 0);
        set_attribute_block(6, i, 2);
        set_attribute_block(7, i, 2);
    }
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The ladder is intentionally a visual route rather than a one-tile-only
 * collision puzzle for this first room: explore the dry floor, then return
 * to the opening at its top. The water channel is the southern boundary. */
static void run_sewer(void)
{
    unsigned char x = 128;
    unsigned char y = 56;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;

    show_sewer();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();

        if (pad & PAD_START) {
            run_inventory();
            show_sewer();
            continue;
        }

        if (pad & PAD_LEFT) {
            if (x > 20) --x;
        } else if (pad & PAD_RIGHT) {
            if (x < 236) ++x;
        }
        if (pad & PAD_UP) {
            if (y > 48) --y;
            else if (x >= 104 && x <= 152) return;
        } else if (pad & PAD_DOWN) {
            if (y < 208) ++y;
        }
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if (++walk_timer == 8) {
                walk_timer = 0;
                walk_frame ^= 1;
            }
        } else {
            walk_timer = 0;
            walk_frame = 0;
        }
        draw_player_sprite(x, y, walk_frame);
    }
}

/* Jimbles: a sentient pint standing behind his own bar counter. Only two
 * tiles are authored (see JIMBLES_TILE_TOP/BOTTOM's comment); OAM
 * mirrors each horizontally (attr bit 0x40) for the right half, and his
 * sprite's bottom edge is positioned to land exactly where the counter's
 * background tile begins, so there's no third ("hidden") row to draw --
 * the counter simply starts right where he ends.
 *
 * Only the front-facing pose exists -- CHR budget is fully spent (see
 * TOKEN_TILE's comment), so a real side-profile walk or a back-facing
 * "mixing a drink" pose would need new art there's no room for right
 * now. run_bar_interior()'s idle animation works around that by sliding
 * this same front sprite left and right instead, rather than a true
 * walk cycle. Reads jimbles_x (file-scope, see its comment) instead of
 * a fixed position so that animation can drive it. */
static void draw_jimbles(void)
{
    set_oam_sprite(44, jimbles_x, 48, JIMBLES_TILE_TOP, 2);
    set_oam_sprite(45, (unsigned char)(jimbles_x + 8), 48, JIMBLES_TILE_TOP, 2 | 0x40);
    set_oam_sprite(46, jimbles_x, 56, JIMBLES_TILE_BOTTOM, 2);
    set_oam_sprite(47, (unsigned char)(jimbles_x + 8), 56, JIMBLES_TILE_BOTTOM_RIGHT, 2);
}

/* Two short, slumped regulars: they only need head-and-shoulder tiles while
 * standing in the crowded corner, so the lower half stays visually buried in
 * the dark floor/puddle rather than consuming another four OAM slots each. */
static unsigned char piss_regular_one_x;
static unsigned char piss_regular_one_y;
static unsigned char piss_regular_two_x;
static unsigned char piss_regular_two_y;
static unsigned char piss_regular_one_timer;
static unsigned char piss_regular_two_timer;
static unsigned char piss_regular_one_steps;
static unsigned char piss_regular_two_steps;
static unsigned char piss_regular_one_direction;
static unsigned char piss_regular_two_direction;
static unsigned char piss_regular_one_facing_up;
static unsigned char piss_regular_two_facing_up;
static unsigned char piss_regular_seed = 0x6D;
static RngState piss_regular_rng;

static void reset_piss_corner_regulars(void)
{
    piss_regular_one_x = 200;
    piss_regular_one_y = 168;
    piss_regular_two_x = 224;
    piss_regular_two_y = 184;
    piss_regular_one_timer = 74;
    piss_regular_two_timer = 141;
    piss_regular_one_steps = 0;
    piss_regular_two_steps = 0;
    piss_regular_one_facing_up = 0;
    piss_regular_two_facing_up = 0;
    /* Change the LFSR stream each bar visit, even before we later add a
     * controller-timing entropy source, so the nook does not restart in a
     * visibly scripted pose after every scene transition. */
    piss_regular_seed = (unsigned char)(piss_regular_seed + 0x3D);
    rng_seed(&piss_regular_rng, piss_regular_seed);
}

/* One independently timed, short shuffle. The direction changes per burst;
 * on reaching a boundary it naturally reverses, keeping all 16x16 sprite
 * pixels within the caller's compact Piss Corner rectangle. */
static void update_piss_regular(unsigned char *x, unsigned char *y,
                                unsigned char *timer, unsigned char *steps,
                                unsigned char *direction, unsigned char *facing_up,
                                unsigned char min_x, unsigned char max_x,
                                unsigned char min_y, unsigned char max_y)
{
    unsigned char roll;

    if (*timer) {
        --*timer;
    } else if (*steps) {
        switch (*direction) {
            case 0:
                if (*x < max_x) ++*x;
                else --*x;
                *facing_up = 0;
                break;
            case 1:
                if (*x > min_x) --*x;
                else ++*x;
                *facing_up = 0;
                break;
            case 2:
                if (*y < max_y) ++*y;
                else --*y;
                *facing_up = 0;
                break;
            default:
                if (*y > min_y) {
                    --*y;
                    *facing_up = 1;
                } else {
                    ++*y;
                    *facing_up = 0;
                }
                break;
        }
        --*steps;
        *timer = (unsigned char)(2 + (rng_next(&piss_regular_rng) & 3));
    } else {
        roll = rng_next(&piss_regular_rng);
        *direction = roll & 3;
        *steps = (unsigned char)(2 + ((roll >> 2) & 3));
        *timer = (unsigned char)(45 + (rng_next(&piss_regular_rng) & 0x7F));
    }
}

static void update_piss_corner_regulars(void)
{
    update_piss_regular(&piss_regular_one_x, &piss_regular_one_y,
                        &piss_regular_one_timer, &piss_regular_one_steps,
                        &piss_regular_one_direction, &piss_regular_one_facing_up,
                        200, 216, 168, 184);
    update_piss_regular(&piss_regular_two_x, &piss_regular_two_y,
                        &piss_regular_two_timer, &piss_regular_two_steps,
                        &piss_regular_two_direction, &piss_regular_two_facing_up,
                        224, 232, 176, 192);
}

static void draw_piss_corner_regulars(void)
{
    if (piss_regular_one_facing_up) {
        set_oam_sprite(30, piss_regular_one_x, piss_regular_one_y, BAR_PATRON_BACK_TOP, 1);
        set_oam_sprite(31, (unsigned char)(piss_regular_one_x + 8), piss_regular_one_y,
                       BAR_PATRON_BACK_TOP, 1 | 0x40);
        set_oam_sprite(32, piss_regular_one_x, (unsigned char)(piss_regular_one_y + 8),
                       BAR_PATRON_BACK_BOTTOM, 1);
        set_oam_sprite(33, (unsigned char)(piss_regular_one_x + 8),
                       (unsigned char)(piss_regular_one_y + 8), BAR_PATRON_BACK_BOTTOM, 1 | 0x40);
    } else {
        set_oam_sprite(30, piss_regular_one_x, piss_regular_one_y, PLAYER_TILE_BASE, 1);
        set_oam_sprite(31, (unsigned char)(piss_regular_one_x + 8), piss_regular_one_y, PLAYER_TILE_BASE + 1, 1);
        set_oam_sprite(32, piss_regular_one_x, (unsigned char)(piss_regular_one_y + 8), PLAYER_TILE_BASE + 2, 1);
        set_oam_sprite(33, (unsigned char)(piss_regular_one_x + 8), (unsigned char)(piss_regular_one_y + 8), PLAYER_TILE_BASE + 3, 1);
    }

    /* Horizontal mirroring needs both the OAM flip bit and tile positions
     * swapped; flipping each half in place reverses the face at the seam. */
    if (piss_regular_two_facing_up) {
        /* A back view is symmetric: use the same proper left/right pair
         * rather than horizontally mirroring the entire person twice. */
        set_oam_sprite(34, piss_regular_two_x, piss_regular_two_y, BAR_PATRON_BACK_TOP, 1);
        set_oam_sprite(35, (unsigned char)(piss_regular_two_x + 8), piss_regular_two_y,
                       BAR_PATRON_BACK_TOP, 1 | 0x40);
        set_oam_sprite(36, piss_regular_two_x, (unsigned char)(piss_regular_two_y + 8),
                       BAR_PATRON_BACK_BOTTOM, 1);
        set_oam_sprite(37, (unsigned char)(piss_regular_two_x + 8),
                       (unsigned char)(piss_regular_two_y + 8), BAR_PATRON_BACK_BOTTOM, 1 | 0x40);
    } else {
        set_oam_sprite(34, piss_regular_two_x, piss_regular_two_y, PLAYER_TILE_BASE + 1, 1 | 0x40);
        set_oam_sprite(35, (unsigned char)(piss_regular_two_x + 8), piss_regular_two_y, PLAYER_TILE_BASE, 1 | 0x40);
        set_oam_sprite(36, piss_regular_two_x, (unsigned char)(piss_regular_two_y + 8), PLAYER_TILE_BASE + 3, 1 | 0x40);
        set_oam_sprite(37, (unsigned char)(piss_regular_two_x + 8), (unsigned char)(piss_regular_two_y + 8), PLAYER_TILE_BASE + 2, 1 | 0x40);
    }

    /* Black-only overlay tiles sit in lower OAM slots than the base patrons,
     * making their eyes and mouths visibly win the NES sprite priority tie.
     * Back-facing poses intentionally have no facial overlay. */
    if (piss_regular_one_facing_up) {
        hide_oam_sprite(PISS_FACE_OAM_BASE);
        hide_oam_sprite(PISS_FACE_OAM_BASE + 1);
    } else {
        set_oam_sprite(PISS_FACE_OAM_BASE, piss_regular_one_x, piss_regular_one_y,
                       PISS_FACE_FEATURE_TILE, 3);
        set_oam_sprite(PISS_FACE_OAM_BASE + 1, (unsigned char)(piss_regular_one_x + 8),
                       piss_regular_one_y, PISS_FACE_FEATURE_TILE, 3 | 0x40);
    }
    if (piss_regular_two_facing_up) {
        hide_oam_sprite(PISS_FACE_OAM_BASE + 2);
        hide_oam_sprite(PISS_FACE_OAM_BASE + 3);
    } else {
        set_oam_sprite(PISS_FACE_OAM_BASE + 2, piss_regular_two_x, piss_regular_two_y,
                       PISS_FACE_FEATURE_TILE, 3);
        set_oam_sprite(PISS_FACE_OAM_BASE + 3, (unsigned char)(piss_regular_two_x + 8),
                       piss_regular_two_y, PISS_FACE_FEATURE_TILE, 3 | 0x40);
    }
}

/* The seated bar regular has the same compact 16px silhouette as the Piss
 * Corner patrons, but faces the counter from the floor in front of it. */
static void draw_bar_patron(void)
{
    set_oam_sprite(38, 80, 72, BAR_PATRON_BACK_TOP, 1);
    set_oam_sprite(39, 88, 72, BAR_PATRON_BACK_TOP, 1 | 0x40);
    set_oam_sprite(40, 80, 80, BAR_PATRON_BACK_BOTTOM, 1);
    set_oam_sprite(41, 88, 80, BAR_PATRON_BACK_BOTTOM, 1 | 0x40);
}

/* Sitting on the floor by the jukebox -- see run_bar_interior() for the
 * walk-over pickup and the jukebox-proximity spend. Static once drawn
 * (like Jimbles), not redrawn every frame, since nothing about it
 * animates; the pickup just hides its one OAM slot directly.
 *
 * Deliberately well clear of the jukebox's own interact zone (x 8-48,
 * y 88-128, in run_bar_interior()) -- picking the token up used to also
 * satisfy the jukebox's has_token/proximity/A check in the very same
 * frame, since the two zones overlapped, so the fanfare played the
 * instant you grabbed it instead of waiting for an actual visit to the
 * jukebox. Also clear of the bar counter's collision block (x 56-199,
 * y 40-87), and clear of the message box's own footprint (rows 19-26,
 * y 152-215) so picking it up doesn't leave it sitting under its own
 * popup. */
#define TOKEN_X 200
#define TOKEN_Y 110
#define TOKEN_OAM_SLOT 42
static void draw_token(void)
{
    set_oam_sprite(TOKEN_OAM_SLOT, TOKEN_X, TOKEN_Y, TOKEN_TILE, 2);
}

/* Aligned to attribute-block boundaries (4 tiles/32px each) on all four
 * sides -- rows 16-23 is exactly block rows 4-5, cols 4-27 is exactly
 * block cols 1-6 -- so the palette-1 recolor below lands only on cells
 * the box itself actually fills, not on the surrounding floor/walls. */
#define MSG_BOX_TOP 16
#define MSG_BOX_LEFT 4
#define MSG_BOX_WIDTH 24
#define MSG_BOX_HEIGHT 8
#define MSG_BOX_RIGHT ((MSG_BOX_LEFT + MSG_BOX_WIDTH) * 8)
#define MSG_BOX_BOTTOM ((MSG_BOX_TOP + MSG_BOX_HEIGHT) * 8)

/* NES sprites are independently placed 8x8 tiles, so a dialogue box cannot
 * pixel-clip an actor. Hiding only tiles that overlap the opaque panel is the
 * closest native equivalent: the tiles above, below, or beside it remain.
 * This is deliberately applied to the OAM shadow, not scene state; callers
 * redraw their normal actors after the message is dismissed. */
static void hide_sprites_under_message_box(void)
{
    unsigned char i;

    for (i = 0; i < 64; ++i) {
        unsigned int offset = (unsigned int)i * 4;
        unsigned char y = oam_buf[offset];
        unsigned char x = oam_buf[offset + 3];

        if (y != 0xFF && x < MSG_BOX_RIGHT && y < MSG_BOX_BOTTOM &&
            (unsigned int)x + 8 > MSG_BOX_LEFT * 8 &&
            (unsigned int)y + 8 > MSG_BOX_TOP * 8) {
            oam_buf[offset] = 0xFF;
        }
    }
}

/* One 24-tile panel row plus (on rows 16 and 20) six attribute bytes fits
 * inside the fixed 32-write NMI queue. Building the frame a row at a time
 * avoids a forced-blank flash while keeping the reveal brief and deliberate. */
static void queue_message_panel_row(unsigned char row)
{
    unsigned char col;
    unsigned char tile;

    for (col = MSG_BOX_LEFT; col < MSG_BOX_LEFT + MSG_BOX_WIDTH; ++col) {
        /* FONT_TILE_BASE is the font's solid blank tile: unlike the bar
         * wall it has no color-zero gaps through which scene colors leak. */
        tile = FONT_TILE_BASE;
        if ((row == MSG_BOX_TOP || row == MSG_BOX_TOP + MSG_BOX_HEIGHT - 1) &&
            col > MSG_BOX_LEFT && col < MSG_BOX_LEFT + MSG_BOX_WIDTH - 1) {
            tile = TILE_BORDER_H;
        } else if (row > MSG_BOX_TOP && row < MSG_BOX_TOP + MSG_BOX_HEIGHT - 1 &&
                   (col == MSG_BOX_LEFT || col == MSG_BOX_LEFT + MSG_BOX_WIDTH - 1)) {
            tile = TILE_BORDER_V;
        }
        (void)vram_queue_batch_tile(row, col, tile);
    }

    if (row == MSG_BOX_TOP || row == MSG_BOX_TOP + 4) {
        unsigned char attr_row = (unsigned char)(row >> 2);
        for (col = 1; col < 7; ++col) {
            (void)vram_queue_batch_address((unsigned int)0x23C0 +
                                            (unsigned int)attr_row * 8 + col, 0x55);
        }
    }
    (void)vram_queue_batch_commit();
}

/* A one-shot dialogue box. No dedicated border art needed -- box_border.s
 * already exists (TILE_BORDER_H/V, drawn by the title screen's own
 * dialogue plate), a thin rule line meant to sit over a solid
 * color-index-2 field, which FONT_TILE_BASE already is. Reusing it here
 * gets a real framed box instead of a flat panel with no new CHR at all.
 * The four corner cells are left as plain fill rather than either border
 * tile crossing there -- there's no dedicated corner art (CHR budget is
 * fully spent, see TOKEN_TILE's comment), so an open notch instead of a
 * hard square corner is the closest approximation of rounded corners
 * available. Drawn in its own dark BG palette 1 (see bar_palette's
 * comment) so it reads as a distinct overlay, same idea as the casino's
 * slot UI panel.
 *
 * Messages take priority only over the actor tiles they cover: overlapping
 * OAM tiles are hidden before the panel is made visible, while actor tiles
 * outside the panel remain on screen. The caller's scene redraw restores the
 * normal complete sprite set after dismissal.
 *
 * The static panel opens via short NMI-queued rows, then text types in one
 * tile per frame. The caller is responsible for redrawing the underlying
 * scene afterward (a show_*_interior() call) to erase the box and its palette
 * override. The second and third lines are optional. */
static void show_message_box(const char *line1, const char *line2, const char *line3)
{
    unsigned char r, pad;
    unsigned char release_frames;
    unsigned char text_row = 18;
    unsigned char text_col = 6;
    unsigned char text_index = 0;
    const char *text = line1;

    hide_sprites_under_message_box();
    while (vram_queue_count) {
        waitvsync();
        oam_flush();
        reset_scroll();
    }
    /* Flush the clipped OAM shadow before the first panel row appears. */
    waitvsync();
    oam_flush();
    reset_scroll();
    for (r = MSG_BOX_TOP; r < MSG_BOX_TOP + MSG_BOX_HEIGHT; ++r) {
        /* Do not begin a row until NMI has fully consumed the previous one.
         * A rejected row-20 batch leaves the lower message half on the room's
         * old attribute palette, which is especially visible after a modal. */
        while (vram_queue_count) {
            waitvsync();
            oam_flush();
            reset_scroll();
        }
        (void)vram_queue_batch_begin();
        queue_message_panel_row(r);
        waitvsync();
        oam_flush();
        reset_scroll();
    }

    /* One tile per frame is tiny compared with the 32-entry NMI budget and
     * leaves dialogue visually alive even while the scene stays rendered. */
    while (text) {
        waitvsync();
        oam_flush();
        reset_scroll();
        if (text[text_index]) {
            if (!vram_queue_count) {
                (void)vram_queue_tile(text_row, (unsigned char)(text_col + text_index),
                                      (unsigned char)(FONT_TILE_BASE + font_glyph_index(text[text_index])));
                ++text_index;
            }
        } else if (text == line1 && line2) {
            text = line2;
            text_row = 20;
            text_index = 0;
        } else if (text == line2 && line3) {
            text = line3;
            text_row = 22;
            text_index = 0;
        } else {
            text = 0;
        }
    }

    /* The final character is published on the last typing frame; wait until
     * the following NMI has consumed it before allowing dismissal. */
    while (vram_queue_count) {
        waitvsync();
        oam_flush();
        reset_scroll();
    }

    /* Require a real, sustained release (A reading clear for a run of
     * frames, not just one) before a fresh press can dismiss this --
     * the same A that triggered the message is very likely still
     * physically held when the box first appears, and a single-frame
     * gap in a held button's reading (an emulator/OS auto-repeat quirk)
     * must not be mistaken for the player actually letting go, or the
     * box flashes open and immediately closes on its own trigger press. */
    release_frames = 0;
    while (release_frames < 10) {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();
        if (pad & PAD_A) {
            release_frames = 0;
        } else {
            ++release_frames;
        }
    }
    do {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();
    } while (!(pad & PAD_A));
}

/* Mart dialogue should not borrow the light cinder-block gray used by the
 * storefront/interior. The message box uses BG palette 1, so temporarily
 * make that palette a dark navy field with green trim; show_mart_interior()
 * restores the room palette immediately after dismissal. */
static void show_mart_message(const char *line1, const char *line2, const char *line3)
{
    PPU.mask = 0;
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x04;
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x2A;
    PPU.vram.data = 0x01;
    PPU.vram.data = 0x30;
    reset_scroll();
    PPU.mask = 0x1E;
    show_message_box(line1, line2, line3);
}

static void show_bar_interior(void)
{
    static const unsigned char bar_palette[32] = {
        /* BG pal0: the room's warm wood floor/walls (unchanged). BG pal1:
         * a dark, contrasting panel for the message box -- same scheme
         * as the casino's slot UI overlay. Index 2 is BAR_TILE_WALL's
         * solid fill and font.s's glyph-blank color (see its header
         * comment), so text sits on a seamless field; index 1 is
         * box_border.s's rule-line color, drawn over that same field. */
        0x07, 0x17, 0x08, 0x28, 0x0F, 0x30, 0x0C, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        /* BG pal3: Piss Corner streaks/puddles. Index 2 stays black for
         * the dark recess, while index 3 is the pale wall fixture. */
        0x0F, 0x28, 0x0F, 0x30,
        0x07, 0x0F, 0x0F, 0x0F, 0x0F, 0x16, 0x27, 0x30,
        0x0F, 0x28, 0x0F, 0x30, 0x0F, 0x0F, 0x0F, 0x0F
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = bar_palette[i];

    /* South wall is 3 tiles thick (bar_interior.s's own header comment
     * describes its door as "a 48x24 southern exit," i.e. 3 tiles tall).
     * North wall is thicker still, at 6 -- the backroom door only needs
     * 3 of those rows, so it sits flush against the floor with visible
     * wall mass above it instead of the door filling the wall's entire
     * height with no margin. Floor is rows 6-26 to match. */
    for (i = 6; i < 27; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 6, BAR_TILE_WALL);
    fill_title_rect(27, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(0, 0, 1, 30, BAR_TILE_WALL);
    fill_title_rect(0, 31, 1, 30, BAR_TILE_WALL);
    for (i = 27; i < 30; ++i) {
        set_tile(i, 14, BAR_TILE_DOOR_JAMB);
        set_tile(i, 15, BAR_TILE_DOOR_FILL);
        set_tile(i, 16, BAR_TILE_DOOR_JAMB);
    }

    /* The backroom door -- reuses the same jamb/fill tiles as the south
     * exit, just on the north wall instead, near the jukebox corner
     * where the back-room planks already hint at more room beyond. Sits
     * in the bottom 3 rows of the 6-thick north wall, flush against the
     * floor. Leads to the casino, but only once door_unlocked is set by
     * playing the jukebox (see run_bar_interior()). */
    for (i = 3; i < 6; ++i) {
        set_tile(i, 25, BAR_TILE_DOOR_JAMB);
        set_tile(i, 26, BAR_TILE_DOOR_FILL);
        set_tile(i, 27, BAR_TILE_DOOR_JAMB);
    }

    /* Shelf and counter, with Jimbles (a sprite, drawn below) standing
     * between them -- see draw_jimbles()'s comment for how the two line
     * up without any actual sprite-behind-background trick. The bottles
     * sit two wall rows above his head, rather than
     * reading as if they are balanced against his glasses. */
    fill_title_rect(4, 8, 16, 1, BAR_TILE_SHELF);
    fill_title_rect(8, 8, 16, 1, BAR_TILE_COUNTER);
    /* Small empty stools establish the patron's place at the counter
     * without adding collision to the central walking aisle. */
    set_tile(10, 14, BAR_TILE_STOOL);
    set_tile(10, 17, BAR_TILE_STOOL);
    set_tile(13, 3, BAR_TILE_JUKEBOX_TOP);
    set_tile(14, 3, BAR_TILE_JUKEBOX_BOT);
    set_tile(11, 27, BAR_TILE_PLANK_R);
    set_tile(12, 27, BAR_TILE_PLANK_R);
    set_tile(11, 28, BAR_TILE_PLANK_L);
    set_tile(12, 28, BAR_TILE_PLANK_L);

    /* Jimbles's punishment nook stays on the same open floor as the bar.
     * Palette 3 supplies the sickly yellow floor streaks; avoiding a dark
     * wall panel keeps the corner readable instead of looking corrupted. */
    set_tile(21, 27, PISS_TILE_PUDDLE);
    set_tile(22, 30, PISS_TILE_PUDDLE);
    set_tile(23, 25, PISS_TILE_PUDDLE);
    set_tile(24, 26, PISS_TILE_PUDDLE);
    set_tile(25, 28, PISS_TILE_PUDDLE);
    /* Unevenly scattered short streaks break up the round puddle shapes. */
    set_tile(19, 26, PISS_TILE_STREAK_A);
    set_tile(20, 29, PISS_TILE_STREAK_B);
    set_tile(21, 25, PISS_TILE_STREAK_B);
    set_tile(22, 28, PISS_TILE_STREAK_A);
    set_tile(23, 30, PISS_TILE_STREAK_B);
    set_tile(24, 25, PISS_TILE_STREAK_A);
    set_tile(25, 29, PISS_TILE_STREAK_B);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(2, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(3, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(4, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(5, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);
    /* Start the yellowed floor one attribute cell above the puddles, so the
     * small scattered streaks share their color at the corner's upper edge. */
    set_attribute_block(4, 6, 3);
    set_attribute_block(4, 7, 3);
    /* Palette 3 turns the floor's thin horizontal detail into sickly yellow
     * streaks and its scattered puddle tiles yellow too, without a label
     * spelling out what this undesirable corner is for. */
    set_attribute_block(5, 6, 3);
    set_attribute_block(5, 7, 3);
    set_attribute_block(6, 6, 3);
    set_attribute_block(6, 7, 3);

    clear_sprites();
    draw_jimbles();
    draw_bar_patron();
    draw_piss_corner_regulars();
    if (!token_taken) draw_token();
    /* Flush the freshly-cleared-plus-Jimbles(-plus-token) buffer to the
     * real PPU OAM before rendering resumes -- see show_neon_avenue()'s
     * comment on the same call for why clear_sprites() alone isn't
     * enough. */
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The bar counter/shelf/patron block (rows 5-9, cols 8-23 -- see
 * show_bar_interior()) is solid: the player has to walk around it, not
 * through it. An 8px margin on every side approximates the player
 * sprite's own footprint, same tolerance TOKEN_X/Y's pickup check uses. */
#define BAR_COUNTER_LEFT   56
#define BAR_COUNTER_RIGHT  199
#define BAR_COUNTER_TOP    40
#define BAR_COUNTER_BOTTOM 87

static unsigned char blocked_by_counter(unsigned char x, unsigned char y)
{
    return x >= BAR_COUNTER_LEFT && x <= BAR_COUNTER_RIGHT &&
           y >= BAR_COUNTER_TOP && y <= BAR_COUNTER_BOTTOM;
}

/* Same shape as run_neon_avenue(): flush first thing after waitvsync(),
 * before any of this frame's work, so the DMA timing is fixed-cost
 * regardless of how much the frame actually needs to do -- see that
 * function's comment for why that ordering matters. No taxi here, so
 * there's much less per-frame work either way, but the pattern stays
 * consistent rather than relying on "this scene is simple enough that
 * the old order would've been fine anyway." */
static unsigned char run_bar_interior(unsigned char from_casino)
{
    /* Coming back from the casino drops the player right at the
     * backroom door (just south of its threshold) instead of all the
     * way back at the avenue entrance, matching how walking out of the
     * door was what got them there in the first place. */
    unsigned char x = from_casino ? 212 : 120;
    unsigned char y = from_casino ? 56 : 208;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;
    unsigned char door_prompted = 0;
    unsigned char jukebox_prompted = 0;
    unsigned char jimbles_prompted = 0;
    unsigned char patron_prompted = 0;
    unsigned char piss_regular_one_prompted = 0;
    unsigned char piss_regular_two_prompted = 0;
    unsigned char piss_regular_two_dialogue = 0;
    /* Jimbles's idle animation: rest at the current position for a
     * while, then slide (1px every 4 frames) to the opposite side of
     * his little range and rest again. See jimbles_x's and
     * draw_jimbles()'s comments for why this is a slide, not a walk. */
    unsigned char jimbles_anim_timer = 90;
    unsigned char jimbles_sliding = 0;
    unsigned char jimbles_target = 128;

    reset_piss_corner_regulars();
    show_bar_interior();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();

        if (jimbles_anim_timer) {
            --jimbles_anim_timer;
        } else if (jimbles_sliding) {
            if (jimbles_x < jimbles_target) {
                ++jimbles_x;
                jimbles_anim_timer = 3;
            } else if (jimbles_x > jimbles_target) {
                --jimbles_x;
                jimbles_anim_timer = 3;
            } else {
                jimbles_sliding = 0;
                jimbles_anim_timer = 90;
            }
        } else {
            jimbles_sliding = 1;
            jimbles_target = (jimbles_x <= 116) ? 128 : 104;
        }
        draw_jimbles();
        update_piss_corner_regulars();
        draw_bar_patron();
        draw_piss_corner_regulars();

        pad = read_pad();
        if (pad & PAD_START) {
            run_inventory();
            show_bar_interior();
            continue;
        }
        /* Dialogue returns while its dismissal press is still held. Re-arm
         * nearby A-hotspots only after that press has been released, so the
         * next deliberate A can talk to the same character again in place. */
        if (!(pad & PAD_A)) {
            jukebox_prompted = 0;
            jimbles_prompted = 0;
            patron_prompted = 0;
            piss_regular_one_prompted = 0;
            piss_regular_two_prompted = 0;
        }
        if (pad & PAD_LEFT) {
            if (x > 20 && !blocked_by_counter((unsigned char)(x - 1), y)) --x;
        } else if (pad & PAD_RIGHT) {
            if (x < 236 && !blocked_by_counter((unsigned char)(x + 1), y)) ++x;
        }
        if (pad & PAD_UP) {
            /* Same doorway-gap idea as the south exit below, mirrored on
             * the north wall: walking further in once already at the
             * wall, inside the door's column span, heads to the casino
             * instead of just stopping -- but only once the jukebox has
             * unlocked it; otherwise a locked-door message explains why
             * not instead of silently doing nothing. */
            if (y > 48) {
                if (!blocked_by_counter(x, (unsigned char)(y - 1))) --y;
                door_prompted = 0;
            } else if (x >= 200 && x <= 224) {
                if (door_unlocked) {
                    return BAR_EXIT_CASINO;
                }
                /* Position-gated, not button-edge-gated: holding UP
                 * pinned against the wall must show this exactly once
                 * per approach, not retrigger on every frame (or on
                 * every stray release/press blip a held key can produce
                 * on real hardware or in an emulator). It only resets
                 * once the player actually steps back from the wall. */
                if (!door_prompted) {
                    door_prompted = 1;
                    show_message_box("THE DOOR IS LOCKED.", 0, 0);
                    show_bar_interior();
                }
            } else {
                door_prompted = 0;
            }
        } else if (pad & PAD_DOWN) {
            if (y < 216) {
                if (!blocked_by_counter(x, (unsigned char)(y + 1))) ++y;
            } else if (x >= 104 && x <= 144) {
                return BAR_EXIT_AVENUE;
            }
        }
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if (++walk_timer == 8) {
                walk_timer = 0;
                walk_frame ^= 1;
            }
        } else {
            walk_timer = 0;
            walk_frame = 0;
        }

        /* Zelda-style walk-over pickup, no button -- same feel as the
         * old adventure's items. */
        if (!token_taken && x >= TOKEN_X - 12 && x <= TOKEN_X + 12 &&
            y >= TOKEN_Y - 12 && y <= TOKEN_Y + 12) {
            token_taken = 1;
            has_token = 1;
            oam_buf[TOKEN_OAM_SLOT * 4] = 0xFF;
            show_message_box("YOU FOUND A JUKEBOX", "TOKEN.", 0);
            show_bar_interior();
        }
        /* Spend it standing near the jukebox. Position-gated like the
         * door lock above: holding A in the zone shows a message exactly
         * once per approach, not once per frame A reads as held (or per
         * stray release/press blip from a held key) -- resets only once
         * the player actually leaves the zone. */
        if (x >= 8 && x <= 48 && y >= 88 && y <= 128) {
            if ((pad & PAD_A) && !jukebox_prompted) {
                jukebox_prompted = 1;
                if (has_token) {
                    has_token = 0;
                    door_unlocked = 1;
                    music_play_jukebox_fanfare();
                    /* A hint the back door just unlocked, not a direct
                     * statement of it -- the click is the mechanism, the
                     * player connects the dots on the next locked-door
                     * message (or just by trying the door). */
                    show_message_box("THE JUKEBOX ROARS", "TO LIFE... CLICK.", 0);
                } else {
                    show_message_box("THE JUKEBOX NEEDS", "A TOKEN.", 0);
                }
                show_bar_interior();
            }
        } else {
            jukebox_prompted = 0;
        }

        /* Jimbles, behind the counter -- approached from directly south
         * of it (the counter's own collision keeps the player from
         * getting any closer), same position-gated once-per-approach
         * pattern as the door and jukebox above. Zone covers his whole
         * idle slide range, 104-128 (see jimbles_x's comment), not just
         * his rest position -- and stops at 104 rather than reaching
         * further left so it can't overlap the patron's zone just below. */
        if (x >= 104 && x <= 156 && y >= 88 && y <= 112) {
            if ((pad & PAD_A) && !jimbles_prompted) {
                jimbles_prompted = 1;
                if (!has_beer) {
                    has_beer = 1;
                    show_message_box("JIMBLES: FIRST ONE'S", "ON THE HOUSE, PAL.",
                                     "HE HANDS YOU A BEER.");
                } else {
                    show_message_box("JIMBLES WIPES A GLASS", "ONE'S YOUR LIMIT, PAL", 0);
                }
                show_bar_interior();
            }
        } else {
            jimbles_prompted = 0;
        }

        /* The bar patron, a couple seats down from Jimbles. Stops at 103
         * so it can't overlap Jimbles's own zone just above. */
        if (x >= 80 && x <= 103 && y >= 88 && y <= 112) {
            if ((pad & PAD_A) && !patron_prompted) {
                patron_prompted = 1;
                show_message_box("THE PATRON JUST", "GRUNTS AT YOU.", 0);
                show_bar_interior();
            }
        } else {
            patron_prompted = 0;
        }

        /* Two unfortunate regulars in Jimbles's lower-right punishment nook.
         * Their zones are split at x=216, matching their visible positions,
         * so either lament can be heard directly. */
        if (x >= 184 && x <= 216 && y >= 160 && y <= 208) {
            if ((pad & PAD_A) && !piss_regular_one_prompted) {
                piss_regular_one_prompted = 1;
                show_message_box("JIMBLES BANISHED ME", "FOR SPILLING A BEER.", 0);
                show_bar_interior();
            }
        } else {
            piss_regular_one_prompted = 0;
        }
        if (x >= 217 && x <= 244 && y >= 176 && y <= 216) {
            if ((pad & PAD_A) && !piss_regular_two_prompted) {
                piss_regular_two_prompted = 1;
                if (piss_regular_two_dialogue == 0) {
                    show_message_box("STINKS IN HERE.", 0, 0);
                } else if (piss_regular_two_dialogue == 1) {
                    show_message_box("SMELLS LIKE MEAT.", 0, 0);
                } else {
                    show_message_box("ROTTEN MEAT.", 0, 0);
                }
                piss_regular_two_dialogue = (unsigned char)((piss_regular_two_dialogue + 1) % 3);
                show_bar_interior();
            }
        } else {
            piss_regular_two_prompted = 0;
        }

        draw_player_sprite(x, y, walk_frame);
    }
}

/* Right-pads a slot symbol to a fixed 7-column field so redrawing it
 * after every pull always overwrites the previous symbol completely --
 * "CHERRY" (6 chars) is the longest of the three, so 7 leaves a blank
 * separator column with no extra clear pass needed. */
static void draw_symbol_padded(unsigned char row, unsigned char col, const char *sym)
{
    unsigned char i = 0;
    while (sym[i]) {
        set_tile(row, (unsigned char)(col + i), (unsigned char)(FONT_TILE_BASE + font_glyph_index(sym[i])));
        ++i;
    }
    while (i < 7) {
        set_tile(row, (unsigned char)(col + i), (unsigned char)(FONT_TILE_BASE + font_glyph_index(' ')));
        ++i;
    }
}

/* Dynamic casino content fits in one 32-tile NMI batch. Initial scene and
 * panel construction still use forced blank; only these frequent redraws need
 * to stay visible while play continues. */
static void queue_symbol_padded(unsigned char row, unsigned char col, const char *sym)
{
    unsigned char i = 0;

    while (sym[i]) {
        (void)vram_queue_tile(row, (unsigned char)(col + i),
                              (unsigned char)(FONT_TILE_BASE + font_glyph_index(sym[i])));
        ++i;
    }
    while (i < 7) {
        (void)vram_queue_tile(row, (unsigned char)(col + i),
                              (unsigned char)(FONT_TILE_BASE + font_glyph_index(' ')));
        ++i;
    }
}

/* Always exactly 2 digits (credits/bet are both kept well under 100),
 * so this never needs its own clear pass either. */
static void draw_digits2(unsigned char row, unsigned char col, unsigned char value)
{
    char buf[3];
    buf[0] = (char)('0' + (value / 10) % 10);
    buf[1] = (char)('0' + value % 10);
    buf[2] = 0;
    draw_text(row, col, buf);
}

static void queue_digits2(unsigned char row, unsigned char col, unsigned char value)
{
    (void)vram_queue_tile(row, col,
                          (unsigned char)(FONT_TILE_BASE + font_glyph_index((char)('0' + (value / 10) % 10))));
    (void)vram_queue_tile(row, (unsigned char)(col + 1),
                          (unsigned char)(FONT_TILE_BASE + font_glyph_index((char)('0' + value % 10))));
}

static void draw_casino_hud(void)
{
    draw_symbol_padded(14, 3, casino_slot_symbols[casino_reel[0]]);
    draw_symbol_padded(14, 13, casino_slot_symbols[casino_reel[1]]);
    draw_symbol_padded(14, 23, casino_slot_symbols[casino_reel[2]]);
    draw_text(18, 4, casino_credits_text);
    draw_digits2(18, 9, casino_credits);
    draw_text(18, 14, casino_bet_text);
    draw_digits2(18, 18, casino_bet);
}

/* The upright lever is drawn once with the panel. During a pull, its knob
 * and lower arm swing left; four queued tile updates are enough to change
 * poses without disturbing the reel batch. */
static void draw_casino_lever(unsigned char pulled)
{
    fill_title_rect(18, 26, 4, 5, BAR_TILE_WALL);
    set_tile(18, 28, TILE_BORDER_V);
    set_tile(19, 28, TILE_BORDER_V);
    if (pulled) {
        set_tile(20, 28, TILE_BORDER_V);
        set_tile(21, 28, PISS_TILE_PUDDLE);
    } else {
        set_tile(20, 28, PISS_TILE_PUDDLE);
    }
}

static void queue_casino_lever(unsigned char pulled)
{
    if (pulled) {
        (void)vram_queue_tile(20, 28, TILE_BORDER_V);
        (void)vram_queue_tile(21, 28, PISS_TILE_PUDDLE);
    } else {
        (void)vram_queue_tile(20, 28, PISS_TILE_PUDDLE);
        (void)vram_queue_tile(21, 28, BAR_TILE_WALL);
    }
}

static void queue_casino_hud(const unsigned char *display_reel, unsigned char lever_pulled)
{
    queue_symbol_padded(14, 3, casino_slot_symbols[display_reel[0]]);
    queue_symbol_padded(14, 13, casino_slot_symbols[display_reel[1]]);
    queue_symbol_padded(14, 23, casino_slot_symbols[display_reel[2]]);
    queue_digits2(18, 9, casino_credits);
    queue_digits2(18, 18, casino_bet);
    queue_casino_lever(lever_pulled);
}

/* The actual "Morrowind resolution"-free gamble: spend the bet, roll
 * three independent symbols, and pay out on a three-of-a-kind. Simple
 * on purpose for this first rebuilt pass -- weighted odds, a proper
 * payout table per symbol, and queued reel animation are exactly what
 * REBUILD_ROADMAP.md calls out as later refinement, not this slice. */
static void show_casino(void)
{
    static const unsigned char casino_palette[32] = {
        /* BG pal0: the room's warm wood floor/walls (unchanged). BG pal1:
         * a dark, contrasting panel for the slot UI overlay -- index 2 is
         * both BAR_TILE_WALL's solid fill and font.s's glyph-blank color
         * (see its header comment), so reusing that tile as the panel
         * background makes text sit on a seamless field instead of
         * visibly-bordered glyph cells. */
        0x07, 0x17, 0x08, 0x28, 0x0F, 0x0F, 0x0C, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x07, 0x0F, 0x0F, 0x0F, 0x0F, 0x16, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = casino_palette[i];

    /* Same wood-floor/wall tiles as the bar -- a shady backroom, not a
     * different building, and it keeps this scene inside the already-
     * exhausted 256-tile CHR budget (see TOKEN_TILE's comment) with no
     * new background art at all. South wall is 3 tiles thick (not 1) to
     * fit a walked exit door back to the bar -- same jamb/fill/jamb art
     * and reasoning as the bar's own doors (see show_bar_interior()). */
    for (i = 1; i < 27; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 1, BAR_TILE_WALL);
    fill_title_rect(27, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(0, 0, 1, 30, BAR_TILE_WALL);
    fill_title_rect(0, 31, 1, 30, BAR_TILE_WALL);
    for (i = 27; i < 30; ++i) {
        set_tile(i, 14, BAR_TILE_DOOR_JAMB);
        set_tile(i, 15, BAR_TILE_DOOR_FILL);
        set_tile(i, 16, BAR_TILE_DOOR_JAMB);
    }
    draw_text(25, 13, casino_exit_text);

    draw_text(2, 7, casino_sign_text);

    /* The slot machine: the bar's own jukebox tiles reused 2x2, same
     * "no CHR budget left" tradeoff as the floor/walls above. Centered
     * in the 30-wide interior (cols 1-30). */
    set_tile(10, 15, BAR_TILE_JUKEBOX_TOP);
    set_tile(10, 16, BAR_TILE_JUKEBOX_TOP);
    set_tile(11, 15, BAR_TILE_JUKEBOX_BOT);
    set_tile(11, 16, BAR_TILE_JUKEBOX_BOT);
    draw_text(9, 14, casino_slots_text);

    /* Blackjack table, on the right side. No dedicated art -- CHR budget
     * is fully spent (see TOKEN_TILE's comment) -- so it borrows the
     * bar's counter/shelf tiles as a stand-in tabletop, the same reuse
     * the slot machine above already does with the jukebox tiles. */
    fill_title_rect(10, 22, 4, 1, BAR_TILE_COUNTER);
    fill_title_rect(11, 22, 4, 1, BAR_TILE_SHELF);
    draw_text(9, 21, casino_blackjack_text);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(2, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(3, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(4, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(5, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);

    /* The reel/credits/bet HUD and its control hints deliberately aren't
     * drawn here -- they stay hidden until the player actually walks up
     * to the machine and interacts (see run_casino()'s slot_ui_active),
     * instead of being visible across the whole room from scene entry. */

    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Reveals the slot machine's own UI -- reel row, credits/bet, and their
 * control hints -- only once the player has actually stepped up and
 * pressed A on it. Forced-blank like every other VRAM update in this
 * file (see show_casino()'s comment), so it's safe to call mid-frame
 * from inside run_casino()'s loop, not just at scene entry. */
/* Rows 12-23 (attribute block rows 3-5, each 4 tiles tall) become a
 * dedicated overlay panel rather than the machine's UI just sitting
 * directly on top of the room's own wood floor: filled solid with
 * BAR_TILE_WALL and repainted with BG palette 1 (see casino_palette's
 * comment), so it reads as a distinct screen, not a transparent blend. */
static void show_casino_slot_ui(void)
{
    unsigned char i, left_col;

    PPU.mask = 0;
    fill_title_rect(12, 1, 30, 12, BAR_TILE_WALL);
    for (i = 0; i < 8; ++i) set_attribute_block(3, i, 1);
    for (i = 0; i < 8; ++i) set_attribute_block(4, i, 1);
    for (i = 0; i < 8; ++i) set_attribute_block(5, i, 1);
    /* Three tall framed windows turn the text fields into a proper reel bank.
     * Each inner area is exactly seven tiles wide, matching the padded
     * symbol renderer and keeping all spin redraws inside its frame. */
    for (i = 0; i < 3; ++i) {
        left_col = (unsigned char)(2 + i * 10);
        fill_title_rect(12, (unsigned char)(left_col + 1), 7, 1, TILE_BORDER_H);
        fill_title_rect(16, (unsigned char)(left_col + 1), 7, 1, TILE_BORDER_H);
        set_tile(13, left_col, TILE_BORDER_V);
        set_tile(14, left_col, TILE_BORDER_V);
        set_tile(15, left_col, TILE_BORDER_V);
        set_tile(13, (unsigned char)(left_col + 8), TILE_BORDER_V);
        set_tile(14, (unsigned char)(left_col + 8), TILE_BORDER_V);
        set_tile(15, (unsigned char)(left_col + 8), TILE_BORDER_V);
    }
    draw_casino_lever(0);
    draw_text(20, 4, casino_controls_text);
    draw_text(22, 4, casino_pull_text);
    draw_casino_hud();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Undoes show_casino_slot_ui() exactly -- floor tiles and palette 0
 * back across the same 12-row band. */
static void hide_casino_slot_ui(void)
{
    unsigned char i;

    PPU.mask = 0;
    fill_title_rect(12, 1, 30, 12, BAR_TILE_FLOOR);
    for (i = 0; i < 8; ++i) set_attribute_block(3, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(4, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(5, i, 0);
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Same "sustained release, not just a one-frame gap" debounce as
 * show_message_box() -- a genuinely held button can still read clear for
 * a single frame (an emulator/OS auto-repeat quirk), which a plain
 * prev_pad edge check mistakes for release-then-repress, firing an
 * action again on every such blip. Returns 1 exactly once per real
 * press-then-release cycle, 0 otherwise (including every frame the
 * button stays held). held and release_run are the caller's persistent
 * per-button state, carried across frames. */
#define DEBOUNCE_RELEASE_FRAMES 4
static unsigned char debounce_press(unsigned char pressed, unsigned char *held,
                                    unsigned char *release_run)
{
    if (pressed) {
        *release_run = 0;
        if (!*held) {
            *held = 1;
            return 1;
        }
        return 0;
    }
    if (*release_run < DEBOUNCE_RELEASE_FRAMES) {
        ++*release_run;
    } else {
        *held = 0;
    }
    return 0;
}

enum {
    INVENTORY_ITEM_PAPER_CLIP,
    INVENTORY_ITEM_BEER,
    INVENTORY_ITEM_TOKEN
};

static unsigned char inventory_item_count(void)
{
    unsigned char count = 0;

    if (has_paper_clip) ++count;
    if (has_beer) ++count;
    if (has_token) ++count;
    return count;
}

static unsigned char inventory_item_at(unsigned char index)
{
    if (has_paper_clip) {
        if (index == 0) return INVENTORY_ITEM_PAPER_CLIP;
        --index;
    }
    if (has_beer) {
        if (index == 0) return INVENTORY_ITEM_BEER;
        --index;
    }
    return INVENTORY_ITEM_TOKEN;
}

static const char *inventory_item_name(unsigned char item)
{
    if (item == INVENTORY_ITEM_PAPER_CLIP) return "PAPER CLIP";
    if (item == INVENTORY_ITEM_BEER) return "JIMBLES'S BEER";
    return "JUKEBOX TOKEN";
}

static void show_inventory_description(unsigned char item)
{
    if (item == INVENTORY_ITEM_PAPER_CLIP) {
        show_message_box("A BENT PAPER CLIP.", "IT COULD BE USEFUL.", 0);
    } else if (item == INVENTORY_ITEM_BEER) {
        show_message_box("JIMBLES'S HOUSE BEER.", "WARM, BUT FREE.", 0);
    } else {
        show_message_box("A JUKEBOX TOKEN.", "GOOD FOR ONE SONG.", 0);
    }
}

/* The inventory is intentionally an opaque screen, not a translucent HUD:
 * that lets it use one dependable palette regardless of the current room. */
static void show_inventory_screen(unsigned char selected)
{
    unsigned char row, col;
    unsigned char count = inventory_item_count();

    PPU.mask = 0;
    fill_title_rect(0, 0, 32, 30, BAR_TILE_WALL);
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x04;
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x2A;
    PPU.vram.data = 0x01;
    PPU.vram.data = 0x30;
    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) set_attribute_block(row, col, 1);
    }
    draw_title_plate(2, 2, 28, 26);
    draw_text(5, 11, "INVENTORY");
    if (count == 0) {
        draw_text(12, 5, "YOUR POCKETS ARE EMPTY.");
    } else {
        for (row = 0; row < count; ++row) {
            unsigned char item_row = (unsigned char)(8 + row * 3);
            set_tile(item_row, 5, row == selected ? TILE_BORDER_V : FONT_TILE_BASE);
            draw_text(item_row, 8, inventory_item_name(inventory_item_at(row)));
        }
    }
    draw_text(22, 7, "A: DESCRIBE");
    draw_text(24, 5, "START OR B: CLOSE");
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

static void update_inventory_selector(unsigned char old_selected,
                                      unsigned char new_selected)
{
    PPU.mask = 0;
    set_tile((unsigned char)(8 + old_selected * 3), 5, FONT_TILE_BASE);
    set_tile((unsigned char)(8 + new_selected * 3), 5, TILE_BORDER_V);
    reset_scroll();
    PPU.mask = 0x1E;
}

static void wait_for_inventory_close_release(void)
{
    unsigned char released = 0;

    while (released < DEBOUNCE_RELEASE_FRAMES) {
        waitvsync();
        oam_flush();
        reset_scroll();
        if (read_pad() & (PAD_START | PAD_B)) {
            released = 0;
        } else {
            ++released;
        }
    }
}

static void run_inventory(void)
{
    unsigned char selected = 0;
    unsigned char count = inventory_item_count();
    unsigned char pad;
    unsigned char start_held = 1, start_run = 0;
    unsigned char b_held = 0, b_run = 0;
    unsigned char a_held = 0, a_run = 0;
    unsigned char up_held = 0, up_run = 0;
    unsigned char down_held = 0, down_run = 0;
    unsigned char left_held = 0, left_run = 0;
    unsigned char right_held = 0, right_run = 0;
    unsigned char up_press, down_press, left_press, right_press;

    show_inventory_screen(selected);
    for (;;) {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();

        if (debounce_press((unsigned char)(pad & PAD_START), &start_held, &start_run) ||
            debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) {
            wait_for_inventory_close_release();
            return;
        }

        up_press = debounce_press((unsigned char)(pad & PAD_UP), &up_held, &up_run);
        down_press = debounce_press((unsigned char)(pad & PAD_DOWN), &down_held, &down_run);
        left_press = debounce_press((unsigned char)(pad & PAD_LEFT), &left_held, &left_run);
        right_press = debounce_press((unsigned char)(pad & PAD_RIGHT), &right_held, &right_run);
        if (count > 1 && (up_press || left_press)) {
            unsigned char old_selected = selected;
            selected = selected ? (unsigned char)(selected - 1) : (unsigned char)(count - 1);
            update_inventory_selector(old_selected, selected);
        } else if (count > 1 && (down_press || right_press)) {
            unsigned char old_selected = selected;
            selected = (unsigned char)((selected + 1) % count);
            update_inventory_selector(old_selected, selected);
        }

        if (count && debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
            show_inventory_description(inventory_item_at(selected));
            show_inventory_screen(selected);
        }
    }
}

/* Same flush-first shape as the other two scenes' loops. Walking freely
 * and playing the slot machine are two distinct modes: A at the machine
 * enters slot_ui_active (revealing its UI, see casino_pull_text's own
 * "B OUT" hint), B backs back out to walking. There's no button-driven
 * exit from the room itself anymore -- walking into the south door is
 * the only way back to the bar, the same as every other door in this
 * game. A and B each share one debounce_press() state across their two
 * uses (enter machine / pull, and back out of slot UI) rather than two
 * separate checks, so the same held press that does one can't also
 * immediately re-fire the other. */
static void run_casino(void)
{
    unsigned char x = 128;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;
    unsigned char at_machine;
    unsigned char slot_ui_active = 0;
    unsigned char hud_dirty = 0;
    unsigned char display_reel[3] = {0, 0, 0};
    unsigned char reel_spin_active = 0;
    unsigned char reel_spin_tick = 0;
    unsigned char reel_spin_step = 0;
    unsigned char lever_pulled = 0;
    unsigned char a_held = 0, a_run = 0;
    unsigned char b_held = 0, b_run = 0;
    unsigned char left_held = 0, left_run = 0;
    unsigned char right_held = 0, right_run = 0;

    show_casino();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();

        /* Deferred from last frame's bet/pull handling. The HUD's 27 dynamic
         * tile writes fit inside the fixed 32-entry queue, so NMI owns the
         * transaction and no blank frame is needed for bet or reel changes. */
        if (hud_dirty) {
            queue_casino_hud(display_reel, lever_pulled);
            hud_dirty = 0;
        }

        pad = read_pad();

        if (pad & PAD_START) {
            run_inventory();
            show_casino();
            if (slot_ui_active) {
                show_casino_slot_ui();
                hud_dirty = 1;
            }
            continue;
        }

        if (slot_ui_active && reel_spin_active) {
        /* Reels share the HUD batch, so advancing once every
             * three frames stays well within NMI's vblank budget. They stop
             * in sequence instead of all snapping to the already-resolved
             * result at once. Input stays locked until the last reel settles. */
            if (++reel_spin_tick == 3) {
                unsigned char reel;

                reel_spin_tick = 0;
                ++reel_spin_step;
                for (reel = 0; reel < 3; ++reel) {
                    if (reel_spin_step < (unsigned char)(5 + reel * 2)) {
                        display_reel[reel] = (unsigned char)((display_reel[reel] + 1) % CASINO_SYMBOL_COUNT);
                    } else {
                        display_reel[reel] = casino_reel[reel];
                    }
                }
                hud_dirty = 1;
                if (reel_spin_step >= 8) {
                    reel_spin_active = 0;
                    lever_pulled = 0;
                }
            }
        } else if (slot_ui_active) {
            if (debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) {
                slot_ui_active = 0;
                hide_casino_slot_ui();
            } else {
                if (debounce_press((unsigned char)(pad & PAD_LEFT), &left_held, &left_run)) {
                    if (casino_bet > 1) --casino_bet;
                    hud_dirty = 1;
                }
                if (debounce_press((unsigned char)(pad & PAD_RIGHT), &right_held, &right_run)) {
                    if (casino_bet < 5) ++casino_bet;
                    hud_dirty = 1;
                }
                if (debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run) &&
                    casino_credits >= casino_bet) {
                    casino_spin(&game_state.casino);
                    reel_spin_active = 1;
                    reel_spin_tick = 0;
                    reel_spin_step = 0;
                    lever_pulled = 1;
                    hud_dirty = 1;
                }
            }
        } else {
            /* Centered on the relocated 2x2 machine (rows 10-11, cols
             * 15-16 -- pixels 120-135, 80-95), with room to approach. */
            at_machine = (x >= 96 && x <= 160 && y >= 64 && y <= 112);

            if (at_machine && debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
                slot_ui_active = 1;
                /* LEFT/RIGHT's debounce state is otherwise untouched by
                 * plain walking (only the bet-adjust branch above calls
                 * debounce_press() on them), so seed it from whatever's
                 * actually held right now -- if the player happened to
                 * still be holding a direction while pressing A to step
                 * up to the machine, that same hold shouldn't also fire
                 * an immediate bet change the instant the panel opens. */
                left_held = (pad & PAD_LEFT) ? 1 : 0;
                left_run = 0;
                right_held = (pad & PAD_RIGHT) ? 1 : 0;
                right_run = 0;
                display_reel[0] = casino_reel[0];
                display_reel[1] = casino_reel[1];
                display_reel[2] = casino_reel[2];
                show_casino_slot_ui();
            } else {
                if (pad & PAD_LEFT) {
                    if (x > 20) --x;
                } else if (pad & PAD_RIGHT) {
                    if (x < 236) ++x;
                }
                if (pad & PAD_UP) {
                    if (y > 40) --y;
                } else if (pad & PAD_DOWN) {
                    /* No more B-to-exit (see this function's header
                     * comment) -- walking into the south door, the same
                     * way every other door in this game works, is now
                     * the only way back to the bar. */
                    if (y < 216) {
                        ++y;
                    } else if (x >= 104 && x <= 144) {
                        return;
                    }
                }
            }
        }
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if (++walk_timer == 8) {
                walk_timer = 0;
                walk_frame ^= 1;
            }
        } else {
            walk_timer = 0;
            walk_frame = 0;
        }

        if (slot_ui_active) {
            /* The slot interface is a full foreground mode, not a panel the
             * player stands on top of. Keep its OAM slots hidden until B
             * closes the machine and returns to room movement. */
            hide_player_sprite();
        } else {
            draw_player_sprite(x, y, walk_frame);
        }
    }
}

void main(void)
{
    unsigned char avenue_arrival = 0;

    waitvsync();
    waitvsync();

    v2_game_reset();
    PPU.control = 0x80;
    show_title_screen();
    music_start();
    wait_for_title_start();

    /* Neon Avenue, the bar, and the casino hand off to each other by
     * returning -- walking into a doorway exits the current scene's
     * loop, and this dispatcher re-enters whichever one comes next. The
     * casino only ever returns to the bar (never straight to the
     * avenue), so it's nested inside the bar's own loop rather than
     * living at the same level as the other two. run_bar_interior()'s
     * argument is whether this entry is a return from the casino (spawn
     * at the backroom door) rather than a fresh arrival from the avenue
     * (spawn at the south entrance) -- see its own comment. */
    for (;;) {
        unsigned char bar_exit;
        unsigned char avenue_exit;

        avenue_exit = run_neon_avenue(avenue_arrival);
        avenue_arrival = 0;
        while (avenue_exit == AVENUE_EXIT_SEWER) {
            run_sewer();
            avenue_exit = run_neon_avenue(1);
        }
        if (avenue_exit == AVENUE_EXIT_MART) {
            unsigned char mart_exit = run_mart_exterior(0);
            while (mart_exit == MART_EXIT_INTERIOR) {
                run_mart_interior();
                mart_exit = run_mart_exterior(1);
            }
            avenue_arrival = 2;
            continue;
        }
        bar_exit = run_bar_interior(0);
        while (bar_exit == BAR_EXIT_CASINO) {
            run_casino();
            bar_exit = run_bar_interior(1);
        }
    }
}
