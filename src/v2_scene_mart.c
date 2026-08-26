#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_game_state.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_mart.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"

#define CHR_TILE_PATH 3
#define DOORKNOB_TILE 51
#define MART_TILE_REGISTER 58
#define NEON_TILE_ASPHALT 192
#define NEON_TILE_SIDEWALK 193
#define NEON_TILE_BRICK 194
#define NEON_TILE_AWNING 195
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199
#define NEON_TILE_WINDOW_FILL 211
#define PLAYER_TILE_BASE 212
#define BAR_TILE_FLOOR 235
#define BAR_TILE_WALL 236
#define BAR_TILE_COUNTER 237
#define BAR_TILE_SHELF 238
#define BAR_TILE_DOOR_JAMB 244
#define BAR_TILE_DOOR_FILL 245

#define has_paper_clip game_state.has_paper_clip

static void show_mart_message(const char *line1, const char *line2,
                              const char *line3);

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
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
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
    draw_storefront_sign();
    draw_text(9, 9, "ALL-NITE MART");
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

unsigned char run_mart_exterior(unsigned char spawn)
{
    /* Street travel from the avenue arrives at the east edge; leaving the
     * store returns directly beneath its centered front door; returning
     * from the side street arrives at the west edge, mirroring the east
     * edge's avenue arrival. */
    unsigned char x = spawn == 1 ? 128 : (spawn == 3 ? 20 : 232);
    unsigned char y = spawn == 1 ? 196 : 200;
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
    show_mart_exterior();
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
                show_mart_exterior();
                continue;
            }
            if (pad & PAD_LEFT) {
                if (x > 12) --x;
                else return MART_EXIT_SIDESTREET;
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
                    show_traffic_death();
                }
            }
        } else {
            if (!(pad & PAD_A)) {
                if (death_released < 4) ++death_released;
            } else if (death_released >= 4) {
                return MART_EXIT_TRAFFIC_DEATH;
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
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x06, 0x30, 0x0F, 0x0F, 0x0F, 0x30
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

void run_mart_interior(void)
{
    unsigned char x = 128;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char pad;
    unsigned char cashier_prompted = 0;
    unsigned char register_prompted = 0;
    unsigned char shelf_prompted = 0;
    unsigned char shelf_dialogue = 0;

    music_select_song(MUSIC_SONG_BAR);

    show_mart_interior();
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
                if (!game_state.has_flamethrower) {
                    if (game_state.casino.credits >= 50) {
                        game_state.casino.credits -= 50;
                        game_state.has_flamethrower = 1;
                        show_mart_message("BEHIND THE MOPS:", "A FLAMETHROWER. $50.", "IT'S YOURS.");
                    } else {
                        show_mart_message("BEHIND THE MOPS:", "A FLAMETHROWER. $50.", "COME BACK RICHER.");
                    }
                } else if (shelf_dialogue == 0) {
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
        draw_player_sprite(x, y, walk_frame, facing);
    }
}

/* Mart dialogue should not borrow the light cinder-block gray used by the
 * storefront/interior. The message box uses BG palette 1, so temporarily
 * make that palette a dark navy field with green trim; show_mart_interior()
 * restores the room palette immediately after dismissal. */
static void show_mart_message(const char *line1, const char *line2, const char *line3)
{
    /* Palette selection persists across scene transitions. Keep dialogue on
     * the mart's reserved palette 1, never the storefront sign's palette 2. */
    dialogue_set_palette(1);
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
