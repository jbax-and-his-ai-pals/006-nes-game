/* world.h -- hardware-independent room-link and entry-spawn data. */
#ifndef WORLD_H
#define WORLD_H
#include "room.h"
#define ROOM_NONE 0xFF
#define WORLD_EXIT_NORTH 0x01
#define WORLD_EXIT_EAST  0x02
#define WORLD_EXIT_SOUTH 0x04
#define WORLD_EXIT_WEST  0x08
#define WORLD_SPAWN_KEEP_X 0x01
#define WORLD_SPAWN_KEEP_Y 0x02
typedef struct { unsigned char north, east, south, west; } RoomConnections;
typedef struct { unsigned char x, y, flags; } RoomSpawn;
unsigned char world_validate_connections(const RoomLayout *layouts,
                                         const RoomConnections *connections,
                                         const unsigned char *one_way_flags,
                                         unsigned char room_count);
void world_apply_spawn(const RoomSpawn *spawn, unsigned char *x, unsigned char *y);
#endif
