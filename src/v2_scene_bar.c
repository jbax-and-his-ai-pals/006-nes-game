#include <nes.h>

#include "font.h"
#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_drink.h"
#include "v2_game_state.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_regulars.h"
#include "v2_runtime.h"
#include "v2_scene_bar.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"
#include "v2_vram_queue.h"

#define JIMBLES_TILE_TOP 61
#define JIMBLES_TILE_BOTTOM 62
#define JIMBLES_TILE_BOTTOM_RIGHT 1
#define PISS_TILE_STREAK_A 55
#define PISS_TILE_STREAK_B 56
#define PISS_TILE_PUDDLE 57
#define BAR_TILE_FLOOR 235
#define BAR_TILE_WALL 236
#define BAR_TILE_COUNTER 237
#define BAR_TILE_SHELF 238
#define BAR_TILE_PLANK_R 239
#define BAR_TILE_JUKEBOX_TOP 240
#define BAR_TILE_JUKEBOX_BOT 241
/* Was its own tile (242); byte-for-byte identical to BAR_TILE_PLANK_R, so
 * that slot now holds the casino lobby's blackjack table art instead (see
 * bar_interior.s) and this just reuses PLANK_R -- same pixels either way. */
#define BAR_TILE_PLANK_L BAR_TILE_PLANK_R
#define BAR_TILE_STOOL 243
#define BAR_TILE_DOOR_JAMB 244
#define BAR_TILE_DOOR_FILL 245
#define TOKEN_TILE 255

/* Chugging-challenge tiles: chr.s's tile 3 is solid index 3 -- the liquid
 * and the foam, a single tile whose color is decided by the palette that
 * attributes the cell (amber for beer, white for the head). FONT_TILE_BASE
 * (8) is a solid index-2 tile that renders palette slot 2 (black panel fill
 * and empty glass); box_border's rules draw the white frame (index 1 lines
 * over an index-2 field that matches the fill).
 *
 * The panel deliberately does NOT use background palette 1. The shared
 * message box rewrites palette 1 to its own blue/green scheme (see
 * show_message_box), so a panel drawn in palette 1 turned blue and green
 * the instant the post-game message appeared. Palette 2 owns the panel body
 * and the amber liquid; palette 3 owns the white text and the white foam.
 * Palettes 0 (the bar's wood) and 1 (the message box) are left untouched. */
#define DRINK_FONT_BASE 8
#define DRINK_FILL_TILE 8
#define DRINK_LIQUID_TILE 3
#define DRINK_FRAME_H_TILE 52
#define DRINK_FRAME_V_TILE 53
#define DRINK_PAL_BODY 2
#define DRINK_PAL_FOAM 3
#define DRINK_PANEL_TOP 8
#define DRINK_PANEL_LEFT 4
#define DRINK_PANEL_WIDTH 24
#define DRINK_PANEL_HEIGHT 16
/* The panel spans rows 8-23 and cols 4-27, which aligns exactly to NES
 * attribute blocks 2-5 (rows) and 1-6 (cols); an attribute byte covers a
 * 4x4 tile block here. Rows 8-11 (header), 12-15 (foam), and 20-23 (status
 * text) are palette 3 (white ink); rows 16-19 hold the amber liquid and use
 * palette 2. */
#define DRINK_ATTR_ROW_FIRST 2
#define DRINK_ATTR_ROW_LAST 5
#define DRINK_ATTR_COL_FIRST 1
#define DRINK_ATTR_COL_LAST 6
#define DRINK_GLASS_LEFT 15
#define DRINK_GLASS_RIGHT 20
#define DRINK_GLASS_RIM_ROW 11
#define DRINK_GLASS_BOTTOM_ROW 20
#define DRINK_INTERIOR_LEFT 16
#define DRINK_INTERIOR_RIGHT 19
#define DRINK_INTERIOR_TOP 12
#define DRINK_INTERIOR_BOTTOM 19
/* Status rows inside the panel (rows 8-23). */
#define DRINK_CONTROLS_ROW 21
#define DRINK_POKE_ROW 22
/* Palette 2 slot 3, the amber liquid's color. Queue writes here mid-frame
 * so the beer visibly darkens as it warms without blanking the screen. */
#define DRINK_WARMTH_COLOR_ADDR 0x3F0B
#define DRINK_DEATH_PAUSE_FRAMES 90

#define token_taken game_state.token_taken
#define has_token game_state.has_token
#define door_unlocked game_state.door_unlocked
#define has_beer game_state.has_beer
#define jimbles_limit_given game_state.jimbles_limit_given
#define jimbles_x game_state.jimbles_x
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
/* The jukebox token is a reward from winning Jimbles's chugging challenge
 * (see the Jimbles block in run_bar_interior()), not a floor pickup, so
 * there is no token sprite to draw or position. */

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
        0x07, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x28, 0x0F, 0x30, 0x0F, 0x0F, 0x0F, 0x30
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
    /* A small north-west side door leads to the Commentarium next door. */
    for (i = 3; i < 6; ++i) {
        set_tile(i, 4, BAR_TILE_DOOR_JAMB);
        set_tile(i, 5, BAR_TILE_DOOR_FILL);
        set_tile(i, 6, BAR_TILE_DOOR_JAMB);
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

    /* Attribute bytes divide their 32px cells into four 16px palette
     * quadrants. Keep the upper half of Piss Corner on the normal wood
     * palette, then begin its yellowed floor below it. That trims the
     * nook's top edge without moving its puddles, regulars, or bounds. */
    PPU.vram.address = 0x23;
    PPU.vram.address = 0xE6;
    PPU.vram.data = 0xF0;
    PPU.vram.address = 0x23;
    PPU.vram.address = 0xE7;
    PPU.vram.data = 0xF0;

    clear_sprites();
    draw_jimbles();
    draw_bar_patron();
    draw_regulars();
    /* Flush the freshly-cleared-plus-Jimbles buffer to the real PPU OAM
     * before rendering resumes -- see show_neon_avenue()'s comment on the
     * same call for why clear_sprites() alone isn't enough. */
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

static void show_bar_message(const char *line1, const char *line2,
                             const char *line3)
{
    /* Piss Corner owns background palette 3. Render dialogue through the
     * otherwise unused palette 1 so exposed puddle cells keep their yellow. */
    dialogue_set_palette(1);
    show_message_box(line1, line2, line3);
    dialogue_set_palette(3);
}

/* ------------------------------------------------------------------ */
/* Jimbles's chugging challenge. Turn-based: five drinks, two settles.
 * Rules live in v2_drink.c (host-tested exhaustively); everything here
 * is presentation -- the glass panel, the foam/liquid tiles, the warmth
 * color shift, and the fail tableaus. Spilling or letting the beer go
 * warm hands off to bar_corner_death() below and the scene returns
 * BAR_EXIT_DEATH, the same respawn contract as the avenue's taxi death. */

static unsigned char drink_warm_color(unsigned char warmth)
{
    /* Amber, then brown, then murky -- the liquid visibly dying. */
    if (warmth == 0) return 0x27;
    if (warmth == 1) return 0x16;
    return 0x06;
}

/* The tile for one interior row, top-down (p 0..7 = glass rows 12..19).
 * Rows 0-3 (12-15) are the foam zone, drawn in palette DRINK_PAL_FOAM where
 * index 3 is solid white; rows 4-7 (16-19) are the liquid zone, drawn in
 * palette DRINK_PAL_BODY where index 3 is amber. Both fill bottom-aligned.
 * A full pour (5 units) also puts one white "head" row at row 15, the bottom
 * of the foam zone, so a full beer reads as a thin white head on four amber
 * rows and the first gulp visibly drops that head. */
static unsigned char drink_row_tile(unsigned char foam, unsigned char liquid,
                                    unsigned char p)
{
    if (p < 4) {
        unsigned char rows = foam > 4 ? 4 : foam;

        if (p >= (unsigned char)(4 - rows)) return DRINK_LIQUID_TILE;
        if (p == 3 && liquid >= 5) return DRINK_LIQUID_TILE;
        return DRINK_FILL_TILE;
    }
    {
        unsigned char rows = liquid > 4 ? 4 : liquid;

        if (p >= (unsigned char)(8 - rows)) return DRINK_LIQUID_TILE;
        return DRINK_FILL_TILE;
    }
}

static void drink_fill_row_direct(unsigned char row, unsigned char tile)
{
    unsigned char col;

    PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
    PPU.vram.address = (unsigned char)((row << 5) + DRINK_INTERIOR_LEFT);
    for (col = DRINK_INTERIOR_LEFT; col <= DRINK_INTERIOR_RIGHT; ++col) {
        PPU.vram.data = tile;
    }
}

static void drink_queue_row(unsigned char row, unsigned char tile)
{
    unsigned char col;

    for (col = DRINK_INTERIOR_LEFT; col <= DRINK_INTERIOR_RIGHT; ++col) {
        (void)vram_queue_tile(row, col, tile);
    }
}

/* Fixed-width 22-tile status line on DRINK_POKE_ROW. Overwriting in place
 * means a short poke never leaves trailing glyphs from a longer one. */
static void drink_queue_poke(const char *text)
{
    unsigned char i;

    for (i = 0; i < 22; ++i) {
        unsigned char tile = DRINK_FILL_TILE;

        if (text[i]) tile = (unsigned char)(DRINK_FONT_BASE + font_glyph_index(text[i]));
        (void)vram_queue_tile(DRINK_POKE_ROW, (unsigned char)(5 + i), tile);
    }
}

/* Re-queue only the glass rows whose tile changed; a full 32-tile interior
 * redraw would overflow the queue mid-frame (the warmth write and the poke
 * share the same 32-entry budget as the glass itself). */
static void drink_update_glass(const DrinkState *drink, unsigned char *prev_foam,
                               unsigned char *prev_liquid)
{
    unsigned char new_foam = drink->foam;
    unsigned char new_liquid = drink->liquid;
    unsigned char row;

    for (row = DRINK_INTERIOR_TOP; row <= DRINK_INTERIOR_BOTTOM; ++row) {
        unsigned char p = (unsigned char)(row - DRINK_INTERIOR_TOP);

        if (drink_row_tile(*prev_foam, *prev_liquid, p) !=
            drink_row_tile(new_foam, new_liquid, p)) {
            drink_queue_row(row, drink_row_tile(new_foam, new_liquid, p));
        }
    }
    *prev_foam = new_foam;
    *prev_liquid = new_liquid;
}

static void drink_pause_frames(unsigned char frames)
{
    while (frames--) {
        waitvsync();
        oam_flush();
        reset_scroll();
    }
}

static void drink_show_spill(void)
{
    unsigned char row, col;

    /* The head is already at the brim (a 3rd consecutive drink pushes foam
     * past 4, filling the foam zone). Crest it over the rim and spill a
     * solid white row onto the header above. */
    for (row = DRINK_INTERIOR_TOP; row < (unsigned char)(DRINK_INTERIOR_TOP + 4); ++row) {
        drink_queue_row(row, DRINK_LIQUID_TILE);
    }
    drink_pause_frames(2);
    for (col = DRINK_GLASS_LEFT; col <= DRINK_GLASS_RIGHT; ++col) {
        (void)vram_queue_tile(DRINK_GLASS_RIM_ROW, col, DRINK_LIQUID_TILE);
        (void)vram_queue_tile((unsigned char)(DRINK_GLASS_RIM_ROW - 1), col, DRINK_LIQUID_TILE);
    }
    drink_pause_frames(2);
    for (col = 14; col < 22; ++col) {
        (void)vram_queue_tile(DRINK_CONTROLS_ROW, col, PISS_TILE_PUDDLE);
    }
    drink_pause_frames(2);
    drink_queue_poke("YOU SPILLED THE BEER.");
    drink_pause_frames(DRINK_DEATH_PAUSE_FRAMES);
}

static void drink_show_snatch(void)
{
    /* Jimbles's hand comes down on the glass; the beer stays amber but the
     * status line sells the moment before the banishment sequence. */
    drink_pause_frames(2);
    drink_queue_poke("JIMBLES SNATCHES IT.");
    drink_pause_frames(DRINK_DEATH_PAUSE_FRAMES);
}

static void drink_queue_warmth(unsigned char warmth)
{
    /* Palette write through the batch API: vram_queue_address itself gets
     * inlined away inside the fixed engine, but the batch trio is retained
     * (the dialogue panel builder uses it), so this always has an address
     * to jump to. */
    (void)vram_queue_batch_begin();
    (void)vram_queue_batch_address(DRINK_WARMTH_COLOR_ADDR, drink_warm_color(warmth));
    (void)vram_queue_batch_commit();
}

/* The panel: forced-blank redraw like show_bar_interior(), then the chug
 * loop keeps the screen on and pushes only tile/palette deltas through the
 * NMI queue. Palette 1 owns the panel -- see the DRINK_* tile defines for
 * why slot 2 is black fill and slot 3 is the shiftable liquid color. */
static void draw_drink_panel(const DrinkState *drink)
{
    unsigned char c, row;

    PPU.mask = 0;
    /* Palette 2: panel body + amber liquid. Palette 3: white text + white
     * foam. Palettes 1 (the message box) and 0 (the bar's wood) are left
     * untouched so the post-game box keeps its blue/green and the bar behind
     * the panel keeps its own look. */
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x08;
    PPU.vram.data = 0x0F;   /* backdrop: black */
    PPU.vram.data = 0x2A;   /* frame lines: white */
    PPU.vram.data = 0x0F;   /* panel fill + empty glass: black */
    PPU.vram.data = drink_warm_color(drink->warmth);   /* liquid amber (shifts) */
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x0C;
    PPU.vram.data = 0x0F;   /* backdrop: black */
    PPU.vram.data = 0x2A;   /* frame lines: white */
    PPU.vram.data = 0x0F;   /* foam-zone fill + empty glass: black */
    PPU.vram.data = 0x30;   /* text + foam: white */
    draw_title_plate(DRINK_PANEL_TOP, DRINK_PANEL_LEFT,
                     DRINK_PANEL_WIDTH, DRINK_PANEL_HEIGHT);
    for (c = DRINK_ATTR_COL_FIRST; c <= DRINK_ATTR_COL_LAST; ++c) {
        set_attribute_block(2, c, DRINK_PAL_FOAM);
        set_attribute_block(3, c, DRINK_PAL_FOAM);
        set_attribute_block(4, c, DRINK_PAL_BODY);
        set_attribute_block(5, c, DRINK_PAL_FOAM);
    }
    draw_text(9, 5, "FIRST ONE'S ON THE");
    draw_text(10, 5, "HOUSE, PAL. DRINK UP!");
    /* Glass frame. */
    for (c = DRINK_GLASS_LEFT; c <= DRINK_GLASS_RIGHT; ++c) {
        set_tile(DRINK_GLASS_RIM_ROW, c, DRINK_FRAME_H_TILE);
        set_tile(DRINK_GLASS_BOTTOM_ROW, c, DRINK_FRAME_H_TILE);
    }
    for (row = DRINK_INTERIOR_TOP; row <= DRINK_INTERIOR_BOTTOM; ++row) {
        set_tile(row, DRINK_GLASS_LEFT, DRINK_FRAME_V_TILE);
        set_tile(row, DRINK_GLASS_RIGHT, DRINK_FRAME_V_TILE);
    }
    /* Initial pour: five liquid rows (the top one renders white as a thin
     * head), no foam. */
    for (row = DRINK_INTERIOR_TOP; row <= DRINK_INTERIOR_BOTTOM; ++row) {
        unsigned char p = (unsigned char)(row - DRINK_INTERIOR_TOP);

        drink_fill_row_direct(row, drink_row_tile(0, drink->liquid, p));
    }
    draw_text(DRINK_CONTROLS_ROW, 6, "A: DRINK   B: SETTLE");
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The full modal: owns the frame loop while the chug is in progress, like
 * run_inventory() does for the inventory screen. Returns the DRINK_RESULT_*;
 * the caller decides the win handoff or the corner banishment. */
static unsigned char drink_beer_minigame(void)
{
    DrinkState drink;
    unsigned char pad = 0;
    unsigned char prev_pad = 0;
    unsigned char prev_foam = 0;
    unsigned char prev_liquid = 0;
    unsigned char poke = 0;
    unsigned char result;

    drink_init(&drink);
    draw_drink_panel(&drink);
    prev_foam = drink.foam;
    prev_liquid = drink.liquid;
    /* Seed prev_pad from the live state so the A that opened the chug (or a
     * held button) doesn't read as a fresh press on the first round: each
     * drink/settle needs a deliberate press-and-release, so you can't just
     * hold A to chug the whole pint. */
    prev_pad = read_pad();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if ((pad & PAD_A) && !(prev_pad & PAD_A)) {
            result = drink_take(&drink);
            music_sfx_pickup();
            drink_update_glass(&drink, &prev_foam, &prev_liquid);
            if (result != DRINK_RESULT_ACTIVE) break;
        } else if ((pad & PAD_B) && !(prev_pad & PAD_B)) {
            result = drink_settle(&drink);
            drink_update_glass(&drink, &prev_foam, &prev_liquid);
            drink_queue_warmth(drink.warmth);
            if (result != DRINK_RESULT_ACTIVE) break;
            if (drink.warmth == 1 && poke != 1) {
                poke = 1;
                drink_queue_poke("JIMBLES TAPS HIS FINGER.");
            } else if (drink.warmth == 2 && poke != 2) {
                poke = 2;
                drink_queue_poke("JIMBLES: ADMIRING IT?");
            }
        }
        prev_pad = pad;
    }
    if (drink.result == DRINK_RESULT_SPILL) {
        music_sfx_hurt();
        drink_show_spill();
    } else if (drink.result == DRINK_RESULT_SNATCH) {
        drink_show_snatch();
    }
    return drink.result;
}

static unsigned char bar_death_centered_col(const char *text)
{
    unsigned char length = 0;

    while (text[length]) ++length;
    return (unsigned char)((32 - length) >> 1);
}

/* A three-line banishment plate, drawn with the active dialogue palette
 * (palette 1, the same blue/green as the shared message box) so the
 * post-game death reads as part of the bar's normal dialogue system.
 * Mirrors show_traffic_death_message() but the punishment line runs to a
 * third row, since "JIMBLES: PISS CORNER. NOW." is too wide for one. */
static void bar_death_plate(const char *line1, const char *line2, const char *line3)
{
    unsigned char i;
    unsigned char palette = dialogue_get_palette();

    PPU.mask = 0;
    PPU.vram.address = 0x3F;
    PPU.vram.address = (unsigned char)(palette << 2);
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x2A;
    PPU.vram.data = 0x01;
    PPU.vram.data = 0x30;
    draw_title_plate(12, 4, 24, 8);
    for (i = 1; i < 7; ++i) {
        set_attribute_block(3, i, palette);
        set_attribute_block(4, i, palette);
    }
    draw_text(14, bar_death_centered_col(line1), line1);
    draw_text(16, bar_death_centered_col(line2), line2);
    draw_text(18, bar_death_centered_col(line3), line3);
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The banishment: the corner regulars welcome their newest member, then the
 * death plate, then the same release-and-press-A respawn as the taxi death. */
static void bar_corner_death(const char *line1, const char *line2, const char *line3)
{
    unsigned char pad = 0;
    unsigned char released = 0;

    show_bar_message("ANOTHER SPILLER.", "WELCOME TO THE CLUB.", 0);
    show_bar_interior();
    dialogue_set_palette(1);
    bar_death_plate(line1, line2, line3);
    dialogue_set_palette(3);
    for (;;) {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();
        if (!(pad & PAD_A)) {
            if (released < 4) ++released;
        } else if (released >= 4) {
            return;
        }
    }
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
unsigned char run_bar_interior(unsigned char arrival)
{
    /* Each interior return gets its own doorway-adjacent spawn rather
     * than dropping the player at the avenue entrance. The Commentarium
     * side door is in the upper-left wall at x 24-56, so arrival 2 lands
     * just below it at the same practical walking position. */
    unsigned char x = arrival == 1 ? 212 : (arrival == 2 ? 40 : 120);
    unsigned char y = arrival == 1 ? 56 : (arrival == 2 ? 56 : 208);
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
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

    music_select_song(MUSIC_SONG_BAR);
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
        draw_regulars();

        pad = read_pad();
        if (pad & PAD_UP) facing = PLAYER_FACING_UP;
        else if (pad & PAD_DOWN) facing = PLAYER_FACING_DOWN;
        else if (pad & PAD_LEFT) facing = PLAYER_FACING_LEFT;
        else if (pad & PAD_RIGHT) facing = PLAYER_FACING_RIGHT;
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
            } else if (x >= 24 && x <= 56) {
                return BAR_EXIT_COMMENTARIUM;
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
                    show_bar_message("THE DOOR IS LOCKED.", 0, 0);
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

        /* The jukebox token is a reward from winning Jimbles's chugging
         * challenge (see the Jimbles block), not a walk-over floor pickup,
         * so there's no floor sprite to pick up here. */
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
                    show_bar_message("THE JUKEBOX ROARS", "TO LIFE... CLICK.",
                                     "DID SOMETHING UNLOCK?");
                } else {
                    show_bar_message("THE JUKEBOX NEEDS", "A TOKEN.", 0);
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
                unsigned char drink_result;

                jimbles_prompted = 1;
                if (!has_beer) {
                    /* The chugging challenge replaces the instant handoff:
                     * win and he slides a beer across for the road; spill
                     * or let it go warm and it's Piss Corner -- see
                     * bar_corner_death() and BAR_EXIT_DEATH. */
                    drink_result = drink_beer_minigame();
                    if (drink_result == DRINK_RESULT_WIN) {
                        has_beer = 1;
                        has_token = 1;
                        show_bar_message("THAT'S THE STUFF, PAL.", "HE HANDS YOU A BEER",
                                         "AND A JUKEBOX TOKEN.");
                        show_bar_interior();
                    } else {
                        bar_corner_death(drink_result == DRINK_RESULT_SPILL
                                             ? "YOU SPILLED THE BEER."
                                             : "YOU LET IT GO WARM.",
                                         "JIMBLES: PISS",
                                         "CORNER. NOW.");
                        return BAR_EXIT_DEATH;
                    }
                } else if (!jimbles_limit_given) {
                    jimbles_limit_given = 1;
                    show_bar_message("JIMBLES: ONE'S YOUR", "LIMIT, PAL.", 0);
                    show_bar_interior();
                } else {
                    show_bar_message("JIMBLES: EVERYONE", "HERE'S A FUCKING", "SLUG.");
                    show_bar_interior();
                }
            }
        } else {
            jimbles_prompted = 0;
        }

        /* The bar patron, a couple seats down from Jimbles. Stops at 103
         * so it can't overlap Jimbles's own zone just above. */
        if (x >= 80 && x <= 103 && y >= 88 && y <= 112) {
            if ((pad & PAD_A) && !patron_prompted) {
                patron_prompted = 1;
                show_bar_message("THE PATRON JUST", "GRUNTS AT YOU.", 0);
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
                show_bar_message("JIMBLES BANISHED ME", "FOR SPILLING A BEER.", 0);
                show_bar_interior();
            }
        } else {
            piss_regular_one_prompted = 0;
        }
        if (x >= 217 && x <= 244 && y >= 176 && y <= 216) {
            if ((pad & PAD_A) && !piss_regular_two_prompted) {
                piss_regular_two_prompted = 1;
                if (piss_regular_two_dialogue == 0) {
                    show_bar_message("STINKS IN HERE.", 0, 0);
                } else if (piss_regular_two_dialogue == 1) {
                    show_bar_message("SMELLS LIKE MEAT.", 0, 0);
                } else {
                    show_bar_message("ROTTEN MEAT.", 0, 0);
                }
                piss_regular_two_dialogue = (unsigned char)((piss_regular_two_dialogue + 1) % 3);
                show_bar_interior();
            }
        } else {
            piss_regular_two_prompted = 0;
        }

        draw_player_sprite(x, y, walk_frame, facing);
    }
}
