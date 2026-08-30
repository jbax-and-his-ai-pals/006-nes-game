#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "rng.h"
#include "v2_dialogue.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_gym.h"
#include "v2_scene_ui.h"

#define CHR_TILE_PATH 3
#define DOORKNOB_TILE 51
#define NEON_TILE_ASPHALT 192
#define NEON_TILE_SIDEWALK 193
#define NEON_TILE_DOOR_L 197
#define NEON_TILE_DOOR_C 198
#define NEON_TILE_DOOR_R 199
#define PLAYER_TILE_BASE 212
#define BAR_TILE_WALL 236
#define BAR_TILE_STOOL 243
#define BAR_TILE_DOOR_JAMB 244
#define BAR_TILE_DOOR_FILL 245
/* This scene's own CHR page (see tools/gen_gym_chr.py): the common bank
 * with a real 2x2 backboard/hoop, a solid court floor, a half-court line
 * and perimeter border, and courtside equipment patched in at indices
 * 70-81 -- not 60-69, since 59-60 are BAR_PATRON_BACK_TOP/BOTTOM (see the
 * wandering-NPC code below), which this scene's own gym rats need unlike
 * the Arcade's marquee. */
#define GYM_BACKBOARD_POST 70
#define GYM_HOOP_TL 71
#define GYM_HOOP_TR 72
#define GYM_HOOP_BL 73
#define GYM_HOOP_BR 74
#define GYM_COURT_FLOOR 75
#define GYM_MAT 76
#define GYM_WEIGHTS_L 77
#define GYM_WEIGHTS_R 78
#define GYM_CENTER_LINE 79
#define GYM_BORDER_H 80
#define GYM_BENCH 81
#define GYM_SIDING 82
#define GYM_ROOF_CAP 83
#define GYM_BALL 84
/* The coach and the two wandering gym rats need no new art at all -- they
 * reuse the game's existing 4-tile player-body sprite, the same way Park's
 * own wandering regulars do (each scene keeps its own copy of this wander
 * logic now -- see the comment above run_gym_interior()'s helpers below).
 * The coach wanders on the same system as the rats now, just its own tight,
 * centered bounds (see run_gym_interior()) so it stays easy to find near
 * half-court instead of roaming the whole floor. It still reuses the
 * player's own sprite palette (1) rather than a unique one, freeing both of
 * this scene's spare sprite-palette slots (0 and 2) for the two gym rats to
 * actually look different from each other. */
#define GYM_COACH_OAM 42
#define GYM_COACH_PALETTE 1
#define BAR_PATRON_BACK_TOP 59
#define BAR_PATRON_BACK_BOTTOM 60

static void show_gym_message(const char *line1, const char *line2, const char *line3);

/* Municipal Gym sits one screen east of Galaxy Lanes & Arcade, reached by
 * the same alley the arcade's own north path uses (see
 * run_arcade_exterior()'s east edge). Every other building in town (Mart,
 * Bar, Arcade, Side Street) shares the same brick-storefront facade,
 * height, and proportions, just recolored -- this one deliberately doesn't:
 * a converted warehouse reads as a genuinely different kind of building,
 * not another reskinned shopfront. Corrugated metal siding replaces brick,
 * a parapet cap replaces the awning, and the building itself is shorter
 * (rows 8-23 instead of 3-23), leaving more open sky above it -- none of
 * which touches run_gym_exterior()'s own collision, since its door
 * threshold is a plain y-coordinate check with no dependency on what's
 * actually drawn there. The windows and door sit at the exact same rows
 * they always have, so growing or shrinking the roofline never risks
 * drifting out of sync with where the door actually triggers. */
static void show_gym_exterior(void)
{
    static const unsigned char gym_palette[32] = {
        0x0F, 0x01, 0x28, 0x0F,
        /* Steel blue-grey siding -- also the window glass's palette
         * (index 2's lighter blue reads as glass against the same wall
         * tone, the same trick every storefront's own windows already
         * use, e.g. the Mart's). */
        0x0F, 0x11, 0x21, 0x30,
        /* A gold trim band near the roofline (the siding tile reused
         * under this palette instead of the wall's own, see below),
         * tying the exterior back to the interior court's own gold. */
        0x0F, 0x17, 0x00, 0x28,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x16, 0x0F, 0x0F, 0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x16, 0x28, 0x30, 0x0F, 0x0F, 0x0F, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_gym_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = gym_palette[i];
    /* Open sky above a shorter building, instead of the tall facade every
     * other storefront fills almost the whole screen with. */
    for (i = 0; i < 8; ++i) fill_row(i, CHR_TILE_PATH);
    fill_title_rect(8, 0, 4, 1, CHR_TILE_PATH);
    fill_title_rect(8, 4, 24, 1, GYM_ROOF_CAP);
    fill_title_rect(8, 28, 4, 1, CHR_TILE_PATH);
    for (i = 9; i < 24; ++i) {
        fill_title_rect(i, 0, 4, 1, CHR_TILE_PATH);
        fill_title_rect(i, 4, 24, 1, GYM_SIDING);
        fill_title_rect(i, 28, 4, 1, CHR_TILE_PATH);
    }
    for (i = 24; i < 26; ++i) fill_row(i, NEON_TILE_SIDEWALK);
    for (i = 26; i < 30; ++i) fill_row(i, NEON_TILE_ASPHALT);
    /* A basketball icon anchoring the name directly on the siding, instead
     * of a bordered plate the name sits inside -- an icon the text floats
     * next to is a different design language, not another box variant. */
    set_tile(10, 7, GYM_BALL);
    draw_text(10, 9, "MUNICIPAL GYM");
    /* A run of three window units (12 tiles) instead of the Mart's two
     * small paired panes -- a big-box gym's own convention, letting the
     * equipment show from the street. Set closer to the trim band above
     * than to the door below, unlike the even gap either side that used to
     * leave the door crowded right underneath. */
    draw_storefront_window(13, 10);
    draw_storefront_window(13, 14);
    draw_storefront_window(13, 18);
    for (i = 20; i < 24; ++i) {
        set_tile(i, 14, NEON_TILE_DOOR_L);
        set_tile(i, 15, NEON_TILE_DOOR_C);
        set_tile(i, 16, NEON_TILE_DOOR_R);
    }
    /* Vertically centered on the 4-row door (rows 20-23) instead of sitting
     * on its top edge. */
    set_tile(21, 16, DOORKNOB_TILE);

    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 0);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 0);
    /* Row 2 (tile rows 8-11) holds the roof cap and the ball/name signage --
     * both get palette 2's gold/shadow instead of the wall's usual palette
     * 1. Rows 3-5 (12-23) are the plain siding body, windows included --
     * their glass shares palette 1 with the wall for the same
     * masonry-artifact reason every storefront's own windows do. The
     * windows straddle the row-3/row-4 boundary, which is harmless since
     * both bands already share palette 1. */
    set_attribute_block(2, 0, 0);
    for (i = 1; i < 7; ++i) set_attribute_block(2, i, 2);
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

/* Only ever called from the interior (the coach and the gym rats never
 * appear outside). Palette 1 is the interior's own unused/reserved slot --
 * palette 3 is what the whole court's top band and backboard/hoop actually
 * render with (see show_gym_interior()'s palette comment), so using it here
 * recolored the entire top of the court to the dialogue's blue/green scheme
 * every time a message opened. */
static void show_gym_message(const char *line1, const char *line2, const char *line3)
{
    dialogue_set_palette(1);
    show_message_box(line1, line2, line3);
}

unsigned char run_gym_exterior(unsigned char spawn)
{
    /* Arriving from the arcade puts the player at the west edge, mirroring
     * how the side street arrives at its own east edge from the Mart.
     * Leaving the gym returns centered on the door. */
    unsigned char x = spawn == 1 ? 128 : 24;
    unsigned char y = spawn == 1 ? 196 : 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char pad;

    /* No dedicated track yet -- reuses the bar's cue the same way the
     * Arcade and side street do until the gym gets its own. */
    music_select_song(MUSIC_SONG_BAR);
    show_gym_exterior();
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
            show_gym_exterior();
            continue;
        }
        if (pad & PAD_LEFT) {
            if (x > 12) --x;
            else return GYM_EXIT_ARCADE;
        } else if (pad & PAD_RIGHT) {
            /* East edge is a dead end for now -- nothing planned past the
             * gym yet. */
            if (x < 244) ++x;
        }
        if (pad & PAD_UP) {
            if (y > 190) --y;
            else if (x >= 104 && x <= 144) return GYM_EXIT_INTERIOR;
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
        draw_player_sprite(x, y, walk_frame, facing);
    }
}

/* A real backboard and hoop replace the old front desk, over a horizontal
 * court occupying roughly the top third of the floor; the rest is an open
 * equipment area with a weights rack, a mat, and two wandering "gym rats"
 * (see run_gym_interior()). The coach is still a placeholder for the
 * planned live 1-on-1 challenge (see ROADMAP.md): talking to them is just
 * banter until that minigame exists. */
static void show_gym_interior(void)
{
    static const unsigned char gym_interior_palette[32] = {
        /* Plain wood-court tone for the floor and walls. Index 3 is gold
         * rather than white -- the center line, perimeter border, and the
         * weights/bench trim all use index 3, and now that the goals'
         * palette 3 band (below) is scoped to just the two goal corners
         * instead of the whole court width, this is the color everything
         * else in the room actually renders that value in. */
        0x0F, 0x17, 0x27, 0x28,
        /* Reserved for the message box (see show_gym_message()) -- no
         * attribute block ever points here, so dialogue never recolors
         * anything still on screen. */
        0x0F, 0x00, 0x10, 0x30,
        /* Title header's own stable gray. */
        0x0F, 0x00, 0x10, 0x30,
        /* Backboard white, rim/trim gold (matching index 3 everywhere
         * else, not a separate orange -- the border/center-line tiles
         * pass through both this palette and palette 0 depending on
         * which goal they're near, and mismatched values there is what
         * made the border read as two different shades of yellow), net
         * grey. The small band of floor right under the hoop shares this
         * palette too, which happens to read as a lighter free-throw-area
         * tint for free. */
        0x0F, 0x30, 0x10, 0x28,
        /* Sprite palettes 0 and 2 are free to set however this scene
         * likes -- the player's own sprite always uses 1 and 3 (see
         * v2_player_actor.c), which stay exactly as every other scene
         * leaves them so the player doesn't recolor when they walk in,
         * and the coach reuses palette 1 rather than claiming one of the
         * free slots. Both free slots keep the same tan skin tone as the
         * player (index 2) and only vary the shirt color (index 1) --
         * red for one gym rat, blue for the other -- instead of the
         * gray-faced look a mismatched skin tone produced before. */
        0x0F, 0x16, 0x27, 0x30,
        0x0F, 0x0F, 0x27, 0x30,
        0x0F, 0x11, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x30
    };
    unsigned char i;

    PPU.mask = 0;
    mmc3_use_gym_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0;
    for (i = 0; i < 32; ++i) PPU.vram.data = gym_interior_palette[i];
    /* The court occupies roughly the top third of the floor (rows 4-11);
     * the rest is the open equipment area, given a different floor texture
     * so the two zones read as distinct areas rather than one long room. */
    for (i = 4; i < 12; ++i) fill_row(i, GYM_COURT_FLOOR);
    for (i = 12; i < 27; ++i) fill_row(i, NEON_TILE_SIDEWALK);
    /* A half-court line splits the court down the middle, and the same
     * line tile frames its left/right edges; a horizontal counterpart
     * frames the top/bottom, so the whole court reads as one bordered
     * rectangle rather than floor that just happens to stop. */
    for (i = 4; i < 12; ++i) {
        set_tile(i, 16, GYM_CENTER_LINE);
        set_tile(i, 2, GYM_CENTER_LINE);
        set_tile(i, 29, GYM_CENTER_LINE);
    }
    fill_title_rect(4, 2, 28, 1, GYM_BORDER_H);
    fill_title_rect(11, 2, 28, 1, GYM_BORDER_H);
    fill_title_rect(0, 0, 32, 4, BAR_TILE_WALL);
    fill_title_rect(27, 0, 32, 3, BAR_TILE_WALL);
    /* The south wall's exit was purely functional before -- walking down
     * within x 104-144 already returned to the exterior (see
     * run_gym_interior()), but nothing marked where. Same door jamb/fill
     * tiles and column placement as the Mart's and the Bar's own south
     * exits. */
    for (i = 27; i < 30; ++i) {
        set_tile(i, 14, BAR_TILE_DOOR_JAMB);
        set_tile(i, 15, BAR_TILE_DOOR_FILL);
        set_tile(i, 16, BAR_TILE_DOOR_JAMB);
    }
    fill_title_rect(0, 0, 2, 30, BAR_TILE_WALL);
    fill_title_rect(0, 30, 2, 30, BAR_TILE_WALL);
    draw_title_plate(0, 4, 24, 4);
    draw_text(1, 9, "MUNICIPAL GYM");
    /* Two goals, one at each end of the horizontal court. Each hoop is a
     * 2x2 block with a thin vertical post immediately outside it (left of
     * the left hoop, right of the right hoop) instead of a wide panel
     * above it -- the post is still the hoop's answer to "attached to
     * what," but a 1-tile-wide post reads as a longer, more horizontal
     * court than a 5-tile panel did. The left goal sits one tile further
     * in than a flush placement would put it, so post+hoop together fit
     * inside a single 4-tile attribute column (cols 4-7) instead of
     * spilling into the next one over -- that's what was turning a wide
     * stretch of ordinary court floor solid white, since the goal's own
     * white/gold palette applies to the whole attribute block, not just
     * the tiles that are actually part of the hoop. The right goal
     * already fit in one column (24-27) without adjustment. */
    fill_title_rect(5, 4, 1, 3, GYM_BACKBOARD_POST);
    set_tile(6, 5, GYM_HOOP_TL);
    set_tile(6, 6, GYM_HOOP_TR);
    set_tile(7, 5, GYM_HOOP_BL);
    set_tile(7, 6, GYM_HOOP_BR);
    fill_title_rect(5, 27, 1, 3, GYM_BACKBOARD_POST);
    set_tile(6, 25, GYM_HOOP_TL);
    set_tile(6, 26, GYM_HOOP_TR);
    set_tile(7, 25, GYM_HOOP_BL);
    set_tile(7, 26, GYM_HOOP_BR);
    /* A row of courtside folding chairs just below the court's own bottom
     * border, not inside the bordered rectangle itself -- open floor is
     * the point of a court, so this isn't the Mart's dense shelving along
     * a wall, and it reads as sideline seating rather than furniture the
     * court happens to contain. */
    for (i = 10; i <= 22; i += 2) set_tile(12, i, BAR_TILE_STOOL);
    /* Free weights scattered around the equipment zone, a bench, and a
     * folded mat -- more to look at than bare floor, and more than one
     * weight rack repeated in place. The coach and the two wandering gym
     * rats are sprites drawn every frame in run_gym_interior() instead of
     * background tiles, since the rats actually move around. */
    set_tile(14, 5, GYM_WEIGHTS_L);
    set_tile(14, 6, GYM_WEIGHTS_R);
    set_tile(17, 24, GYM_WEIGHTS_L);
    set_tile(17, 25, GYM_WEIGHTS_R);
    set_tile(23, 13, GYM_WEIGHTS_L);
    set_tile(23, 14, GYM_WEIGHTS_R);
    set_tile(20, 5, GYM_BENCH);
    set_tile(20, 6, GYM_BENCH);
    set_tile(19, 21, GYM_MAT);
    set_tile(19, 22, GYM_MAT);

    for (i = 0; i < 8; ++i) {
        set_attribute_block(0, i, 2);
        set_attribute_block(2, i, 0);
        set_attribute_block(3, i, 0);
        set_attribute_block(4, i, 0);
        set_attribute_block(5, i, 0);
        set_attribute_block(6, i, 0);
        set_attribute_block(7, i, 0);
    }
    /* Row 1 (tile rows 4-7) used to be palette 3 across its full width,
     * which recolored the entire top of the court -- not just the goals --
     * to the backboard's white/gold scheme, splitting the floor into two
     * visibly different-colored bands. Scoping it to just the one 4-tile
     * attribute column each goal actually occupies (cols 4-7 for the left
     * post+hoop, cols 24-27 for the right hoop+post -- see the goal
     * placement above) confines that white/gold look to the goals
     * themselves and one unavoidable spillover tile each (a whole
     * attribute column is the smallest unit the PPU can recolor); the
     * rest of the court's row stays on the same palette 0 as the rest of
     * the floor. */
    set_attribute_block(1, 0, 0);
    set_attribute_block(1, 1, 3);
    set_attribute_block(1, 2, 0);
    set_attribute_block(1, 3, 0);
    set_attribute_block(1, 4, 0);
    set_attribute_block(1, 5, 0);
    set_attribute_block(1, 6, 3);
    set_attribute_block(1, 7, 0);
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* The two wandering gym rats -- and now the coach too, on the same system
 * with its own tight, centered bounds -- used to share a module,
 * v2_regulars.c, with Park's and the Bar's own wandering NPCs, living in
 * the fixed engine so all three scenes could call it. That shared copy
 * kept costing fixed-engine budget every time any of the three grew, so
 * each scene now keeps its own copy instead -- this is the Gym's.
 * Position/timer state has to be local to run_gym_interior() rather than
 * file-scope statics: a scene's own PRG pair (see cfg/nes_mmc3_scene.cfg)
 * has no BSS/RAM region of its own, only CODE/RODATA, so there's nowhere
 * for a persistent scene-level variable to live. */
static void update_gym_regular(unsigned char *x, unsigned char *y, unsigned char *timer,
                               unsigned char *steps, unsigned char *direction,
                               unsigned char *facing_up, RngState *rng, unsigned char min_x,
                               unsigned char max_x, unsigned char min_y, unsigned char max_y)
{
    unsigned char roll;
    if (*timer) --*timer;
    else if (*steps) {
        switch (*direction) {
            case 0: if (*x < max_x) ++*x; else --*x; *facing_up = 0; break;
            case 1: if (*x > min_x) --*x; else ++*x; *facing_up = 0; break;
            case 2: if (*y < max_y) ++*y; else --*y; *facing_up = 0; break;
            default: if (*y > min_y) { --*y; *facing_up = 1; } else { ++*y; *facing_up = 0; } break;
        }
        --*steps;
        *timer = (unsigned char)(2 + (rng_next(rng) & 3));
    } else {
        roll = rng_next(rng);
        *direction = roll & 3;
        *steps = (unsigned char)(2 + ((roll >> 2) & 3));
        *timer = (unsigned char)(45 + (rng_next(rng) & 0x7F));
    }
}

/* palette picks the sprite palette (0 or 2, see the palette comment above --
 * 1 and 3 are the player's own). direction 0/1 (right/left) now draws the
 * player's own narrow 2-tile side-profile art (PLAYER_TILE_BASE+6/+7, see
 * v2_player_actor.c) mirrored via the OAM flip bit, instead of just
 * flipping the wide front-facing pose left-right -- a true side view
 * instead of a subtle, easy-to-miss mirror of a pose that doesn't
 * obviously face either direction to begin with. direction 2 (down) keeps
 * the wide 4-tile front view. No separate face-dot overlay here, unlike
 * Park's and the Bar's own copy of this drawer, and the side profile is
 * only 2 sprites instead of 4 -- both cut how often a rat's row lines up
 * with the player's or the other rat's and exceeds the NES's
 * 8-sprites-per-scanline limit, dropping/glitching whichever sprite lost
 * out. */
static void draw_gym_regular(unsigned char slot, unsigned char x, unsigned char y,
                             unsigned char facing_up, unsigned char direction,
                             unsigned char palette)
{
    if (facing_up) {
        set_oam_sprite(slot, x, y, BAR_PATRON_BACK_TOP, palette);
        set_oam_sprite(slot + 1, (unsigned char)(x + 8), y, BAR_PATRON_BACK_TOP, (unsigned char)(0x40 | palette));
        set_oam_sprite(slot + 2, x, (unsigned char)(y + 8), BAR_PATRON_BACK_BOTTOM, palette);
        set_oam_sprite(slot + 3, (unsigned char)(x + 8), (unsigned char)(y + 8), BAR_PATRON_BACK_BOTTOM, (unsigned char)(0x40 | palette));
    } else if (direction == 0 || direction == 1) {
        unsigned char side_attr = direction == 1 ? (unsigned char)(0x40 | palette) : palette;

        set_oam_sprite(slot, (unsigned char)(x + 4), y, PLAYER_TILE_BASE + 6, side_attr);
        hide_oam_sprite(slot + 1);
        set_oam_sprite(slot + 2, (unsigned char)(x + 4), (unsigned char)(y + 8), PLAYER_TILE_BASE + 7, side_attr);
        hide_oam_sprite(slot + 3);
    } else {
        set_oam_sprite(slot, x, y, PLAYER_TILE_BASE, palette);
        set_oam_sprite(slot + 1, (unsigned char)(x + 8), y, PLAYER_TILE_BASE + 1, palette);
        set_oam_sprite(slot + 2, x, (unsigned char)(y + 8), PLAYER_TILE_BASE + 2, palette);
        set_oam_sprite(slot + 3, (unsigned char)(x + 8), (unsigned char)(y + 8), PLAYER_TILE_BASE + 3, palette);
    }
}

static unsigned char gym_regular_is_near(unsigned char regular_x, unsigned char regular_y,
                                         unsigned char x, unsigned char y)
{
    return x >= regular_x - 12 && x <= regular_x + 20 &&
           y >= regular_y - 12 && y <= regular_y + 24;
}

void run_gym_interior(void)
{
    unsigned char x = 128;
    unsigned char y = 200;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char pad;
    unsigned char coach_prompted = 0;
    unsigned char rat_prompted = 0;
    unsigned char rat_dialogue = 0;
    unsigned char rat_one_x = 70, rat_one_y = 140;
    unsigned char rat_two_x = 190, rat_two_y = 140;
    unsigned char rat_one_timer = 50, rat_two_timer = 110;
    unsigned char rat_one_steps = 0, rat_two_steps = 0;
    unsigned char rat_one_direction = 0, rat_two_direction = 0;
    unsigned char rat_one_facing_up = 0, rat_two_facing_up = 0;
    /* The coach wanders on the same system, just a tight box centered on
     * half-court (x 90-160, y 64-88 -- the court's own walkable band, clear
     * of the hoops above) so it stays easy to find rather than roaming the
     * whole floor. */
    unsigned char coach_x = 120, coach_y = 76;
    unsigned char coach_timer = 20, coach_steps = 0;
    unsigned char coach_direction = 0, coach_facing_up = 0;
    RngState rat_rng;

    music_select_song(MUSIC_SONG_BAR);
    rng_seed(&rat_rng, random_seed());
    show_gym_interior();
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
            show_gym_interior();
            continue;
        }
        if (!(pad & PAD_A)) { coach_prompted = 0; rat_prompted = 0; }
        if (pad & PAD_LEFT) {
            if (x > 20) --x;
        } else if (pad & PAD_RIGHT) {
            if (x < 236) ++x;
        }
        if (pad & PAD_UP) {
            if (y > 64) --y;
        } else if (pad & PAD_DOWN) {
            if (y < 216) ++y;
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
        /* Placeholder for the planned live 1-on-1 challenge -- the coach
         * just talks for now. Follows the coach's current wander position
         * rather than a fixed box, same as the rats' own check below. */
        if (gym_regular_is_near(coach_x, coach_y, x, y)) {
            if ((pad & PAD_A) && !coach_prompted) {
                coach_prompted = 1;
                show_gym_message("COACH: YOU WANT A", "GAME? COURT'S CLOSED", "TIL I FIX THE NET.");
                show_gym_interior();
            }
        } else {
            coach_prompted = 0;
        }
        /* The two gym rats are flavor only, rotating through a few stock
         * lines. */
        if (gym_regular_is_near(rat_one_x, rat_one_y, x, y) ||
            gym_regular_is_near(rat_two_x, rat_two_y, x, y)) {
            if ((pad & PAD_A) && !rat_prompted) {
                rat_prompted = 1;
                switch (rat_dialogue) {
                    case 0:
                        show_gym_message("GYM RAT: DO YOU", "EVEN LIFT?", 0);
                        break;
                    case 1:
                        show_gym_message("GYM RAT: ONE MORE", "SET. MAYBE TWO.", 0);
                        break;
                    default:
                        show_gym_message("GYM RAT: COACH SAYS", "I'M 'PROGRESSING.'", 0);
                        break;
                }
                rat_dialogue = (unsigned char)((rat_dialogue + 1) % 3);
                show_gym_interior();
            }
        } else {
            rat_prompted = 0;
        }
        update_gym_regular(&rat_one_x, &rat_one_y, &rat_one_timer, &rat_one_steps,
                          &rat_one_direction, &rat_one_facing_up, &rat_rng, 40, 100, 100, 184);
        update_gym_regular(&rat_two_x, &rat_two_y, &rat_two_timer, &rat_two_steps,
                          &rat_two_direction, &rat_two_facing_up, &rat_rng, 160, 220, 100, 184);
        update_gym_regular(&coach_x, &coach_y, &coach_timer, &coach_steps,
                          &coach_direction, &coach_facing_up, &rat_rng, 90, 160, 64, 88);
        draw_gym_regular(30, rat_one_x, rat_one_y, rat_one_facing_up, rat_one_direction, 0);
        draw_gym_regular(34, rat_two_x, rat_two_y, rat_two_facing_up, rat_two_direction, 2);
        draw_gym_regular(GYM_COACH_OAM, coach_x, coach_y, coach_facing_up, coach_direction, GYM_COACH_PALETTE);
        draw_player_sprite(x, y, walk_frame, facing);
    }
}
