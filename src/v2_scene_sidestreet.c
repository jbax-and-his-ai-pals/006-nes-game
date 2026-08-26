#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_sidestreet.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"

#define CHR_TILE_PATH 3
#define SIDESTREET_TILE_HLINE 1
#define SIDESTREET_TILE_LINE 2
#define DOORKNOB_TILE 51
#define NEON_TILE_ASPHALT 192
#define NEON_TILE_SIDEWALK 193
#define NEON_TILE_BRICK 194
#define NEON_TILE_AWNING 195
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199

static void show_sidestreet_message(const char *line1, const char *line2, const char *line3);

/* One screen west of the Mart: two 12-tile-wide storefronts (Pawn Shop and
 * Laundromat), each running flush to its own screen edge, with an 8-tile
 * walkable gap between them -- instead of the one wide facade the Mart/Bar/
 * Arcade share with a path margin on both sides. draw_storefront_sign() is
 * hardcoded to a fixed 16-tile span sized for a 24-tile-wide building (see
 * its comment in v2_scene_ui.c), too wide to reuse here, so each shop gets
 * its own narrower plate via draw_storefront_sign_at() instead -- same tile
 * kit, same look, just sized to fit. The center gap leads north to the
 * Arcade -- see run_sidestreet_exterior()'s own comment on the horizon. */
static void show_sidestreet_exterior(void)
{
    static const unsigned char sidestreet_palette[32] = {
        0x0F, 0x01, 0x28, 0x0F,
        /* Pawn Shop: a worn brown/tan brick instead of the Mart's cinder
         * gray or the Arcade's violet. */
        0x0F, 0x07, 0x17, 0x30,
        /* Laundromat: a clean blue instead -- deliberately its own
         * building-only color, not shared with anything else on this
         * street, so the two shops read as distinct at a glance. */
        0x0F, 0x11, 0x21, 0x30,
        /* Palette 3 is deliberately left unused by any attribute block in
         * this scene -- reserved entirely for dialogue_set_palette(3), so
         * both the traffic-death message and the "not open yet" door
         * messages can use it with zero risk of recoloring anything still
         * visible, unlike the Arcade's own pal3, which had to share space
         * with the window art already living there (see run_arcade_
         * exterior()'s comment on that tradeoff). */
        0x0F, 0x00, 0x10, 0x20,
        /* Shared player/taxi/crash sprite palettes -- identical to every
         * other exterior, not scene-specific. */
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_sidestreet_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = sidestreet_palette[i];
    for (i = 0; i < 3; ++i) fill_row(i, CHR_TILE_PATH);
    for (i = 3; i < 24; ++i) {
        fill_title_rect(i, 0, 12, 1, NEON_TILE_BRICK);
        fill_title_rect(i, 12, 8, 1, CHR_TILE_PATH);
        fill_title_rect(i, 20, 12, 1, NEON_TILE_BRICK);
    }
    /* A perspective cheat: two rail lines taper inward as the path climbs
     * north, stepping every 2 rows (the steepest cadence a single-tile
     * shift can give), stopping at width 4 rather than narrowing all the
     * way down -- one more step would put the two rails one tile apart
     * with nothing between them, reading as a leftover stub rather than
     * part of the taper. The horizon cap sits directly above the last
     * taper row and spans the full gap, wall to wall, so it reads as
     * closing the street rather than just capping the rails. Both line
     * tiles are dedicated art (see tools/gen_sidestreet_chr.py) rather
     * than a reuse of the message box's border tile or the sidewalk's
     * curb tile -- both of those fill most of the 8x8 with a second color
     * instead of staying transparent, which reads as a solid bar or a row
     * of dashes rather than a line. The gap between the rails is fully
     * dusted -- this is a road too, the same as the asphalt it flows into
     * below. */
    for (i = 19; i < 24; ++i) {
        unsigned char left, right, col;
        if (i >= 22) { left = 12; right = 19; }
        else if (i >= 20) { left = 13; right = 18; }
        else { left = 14; right = 17; }
        set_tile(i, left, SIDESTREET_TILE_LINE);
        set_tile(i, right, SIDESTREET_TILE_LINE);
        for (col = (unsigned char)(left + 1); col < right; ++col) {
            set_tile(i, col, NEON_TILE_ASPHALT);
        }
    }
    for (i = 12; i < 20; ++i) set_tile(18, i, SIDESTREET_TILE_HLINE);
    for (i = 24; i < 26; ++i) {
        fill_title_rect(i, 0, 12, 1, NEON_TILE_SIDEWALK);
        /* No curb across the gap -- the sidewalk breaks here so the north
         * path reads as flowing straight down into the main road instead
         * of being cut off by a crossing sidewalk. */
        fill_title_rect(i, 12, 8, 1, NEON_TILE_ASPHALT);
        fill_title_rect(i, 20, 12, 1, NEON_TILE_SIDEWALK);
    }
    for (i = 26; i < 30; ++i) fill_row(i, NEON_TILE_ASPHALT);
    fill_title_rect(3, 0, 12, 1, NEON_TILE_AWNING);
    fill_title_rect(3, 20, 12, 1, NEON_TILE_AWNING);
    draw_storefront_sign_at(8, 0, 12);
    draw_storefront_sign_at(8, 20, 12);
    draw_text(9, 1, "PAWN SHOP");
    draw_text(9, 21, "LAUNDROMAT");
    for (i = 17; i < 24; ++i) {
        set_tile(i, 6, NEON_TILE_DOOR_L);
        set_tile(i, 7, NEON_TILE_DOOR_C);
        set_tile(i, 8, NEON_TILE_DOOR_R);
        set_tile(i, 22, NEON_TILE_DOOR_L);
        set_tile(i, 23, NEON_TILE_DOOR_C);
        set_tile(i, 24, NEON_TILE_DOOR_R);
    }
    /* Doorknob sits on the door's right-hand tile, not dead center -- see
     * run_arcade_exterior()'s door, which uses the same offset. */
    set_tile(20, 8, DOORKNOB_TILE);
    set_tile(20, 24, DOORKNOB_TILE);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 1; i <= 5; ++i) {
        set_attribute_block(i, 0, 1);
        set_attribute_block(i, 1, 1);
        set_attribute_block(i, 2, 1);
        set_attribute_block(i, 3, 0);
        set_attribute_block(i, 4, 0);
        set_attribute_block(i, 5, 2);
        set_attribute_block(i, 6, 2);
        set_attribute_block(i, 7, 2);
    }
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Unlike the Arcade (see its own show_arcade_message() comment), palette 3
 * is never claimed by this scene's background art, so it's always safe for
 * show_message_box()'s own repaint -- no footprint reasoning needed here. */
static void show_sidestreet_message(const char *line1, const char *line2, const char *line3)
{
    dialogue_set_palette(3);
    show_message_box(line1, line2, line3);
}

unsigned char run_sidestreet_exterior(void)
{
    /* Arrives at the east edge -- the only entrance is the footpath from
     * the Mart. */
    unsigned char x = 232;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char pad;
    unsigned char traffic_state = 0;
    unsigned char taxi_moving_left = 0;
    unsigned char crash_timer = 0;
    unsigned char death_released = 0;
    int taxi_x = 0;

    music_select_song(MUSIC_SONG_BAR);
    show_sidestreet_exterior();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if (pad & PAD_UP) facing = PLAYER_FACING_UP;
        else if (pad & PAD_DOWN) facing = PLAYER_FACING_DOWN;
        else if (pad & PAD_LEFT) facing = PLAYER_FACING_LEFT;
        else if (pad & PAD_RIGHT) facing = PLAYER_FACING_RIGHT;
        if (traffic_state == 0) {
            if (pad & PAD_START) {
                run_inventory();
                show_sidestreet_exterior();
                continue;
            }
            if (pad & PAD_LEFT) {
                /* North of the sidewalk (y < 190, i.e. already in the
                 * alley between the two buildings), clamp to the gap's own
                 * width instead of the full street -- otherwise the player
                 * could slide sideways out of the alley and through brick
                 * that has no collision of its own to stop them. */
                if (y < 190) {
                    if (x > 96) --x;
                } else if (x > 12) {
                    /* West edge is a dead end for now -- nothing planned
                     * past this street yet. */
                    --x;
                }
            } else if (pad & PAD_RIGHT) {
                if (y < 190) {
                    if (x < 159) ++x;
                } else if (x < 244) {
                    ++x;
                } else {
                    return SIDESTREET_EXIT_MART;
                }
            }
            if (pad & PAD_UP) {
                if (x >= 96 && x <= 159) {
                    /* The gap leads north to the Arcade, capped by the
                     * horizon line at row 18 (y 144) -- the rail lines and
                     * horizon are the *only* solid ground here, so the
                     * boundary has to actually stop the player, not just
                     * be decoration they can walk through. Reaching it and
                     * pressing up again is the transition, the same way a
                     * door's y <= 190 threshold triggers its message. */
                    if (y > 144) --y;
                    else return SIDESTREET_EXIT_ARCADE;
                } else if (x >= 40 && x <= 80) {
                    if (y > 190) --y;
                    else {
                        /* Pawn Shop door -- no interior yet. */
                        show_sidestreet_message("THE PAWN SHOP", "IS NOT OPEN YET.", "COME BACK SOON.");
                        show_sidestreet_exterior();
                        continue;
                    }
                } else if (x >= 168 && x <= 208) {
                    if (y > 190) --y;
                    else {
                        /* Laundromat door -- no interior yet. */
                        show_sidestreet_message("THE LAUNDROMAT", "IS NOT OPEN YET.", "COME BACK SOON.");
                        show_sidestreet_exterior();
                        continue;
                    }
                } else if (y > 190) {
                    --y;
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
            draw_player_sprite(x, y, walk_frame, facing);
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
                    dialogue_set_palette(3);
                    show_traffic_death();
                }
            }
        } else {
            if (!(pad & PAD_A)) {
                if (death_released < 4) ++death_released;
            } else if (death_released >= 4) {
                return SIDESTREET_EXIT_TRAFFIC_DEATH;
            }
        }
    }
}
