#include "world.h"

static unsigned char layout_exit_open(const RoomLayout *layout, unsigned char direction)
{
    if (direction == WORLD_EXIT_NORTH) return layout->exit_n;
    if (direction == WORLD_EXIT_EAST) return layout->exit_e;
    if (direction == WORLD_EXIT_SOUTH) return layout->exit_s;
    return layout->exit_w;
}

static unsigned char connection_at(const RoomConnections *connection, unsigned char direction)
{
    if (direction == WORLD_EXIT_NORTH) return connection->north;
    if (direction == WORLD_EXIT_EAST) return connection->east;
    if (direction == WORLD_EXIT_SOUTH) return connection->south;
    return connection->west;
}

static unsigned char opposite(unsigned char direction)
{
    if (direction == WORLD_EXIT_NORTH) return WORLD_EXIT_SOUTH;
    if (direction == WORLD_EXIT_EAST) return WORLD_EXIT_WEST;
    if (direction == WORLD_EXIT_SOUTH) return WORLD_EXIT_NORTH;
    return WORLD_EXIT_EAST;
}

unsigned char world_validate_connections(const RoomLayout *layouts,
                                         const RoomConnections *connections,
                                         const unsigned char *one_way_flags,
                                         unsigned char room_count)
{
    static const unsigned char directions[] = {WORLD_EXIT_NORTH, WORLD_EXIT_EAST, WORLD_EXIT_SOUTH, WORLD_EXIT_WEST};
    unsigned char room, i, direction, destination;
    for (room = 0; room < room_count; ++room) {
        for (i = 0; i < 4; ++i) {
            direction = directions[i];
            destination = connection_at(&connections[room], direction);
            if ((layout_exit_open(&layouts[room], direction) != 0) != (destination != ROOM_NONE)) return 0;
            if (destination == ROOM_NONE) continue;
            if (destination >= room_count) return 0;
            if (!(one_way_flags[room] & direction) &&
                connection_at(&connections[destination], opposite(direction)) != room) return 0;
        }
    }
    return 1;
}

void world_apply_spawn(const RoomSpawn *spawn, unsigned char *x, unsigned char *y)
{
    if (!(spawn->flags & WORLD_SPAWN_KEEP_X)) *x = spawn->x;
    if (!(spawn->flags & WORLD_SPAWN_KEEP_Y)) *y = spawn->y;
}
