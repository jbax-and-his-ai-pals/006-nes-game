#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_arcade.h"
#include "v2_scene_ui.h"

#define CHR_TILE_PATH 3
#define ROAD_LINE_TILE 2
#define DOORKNOB_TILE 51
#define NEON_TILE_ASPHALT 192
#define NEON_TILE_SIDEWALK 193
#define NEON_TILE_BRICK 194
#define NEON_TILE_AWNING 195
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199

static void show_arcade_message(const char *line1, const char *line2, const char *line3);

/* Galaxy Lanes & Arcade sits north of the Pawn Shop/Laundromat side street,
 * reached by the alley between them -- same shared avenue tile set and
 * full-height storefront treatment as the Mart and the bar, so the street
 * reads as one continuous block, just recolored into a neon purple/gold
 * arcade front instead of the Mart's cinder gray. */
static void show_arcade_exterior(void)
{
    static const unsigned char arcade_palette[32] = {
        0x0F, 0x01, 0x28, 0x0F,
        /* The shared running-bond tile becomes a dark violet storefront
         * here; the awning uses index 2/3 as a brighter purple/white
         * stripe instead of the Mart's steel gray. */
        0x0F, 0x14, 0x24, 0x30,
        /* The plate art maps index 1 to trim and index 2 to its black
         * center (see draw_storefront_sign()'s own comment) -- gold trim
         * instead of the Mart's green, for a marquee feel. */
        0x0F, 0x28, 0x0F, 0x30,
        /* Window blocks keep the wall's dark violet as their index 1, with
         * a cyan glass instead of the Mart's blue. */
        0x0F, 0x14, 0x2C, 0x30,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    /* The side street's own CHR page (see tools/gen_sidestreet_chr.py) is
     * just the common bank with a thin line tile patched in at tile 2 --
     * this scene doesn't use tile 2 for anything else (it's normally the
     * Piss Corner regulars' facial overlay), so reusing that page here
     * gets the same road art for free instead of needing a second
     * dedicated CHR bank. */
    mmc3_use_sidestreet_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = arcade_palette[i];
    for (i = 0; i < 3; ++i) fill_row(i, CHR_TILE_PATH);
    for (i = 3; i < 24; ++i) {
        fill_title_rect(i, 0, 4, 1, CHR_TILE_PATH);
        fill_title_rect(i, 4, 24, 1, NEON_TILE_BRICK);
        fill_title_rect(i, 28, 4, 1, CHR_TILE_PATH);
    }
    for (i = 24; i < 26; ++i) {
        fill_title_rect(i, 0, 12, 1, NEON_TILE_SIDEWALK);
        fill_title_rect(i, 20, 12, 1, NEON_TILE_SIDEWALK);
    }
    for (i = 26; i < 30; ++i) {
        fill_title_rect(i, 0, 12, 1, NEON_TILE_ASPHALT);
        fill_title_rect(i, 20, 12, 1, NEON_TILE_ASPHALT);
    }
    /* The road splitting the sidewalk narrows as it runs south, off the
     * bottom of the screen -- the same perspective cheat as the side
     * street's own north path (see v2_scene_sidestreet.c), just the other
     * direction and with no horizon cap, since this one doesn't stop: it
     * keeps going, back down to the alley below. */
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
    /* Same rounded neon storefront sign kit as the Mart and the bar, just
     * recolored -- see the palette comment above. */
    draw_storefront_sign();
    draw_text(9, 13, "ARCADE");
    draw_storefront_window(13, 8);
    draw_storefront_window(13, 20);
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

/* show_message_box() writes its own fixed color scheme into whatever
 * palette slot dialogue_get_palette() currently returns (global,
 * persistent state -- not something this scene controls by default) and
 * also repaints attr rows 4-5, cols 1-6 to that same slot for its own
 * panel. Palette 3 (this facade's window blocks, at rows 3-4) is the
 * closest fit -- its row-4 cells are inside the panel's footprint and
 * get overwritten either way, and its row-3 cells are a small enough
 * miss that it beats every other slot (0 and 1 both stay visible well
 * outside the panel; 2 is the sign, always visible). */
static void show_arcade_message(const char *line1, const char *line2, const char *line3)
{
    dialogue_set_palette(3);
    show_message_box(line1, line2, line3);
}

void run_arcade_exterior(void)
{
    /* Arrives from the south -- the only entrance is the alley behind the
     * Pawn Shop and Laundromat, one screen south, reached by walking back
     * down the road splitting the sidewalk below. */
    unsigned char x = 128;
    unsigned char y = 204;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_UP;
    unsigned char pad;

    music_select_song(MUSIC_SONG_BAR);
    show_arcade_exterior();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
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
            /* East edge is a dead end too -- the only way out is back
             * down the road below. */
            if (x < 244) ++x;
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
                else return;
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
