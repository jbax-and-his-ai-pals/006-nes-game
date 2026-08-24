#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "v2_dialogue.h"
#include "v2_game_state.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_regulars.h"
#include "v2_runtime.h"
#include "v2_scene_bar.h"
#include "v2_scene_ui.h"

#define JIMBLES_TILE_TOP 61
#define JIMBLES_TILE_BOTTOM 62
#define JIMBLES_TILE_BOTTOM_RIGHT 1
#define PISS_TILE_STREAK_A 55
#define PISS_TILE_STREAK_B 56
#define PISS_TILE_PUDDLE 57
#define BAR_TILE_FLOOR 235
#define BAR_TILE_WALL 236
#define BAR_TILE_COUNTER 237
#define BAR_TILE_SHELF 238
#define BAR_TILE_PLANK_R 239
#define BAR_TILE_JUKEBOX_TOP 240
#define BAR_TILE_JUKEBOX_BOT 241
/* Was its own tile (242); byte-for-byte identical to BAR_TILE_PLANK_R, so
 * that slot now holds the casino lobby's blackjack table art instead (see
 * bar_interior.s) and this just reuses PLANK_R -- same pixels either way. */
#define BAR_TILE_PLANK_L BAR_TILE_PLANK_R
#define BAR_TILE_STOOL 243
#define BAR_TILE_DOOR_JAMB 244
#define BAR_TILE_DOOR_FILL 245
#define TOKEN_TILE 255

#define token_taken game_state.token_taken
#define has_token game_state.has_token
#define door_unlocked game_state.door_unlocked
#define has_beer game_state.has_beer
#define jimbles_limit_given game_state.jimbles_limit_given
#define jimbles_x game_state.jimbles_x
/* Jimbles: a sentient pint standing behind his own bar counter. Only two
 * tiles are authored (see JIMBLES_TILE_TOP/BOTTOM's comment); OAM
 * mirrors each horizontally (attr bit 0x40) for the right half, and his
 * sprite's bottom edge is positioned to land exactly where the counter's
 * background tile begins, so there's no third ("hidden") row to draw --
 * the counter simply starts right where he ends.
 *
 * Only the front-facing pose exists -- CHR budget is fully spent (see
 * TOKEN_TILE's comment), so a real side-profile walk or a back-facing
 * "mixing a drink" pose would need new art there's no room for right
 * now. run_bar_interior()'s idle animation works around that by sliding
 * this same front sprite left and right instead, rather than a true
 * walk cycle. Reads jimbles_x (file-scope, see its comment) instead of
 * a fixed position so that animation can drive it. */
static void draw_jimbles(void)
{
    set_oam_sprite(44, jimbles_x, 48, JIMBLES_TILE_TOP, 2);
    set_oam_sprite(45, (unsigned char)(jimbles_x + 8), 48, JIMBLES_TILE_TOP, 2 | 0x40);
    set_oam_sprite(46, jimbles_x, 56, JIMBLES_TILE_BOTTOM, 2);
    set_oam_sprite(47, (unsigned char)(jimbles_x + 8), 56, JIMBLES_TILE_BOTTOM_RIGHT, 2);
}

/* Two short, slumped regulars: they only need head-and-shoulder tiles while
 * standing in the crowded corner, so the lower half stays visually buried in
 * the dark floor/puddle rather than consuming another four OAM slots each. */
/* Sitting on the floor by the jukebox -- see run_bar_interior() for the
 * walk-over pickup and the jukebox-proximity spend. Static once drawn
 * (like Jimbles), not redrawn every frame, since nothing about it
 * animates; the pickup just hides its one OAM slot directly.
 *
 * Deliberately well clear of the jukebox's own interact zone (x 8-48,
 * y 88-128, in run_bar_interior()) -- picking the token up used to also
 * satisfy the jukebox's has_token/proximity/A check in the very same
 * frame, since the two zones overlapped, so the fanfare played the
 * instant you grabbed it instead of waiting for an actual visit to the
 * jukebox. Also clear of the bar counter's collision block (x 56-199,
 * y 40-87), and clear of the message box's own footprint (rows 19-26,
 * y 152-215) so picking it up doesn't leave it sitting under its own
 * popup. */
#define TOKEN_X 200
#define TOKEN_Y 110
#define TOKEN_OAM_SLOT 42
static void draw_token(void)
{
    set_oam_sprite(TOKEN_OAM_SLOT, TOKEN_X, TOKEN_Y, TOKEN_TILE, 2);
}

static void show_bar_interior(void)
{
    static const unsigned char bar_palette[32] = {
        /* BG pal0: the room's warm wood floor/walls (unchanged). BG pal1:
         * a dark, contrasting panel for the message box -- same scheme
         * as the casino's slot UI overlay. Index 2 is BAR_TILE_WALL's
         * solid fill and font.s's glyph-blank color (see its header
         * comment), so text sits on a seamless field; index 1 is
         * box_border.s's rule-line color, drawn over that same field. */
        0x07, 0x17, 0x08, 0x28, 0x0F, 0x30, 0x0C, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        /* BG pal3: Piss Corner streaks/puddles. Index 2 stays black for
         * the dark recess, while index 3 is the pale wall fixture. */
        0x0F, 0x28, 0x0F, 0x30,
        0x07, 0x0F, 0x0F, 0x0F, 0x0F, 0x16, 0x27, 0x30,
        0x0F, 0x28, 0x0F, 0x30, 0x0F, 0x0F, 0x0F, 0x0F
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = bar_palette[i];

    /* South wall is 3 tiles thick (bar_interior.s's own header comment
     * describes its door as "a 48x24 southern exit," i.e. 3 tiles tall).
     * North wall is thicker still, at 6 -- the backroom door only needs
     * 3 of those rows, so it sits flush against the floor with visible
     * wall mass above it instead of the door filling the wall's entire
     * height with no margin. Floor is rows 6-26 to match. */
    for (i = 6; i < 27; ++i) fill_row(i, BAR_TILE_FLOOR);
    fill_title_rect(0, 0, 32, 6, BAR_TILE_WALL);
    fill_title_rect(27, 0, 32, 3, BAR_TILE_WALL);
    fill_title_rect(0, 0, 1, 30, BAR_TILE_WALL);
    fill_title_rect(0, 31, 1, 30, BAR_TILE_WALL);
    for (i = 27; i < 30; ++i) {
        set_tile(i, 14, BAR_TILE_DOOR_JAMB);
        set_tile(i, 15, BAR_TILE_DOOR_FILL);
        set_tile(i, 16, BAR_TILE_DOOR_JAMB);
    }

    /* The backroom door -- reuses the same jamb/fill tiles as the south
     * exit, just on the north wall instead, near the jukebox corner
     * where the back-room planks already hint at more room beyond. Sits
     * in the bottom 3 rows of the 6-thick north wall, flush against the
     * floor. Leads to the casino, but only once door_unlocked is set by
     * playing the jukebox (see run_bar_interior()). */
    for (i = 3; i < 6; ++i) {
        set_tile(i, 25, BAR_TILE_DOOR_JAMB);
        set_tile(i, 26, BAR_TILE_DOOR_FILL);
        set_tile(i, 27, BAR_TILE_DOOR_JAMB);
    }

    /* Shelf and counter, with Jimbles (a sprite, drawn below) standing
     * between them -- see draw_jimbles()'s comment for how the two line
     * up without any actual sprite-behind-background trick. The bottles
     * sit two wall rows above his head, rather than
     * reading as if they are balanced against his glasses. */
    fill_title_rect(4, 8, 16, 1, BAR_TILE_SHELF);
    fill_title_rect(8, 8, 16, 1, BAR_TILE_COUNTER);
    /* Small empty stools establish the patron's place at the counter
     * without adding collision to the central walking aisle. */
    set_tile(10, 14, BAR_TILE_STOOL);
    set_tile(10, 17, BAR_TILE_STOOL);
    set_tile(13, 3, BAR_TILE_JUKEBOX_TOP);
    set_tile(14, 3, BAR_TILE_JUKEBOX_BOT);
    set_tile(11, 27, BAR_TILE_PLANK_R);
    set_tile(12, 27, BAR_TILE_PLANK_R);
    set_tile(11, 28, BAR_TILE_PLANK_L);
    set_tile(12, 28, BAR_TILE_PLANK_L);

    /* Jimbles's punishment nook stays on the same open floor as the bar.
     * Palette 3 supplies the sickly yellow floor streaks; avoiding a dark
     * wall panel keeps the corner readable instead of looking corrupted. */
    set_tile(21, 27, PISS_TILE_PUDDLE);
    set_tile(22, 30, PISS_TILE_PUDDLE);
    set_tile(23, 25, PISS_TILE_PUDDLE);
    set_tile(24, 26, PISS_TILE_PUDDLE);
    set_tile(25, 28, PISS_TILE_PUDDLE);
    /* Unevenly scattered short streaks break up the round puddle shapes. */
    set_tile(19, 26, PISS_TILE_STREAK_A);
    set_tile(20, 29, PISS_TILE_STREAK_B);
    set_tile(21, 25, PISS_TILE_STREAK_B);
    set_tile(22, 28, PISS_TILE_STREAK_A);
    set_tile(23, 30, PISS_TILE_STREAK_B);
    set_tile(24, 25, PISS_TILE_STREAK_A);
    set_tile(25, 29, PISS_TILE_STREAK_B);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(2, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(3, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(4, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(5, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(6, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(7, i, 0);
    /* Start the yellowed floor one attribute cell above the puddles, so the
     * small scattered streaks share their color at the corner's upper edge. */
    set_attribute_block(4, 6, 3);
    set_attribute_block(4, 7, 3);
    /* Palette 3 turns the floor's thin horizontal detail into sickly yellow
     * streaks and its scattered puddle tiles yellow too, without a label
     * spelling out what this undesirable corner is for. */
    set_attribute_block(5, 6, 3);
    set_attribute_block(5, 7, 3);
    set_attribute_block(6, 6, 3);
    set_attribute_block(6, 7, 3);

    clear_sprites();
    draw_jimbles();
    draw_bar_patron();
    draw_regulars();
    if (!token_taken) draw_token();
    /* Flush the freshly-cleared-plus-Jimbles(-plus-token) buffer to the
     * real PPU OAM before rendering resumes -- see show_neon_avenue()'s
     * comment on the same call for why clear_sprites() alone isn't
     * enough. */
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The bar counter/shelf/patron block (rows 5-9, cols 8-23 -- see
 * show_bar_interior()) is solid: the player has to walk around it, not
 * through it. An 8px margin on every side approximates the player
 * sprite's own footprint, same tolerance TOKEN_X/Y's pickup check uses. */
#define BAR_COUNTER_LEFT   56
#define BAR_COUNTER_RIGHT  199
#define BAR_COUNTER_TOP    40
#define BAR_COUNTER_BOTTOM 87

static unsigned char blocked_by_counter(unsigned char x, unsigned char y)
{
    return x >= BAR_COUNTER_LEFT && x <= BAR_COUNTER_RIGHT &&
           y >= BAR_COUNTER_TOP && y <= BAR_COUNTER_BOTTOM;
}

/* Same shape as run_neon_avenue(): flush first thing after waitvsync(),
 * before any of this frame's work, so the DMA timing is fixed-cost
 * regardless of how much the frame actually needs to do -- see that
 * function's comment for why that ordering matters. No taxi here, so
 * there's much less per-frame work either way, but the pattern stays
 * consistent rather than relying on "this scene is simple enough that
 * the old order would've been fine anyway." */
unsigned char run_bar_interior(unsigned char from_casino)
{
    /* Coming back from the casino drops the player right at the
     * backroom door (just south of its threshold) instead of all the
     * way back at the avenue entrance, matching how walking out of the
     * door was what got them there in the first place. */
    unsigned char x = from_casino ? 212 : 120;
    unsigned char y = from_casino ? 56 : 208;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char pad;
    unsigned char door_prompted = 0;
    unsigned char jukebox_prompted = 0;
    unsigned char jimbles_prompted = 0;
    unsigned char patron_prompted = 0;
    unsigned char piss_regular_one_prompted = 0;

    unsigned char piss_regular_two_prompted = 0;
    unsigned char piss_regular_two_dialogue = 0;
    /* Jimbles's idle animation: rest at the current position for a
     * while, then slide (1px every 4 frames) to the opposite side of
     * his little range and rest again. See jimbles_x's and
     * draw_jimbles()'s comments for why this is a slide, not a walk. */
    unsigned char jimbles_anim_timer = 90;
    unsigned char jimbles_sliding = 0;
    unsigned char jimbles_target = 128;

    music_select_song(MUSIC_SONG_BAR);
    reset_piss_corner_regulars();
    show_bar_interior();
    for (;;) {
        waitvsync();
        reset_scroll();
        oam_flush();

        if (jimbles_anim_timer) {
            --jimbles_anim_timer;
        } else if (jimbles_sliding) {
            if (jimbles_x < jimbles_target) {
                ++jimbles_x;
                jimbles_anim_timer = 3;
            } else if (jimbles_x > jimbles_target) {
                --jimbles_x;
                jimbles_anim_timer = 3;
            } else {
                jimbles_sliding = 0;
                jimbles_anim_timer = 90;
            }
        } else {
            jimbles_sliding = 1;
            jimbles_target = (jimbles_x <= 116) ? 128 : 104;
        }
        draw_jimbles();
        update_piss_corner_regulars();
        draw_bar_patron();
        draw_regulars();

        pad = read_pad();
        if (pad & PAD_START) {
            run_inventory();
            show_bar_interior();
            continue;
        }
        /* Dialogue returns while its dismissal press is still held. Re-arm
         * nearby A-hotspots only after that press has been released, so the
         * next deliberate A can talk to the same character again in place. */
        if (!(pad & PAD_A)) {
            jukebox_prompted = 0;
            jimbles_prompted = 0;
            patron_prompted = 0;
            piss_regular_one_prompted = 0;
            piss_regular_two_prompted = 0;
        }
        if (pad & PAD_LEFT) {
            if (x > 20 && !blocked_by_counter((unsigned char)(x - 1), y)) --x;
        } else if (pad & PAD_RIGHT) {
            if (x < 236 && !blocked_by_counter((unsigned char)(x + 1), y)) ++x;
        }
        if (pad & PAD_UP) {
            /* Same doorway-gap idea as the south exit below, mirrored on
             * the north wall: walking further in once already at the
             * wall, inside the door's column span, heads to the casino
             * instead of just stopping -- but only once the jukebox has
             * unlocked it; otherwise a locked-door message explains why
             * not instead of silently doing nothing. */
            if (y > 48) {
                if (!blocked_by_counter(x, (unsigned char)(y - 1))) --y;
                door_prompted = 0;
            } else if (x >= 200 && x <= 224) {
                if (door_unlocked) {
                    return BAR_EXIT_CASINO;
                }
                /* Position-gated, not button-edge-gated: holding UP
                 * pinned against the wall must show this exactly once
                 * per approach, not retrigger on every frame (or on
                 * every stray release/press blip a held key can produce
                 * on real hardware or in an emulator). It only resets
                 * once the player actually steps back from the wall. */
                if (!door_prompted) {
                    door_prompted = 1;
                    show_message_box("THE DOOR IS LOCKED.", 0, 0);
                    show_bar_interior();
                }
            } else {
                door_prompted = 0;
            }
        } else if (pad & PAD_DOWN) {
            if (y < 216) {
                if (!blocked_by_counter(x, (unsigned char)(y + 1))) ++y;
            } else if (x >= 104 && x <= 144) {
                return BAR_EXIT_AVENUE;
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

        /* Zelda-style walk-over pickup, no button -- same feel as the
         * old adventure's items. */
        if (!token_taken && x >= TOKEN_X - 12 && x <= TOKEN_X + 12 &&
            y >= TOKEN_Y - 12 && y <= TOKEN_Y + 12) {
            token_taken = 1;
            has_token = 1;
            oam_buf[TOKEN_OAM_SLOT * 4] = 0xFF;
            show_message_box("YOU FOUND A JUKEBOX", "TOKEN.", 0);
            show_bar_interior();
        }
        /* Spend it standing near the jukebox. Position-gated like the
         * door lock above: holding A in the zone shows a message exactly
         * once per approach, not once per frame A reads as held (or per
         * stray release/press blip from a held key) -- resets only once
         * the player actually leaves the zone. */
        if (x >= 8 && x <= 48 && y >= 88 && y <= 128) {
            if ((pad & PAD_A) && !jukebox_prompted) {
                jukebox_prompted = 1;
                if (has_token) {
                    has_token = 0;
                    door_unlocked = 1;
                    music_play_jukebox_fanfare();
                    /* A hint the back door just unlocked, not a direct
                     * statement of it -- the click is the mechanism, the
                     * player connects the dots on the next locked-door
                     * message (or just by trying the door). */
                    show_message_box("THE JUKEBOX ROARS", "TO LIFE... CLICK.", 0);
                } else {
                    show_message_box("THE JUKEBOX NEEDS", "A TOKEN.", 0);
                }
                show_bar_interior();
            }
        } else {
            jukebox_prompted = 0;
        }

        /* Jimbles, behind the counter -- approached from directly south
         * of it (the counter's own collision keeps the player from
         * getting any closer), same position-gated once-per-approach
         * pattern as the door and jukebox above. Zone covers his whole
         * idle slide range, 104-128 (see jimbles_x's comment), not just
         * his rest position -- and stops at 104 rather than reaching
         * further left so it can't overlap the patron's zone just below. */
        if (x >= 104 && x <= 156 && y >= 88 && y <= 112) {
            if ((pad & PAD_A) && !jimbles_prompted) {
                jimbles_prompted = 1;
                if (!has_beer) {
                    has_beer = 1;
                    show_message_box("JIMBLES: FIRST ONE'S", "ON THE HOUSE, PAL.",
                                     "HE HANDS YOU A BEER.");
                } else if (!jimbles_limit_given) {
                    jimbles_limit_given = 1;
                    show_message_box("JIMBLES: ONE'S YOUR", "LIMIT, PAL.", 0);
                } else {
                    show_message_box("JIMBLES: EVERYONE", "HERE'S A FUCKING", "SLUG.");
                }
                show_bar_interior();
            }
        } else {
            jimbles_prompted = 0;
        }

        /* The bar patron, a couple seats down from Jimbles. Stops at 103
         * so it can't overlap Jimbles's own zone just above. */
        if (x >= 80 && x <= 103 && y >= 88 && y <= 112) {
            if ((pad & PAD_A) && !patron_prompted) {
                patron_prompted = 1;
                show_message_box("THE PATRON JUST", "GRUNTS AT YOU.", 0);
                show_bar_interior();
            }
        } else {
            patron_prompted = 0;
        }

        /* Two unfortunate regulars in Jimbles's lower-right punishment nook.
         * Their zones are split at x=216, matching their visible positions,
         * so either lament can be heard directly. */
        if (x >= 184 && x <= 216 && y >= 160 && y <= 208) {
            if ((pad & PAD_A) && !piss_regular_one_prompted) {
                piss_regular_one_prompted = 1;
                show_message_box("JIMBLES BANISHED ME", "FOR SPILLING A BEER.", 0);
                show_bar_interior();
            }
        } else {
            piss_regular_one_prompted = 0;
        }
        if (x >= 217 && x <= 244 && y >= 176 && y <= 216) {
            if ((pad & PAD_A) && !piss_regular_two_prompted) {
                piss_regular_two_prompted = 1;
                if (piss_regular_two_dialogue == 0) {
                    show_message_box("STINKS IN HERE.", 0, 0);
                } else if (piss_regular_two_dialogue == 1) {
                    show_message_box("SMELLS LIKE MEAT.", 0, 0);
                } else {
                    show_message_box("ROTTEN MEAT.", 0, 0);
                }
                piss_regular_two_dialogue = (unsigned char)((piss_regular_two_dialogue + 1) % 3);
                show_bar_interior();
            }
        } else {
            piss_regular_two_prompted = 0;
        }

        draw_player_sprite(x, y, walk_frame);
    }
}
