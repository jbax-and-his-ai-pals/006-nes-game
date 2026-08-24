#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_regulars.h"
#include "v2_runtime.h"
#include "v2_scene_park.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"

#define NEON_TILE_ASPHALT 192
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_WINDOW_FILL 211
#define MANHOLE_TILE_L 6
#define MANHOLE_TILE_R 7
#define MANHOLE_TILE_M 54
#define BAR_TILE_FLOOR 235
#define BAR_TILE_WALL 236
#define BAR_TILE_COUNTER 237
#define BAR_TILE_STOOL 243
/* Moonlight Park is intentionally a quiet counterpoint to the storefronts:
 * a green, open tableau with a footpath, fountain, bench, and oversized
 * trees. Its isolated CHR page begins as a common-tile clone, so its palette
 * and composition can evolve without changing the street scenes. */
static void show_park(void)
{
    static const unsigned char park_palette[32] = {
        /* Grass, foliage, path wood, and water each have a dedicated
         * background palette. The shared bar tiles now read as park shapes
         * instead of inheriting the bar's red/brown color language. */
        0x0F, 0x0A, 0x1A, 0x30, 0x0F, 0x0B, 0x19, 0x30,
        0x0F, 0x07, 0x17, 0x30, 0x0F, 0x01, 0x21, 0x30,
        0x0F, 0x16, 0x27, 0x30, 0x0F, 0x16, 0x06, 0x30,
        /* Sprite palette 2 is reserved for the tree trunks: three browns,
         * no white, so the reused counter tile reads as bark. */
        0x0F, 0x07, 0x17, 0x27, 0x0F, 0x16, 0x28, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_park_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = park_palette[i];
    for (i = 0; i < 30; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(26, 0, 32, 4, NEON_TILE_ASPHALT);
    /* Four uneven tree clumps frame the open lawn. Palette 1 gives the
     * shared wall tile its dark-green canopy rather than brick-red fill. */
    fill_title_rect(3, 1, 6, 5, BAR_TILE_WALL);
    fill_title_rect(4, 25, 6, 5, BAR_TILE_WALL);
    fill_title_rect(16, 2, 5, 4, BAR_TILE_WALL);
    fill_title_rect(17, 26, 4, 4, BAR_TILE_WALL);
    fill_title_rect(7, 3, 2, 4, BAR_TILE_COUNTER);
    fill_title_rect(8, 27, 2, 4, BAR_TILE_COUNTER);
    draw_text(1, 9, "MOONLIGHT PARK");
    /* Blue pond with a small round fountain in its center. */
    fill_title_rect(7, 11, 10, 7, NEON_TILE_WINDOW_FILL);
    fill_title_rect(8, 10, 12, 9, NEON_TILE_WINDOW_FILL);
    fill_title_rect(9, 10, 12, 9, NEON_TILE_WINDOW_FILL);
    fill_title_rect(10, 11, 10, 7, NEON_TILE_WINDOW_FILL);
    set_tile(8, 14, MANHOLE_TILE_L);
    set_tile(8, 15, MANHOLE_TILE_M);
    set_tile(8, 16, MANHOLE_TILE_M);
    set_tile(8, 17, MANHOLE_TILE_R);
    set_tile(9, 14, MANHOLE_TILE_L);
    set_tile(9, 15, MANHOLE_TILE_M);
    set_tile(9, 16, MANHOLE_TILE_M);
    set_tile(9, 17, MANHOLE_TILE_R);
    /* A single winding brown walk and two benches keep the lawn readable
     * without reverting the whole scene to the bar's striped floor. */
    fill_title_rect(20, 4, 24, 2, BAR_TILE_COUNTER);
    fill_title_rect(18, 4, 5, 1, BAR_TILE_COUNTER);
    fill_title_rect(19, 4, 5, 1, BAR_TILE_STOOL);
    fill_title_rect(18, 23, 5, 1, BAR_TILE_COUNTER);
    fill_title_rect(19, 23, 5, 1, BAR_TILE_STOOL);
    for (i = 0; i < 8; ++i) {
        set_attribute_block(0, i, 0);
        set_attribute_block(1, i, 0);
        set_attribute_block(2, i, 0);
        set_attribute_block(3, i, 0);
        set_attribute_block(4, i, 0);
        set_attribute_block(5, i, 0);
        set_attribute_block(6, i, 0);
        set_attribute_block(7, i, 0);
    }
    /* Tree canopy blocks. */
    set_attribute_block(1, 0, 1);
    set_attribute_block(1, 1, 1);
    set_attribute_block(1, 6, 1);
    set_attribute_block(1, 7, 1);
    set_attribute_block(2, 0, 1);
    set_attribute_block(2, 1, 1);
    set_attribute_block(2, 6, 1);
    set_attribute_block(2, 7, 1);
    set_attribute_block(4, 0, 1);
    set_attribute_block(4, 1, 1);
    set_attribute_block(4, 6, 1);
    set_attribute_block(4, 7, 1);
    /* Pond is blue; the low path and benches are brown. */
    set_attribute_block(2, 3, 3);
    set_attribute_block(2, 4, 3);
    set_attribute_block(3, 3, 3);
    set_attribute_block(3, 4, 3);
    for (i = 1; i < 7; ++i) set_attribute_block(5, i, 2);
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The foliage remains background-only and deliberately non-solid. Brown
 * trunks are sprite-backed so the shared 4x4 background-palette granularity
 * cannot turn whole tree canopies brown just to color a two-tile stem. */
static void draw_park_trunks(void)
{
    set_oam_sprite(40, 28, 72, NEON_TILE_DOOR_C, 2);
    set_oam_sprite(41, 28, 80, NEON_TILE_DOOR_C, 2);
    set_oam_sprite(42, 220, 80, NEON_TILE_DOOR_C, 2);
    set_oam_sprite(43, 220, 88, NEON_TILE_DOOR_C, 2);
}

static unsigned char park_trunk_blocks(unsigned char x, unsigned char y)
{
    /* Player coordinates are feet. These narrow boxes cover only the brown
     * stems, leaving every green canopy region freely walkable "behind." */
    if (x >= 20 && x <= 44 && y >= 64 && y <= 104) return 1;
    if (x >= 212 && x <= 236 && y >= 72 && y <= 112) return 1;
    return 0;
}

unsigned char run_park(void)
{
    unsigned char x = 24;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char fountain_prompted = 0;
    unsigned char pad;
    unsigned char sprite_slot;
    unsigned char traffic_state = 0;
    unsigned char taxi_moving_left = 0;
    unsigned char death_released = 0;
    int taxi_x = 0;
    music_select_song(MUSIC_SONG_PARK);
    show_park();
    /* Keep a distinct ambient stream. Music remains NMI-clocked. */
    reset_park_regulars();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if (traffic_state == 1) {
            taxi_x += taxi_moving_left ? -3 : 3;
            draw_taxi_sprite(taxi_x, taxi_moving_left);
            if (taxi_x <= (int)x + 8 && taxi_x + 32 >= (int)x - 8) {
                traffic_state = 2;
                hide_player_sprite();
                clear_sprites();
                draw_traffic_particles(x, taxi_moving_left);
                draw_crash_tableau(x, taxi_moving_left);
                music_sfx_hurt();
                show_traffic_death();
            }
            continue;
        }
        if (traffic_state == 2) {
            if (!(pad & PAD_A)) {
                if (death_released < 4) ++death_released;
            } else if (death_released >= 4) {
                return PARK_EXIT_TRAFFIC_DEATH;
            }
            continue;
        }
        if (pad & PAD_START) {
            run_inventory();
            show_park();
            continue;
        }
        if (pad & PAD_LEFT) {
            if (x > 12) {
                if (!park_trunk_blocks((unsigned char)(x - 1), y)) --x;
            } else {
                return 0;
            }
        } else if (pad & PAD_RIGHT) {
            if (x < 244 && !park_trunk_blocks((unsigned char)(x + 1), y)) ++x;
        }
        if (pad & PAD_UP) {
            if (y > 40 && !park_trunk_blocks(x, (unsigned char)(y - 1))) --y;
        } else if (pad & PAD_DOWN) {
            if (y < 216 && !park_trunk_blocks(x, (unsigned char)(y + 1))) ++y;
        }
        if (y >= 214) {
            traffic_state = 1;
            taxi_moving_left = x < 128;
            taxi_x = taxi_moving_left ? 224 : 0;
            clear_sprites();
        }
        if (x >= 96 && x <= 168 && y >= 56 && y <= 120) {
            if ((pad & PAD_A) && !fountain_prompted) {
                fountain_prompted = 1;
                show_message_box("THE FOUNTAIN IS DRY.", "SOMEBODY STOLE", "THE WATER.");
                show_park();
            }
        } else {
            fountain_prompted = 0;
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
        /* Two separately seeded, slow patrons make the park feel occupied
         * and guarantee the idle scene remains on the same frame cadence as
         * movement-heavy scenes without coupling any behavior to input. */
        update_park_regulars();
        draw_regulars();
        draw_park_trunks();
        draw_player_sprite(x, y, walk_frame);
        /* The canopy is foreground scenery. Only while the 16x24 player
         * overlaps one of its four rectangles does bit 5 put the player
         * behind nonzero background pixels; the small brown trunks stay
         * collision-solid, but the leafy mass is freely passable. */
        if ((x < 64 || x > 192) && (y < 72 || (y > 120 && y < 176))) {
            for (sprite_slot = 24; sprite_slot < 30; ++sprite_slot) {
                oam_buf[(unsigned int)sprite_slot * 4 + 2] |= 0x20;
            }
        }
    }
}
