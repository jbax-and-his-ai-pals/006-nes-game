/*
 * test_movement.c -- unit tests for src/movement.c, run under cc65's
 * sim65 (no NES hardware, no emulator). Compiles the real
 * update_position() (and the real room.c it collides against), not a
 * mirror of them.
 *
 * Coordinates below are chosen against the "all_open" test layout: the
 * exit-crossing path sits on tile rows {14,15} and columns {15,16}, and
 * there's a decorative tree at tile (row 5, col 6). See tests/test_room.c
 * for direct coverage of the layout logic itself.
 */

#include <stdio.h>

#include "../src/movement.h"

static int failures = 0;

static void check_eq(const char *name, int actual, int expected)
{
    if (actual != expected) {
        printf("FAIL %s: got %d, expected %d\n", name, actual, expected);
        ++failures;
    } else {
        printf("PASS %s\n", name);
    }
}

int main(void)
{
    unsigned char x, y;
    static const RoomDecoration decorations[] = {
        {5, 6}, {0xFF, 0xFF}
    };
    static const RoomLayout all_open  = {1, 1, 1, 1, decorations};
    static const RoomLayout no_east   = {1, 0, 1, 1, decorations};

    /* Open grass, away from any path/border/decoration: moves freely. */
    x = 80; y = 80; /* tile (row 10, col 10) */
    update_position(&x, &y, 0, &all_open, 0);
    check_eq("no input: x unchanged", x, 80);
    check_eq("no input: y unchanged", y, 80);

    x = 80; y = 80;
    update_position(&x, &y, PAD_LEFT_MASK, &all_open, 0);
    check_eq("open floor, left: x decrements", x, 79);

    x = 80; y = 80;
    update_position(&x, &y, PAD_RIGHT_MASK, &all_open, 0);
    check_eq("open floor, right: x increments", x, 81);

    x = 80; y = 80;
    update_position(&x, &y, PAD_UP_MASK, &all_open, 0);
    check_eq("open floor, up: y decrements", y, 79);

    x = 80; y = 80;
    update_position(&x, &y, PAD_DOWN_MASK, &all_open, 0);
    check_eq("open floor, down: y increments", y, 81);

    x = 80; y = 80;
    update_position(&x, &y, PAD_LEFT_MASK | PAD_UP_MASK, &all_open, 0);
    check_eq("diagonal input: x decrements", x, 79);
    check_eq("diagonal input: y decrements", y, 79);

    /* Tree line on the border (row 5 is not an exit row): blocked. */
    x = 8; y = 40; /* tile (row 5, col 1), against the west tree line */
    update_position(&x, &y, PAD_LEFT_MASK, &all_open, 0);
    check_eq("blocked by border tree: x unchanged", x, 8);

    /* West exit gap (row 14 is an exit row): passable, right up to the
     * screen edge -- this is the same border column that blocked above,
     * just at a row where the tree line opens up. */
    x = 8; y = 112; /* tile (row 14, col 1) */
    update_position(&x, &y, PAD_LEFT_MASK, &all_open, 0);
    check_eq("west exit gap is walkable", x, 7);

    /* Decorative tree at (row 5, col 6): approaching from the left tile
     * (row 5, col 5, open floor) is blocked the moment the sprite's
     * right edge would enter the tree's tile, even before the sprite is
     * tile-aligned with it. */
    x = 40; y = 40; /* tile (row 5, col 5) */
    update_position(&x, &y, PAD_RIGHT_MASK, &all_open, 0);
    check_eq("blocked by decorative tree", x, 40);

    /* The screen-edge backstop stops further movement once the sprite is
     * flush with the edge, even on a path tile where nothing would
     * otherwise block it -- and reports that as an exit attempt. */
    x = 0; y = 112;
    check_eq("screen edge stops movement even on a path tile: exit flag",
             update_position(&x, &y, PAD_LEFT_MASK, &all_open, 0), EXIT_WEST);
    check_eq("screen edge stops movement even on a path tile: x", x, 0);

    /* SCREEN_MIN_Y is 1, not 0 -- see movement.h for why (the OAM Y
     * hardware quirk in main.c's set_sprite() would underflow at 0). */
    x = 120; y = SCREEN_MIN_Y;
    update_position(&x, &y, PAD_UP_MASK, &all_open, 0);
    check_eq("y cannot reach 0", y, SCREEN_MIN_Y);

    /* Exit detection: only fires when already flush against that edge
     * and pressing further into it, and reports the right direction. */
    x = SCREEN_MAX_X; y = 112;
    check_eq("east exit fires at the east edge",
             update_position(&x, &y, PAD_RIGHT_MASK, &all_open, 0), EXIT_EAST);

    x = 120; y = SCREEN_MIN_Y;
    check_eq("north exit fires at the north edge",
             update_position(&x, &y, PAD_UP_MASK, &all_open, 0), EXIT_NORTH);

    x = 120; y = SCREEN_MAX_Y;
    check_eq("south exit fires at the south edge",
             update_position(&x, &y, PAD_DOWN_MASK, &all_open, 0), EXIT_SOUTH);

    x = 80; y = 80;
    check_eq("no exit fires away from any edge",
             update_position(&x, &y, PAD_LEFT_MASK, &all_open, 0), EXIT_NONE);

    /* A closed exit (room.h's RoomLayout) is a real tree line, so the
     * player is blocked by ordinary tile collision, not stopped at the
     * screen edge -- meaning no exit flag fires, since they never reach
     * x == SCREEN_MAX_X in the first place. */
    {
        int i;
        unsigned char last_exits = EXIT_NONE;

        x = 40; y = 112; /* row 14: an exit row, but east is closed here */
        for (i = 0; i < 200; ++i) {
            last_exits = update_position(&x, &y, PAD_RIGHT_MASK, &no_east, 0);
        }
        check_eq("closed east exit: never reaches the screen edge", x < SCREEN_MAX_X, 1);
        check_eq("closed east exit: no exit flag on the final frame", last_exits, EXIT_NONE);
    }

    /* Convergence tests: hold a direction for far longer than needed and
     * check where the player actually settles, at a column/row with no
     * exit gap (so a real tree line is guaranteed to stop it) -- this is
     * what "walk into a wall and see where you stop" means in-game,
     * rather than just checking a single step. These exist because a
     * previous bug (a sprite-rendering hardware quirk, not this file) was
     * only visible as a walk-into-a-wall symptom, not a single-step one.
     * x=40/y=40 sit at tile (row 5, col 5), one tile away from both the
     * border and the decorative tree at (row 5, col 6) in every
     * direction that matters below. */
    {
        int i;

        x = 40; y = 100;
        for (i = 0; i < 200; ++i) update_position(&x, &y, PAD_DOWN_MASK, &all_open, 0);
        check_eq("settles walking down: x", x, 40);
        check_eq("settles walking down: bottom edge flush with row 29", y + 7, 231);

        x = 40; y = 140;
        for (i = 0; i < 200; ++i) update_position(&x, &y, PAD_UP_MASK, &all_open, 0);
        check_eq("settles walking up: x", x, 40);
        check_eq("settles walking up: top edge flush with row 0", y, 8);

        x = 100; y = 40;
        for (i = 0; i < 200; ++i) update_position(&x, &y, PAD_LEFT_MASK, &all_open, 0);
        check_eq("settles walking left: y", y, 40);
        check_eq("settles walking left: flush with decorative tree", x, 56);

        x = 20; y = 40;
        for (i = 0; i < 200; ++i) update_position(&x, &y, PAD_RIGHT_MASK, &all_open, 0);
        check_eq("settles walking right: y", y, 40);
        check_eq("settles walking right: flush with decorative tree", x + 7, 47);
    }

    /* boxes_overlap(): exact-overlap (pad=0) and padded "nearby" checks. */
    check_eq("identical boxes overlap",        boxes_overlap(50, 50, 50, 50, 0), 1);
    check_eq("boxes 8px apart horizontally do not overlap (pad 0)", boxes_overlap(50, 50, 58, 50, 0), 0);
    check_eq("boxes 8px apart vertically do not overlap (pad 0)", boxes_overlap(50, 50, 50, 58, 0), 0);
    check_eq("boxes overlapping by 1px do overlap", boxes_overlap(50, 50, 57, 50, 0), 1);
    check_eq("far-apart boxes: pad 4 still not enough", boxes_overlap(50, 50, 63, 50, 4), 0);
    check_eq("boxes 8px apart: pad 4 brings them into range",
             boxes_overlap(50, 50, 58, 50, 4), 1);
    check_eq("overlap check is symmetric",
             boxes_overlap(50, 50, 58, 50, 4), boxes_overlap(58, 50, 50, 50, 4));
    check_eq("near a screen edge does not overflow/false-positive",
             boxes_overlap(SCREEN_MAX_X, SCREEN_MAX_Y, 0, 0, 4), 0);

    /* An extra obstacle (e.g. an NPC) blocks movement the same way a
     * room tile does, on open floor away from any tile obstacle. */
    {
        Obstacle npc;
        npc.x = 88; npc.y = 80; /* tile (row 10, col 11) -- open floor here */

        x = 80; y = 80; /* tile (row 10, col 10), adjacent to the obstacle */
        update_position(&x, &y, PAD_RIGHT_MASK, &all_open, &npc);
        check_eq("blocked by extra obstacle", x, 80);

    x = 80; y = 80;
    update_position(&x, &y, PAD_RIGHT_MASK, &all_open, 0);
    check_eq("same spot, no obstacle given: not blocked", x, 81);

    check_eq("shared tile collision allows open floor", box_hits_tiles(&all_open, 80, 80), 0);
    check_eq("shared tile collision blocks a decorative tree", box_hits_tiles(&all_open, 48, 40), 1);
    }

    /* Knockback uses the same tile/boundary rules as movement but cannot
     * create an exit event. It is deliberately one pixel per call so the
     * game can animate a short, readable recoil over several frames. */
    check_eq("knockback chooses right away from source", knockback_direction_away(80, 80, 96, 80), PAD_RIGHT_MASK);
    check_eq("knockback chooses up away from source", knockback_direction_away(80, 80, 80, 64), PAD_UP_MASK);
    check_eq("knockback horizontal tie is stable", knockback_direction_away(80, 80, 72, 72), PAD_LEFT_MASK);

    x = 80; y = 80;
    knockback_step(&x, &y, PAD_RIGHT_MASK, &all_open);
    check_eq("knockback moves on open floor", x, 81);

    x = 40; y = 40;
    knockback_step(&x, &y, PAD_RIGHT_MASK, &all_open);
    check_eq("knockback stops at a tree", x, 40);

    x = 120; y = SCREEN_MIN_Y;
    knockback_step(&x, &y, PAD_UP_MASK, &all_open);
    check_eq("knockback stops at screen top", y, SCREEN_MIN_Y);

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
