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
#include "v2_dialogue.h"
#include "v2_game_state.h"
#include "v2_input.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_regulars.h"
#include "v2_scene_casino.h"
#include "v2_scene_commentarium.h"
#include "v2_scene_avenue.h"
#include "v2_scene_bar.h"
#include "v2_scene_mart.h"
#include "v2_scene_park.h"
#include "v2_scene_sewer.h"
#include "v2_scene_zoo.h"
#include "v2_runtime.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"
#include "v2_vram_queue.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

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
 * explicit CHR order in build.bat, checked against game_runtime.map. */
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

/* traffic_bones.s -- linked into CHR all along but never referenced by any
 * tile constant until now, so the bone scatter it was authored for never
 * actually appeared after a hit. Six tiles: bone-left, bone-right,
 * wishbone, femur, club-bone, skull-and-crossbones. */

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

/* bar_interior.s -- Jimbles's Bar, rebuilt as its own scene. Floor,
 * wall, counter, bottle shelf, one back-room plank (v2_scene_bar.c draws
 * both plank positions with it now -- see BAR_TILE_PLANK_L there), a
 * two-tile jukebox, a stool, and a two-tile door (jamb + fill, used on
 * both sides of the doorway gap). Tile 242 (the former second plank) now
 * holds part of the casino lobby's blackjack table art instead -- see
 * v2_scene_casino.c. */
#define BAR_TILE_FLOOR        235
#define BAR_TILE_WALL         236
#define BAR_TILE_COUNTER      237
#define BAR_TILE_SHELF        238
#define BAR_TILE_PLANK_R      239
#define BAR_TILE_JUKEBOX_TOP  240
#define BAR_TILE_JUKEBOX_BOT  241
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
static void show_mart_message(const char *line1, const char *line2, const char *line3);

/* Keep the established scene code readable while making its persistent data
 * explicitly owned by the fixed-core game state. */
#define token_taken game_state.token_taken
#define has_token game_state.has_token
#define door_unlocked game_state.door_unlocked
#define has_beer game_state.has_beer
#define has_paper_clip game_state.has_paper_clip
#define has_sewer_key game_state.has_sewer_key
#define jimbles_x game_state.jimbles_x
#define casino_credits game_state.casino.credits
#define casino_bet game_state.casino.bet
#define casino_reel game_state.casino.reel


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

/* Rows 8-11 exactly -- one whole attribute block (row-block 2), cols
 * 8-23 (column-blocks 2-5) -- so every tile the neon sign palette
 * touches is actually sign art, none of it bare brick. Copy belongs to
 * the owning storefront, not this shared frame helper. */
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
void main(void)
{
    unsigned char avenue_arrival = 0;

    waitvsync();
    waitvsync();

    v2_game_reset();
    PPU.control = 0x80;
    show_title_screen();
    music_start();
    /* The title plays its own banked theme (see src/title_music.s), not the
     * avenue loop music_start() loads by default. */
    music_select_song(MUSIC_SONG_TITLE);
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
        unsigned char mart_exit;
        unsigned char sewer_exit;
        unsigned char park_exit;
        unsigned char zoo_exit;

        avenue_exit = run_neon_avenue(avenue_arrival);
        avenue_arrival = 0;
        while (avenue_exit == AVENUE_EXIT_SEWER) {
            v2_set_route(V2_SCENE_SEWER, 0);
            sewer_exit = run_sewer();
            if (sewer_exit == SEWER_EXIT_TRAFFIC_DEATH) {
                avenue_exit = AVENUE_EXIT_TRAFFIC_DEATH;
                break;
            }
            if (sewer_exit == SEWER_EXIT_ZOO) {
                v2_set_route(V2_SCENE_ZOO, 0);
                zoo_exit = run_zoo();
                if (zoo_exit == ZOO_EXIT_TRAFFIC_DEATH) {
                    avenue_exit = AVENUE_EXIT_TRAFFIC_DEATH;
                    break;
                }
                continue;
            }
            v2_set_route(V2_SCENE_AVENUE, 1);
            avenue_exit = run_neon_avenue(1);
        }
        if (avenue_exit == AVENUE_EXIT_TRAFFIC_DEATH) {
            avenue_arrival = 0;
            continue;
        }
        if (avenue_exit == AVENUE_EXIT_PARK) {
            v2_set_route(V2_SCENE_PARK, 0);
            park_exit = run_park();
            avenue_arrival = park_exit == PARK_EXIT_TRAFFIC_DEATH ? 0 : 3;
            continue;
        }
        if (avenue_exit == AVENUE_EXIT_MART) {
            v2_set_route(V2_SCENE_MART, 0);
            mart_exit = run_mart_exterior(0);
            while (mart_exit == MART_EXIT_INTERIOR) {
                v2_set_route(V2_SCENE_MART, 2);
                run_mart_interior();
                v2_set_route(V2_SCENE_MART, 1);
                mart_exit = run_mart_exterior(1);
            }
            avenue_arrival = mart_exit == MART_EXIT_TRAFFIC_DEATH ? 0 : 2;
            continue;
        }
        v2_set_route(V2_SCENE_BAR, 0);
        bar_exit = run_bar_interior(0);
        /* The bar is a hub, not a chain: exiting to the casino or commentarium
         * returns to the bar at that sub-room's door, and the player can head
         * straight on to the other. So loop here and re-check whichever exit
         * was just used. The old linear three-loop form missed a casino-then-
         * commentarium hop -- after the casino loop, bar_exit was already
         * COMMENTARIUM, but that loop sat earlier in the sequence, so it never
         * ran and the player fell through to the avenue spawn ("outside the
         * bar"). Only stop once the player leaves through the avenue door. */
        while (bar_exit != BAR_EXIT_AVENUE) {
            if (bar_exit == BAR_EXIT_DEATH) {
                /* Spilled or let the beer go warm: respawn at the bar
                 * entrance, the same contract as the avenue's taxi death. */
                bar_exit = run_bar_interior(0);
            } else if (bar_exit == BAR_EXIT_COMMENTARIUM) {
                v2_set_route(V2_SCENE_COMMENTARIUM, 0);
                run_commentarium();
                v2_set_route(V2_SCENE_BAR, 2);
                bar_exit = run_bar_interior(2);
            } else if (bar_exit == BAR_EXIT_CASINO) {
                v2_set_route(V2_SCENE_CASINO, 0);
                if (run_casino() == CASINO_EXIT_BROKE) {
                    /* Gambled it all away: kicked back to the bar's
                     * avenue entrance, flat broke. */
                    v2_set_route(V2_SCENE_BAR, 0);
                    bar_exit = run_bar_interior(0);
                } else {
                    v2_set_route(V2_SCENE_BAR, 1);
                    bar_exit = run_bar_interior(1);
                }
            } else {
                /* No other bar exit code exists; guard against spinning. */
                break;
            }
        }
    }
}
