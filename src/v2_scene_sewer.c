#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_game_state.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_sewer.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"

#define NEON_TILE_ASPHALT 192
#define NEON_TILE_SIDEWALK 193
#define NEON_TILE_WINDOW_V 210
#define NEON_TILE_WINDOW_FILL 211
#define SEWER_TILE_FRAME_TL 51
#define SEWER_TILE_FRAME_T 52
#define BAR_TILE_FLOOR 235
#define BAR_TILE_WALL 236
#define TOKEN_TILE 255
#define has_sewer_key game_state.has_sewer_key

/* Sewer scenery uses a dedicated CHR page, so swap to the common page for
 * the same smooth blue-and-green title-plate message treatment as all UI. */
static void show_sewer_message(const char *line1, const char *line2)
{
    unsigned char pad;

    PPU.mask = 0;
    mmc3_use_common_chr();
    show_traffic_death_message(line1, line2);

    do {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();
    } while (pad & PAD_A);
    do {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();
    } while (!(pad & PAD_A));
}

/* A compact first sewer chamber. The shared floor/wall/asphalt/sidewalk and
 * window-edge tiles become damp stone, black water, and a ladder under a new
 * palette; dedicated sewer art can replace these once scene CHR banks exist. */
static void show_sewer_room(unsigned char room)
{
    static const unsigned char sewer_palette[32] = {
        0x0F, 0x01, 0x21, 0x30, 0x0F, 0x00, 0x10, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x28, 0x21, 0x30,
        0x0F, 0x16, 0x06, 0x30, 0x0F, 0x0F, 0x28, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_sewer_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = sewer_palette[i];

    for (i = 3; i < 25; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(25, 0, 32, 5, NEON_TILE_ASPHALT);
    fill_title_rect(0, 0, 2, 30, BAR_TILE_WALL);
    fill_title_rect(0, 30, 2, 30, BAR_TILE_WALL);

    if (room == 0) {
        /* The shaft wall ends at the ladder's final rung; the open floor
         * begins directly below it and carries the eastward passage. */
        fill_title_rect(3, 0, 32, 10, BAR_TILE_WALL);
        fill_title_rect(13, 30, 2, 12, BAR_TILE_FLOOR);
        draw_text(1, 11, "LADDER SHAFT");
        for (i = 3; i < 13; ++i) {
            set_tile(i, 14, NEON_TILE_WINDOW_V);
            set_tile(i, 17, NEON_TILE_WINDOW_V);
            if ((i & 1) == 0) {
                set_tile(i, 15, NEON_TILE_SIDEWALK);
                set_tile(i, 16, NEON_TILE_SIDEWALK);
            }
        }
    } else if (room == 1) {
        /* Match the shaft room: a solid upper wall gives the junction a
         * proper floor line, while open side edges advertise both routes. */
        fill_title_rect(3, 0, 32, 10, BAR_TILE_WALL);
        fill_title_rect(13, 0, 2, 12, BAR_TILE_FLOOR);
        fill_title_rect(13, 30, 2, 12, BAR_TILE_FLOOR);
        draw_text(1, 10, "FLOW JUNCTION");
        fill_title_rect(6, 2, 28, 3, NEON_TILE_WINDOW_FILL);
        fill_title_rect(6, 2, 28, 1, SEWER_TILE_FRAME_T);
        set_tile(6, 2, SEWER_TILE_FRAME_TL);
        for (i = 2; i < 30; i += 3) set_tile(7, i, NEON_TILE_WINDOW_V);
        set_tile(8, 2, SEWER_TILE_FRAME_TL);
    } else {
        fill_title_rect(3, 0, 32, 10, BAR_TILE_WALL);
        fill_title_rect(13, 0, 2, 12, BAR_TILE_FLOOR);
        fill_title_rect(13, 30, 2, 12, BAR_TILE_FLOOR);
        fill_title_rect(6, 2, 28, 3, NEON_TILE_WINDOW_FILL);
        fill_title_rect(6, 2, 28, 1, SEWER_TILE_FRAME_T);
        set_tile(6, 2, SEWER_TILE_FRAME_TL);
        for (i = 2; i < 30; i += 3) set_tile(7, i, NEON_TILE_WINDOW_V);
        set_tile(8, 2, SEWER_TILE_FRAME_TL);
        if (!has_sewer_key) set_tile(20, 22, TOKEN_TILE);
    }
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

/* The ladder rails make the shaft a narrow route while preserving the
 * approach from the floor below. */
static unsigned char sewer_upper_wall_blocks(unsigned char room, unsigned char x,
                                             unsigned char y)
{
    if (room == 0) return y <= 112 && (x < 120 || x > 136);
    return (room == 1 || room == 2) && y <= 112;
}

unsigned char run_sewer(void)
{
    unsigned char x = 128;
    unsigned char y = 56;
    unsigned char room = 0;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;
    unsigned char traffic_state = 0;
    unsigned char taxi_moving_left = 0;
    unsigned char crash_timer = 0;
    unsigned char death_released = 0;
    int taxi_x = 0;

    music_select_song(MUSIC_SONG_SEWER);
    show_sewer_room(room);
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();

        if (traffic_state == 0 && (pad & PAD_START)) {
            run_inventory();
            show_sewer_room(room);
            continue;
        }

        if (traffic_state == 0 && (pad & PAD_LEFT)) {
            if (x > 20) {
                if (!sewer_upper_wall_blocks(room, x - 1, y)) --x;
            } else if (room > 0) {
                --room;
                x = 232;
                show_sewer_room(room);
            }
        } else if (traffic_state == 0 && (pad & PAD_RIGHT)) {
            if (x < 236) {
                if (!sewer_upper_wall_blocks(room, x + 1, y)) ++x;
            } else if (room < 2) {
                if (room != 0 || y > 112) {
                    ++room;
                    x = 24;
                    show_sewer_room(room);
                }
            } else {
                return SEWER_EXIT_ZOO;
            }
        }
        if (traffic_state == 0 && (pad & PAD_UP)) {
            if (y > 48) {
                if (!sewer_upper_wall_blocks(room, x, y - 1)) --y;
            } else if (room == 0 && x >= 104 && x <= 152) return SEWER_EXIT_AVENUE;
        } else if (traffic_state == 0 && (pad & PAD_DOWN)) {
            if (y < (room < 2 ? 216 : 208) &&
                !sewer_upper_wall_blocks(room, x, y + 1)) ++y;
        }
        if (room == 2 && !has_sewer_key && x >= 164 && x <= 196 &&
            y >= 144 && y <= 176) {
            has_sewer_key = 1;
            show_sewer_message("YOU FOUND A SEWER KEY.",
                               "IT SMELLS LIKE A DRAIN.");
            show_sewer_room(room);
        }
        if (traffic_state == 0 && room < 2 && y >= 214) {
            traffic_state = 1;
            taxi_moving_left = x < 128;
            taxi_x = taxi_moving_left ? 224 : 0;
        }
        if (traffic_state == 1 || traffic_state == 2) {
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
                    /* The traffic plate uses the shared smooth border tiles,
                     * not the sewer scenery replacements. */
                    PPU.mask = 0;
                    mmc3_use_common_chr();
                    show_traffic_death_message("HOW THE HELL DID A", "TAXI GET DOWN HERE?");
                }
            }
        } else if (traffic_state == 3) {
            if (!(pad & PAD_A)) {
                if (death_released < 4) ++death_released;
            } else if (death_released >= 4) {
                return SEWER_EXIT_TRAFFIC_DEATH;
            }
        } else if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if (++walk_timer == 8) {
                walk_timer = 0;
                walk_frame ^= 1;
            }
        } else {
            walk_timer = 0;
            walk_frame = 0;
        }
        if (traffic_state == 0) draw_player_sprite(x, y, walk_frame);
    }
}
