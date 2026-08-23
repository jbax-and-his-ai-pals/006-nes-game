/*
 * test_room.c -- unit tests for src/room.c's layout logic, run under
 * cc65's sim65 (no NES hardware, no emulator).
 */

#include <stdio.h>

#include "../src/room.h"

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
    static const RoomDecoration decorations[] = {
        {5, 6}, {0xFF, 0xFF}
    };
    static const RoomLayout all_open = {1, 1, 1, 1, decorations, 1};
    static const RoomLayout east_closed = {1, 0, 1, 1, decorations, 1};
    static const RoomLayout no_path = {0, 0, 1, 0, decorations, 0};
    static const RoomLayout empty_stage = {0, 0, 1, 0, 0, 0};
    static const RoomLayout compact_house = {0, 0, 1, 0, decorations, 0, 4};
    static const RoomLayout tiny_house = {0, 0, 0, 0, decorations, 0, 0, 8, 8};
    static const RoomLayout walled_house = {0, 0, 0, 0, decorations, 0, 0, 8, 8, 1, 15};

    check_eq("top-left corner is a tree",        room_tile_at(&all_open, 0, 0),  TILE_TREE);
    check_eq("top border away from exit is a tree", room_tile_at(&all_open, 0, 5), TILE_TREE);

    check_eq("north exit gap, left col is path",  room_tile_at(&all_open, 0, 15), TILE_PATH);
    check_eq("north exit gap, right col is path", room_tile_at(&all_open, 0, 16), TILE_PATH);
    check_eq("south exit gap is path",
             room_tile_at(&all_open, ROOM_HEIGHT_TILES - 1, 15), TILE_PATH);
    check_eq("west exit gap, top row is path",    room_tile_at(&all_open, 14, 0), TILE_PATH);
    check_eq("west exit gap, bottom row is path", room_tile_at(&all_open, 15, 0), TILE_PATH);
    check_eq("east exit gap is path",
             room_tile_at(&all_open, 14, ROOM_WIDTH_TILES - 1), TILE_PATH);

    check_eq("room center sits on the crossing path", room_tile_at(&all_open, 14, 15), TILE_PATH);
    check_eq("open interior tile is floor",            room_tile_at(&all_open, 10, 10), TILE_FLOOR);

    check_eq("decorative tree is a tree", room_tile_at(&all_open, 5, 6), TILE_TREE);
    check_eq("decorative tree blocks",    room_tile_blocks(&all_open, 5, 6), 1);
    check_eq("floor does not block",      room_tile_blocks(&all_open, 10, 10), 0);
    check_eq("path does not block",       room_tile_blocks(&all_open, 14, 15), 0);
    check_eq("border tree blocks",        room_tile_blocks(&all_open, 0, 0), 1);

    check_eq("out-of-range row is treated as a tree",
             room_tile_at(&all_open, ROOM_HEIGHT_TILES, 0), TILE_TREE);
    check_eq("out-of-range col is treated as a tree",
             room_tile_at(&all_open, 0, ROOM_WIDTH_TILES), TILE_TREE);

    /* A closed exit stays solid border even at the gap columns/rows --
     * this is what lets some rooms only have a subset of exits. */
    check_eq("closed east exit is a tree, not a path",
             room_tile_at(&east_closed, 14, ROOM_WIDTH_TILES - 1), TILE_TREE);
    check_eq("other exits on the same room are unaffected",
             room_tile_at(&east_closed, 0, 15), TILE_PATH);
    check_eq("crossing path still reaches the closed side from inside",
             room_tile_at(&east_closed, 14, ROOM_WIDTH_TILES - 2), TILE_PATH);

    /* has_path == 0 (a house interior, say) skips the interior crossing
     * path entirely -- what would be path tiles in an outdoor room are
     * just open floor -- but a border exit gap stays walkable either
     * way, since that's not part of the crossing path check at all. */
    check_eq("no-path room: interior crossing spot is floor, not path",
             room_tile_at(&no_path, 14, 15), TILE_FLOOR);
    check_eq("no-path room: room center is floor, not path",
             room_tile_at(&no_path, 14, 16), TILE_FLOOR);
    check_eq("no-path room: south exit gap is still walkable",
             room_tile_at(&no_path, ROOM_HEIGHT_TILES - 1, 15), TILE_PATH);
    check_eq("no-path room: closed north side is still a solid tree",
             room_tile_at(&no_path, 0, 15), TILE_TREE);

    check_eq("empty stage has no phantom decoration collision",
             room_tile_at(&empty_stage, 10, 10), TILE_FLOOR);
    check_eq("empty stage keeps its south exit",
             room_tile_at(&empty_stage, ROOM_HEIGHT_TILES - 1, 15), TILE_PATH);

    check_eq("compact interior has a dark outer surround",
             room_tile_at(&compact_house, 4, 3), TILE_VOID);
    check_eq("compact interior keeps its center as floor",
             room_tile_at(&compact_house, 10, 10), TILE_FLOOR);
    check_eq("compact interior keeps a south doorway passage",
             room_tile_at(&compact_house, 27, 15), TILE_PATH);
    check_eq("compact interior dark surround blocks movement",
             room_tile_blocks(&compact_house, 4, 3), 1);
    check_eq("tiny house has a centered floor", room_tile_at(&tiny_house, 11, 12), TILE_FLOOR);
    check_eq("tiny house is only eight tiles wide", room_tile_at(&tiny_house, 11, 20), TILE_VOID);
    check_eq("tiny house is only eight tiles tall", room_tile_at(&tiny_house, 19, 12), TILE_VOID);
    check_eq("walled house has a blocking north wall", room_tile_at(&walled_house, 11, 15), TILE_TREE);
    check_eq("walled house has a blocking side wall", room_tile_at(&walled_house, 14, 12), TILE_TREE);
    check_eq("walled house door is walkable", room_tile_at(&walled_house, 18, 15), TILE_PATH);
    check_eq("walled house blocks the rest of its south wall", room_tile_blocks(&walled_house, 18, 14), 1);

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
