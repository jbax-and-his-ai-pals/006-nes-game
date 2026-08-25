#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_game_state.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_ui.h"
#include "v2_scene_zoo.h"
#include "v2_traffic.h"

extern volatile unsigned char banked_scene_arg;

#define TILE_ASPHALT 192
#define TILE_SIDEWALK 193
#define TILE_DOOR 198
#define TILE_LADDER_RAIL 210
#define TILE_WINDOW 211
#define TILE_FLOOR 235
#define TILE_WALL 236
#define TILE_COUNTER 237
#define TILE_SLUG 255
#define PLAYER_TILE_BASE 212
#define ZOO_MANHOLE_LEFT 4
#define ZOO_MANHOLE_TOP 23
#define ZOO_MANHOLE_X 48
#define ZOO_MANHOLE_Y 188
#define MANHOLE_TILE_L 6
#define MANHOLE_TILE_R 7
#define MANHOLE_TILE_M 54

static void zoo_palette(unsigned char room)
{
    static const unsigned char hatch_palette[32] = {
        0x0F, 0x01, 0x21, 0x30, 0x0F, 0x00, 0x10, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x06, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    static const unsigned char service_palette[32] = {
        0x0F, 0x01, 0x11, 0x30, 0x0F, 0x0A, 0x1A, 0x30,
        0x0F, 0x06, 0x16, 0x30, 0x0F, 0x00, 0x10, 0x30,
        0x0F, 0x16, 0x27, 0x30, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    static const unsigned char street_palette[32] = {
        0x0F, 0x00, 0x10, 0x30, 0x0F, 0x06, 0x16, 0x30,
        0x0F, 0x01, 0x21, 0x30, 0x0F, 0x0A, 0x1A, 0x30,
        0x0F, 0x16, 0x27, 0x30, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    const unsigned char *palette = room == 0 ? hatch_palette :
                                   (room == 1 || room == 3 ? street_palette : service_palette);
    unsigned char i;

    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = palette[i];
}

static void show_zoo_room(unsigned char room)
{
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    zoo_palette(room);
    for (i = 0; i < 30; ++i) fill_row(i, TILE_FLOOR);
    fill_title_rect(0, 0, 32, 3, TILE_WALL);

    if (room == 0) {
        /* The hatch is a clean ladder chamber; the infestation is in the
         * maintenance building on the zoo grounds above. */
        fill_title_rect(3, 0, 32, 10, TILE_WALL);
        fill_title_rect(0, 30, 2, 30, TILE_WALL);
        fill_title_rect(13, 0, 2, 12, TILE_FLOOR);
        for (i = 3; i < 13; ++i) {
            set_tile(i, 14, TILE_LADDER_RAIL);
            set_tile(i, 17, TILE_LADDER_RAIL);
            if ((i & 1) == 0) {
                set_tile(i, 15, TILE_SIDEWALK);
                set_tile(i, 16, TILE_SIDEWALK);
            }
        }
    } else if (room == 1) {
        fill_title_rect(3, 0, 32, 19, TILE_WALL);
        fill_title_rect(22, 0, 32, 3, TILE_SIDEWALK);
        fill_title_rect(25, 0, 32, 5, TILE_ASPHALT);
        fill_title_rect(6, 9, 14, 12, TILE_FLOOR);
        fill_title_rect(7, 10, 12, 10, TILE_WALL);
        fill_title_rect(15, 14, 4, 4, TILE_DOOR);
        set_tile(ZOO_MANHOLE_TOP, ZOO_MANHOLE_LEFT, MANHOLE_TILE_L);
        set_tile(ZOO_MANHOLE_TOP, ZOO_MANHOLE_LEFT + 1, MANHOLE_TILE_M);
        set_tile(ZOO_MANHOLE_TOP, ZOO_MANHOLE_LEFT + 2, MANHOLE_TILE_M);
        set_tile(ZOO_MANHOLE_TOP, ZOO_MANHOLE_LEFT + 3, MANHOLE_TILE_R);
        draw_text(1, 10, "POCKET ZOO");
        draw_text(4, 9, "CLOSED FOR");
        draw_text(5, 8, "MAINTENANCE");
        if (game_state.zoo_gate_open) {
            draw_text(18, 7, "GATE OPEN");
        } else {
            draw_text(18, 7, "GATE LOCKED");
        }
        draw_text(21, 21, "ROAD");
    } else if (room == 2) {
        fill_title_rect(3, 0, 32, 3, TILE_WALL);
        fill_title_rect(4, 0, 2, 21, TILE_WALL);
        fill_title_rect(4, 30, 2, 21, TILE_WALL);
        fill_title_rect(7, 8, 16, 2, TILE_COUNTER);
        fill_title_rect(8, 10, 12, 2, TILE_WINDOW);
        draw_text(1, 6, "ZOO MAINTENANCE");
        if (!game_state.zoo_slugs_cleared) {
            for (i = 0; i < 8; ++i) {
                set_tile((unsigned char)(13 + (i & 1)), (unsigned char)(10 + i * 2), TILE_SLUG);
            }
            draw_text(17, 8, "SLUG INFESTATION");
        } else {
            draw_text(17, 8, "SLUGS CLEARED");
        }
        draw_text(23, 11, "ZOO");
    } else {
        /* The first public exhibit is a deliberately dense little pond
         * tableau. It gives the unlocked gate a tangible destination while
         * leaving space for dedicated animal CHR later. */
        fill_title_rect(3, 0, 32, 22, TILE_FLOOR);
        fill_title_rect(5, 2, 28, 2, TILE_WALL);
        fill_title_rect(7, 4, 24, 11, TILE_WINDOW);
        fill_title_rect(8, 3, 26, 9, TILE_WINDOW);
        fill_title_rect(10, 12, 8, 5, TILE_FLOOR);
        fill_title_rect(17, 1, 30, 2, TILE_COUNTER);
        draw_text(1, 8, "POCKET ZOO POND");
        draw_text(4, 9, "DO NOT FEED");
        draw_text(20, 2, "GATE");
        if (room == 4) {
            fill_title_rect(3, 0, 32, 22, TILE_WALL);
            fill_title_rect(5, 3, 26, 2, TILE_COUNTER);
            fill_title_rect(8, 4, 24, 11, TILE_WINDOW);
            fill_title_rect(9, 5, 22, 9, TILE_WINDOW);
            fill_title_rect(17, 1, 30, 2, TILE_FLOOR);
            draw_text(1, 7, "REPTILE HOUSE");
            draw_text(4, 8, "KEEP HANDS OUT");
            draw_text(20, 2, "POND");
        }
    }
    for (i = 0; i < 8; ++i) {
        if (room == 0) {
            set_attribute_block(0, i, 1);
            set_attribute_block(1, i, 0);
            set_attribute_block(2, i, 0);
            set_attribute_block(3, i, 0);
            set_attribute_block(4, i, 0);
            set_attribute_block(5, i, 0);
            set_attribute_block(6, i, 2);
            set_attribute_block(7, i, 2);
            continue;
        }
        set_attribute_block(0, i, (room == 1 || room == 3) ? 2 : 1);
        set_attribute_block(1, i, (room == 1 || room == 3) ? 2 : 1);
        set_attribute_block(2, i, (room == 1 || room == 3) ? 0 : 1);
        set_attribute_block(3, i, (room == 1 || room == 3) ? 0 : 1);
        set_attribute_block(4, i, (room == 1 || room == 3) ? 0 : 1);
        set_attribute_block(5, i, (room == 1 || room == 3) ? 0 : 1);
        set_attribute_block(6, i, (room == 1 || room == 3) ? 3 : 0);
        set_attribute_block(7, i, (room == 1 || room == 3) ? 3 : 0);
    }
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

static void draw_keeper_nibs(void)
{
    /* A familiar, compact patron silhouette makes Nibs readable now; a
     * dedicated keeper sprite can replace these four shared tiles later. */
    set_oam_sprite(40, 188, 104, PLAYER_TILE_BASE, 2);
    set_oam_sprite(41, 196, 104, PLAYER_TILE_BASE + 1, 2);
    set_oam_sprite(42, 188, 112, PLAYER_TILE_BASE + 2, 2);
    set_oam_sprite(43, 196, 112, PLAYER_TILE_BASE + 3, 2);
}

static void draw_pond_animals(void)
{
    /* Reused tiles are placeholder ducks: bright, separated silhouettes on
     * the blue pond until the Zoo receives a dedicated CHR art page. */
    set_oam_sprite(44, 88, 112, PLAYER_TILE_BASE, 3);
    set_oam_sprite(45, 96, 112, PLAYER_TILE_BASE + 1, 3);
    set_oam_sprite(46, 152, 136, PLAYER_TILE_BASE + 2, 3);
    set_oam_sprite(47, 160, 136, PLAYER_TILE_BASE + 3, 3);
}

static void draw_house_reptile(void)
{
    /* The green sprite palette makes this shared-tile silhouette read as a
     * coiled resident until a dedicated snake sheet is drawn. */
    set_oam_sprite(44, 120, 108, PLAYER_TILE_BASE + 2, 2);
    set_oam_sprite(45, 128, 108, PLAYER_TILE_BASE + 3, 2);
    set_oam_sprite(46, 136, 116, PLAYER_TILE_BASE, 2);
    set_oam_sprite(47, 144, 116, PLAYER_TILE_BASE + 1, 2);
}

static unsigned char pond_blocks(unsigned char x, unsigned char y)
{
    if (x < 32 || x > 224 || y < 56 || y > 144) return 0;
    /* The raised island in the middle remains walkable; all other blue pond
     * tiles are actual water rather than decorative floor. */
    return !(x >= 88 && x <= 160 && y >= 80 && y <= 128);
}

static unsigned char zoo_blocks(unsigned char room, unsigned char x, unsigned char y)
{
    if (room == 0) {
        if (x >= 236) return 1;
        return y <= 112 && (x < 120 || x > 136);
    }
    return room == 3 && pond_blocks(x, y);
}

unsigned char run_zoo(void)
{
    unsigned char room = 0;
    unsigned char x = 24;
    unsigned char y = game_state.transfer_y_valid ? game_state.transfer_y : 200;
    unsigned char walk_frame = 0, walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char prompt_held = 0;
    unsigned char traffic_state = 0, taxi_moving_left = 0, death_released = 0;
    unsigned char pad;
    int taxi_x = 0;

    game_state.transfer_y_valid = 0;
    music_select_song(MUSIC_SONG_ZOO);
    show_zoo_room(room);
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();
        if (pad & PAD_UP) facing = PLAYER_FACING_UP;
        else if (pad & PAD_DOWN) facing = PLAYER_FACING_DOWN;
        else if (pad & PAD_LEFT) facing = PLAYER_FACING_LEFT;
        else if (pad & PAD_RIGHT) facing = PLAYER_FACING_RIGHT;

        if (traffic_state == 1 || traffic_state == 2) {
            taxi_x += taxi_moving_left ? -3 : 3;
            if (taxi_x > -32 && taxi_x < 256) draw_taxi_sprite(taxi_x, taxi_moving_left);
            if (traffic_state == 1 && taxi_x <= (int)x + 8 && taxi_x + 32 >= (int)x - 8) {
                traffic_state = 2;
                hide_player_sprite();
                draw_traffic_particles(x, taxi_moving_left);
                draw_crash_tableau(x, taxi_moving_left);
                music_sfx_hurt();
                /* Palette 3 is unused in the hatch, but belongs to the
                 * exterior road. Keep the shared plate off scenery. */
                dialogue_set_palette(room == 0 ? 3 : 1);
                show_traffic_death();
            }
            if (traffic_state == 2 && (pad & PAD_A)) {
                return ZOO_EXIT_TRAFFIC_DEATH;
            }
            continue;
        }
        if (pad & PAD_START) {
            run_inventory();
            show_zoo_room(room);
            continue;
        }
        if (pad & PAD_LEFT) {
            if (x > 12) {
                if (!zoo_blocks(room, (unsigned char)(x - 1), y)) --x;
            } else if (room == 0 && y > 112) {
                /* The lower-left opening returns to the key tunnel. The
                 * ladder rails above it are ordinary solid boundaries. */
                game_state.transfer_y = y;
                game_state.transfer_y_valid = 1;
                banked_scene_arg = 1;
                return ZOO_EXIT_SEWER;
            } else if (room == 3) {
                room = 1; x = 232; show_zoo_room(room);
            } else if (room == 4) {
                room = 3; x = 232; show_zoo_room(room);
            }
        } else if (pad & PAD_RIGHT) {
            if (x < 244 && !zoo_blocks(room, (unsigned char)(x + 1), y)) ++x;
            else if (room == 3 && y >= 160) {
                room = 4; x = 24; show_zoo_room(room);
            }
        }
        if (pad & PAD_UP) {
            if (room == 1 && x >= 104 && x <= 152 && y >= 128 && y <= 160) {
                room = 2; x = 128; y = 200; show_zoo_room(room);
            } else if (y > 40 && !zoo_blocks(room, x, (unsigned char)(y - 1))) --y;
            /* A blocked ladder rail is not an exit. Only its top opening
             * transitions to the zoo grounds. */
            else if (room == 0 && y <= 40 && x >= 104 && x <= 152) {
                /* Emerge immediately above the exterior's service cover. */
                room = 1; x = ZOO_MANHOLE_X; y = 168; show_zoo_room(room);
            } else if (room == 1 && game_state.zoo_gate_open && x >= 104 && x <= 152) {
                room = 3; x = 128; y = 200; show_zoo_room(room);
            }
        } else if (pad & PAD_DOWN) {
            if (room == 2 && x >= 104 && x <= 152 && y >= 208) {
                room = 1; x = 128; y = 168; show_zoo_room(room);
            } else if (y < 216 && !zoo_blocks(room, x, (unsigned char)(y + 1))) ++y;
        }
        if ((room == 0 || room == 1) && y >= 214) {
            traffic_state = 1;
            taxi_moving_left = x < 128;
            taxi_x = taxi_moving_left ? 224 : 0;
            clear_sprites();
        }
        /* The exterior cover is the clear return path to the service hatch.
         * Require the player's feet to stand on its compact visible center. */
        if (room == 1 && x >= ZOO_MANHOLE_X - 12 && x <= ZOO_MANHOLE_X + 12 &&
            y >= ZOO_MANHOLE_Y && y <= ZOO_MANHOLE_Y + 8) {
            room = 0;
            x = 128;
            /* Start high enough on the hatch ladder that this is a climb,
             * not an immediate return to the exterior. */
            y = 56;
            show_zoo_room(room);
            continue;
        }
        if (!(pad & PAD_A)) prompt_held = 0;
        if ((pad & PAD_A) && !prompt_held) {
            prompt_held = 1;
            if (room == 2 && x >= 72 && x <= 184 && y >= 88 && y <= 160) {
                if (!game_state.zoo_slugs_cleared && game_state.has_flamethrower) {
                    game_state.zoo_slugs_cleared = 1;
                    show_message_box("FWOOSH.", "THE SLUGS EVAPORATE.", "THE ROOM IS CLEAR.");
                } else if (!game_state.zoo_slugs_cleared) {
                    show_message_box("A WALL OF SLUGS.", "IT NEEDS SOMETHING", "VERY HOT.");
                } else {
                    show_message_box("THE SLUGS ARE GONE.", "THE ROOM IS", "CLEAR.");
                }
                show_zoo_room(room);
            } else if (room == 1 && x >= 168 && x <= 224 && y >= 88 && y <= 152) {
                if (!game_state.zoo_slugs_cleared) {
                    show_message_box("KEEPER NIBS: NOPE.", "CLEAR THE MAINTENANCE", "BUILDING FIRST.");
                } else if (!game_state.zoo_gate_open) {
                    game_state.zoo_gate_open = 1;
                    show_message_box("NIBS: YOU DID IT.", "I'M OPENING THE GATE.", "TRY NOT TO FEED ANYTHING.");
                } else if (!game_state.has_zoo_map) {
                    game_state.has_zoo_map = 1;
                    show_message_box("NIBS HANDS YOU", "A ZOO MAP.", "THE DUCK IS CIRCLED.");
                } else {
                    show_message_box("NIBS: WE'RE OPEN.", "THE ANIMALS ARE", "STILL JUDGING YOU.");
                }
                show_zoo_room(room);
            } else if (room == 1 && x >= 104 && x <= 152 && y <= 104) {
                if (!game_state.zoo_gate_open) show_message_box("THE GATE IS LOCKED.", "NIBS IS WATCHING", "THE SERVICE SIDE.");
                else show_message_box("THE GATE IS OPEN.", "WALK UP TO", "ENTER THE ZOO.");
                show_zoo_room(room);
            } else if (room == 3 && x >= 72 && x <= 184 && y >= 88 && y <= 160) {
                show_message_box("THE DUCKS STARE.", "THEY KNOW ABOUT", "THE SLUGS.");
                show_zoo_room(room);
            } else if (room == 4 && x >= 96 && x <= 176 && y >= 80 && y <= 152) {
                show_message_box("THE SNAKE BLINKS.", "THEN PRETENDS", "IT DIDN'T.");
                show_zoo_room(room);
            }
        }
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            if (++walk_timer == 8) { walk_timer = 0; walk_frame ^= 1; }
        } else { walk_timer = 0; walk_frame = 0; }
        if (room == 1) draw_keeper_nibs();
        if (room == 3) draw_pond_animals();
        if (room == 4) draw_house_reptile();
        draw_player_sprite(x, y, walk_frame, facing);
    }
}
