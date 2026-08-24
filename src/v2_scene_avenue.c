#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_avenue.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"

#define CHR_TILE_PATH 3
#define DOORKNOB_TILE 51
#define NEON_TILE_ASPHALT 192
#define NEON_TILE_SIDEWALK 193
#define NEON_TILE_BRICK 194
#define NEON_TILE_AWNING 195
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199
#define MANHOLE_LEFT 4
#define MANHOLE_TOP 25
#define MANHOLE_X 48
#define MANHOLE_Y 204
#define MANHOLE_TILE_L 6
#define MANHOLE_TILE_R 7
#define MANHOLE_TILE_M 54

extern volatile unsigned char title_mouth_dirty;
extern volatile unsigned char title_message_dirty;
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
    draw_storefront_sign();
    draw_text(9, 10, "JIMBLES'S BAR");
    /* The one brick row beneath the plate keeps the sign from crowding the
     * windows. Windows cross two attribute blocks here; palette 3's index-1
     * is intentionally the same red as brick, so the surrounding wall stays
     * red while each window's index-2 glass remains blue. */
    draw_storefront_window(13, 8);
    draw_storefront_window(13, 20);
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

unsigned char run_neon_avenue(unsigned char arrival)
{
    unsigned char x = arrival == 1 ? MANHOLE_X :
                      (arrival == 2 ? 24 : (arrival == 3 ? 232 : 128));
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

    music_select_song(MUSIC_SONG_AVENUE);
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
                else return AVENUE_EXIT_PARK;
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
                return AVENUE_EXIT_TRAFFIC_DEATH;
            }
        }
    }
}
