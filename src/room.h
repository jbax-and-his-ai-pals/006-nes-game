/*
 * room.h -- hardware-independent room layout.
 *
 * This is the single source of truth for "what's at tile (row, col)" --
 * both the renderer (main.c's draw_room()) and the collision system
 * (movement.c) read from here, so the visuals and the collision can
 * never disagree with each other. Like movement.h, this has no
 * dependency on <nes.h> or any hardware register, so it's unit-testable
 * under cc65's sim65 (see tests/test_room.c).
 *
 * A room's *shape* (border, exits, crossing path, decoration positions)
 * lives here as data (RoomLayout). Its *appearance* (which CHR tile and
 * palette colors represent "floor"/"path"/"tree") is main.c's job --
 * that split is what lets two rooms share this exact layout logic while
 * looking like a meadow and a desert.
 */

#ifndef ROOM_H
#define ROOM_H

/* The NES nametable is always exactly this size -- a "room" is one full
 * nametable (32x30 tiles of 8x8 = 256x240 pixels, the whole screen). */
#define ROOM_WIDTH_TILES   32
#define ROOM_HEIGHT_TILES  30

typedef enum {
    TILE_FLOOR,   /* walkable open ground */
    TILE_PATH,    /* walkable path -- leads to/through the exits */
    TILE_TREE,    /* blocking obstacle (tree, rock, whatever the biome uses) */
    TILE_VOID     /* dark exterior around a compact interior room */
} RoomTile;

typedef struct {
    unsigned char row, col;
} RoomDecoration;

typedef struct {
    /* 1 = this side has an opening in the border, 0 = solid all the way
     * across. Some rooms will only have a subset of these. */
    unsigned char exit_n, exit_e, exit_s, exit_w;

    /* Obstacles scattered off the crossing path, so the room isn't a
     * featureless field. Terminated by a {0xFF, 0xFF} sentinel entry. */
    const RoomDecoration *decorations;

    /* 1 = the usual outdoor crossing path connects every open exit
     * across the room's interior (every overworld room so far); 0 =
     * no interior path at all -- just open floor plus whatever
     * decorations are listed, for rooms that don't want the outdoor
     * "roads through a field" look, like a house's furnished interior.
     * A border exit gap stays walkable either way; this only affects
     * the interior tiles the path would otherwise cross. */
    unsigned char has_path;

    /* Number of tiles reserved around every edge as a dark surround. A
     * centered two-tile-wide passage still connects any open exit to the
     * smaller room. Zero keeps the original full-screen room shape. */
    unsigned char interior_margin;

    /* Optional centered compact room dimensions in tiles. When non-zero,
     * everything outside this rectangle is TILE_VOID. Compact rooms use a
     * portal or other in-room interaction for their door, not a screen edge. */
    unsigned char interior_width, interior_height;

    /* A compact room can surround its floor with blocking wall tiles. The
     * bottom-wall tile at interior_door_col stays walkable for its door. */
    unsigned char interior_has_walls, interior_door_col;
} RoomLayout;

/* row is 0..ROOM_HEIGHT_TILES-1, col is 0..ROOM_WIDTH_TILES-1.
 * Out-of-range coordinates return TILE_TREE, so callers don't need
 * their own bounds checks. */
RoomTile room_tile_at(const RoomLayout *layout, unsigned char row, unsigned char col);

/* Convenience: whether the tile blocks movement. */
unsigned char room_tile_blocks(const RoomLayout *layout, unsigned char row, unsigned char col);

#endif
