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
#define FONT_TILE_BASE 8
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
        0x0F, 0x16, 0x27, 0x30, 0x0F, 0x0F, 0x27, 0x30,
        /* Sprite palette 2 is reserved for the tree trunks: three browns,
         * no white, so the reused counter tile reads as bark. */
        0x0F, 0x07, 0x17, 0x27, 0x0F, 0x0F, 0x0F, 0x30
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
    /* Tree canopy shares the grass palette, reserving palette 1 for dialogue
     * so the fountain can retain its independent blue palette during text. */
    /* Pond is blue; the low path and benches are brown. */
    set_attribute_block(2, 3, 3);
    set_attribute_block(2, 4, 3);
    set_attribute_block(3, 3, 3);
    set_attribute_block(3, 4, 3);
    for (i = 1; i < 7; ++i) set_attribute_block(5, i, 2);
    /* Match the street scenes: black asphalt with blue wet-road glints. */
    for (i = 0; i < 8; ++i) {
        set_attribute_block(6, i, 3);
        set_attribute_block(7, i, 3);
    }
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
    unsigned char row;

    /* The font's solid fill tile takes sprite palette 2's bark brown,
     * producing simple, fully opaque 16x40 trunks without canopy colors. */
    for (row = 0; row < 5; ++row) {
        set_oam_sprite((unsigned char)(44 + row * 2), 24,
                       (unsigned char)(64 + row * 8), FONT_TILE_BASE, 2);
        set_oam_sprite((unsigned char)(45 + row * 2), 32,
                       (unsigned char)(64 + row * 8), FONT_TILE_BASE, 2);
        set_oam_sprite((unsigned char)(54 + row * 2), 216,
                       (unsigned char)(72 + row * 8), FONT_TILE_BASE, 2);
        set_oam_sprite((unsigned char)(55 + row * 2), 224,
                       (unsigned char)(72 + row * 8), FONT_TILE_BASE, 2);
    }
}

static void hide_park_actors_for_traffic(void)
{
    unsigned char slot;

    /* Preserve slots 44-63: they are the permanent sprite-backed trunks.
     * The player, patrons, and taxi all have disposable slots below them. */
    hide_player_sprite();
    hide_taxi_sprite();
    for (slot = 20; slot < 24; ++slot) {
        oam_buf[(unsigned int)slot * 4] = 0xFF;
    }
    for (slot = 30; slot < 38; ++slot) {
        oam_buf[(unsigned int)slot * 4] = 0xFF;
    }
}

static unsigned char park_trunk_blocks(unsigned char x, unsigned char y)
{
    /* Player coordinates are feet. These narrow boxes cover only the brown
     * stems, leaving every green canopy region freely walkable "behind." */
    if (x >= 20 && x <= 44 && y >= 64 && y <= 104) return 1;
    if (x >= 212 && x <= 236 && y >= 72 && y <= 112) return 1;
    return 0;
}

static unsigned char park_fountain_blocks(unsigned char x, unsigned char y)
{
    /* x/y are the player's feet. Keep the complete 16x24 body clear of the
     * fountain's blue tile field, not merely its lower collision point. */
    return x > 72 && x < 184 && y > 56 && y < 168;
}

static unsigned char park_blocks(unsigned char x, unsigned char y)
{
    return park_trunk_blocks(x, y) || park_fountain_blocks(x, y);
}

static unsigned char park_player_is_in_top_foliage(unsigned char x, unsigned char y)
{
    /* Only the upper halves of the four canopy rectangles are foreground.
     * The lower halves remain ordinary lawn, even beside a tree trunk. */
    return (x >= 8 && x <= 56 && y >= 24 && y <= 43) ||
           (x >= 200 && x <= 248 && y >= 32 && y <= 51) ||
           (x >= 16 && x <= 56 && y >= 128 && y <= 143) ||
           (x >= 208 && x <= 240 && y >= 136 && y <= 151);
}

static void show_park_message(const char *line1, const char *line2,
                              const char *line3)
{
    dialogue_set_palette(1);
    show_message_box(line1, line2, line3);
    dialogue_set_palette(3);
}

unsigned char run_park(void)
{
    unsigned char x = 24;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char fountain_prompted = 0;
    unsigned char park_regular_one_prompted = 0;
    unsigned char park_regular_two_prompted = 0;
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
        if (pad & PAD_UP) facing = PLAYER_FACING_UP;
        else if (pad & PAD_DOWN) facing = PLAYER_FACING_DOWN;
        else if (pad & PAD_LEFT) facing = PLAYER_FACING_LEFT;
        else if (pad & PAD_RIGHT) facing = PLAYER_FACING_RIGHT;
        if (traffic_state == 1) {
            taxi_x += taxi_moving_left ? -3 : 3;
            draw_taxi_sprite(taxi_x, taxi_moving_left);
            if (taxi_x <= (int)x + 8 && taxi_x + 32 >= (int)x - 8) {
                traffic_state = 2;
                hide_park_actors_for_traffic();
                set_traffic_crash_oam_base(20);
                draw_traffic_particles(x, taxi_moving_left);
                draw_crash_tableau(x, taxi_moving_left);
                set_traffic_crash_oam_base(48);
                music_sfx_hurt();
                dialogue_set_palette(1);
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
        if (!(pad & PAD_A)) {
            fountain_prompted = 0;
            park_regular_one_prompted = 0;
            park_regular_two_prompted = 0;
        }
        if (pad & PAD_LEFT) {
            if (x > 12) {
                if (!park_blocks((unsigned char)(x - 1), y)) --x;
            } else {
                return 0;
            }
        } else if (pad & PAD_RIGHT) {
            if (x < 244 && !park_blocks((unsigned char)(x + 1), y)) ++x;
        }
        if (pad & PAD_UP) {
            if (y > 40 && !park_blocks(x, (unsigned char)(y - 1))) --y;
        } else if (pad & PAD_DOWN) {
            if (y < 216 && !park_blocks(x, (unsigned char)(y + 1))) ++y;
        }
        if (y >= 214) {
            traffic_state = 1;
            taxi_moving_left = x < 128;
            taxi_x = taxi_moving_left ? 224 : 0;
            hide_park_actors_for_traffic();
        }
        /* The fountain is solid, so this interaction area includes its full
         * footprint plus the walkable perimeter at the south edge. */
        if (x >= 72 && x <= 184 && y >= 40 && y <= 176) {
            if ((pad & PAD_A) && !fountain_prompted) {
                fountain_prompted = 1;
                show_park_message("THE FOUNTAIN IS DRY.", "SOMEBODY STOLE", "THE WATER.");
                show_park();
            }
        }
        if (park_regular_is_near(0, x, y)) {
            if ((pad & PAD_A) && !park_regular_one_prompted) {
                park_regular_one_prompted = 1;
                show_park_message("I CAME FOR THE", "FOUNTAIN, GIGGITY,",
                                  "BUT IT'S DRY.");
                show_park();
            }
        }
        if (park_regular_is_near(1, x, y)) {
            if ((pad & PAD_A) && !park_regular_two_prompted) {
                park_regular_two_prompted = 1;
                show_park_message("THE TREES KNOW", "TOO MUCH.", 0);
                show_park();
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
        /* Two separately seeded, slow patrons make the park feel occupied
         * and guarantee the idle scene remains on the same frame cadence as
         * movement-heavy scenes without coupling any behavior to input. */
        update_park_regulars();
        draw_regulars();
        draw_park_trunks();
        draw_player_sprite(x, y, walk_frame, facing);
        /* The canopy is foreground scenery only in its upper halves; the
         * lower canopy/lawn region must never hide a grounded player. */
        if (park_player_is_in_top_foliage(x, y)) {
            for (sprite_slot = 24; sprite_slot < 30; ++sprite_slot) {
                oam_buf[(unsigned int)sprite_slot * 4 + 2] |= 0x20;
            }
        }
    }
}
