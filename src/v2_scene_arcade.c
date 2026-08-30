#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_arcade.h"
#include "v2_scene_ui.h"
#include "v2_vram_queue.h"

#define CHR_TILE_PATH 3
#define ROAD_LINE_TILE 2
#define DOORKNOB_TILE 51
#define NEON_TILE_ASPHALT 192
#define NEON_TILE_BRICK 194
#define NEON_TILE_AWNING 195
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199
/* These five tiles are the arcade's own neon marquee kit, patched into the
 * shared common bank at indices 60-64 (see tools/gen_arcade_chr.py). They
 * sit in a span this facade never draws, so they cannot break anything the
 * rest of the game reuses here. */
#define MARQUEE_TUBE_TOP 60
#define MARQUEE_PANEL 61
#define MARQUEE_TUBE_UNDER 62
#define MARQUEE_DOT 63
#define NEON_LIT_WINDOW 64

static void show_arcade_message(const char *line1, const char *line2, const char *line3);

/* The marquee's 16-bulb run (row 6, cols 8-23) chases live: at any phase, a
 * 2-bulb lit band slides along the canopy, wrapping every 4 steps. Only the
 * DOT and PANEL tiles change (no new CHR). This runs every few frames while
 * rendering is on, so it must go through the NMI-drained VRAM queue rather
 * than set_tile()'s direct $2006/$2007 writes -- a vblank NMI landing
 * between those two writes desyncs the PPU address latch and corrupts
 * whatever the NMI itself then writes (OAM DMA, its own queue flush),
 * which is what caused the full-screen corruption/flicker seen in testing.
 * Setup uses draw_arcade_marquee_bulbs_direct() instead -- see its own
 * comment for why the queue is specifically wrong there. */
static void draw_arcade_marquee_bulbs(unsigned char phase)
{
    unsigned char i, col, lit;

    for (i = 0; i < 16; ++i) {
        col = (unsigned char)(8 + i);
        lit = (unsigned char)(((i + phase) & 3) < 2);
        (void)vram_queue_tile(6, col, lit ? MARQUEE_DOT : MARQUEE_PANEL);
    }
}

/* Setup-only variant: writes the initial bulb frame with set_tile()'s direct
 * writes instead of the queue. show_arcade_exterior() runs under forced
 * blank, but it still does ~40 more lines of its own direct $2006/$2007
 * writes (window panes, door, every attribute block) after this call, and
 * forced blank does not stop vblank NMIs from firing. Queuing here would
 * leave that many bytes sitting in the queue for the next NMI to drain --
 * and that drain's own PPU_STATUS read resets the shared address latch out
 * from under whichever of those later direct writes is then mid-sequence,
 * corrupting one nearby tile or attribute byte. That was the source of both
 * the stray blank tile at row 6 and the mis-colored 4x4 block near the left
 * margin seen in testing -- deterministic, since it depends only on where
 * setup's writes happen to land relative to the next vblank, not on timing
 * noise. Direct writes here are safe: forced blank already gives the CPU
 * exclusive VRAM access, and nothing is left in the queue for an NMI to act
 * on. */
static void draw_arcade_marquee_bulbs_direct(unsigned char phase)
{
    unsigned char i, col, lit;

    for (i = 0; i < 16; ++i) {
        col = (unsigned char)(8 + i);
        lit = (unsigned char)(((i + phase) & 3) < 2);
        set_tile(6, col, lit ? MARQUEE_DOT : MARQUEE_PANEL);
    }
}

/* Galaxy Lanes & Arcade sits north of the Pawn Shop/Laundromat side street,
 * reached by the alley between them. It shares the avenue tile set, but this
 * front is not the bar's flat plate recolored: a projected neon marquee
 * (lit top lip, "ARCADE" face, chasing bulbs, lit under-lip) hangs over the
 * door, and two small lit panes replace the bar's big paired windows. The
 * marquee kit is this scene's own CHR page (see tools/gen_arcade_chr.py). */
static void show_arcade_exterior(void)
{
    static const unsigned char arcade_palette[32] = {
        0x0F, 0x01, 0x28, 0x0F,
        /* The shared running-bond tile becomes a dark violet storefront
         * here; the awning uses index 2/3 as a brighter purple/white
         * stripe instead of the Mart's steel gray. */
        0x0F, 0x14, 0x24, 0x30,
        /* The marquee face and its chasing bulbs use index 2/3 -- a bright
         * purple face with white neon (index 3) on top, under, and as the
         * bulbs -- so the canopy reads as lit from inside. */
        0x0F, 0x14, 0x24, 0x30,
        /* Index 2 here is the small lit panes' glow; keep the wall's dark
         * violet as index 1 so a pane reads as a lit window, not paint. */
        0x0F, 0x14, 0x2C, 0x30,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    /* The arcade's own CHR page (see tools/gen_arcade_chr.py): the common
     * bank with the alley road line re-patched at tile 2 plus the neon
     * marquee kit at indices 60-64. */
    mmc3_use_arcade_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = arcade_palette[i];
    for (i = 0; i < 3; ++i) fill_row(i, CHR_TILE_PATH);
    for (i = 3; i < 24; ++i) {
        fill_title_rect(i, 0, 4, 1, CHR_TILE_PATH);
        fill_title_rect(i, 4, 24, 1, NEON_TILE_BRICK);
        fill_title_rect(i, 28, 4, 1, CHR_TILE_PATH);
    }
    /* No sidewalk here -- behind the arcade is a cul-de-sac court, so the
     * whole lower stretch is road. Rows 24-25 fill asphalt at the outer
     * columns, matching rows 26-29, and the center road runs through them. */
    for (i = 24; i < 26; ++i) {
        fill_title_rect(i, 0, 12, 1, NEON_TILE_ASPHALT);
        fill_title_rect(i, 20, 12, 1, NEON_TILE_ASPHALT);
    }
    for (i = 26; i < 30; ++i) {
        fill_title_rect(i, 0, 12, 1, NEON_TILE_ASPHALT);
        fill_title_rect(i, 20, 12, 1, NEON_TILE_ASPHALT);
    }
    /* The road widening to the cul-de-sac court narrows as it runs south,
     * off the bottom of the screen -- the same perspective cheat as the
     * side street's own north path (see v2_scene_sidestreet.c), just the
     * other direction and with no horizon cap, since this one doesn't
     * stop: it keeps going, back down to the alley below. */
    for (i = 24; i < 30; ++i) {
        unsigned char left, right, col;
        if (i < 26) { left = 12; right = 19; }
        else if (i < 28) { left = 13; right = 18; }
        else { left = 14; right = 17; }
        set_tile(i, left, ROAD_LINE_TILE);
        set_tile(i, right, ROAD_LINE_TILE);
        for (col = (unsigned char)(left + 1); col < right; ++col) {
            set_tile(i, col, NEON_TILE_ASPHALT);
        }
    }
    fill_title_rect(3, 4, 24, 1, NEON_TILE_AWNING);
    /* The projected neon marquee hangs over the door, spanning the same
     * 16-tile reach the bar's plate used but as a proper movie-theater
     * canopy instead: a lit top lip, a bright "ARCADE" face, a run of
     * chasing bulbs, and a lit under-lip framing the door below. */
    fill_title_rect(4, 8, 16, 1, MARQUEE_TUBE_TOP);
    fill_title_rect(5, 8, 16, 1, MARQUEE_PANEL);
    draw_text(5, 13, "ARCADE");
    draw_arcade_marquee_bulbs_direct(0);
    fill_title_rect(7, 8, 16, 1, MARQUEE_TUBE_UNDER);
    /* Two small lit panes flank the door -- smaller and glowing rather than
     * the bar's big dark paired windows, so the front has its own scale. */
    for (i = 10; i < 13; ++i) {
        set_tile(i, 10, NEON_LIT_WINDOW);
        set_tile(i, 20, NEON_LIT_WINDOW);
    }
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
    /* Palette 1 carries the whole facade (dark violet wall + purple marquee
     * face with white neon), so every block in the storefront reach stays on
     * it. */
    for (i = 1; i < 7; ++i) {
        set_attribute_block(1, i, 1);
        set_attribute_block(2, i, 1);
        set_attribute_block(3, i, 1);
        set_attribute_block(4, i, 1);
        set_attribute_block(5, i, 1);
    }
    /* The two small lit panes sit on their own block, index 1 kept the wall's
     * dark violet and index 2 a cyan glow. They span tile rows 10-12 at cols
     * 10 and 20, i.e. attribute block rows 2-3 and cols 2 / 5. The leftover
     * brick in each block shares palette 3's index 1 with the wall, so it
     * stays dark-violet either way. */
    set_attribute_block(2, 2, 3);
    set_attribute_block(3, 2, 3);
    set_attribute_block(2, 5, 3);
    set_attribute_block(3, 5, 3);
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* show_message_box() writes its own fixed color scheme into whatever
 * palette slot dialogue_get_palette() currently returns (global,
 * persistent state -- not something this scene controls by default) and
 * also repaints attr rows 4-5, cols 1-6 to that same slot for its own
 * panel. Palette 3 holds the two lit panes' cyan glow (attr rows 2-3,
 * cols 2 / 5), which sits entirely outside that rows 4-5 footprint, so
 * the panel never touches it; show_arcade_exterior() restores the room
 * palette after the box is dismissed. */
static void show_arcade_message(const char *line1, const char *line2, const char *line3)
{
    dialogue_set_palette(3);
    show_message_box(line1, line2, line3);
}

unsigned char run_arcade_exterior(void)
{
    /* Arrives from the south -- the only entrance is the alley behind the
     * Pawn Shop and Laundromat, one screen south, reached by walking back
     * down the road below. */
    unsigned char x = 128;
    unsigned char y = 204;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_UP;
    unsigned char pad;
    unsigned char marquee_phase = 0;
    unsigned char marquee_timer = 0;

    music_select_song(MUSIC_SONG_BAR);
    show_arcade_exterior();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        /* The marquee bulbs chase every 4 frames (about 15 steps/sec),
         * wrapping through 4 phases. Updating during vblank keeps the sign
         * from tearing against the redraw. */
        if (++marquee_timer == 4) {
            marquee_timer = 0;
            marquee_phase = (unsigned char)((marquee_phase + 1) & 3);
            draw_arcade_marquee_bulbs(marquee_phase);
        }
        pad = read_pad();
        if (pad & PAD_UP) facing = PLAYER_FACING_UP;
        else if (pad & PAD_DOWN) facing = PLAYER_FACING_DOWN;
        else if (pad & PAD_LEFT) facing = PLAYER_FACING_LEFT;
        else if (pad & PAD_RIGHT) facing = PLAYER_FACING_RIGHT;
        if (pad & PAD_START) {
            run_inventory();
            show_arcade_exterior();
            continue;
        }
        if (pad & PAD_LEFT) {
            /* West edge is a dead end for now -- nothing planned past
             * the arcade yet, so it just stops the player like any
             * other wall. */
            if (x > 12) --x;
        } else if (pad & PAD_RIGHT) {
            /* East leads to the Municipal Gym -- functional now, but the
             * facade art here still shows plain brick rather than an actual
             * passage; see this file's header comment. */
            if (x < 244) ++x;
            else return ARCADE_EXIT_GYM;
        }
        if (pad & PAD_UP) {
            if (y > 190) {
                --y;
            } else if (x >= 104 && x <= 144) {
                /* No interior yet -- see v2_scene_arcade.h. */
                show_arcade_message("GALAXY LANES & ARCADE", "NOT OPEN YET.", "COME BACK SOON.");
                show_arcade_exterior();
                continue;
            }
        } else if (pad & PAD_DOWN) {
            if (x >= 96 && x <= 159) {
                /* The road leads back down to the side street -- reaching
                 * the bottom edge here and pressing down again is the
                 * transition, the same way the side street's own horizon
                 * works walking up. */
                if (y < 216) ++y;
                else return ARCADE_EXIT_SIDESTREET;
            } else if (y < 216) {
                ++y;
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
        draw_player_sprite(x, y, walk_frame, facing);
    }
}
