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
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199
/* This scene's own CHR page (see tools/gen_mart_chr.py): the common bank
 * with a solid wall panel, a seamed variant of it used sparingly as accent
 * lines (not tiled into a grid -- that read as busy stripes rather than
 * construction), the same roofline cap as the Municipal Gym's own page (see
 * v2_scene_gym.c), and a genuinely solid glass tile with a diagonal shine,
 * patched in at indices 60-64 -- lower indices than the Gym's own (70+)
 * since the Mart has no wandering NPCs competing for
 * BAR_PATRON_BACK_TOP/BOTTOM (59-60). The glass has its own tile rather
 * than reusing the common bank's NEON_TILE_WINDOW_FILL: that tile has a
 * mullion hint baked in for use in pairs, and repeating it across a bigger
 * window just repeated that hint into an unwanted grid. */
#define MART_SIDING 60
#define MART_ROOF_CAP 61
#define MART_GLASS 62
#define MART_SIDING_SEAM 63
#define MART_SIDING_SEAM_H 64
#define MART_SIDING_SEAM_L 65
#define MART_DOOR_LINTEL 66
#define TILE_BORDER_H 52
#define TILE_BORDER_V 53
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

/* A plain bordered window, unlike draw_storefront_window()'s cross-mullion
 * grid every other building's storefront uses -- a flat rectangle of glass
 * behind a simple frame reads glassier than a many-paned grid does. */
static void draw_mart_window(unsigned char top_row, unsigned char left_col,
                             unsigned char width, unsigned char height)
{
    unsigned char row, col;

    for (col = left_col; col < (unsigned char)(left_col + width); ++col) {
        set_tile(top_row, col, TILE_BORDER_H);
        set_tile((unsigned char)(top_row + height - 1), col, TILE_BORDER_H);
    }
    for (row = (unsigned char)(top_row + 1); row < (unsigned char)(top_row + height - 1); ++row) {
        set_tile(row, left_col, TILE_BORDER_V);
        fill_title_rect(row, (unsigned char)(left_col + 1), (unsigned char)(width - 2), 1,
                        MART_GLASS);
        set_tile(row, (unsigned char)(left_col + width - 1), TILE_BORDER_V);
    }
}

/* All-Nite Mart sits one screen west of Jimbles's Bar. Every other building
 * in town used to share one brick-storefront facade, just recolored; this
 * one now shares the Municipal Gym's warehouse-style shape instead (see
 * v2_scene_gym.c's own show_gym_exterior(), which this mirrors closely) --
 * a parapet roofline cap and a shorter building under more open sky. The
 * wall itself is a genuinely different color from every other building
 * (teal, not a shade of the Gym's steel-blue), textured with just two
 * accent lines -- a vertical seam at each edge of the building and a
 * horizontal one along its base -- rather than a tiled grid, which read as
 * busy pinstripes instead of construction detail. The windows are plain
 * bordered rectangles (see draw_mart_window() above) instead of a
 * cross-mullion grid. The name now sits on a proper bordered sign plate
 * (see draw_storefront_sign_at() in v2_scene_ui.c), the same one every
 * brick storefront uses including Jimbles's Bar, instead of hand-painted
 * text loose on the trim band. */
static void show_mart_exterior(void)
{
    static const unsigned char mart_palette[32] = {
        0x0F, 0x01, 0x28, 0x0F,
        /* Teal siding -- also the window glass's palette (index 2's blue
         * reads as glass against the wall tone, the same trick the Gym's
         * own windows use). */
        0x0F, 0x1B, 0x21, 0x30,
        /* A green trim band near the roofline -- the Mart's own
         * established accent color (its old sign plate was green too),
         * carried onto the new shape instead of the Gym's gold. */
        0x0F, 0x1A, 0x00, 0x2A,
        /* The sign plate's own palette -- a black field with a white
         * border, the same two-color scheme Jimbles's Bar's own sign uses
         * (see neon_avenue.s's sign-plate tile comments), just white
         * instead of that one's pink. Kept separate from pal2 above
         * instead of reusing it: pal2's index 1 is also the plain wall
         * tiles flanking the sign, which need to stay green, not turn
         * white along with the border. */
        0x0F, 0x30, 0x0F, 0x30,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_mart_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = mart_palette[i];
    /* Open sky above a shorter building, instead of the tall facade every
     * brick storefront still fills almost the whole screen with. The cap
     * shares row 8 with the sign plate below (its top border punches a
     * gap in the middle of it) -- giving the cap its own row one tile
     * higher looked like an improvement on paper, but that row's other
     * three tile-rows (4-6) are still blank sky sharing the SAME attribute
     * block as the cap, so the whole block got tinted the cap's green and
     * the sky itself turned into a huge green slab. Row 8's block-mates
     * (9-11) are already meant to be green trim, so it's the only place
     * the cap can sit without that bleed. */
    for (i = 0; i < 8; ++i) fill_row(i, CHR_TILE_PATH);
    fill_title_rect(8, 0, 4, 1, CHR_TILE_PATH);
    fill_title_rect(8, 4, 24, 1, MART_ROOF_CAP);
    fill_title_rect(8, 28, 4, 1, CHR_TILE_PATH);
    /* Plain wall, with just two accent lines: a vertical seam at each edge
     * of the building (col 4 and col 27, one column inside each window's
     * own frame -- see draw_mart_window() below) and a horizontal seam
     * along the base (the last wall row before the sidewalk), reading as a
     * foundation line. Two lines, not a tiled grid -- the grid read as
     * busy pinstripes rather than construction detail. The left edge uses
     * the mirrored seam tile (MART_SIDING_SEAM_L) so both flanks read the
     * same way: a sliver of green between the line and the open sky
     * outside it, with the line itself hugging the window frame inside it.
     * The vertical lines only start at row 12, not the trim band above it
     * (9-11) -- their bright pixel is index 3, white under the wall's own
     * palette 1 but green under the trim's palette 2 (index 3 there is the
     * banner text's own ink color), so running them up into the trim band
     * turned them green instead of white. */
    for (i = 9; i < 24; ++i) {
        unsigned char is_base_row = i == 23;

        fill_title_rect(i, 0, 4, 1, CHR_TILE_PATH);
        if (is_base_row) {
            fill_title_rect(i, 4, 24, 1, MART_SIDING_SEAM_H);
        } else {
            fill_title_rect(i, 4, 24, 1, MART_SIDING);
            if (i >= 12) {
                set_tile(i, 4, MART_SIDING_SEAM_L);
                set_tile(i, 27, MART_SIDING_SEAM);
            }
        }
        fill_title_rect(i, 28, 4, 1, CHR_TILE_PATH);
    }
    for (i = 24; i < 26; ++i) fill_row(i, NEON_TILE_SIDEWALK);
    for (i = 26; i < 30; ++i) fill_row(i, NEON_TILE_ASPHALT);
    /* A proper bordered sign plate, the same shape and scale as Jimbles's
     * Bar's own (see draw_storefront_sign() in v2_scene_avenue.c) -- 4 rows
     * tall, 16 wide, centered over the door. It sits in the roof-cap row
     * (row 8) the same way the Bar's plate sits in its own row-block 2,
     * overwriting the middle of the cap with the plate's top border while
     * the cap's own tiles remain visible at each flank. */
    draw_storefront_sign_at(8, 8, 16);
    draw_text(9, 9, "ALL-NITE MART");
    /* Two big windows flanking the door, doubled in height from the last
     * pass (12-19, 8 tall) -- the door shrinks to 4 rows (20-23) to make
     * room, still starting right where the windows end. */
    draw_mart_window(12, 5, 8, 8);
    draw_mart_window(12, 19, 8, 8);
    /* A lintel bar over the door, level with the windows' own bottom
     * border (row 19) -- without it the door read as a hole punched
     * straight into the wall with nothing marking its top. A dedicated
     * solid-white tile, not TILE_BORDER_H (the window frame's border) --
     * that one's two-toned and read as an odd blue-and-green stripe when
     * standing alone instead of against glass. */
    fill_title_rect(19, 14, 3, 1, MART_DOOR_LINTEL);
    for (i = 20; i < 24; ++i) {
        set_tile(i, 14, NEON_TILE_DOOR_L);
        set_tile(i, 15, NEON_TILE_DOOR_C);
        set_tile(i, 16, NEON_TILE_DOOR_R);
    }
    /* Vertically centered on the 4-row door (rows 20-23), the same fix the
     * Gym's own doorknob needed. */
    set_tile(21, 16, DOORKNOB_TILE);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 0);
    /* Row 2 (tile rows 8-11): the cap's own flanks (column-blocks 1 and 6)
     * get palette 2's green/shadow, but the sign's exact span
     * (column-blocks 2-5, matching its 16-tile width starting at col 8)
     * gets its own dedicated palette 3 instead -- a black plate with a
     * white border, not sharing pal2's green. Rows 3-5 (12-23) are the
     * plain siding body, windows included -- their glass shares palette 1
     * with the wall for the same masonry-artifact reason as before. */
    set_attribute_block(2, 0, 0);
    set_attribute_block(2, 1, 2);
    for (i = 2; i < 6; ++i) set_attribute_block(2, i, 3);
    set_attribute_block(2, 6, 2);
    set_attribute_block(2, 7, 0);
    set_attribute_block(3, 0, 0);
    for (i = 1; i < 7; ++i) set_attribute_block(3, i, 1);
    set_attribute_block(3, 7, 0);
    set_attribute_block(4, 0, 0);
    for (i = 1; i < 7; ++i) set_attribute_block(4, i, 1);
    set_attribute_block(4, 7, 0);
    set_attribute_block(5, 0, 0);
    for (i = 1; i < 7; ++i) set_attribute_block(5, i, 1);
    set_attribute_block(5, 7, 0);
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
    mmc3_use_mart_chr();
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
