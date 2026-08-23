#include <stdio.h>
#include "../src/world.h"
static int failures = 0;
static void check(const char *name, int actual, int expected) { if (actual != expected) { printf("FAIL %s\n", name); ++failures; } else printf("PASS %s\n", name); }
int main(void)
{
    static const RoomDecoration decorations[] = {{0xFF, 0xFF}};
    static const RoomLayout layouts[] = {{0,1,0,0,decorations,1}, {0,0,0,1,decorations,1}};
    static const RoomLayout one_way_layouts[] = {{0,1,0,0,decorations,1}, {0,0,0,0,decorations,1}};
    static const RoomConnections valid[] = {{ROOM_NONE,1,ROOM_NONE,ROOM_NONE}, {ROOM_NONE,ROOM_NONE,ROOM_NONE,0}};
    static const RoomConnections bad_exit[] = {{ROOM_NONE,1,ROOM_NONE,ROOM_NONE}, {ROOM_NONE,ROOM_NONE,ROOM_NONE,ROOM_NONE}};
    static const unsigned char no_one_way[] = {0,0};
    static const unsigned char one_way[] = {WORLD_EXIT_EAST,0};
    unsigned char x = 55, y = 66;
    static const RoomSpawn keep_x = {0, 232, WORLD_SPAWN_KEEP_X};
    check("reciprocal links validate", world_validate_connections(layouts, valid, no_one_way, 2), 1);
    check("missing reciprocal link fails", world_validate_connections(layouts, bad_exit, no_one_way, 2), 0);
    check("declared one-way link validates", world_validate_connections(one_way_layouts, bad_exit, one_way, 2), 1);
    world_apply_spawn(&keep_x, &x, &y);
    check("spawn preserves requested axis", x, 55);
    check("spawn updates fixed axis", y, 232);
    return failures ? 1 : 0;
}
