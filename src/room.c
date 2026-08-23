#include "room.h"

/* North/south exits are a 2-tile gap centered on the top/bottom border;
 * east/west exits are a 2-tile gap centered on the left/right border. A
 * path connects all open exits across the room, Zelda / Final Fantasy
 * Adventure style. The crossing path itself doesn't care which sides are
 * actually open (it doesn't disappear just because a given side's border
 * stays closed) -- a closed exit just means the path stops at a solid
 * wall instead of continuing through it. That's for layout->has_path
 * rooms only, though (every outdoor room so far) -- a has_path == 0 room
 * (a house interior, say) skips the interior path entirely and is just
 * open floor, while a border exit gap stays walkable regardless. */
#define EXIT_COL_LOW    15
#define EXIT_COL_HIGH   16
#define EXIT_ROW_LOW    14
#define EXIT_ROW_HIGH   15

static unsigned char is_v_path_col(unsigned char col)
{
    return col == EXIT_COL_LOW || col == EXIT_COL_HIGH;
}

static unsigned char is_h_path_row(unsigned char row)
{
    return row == EXIT_ROW_LOW || row == EXIT_ROW_HIGH;
}

static unsigned char is_decoration(const RoomLayout *layout, unsigned char row, unsigned char col)
{
    const RoomDecoration *d = layout->decorations;
    if (d == 0) return 0;
    while (!(d->row == 0xFF && d->col == 0xFF)) {
        if (d->row == row && d->col == col) return 1;
        ++d;
    }
    return 0;
}

RoomTile room_tile_at(const RoomLayout *layout, unsigned char row, unsigned char col)
{
    unsigned char is_border_row, is_border_col;
    unsigned char on_v_path, on_h_path;

    if (row >= ROOM_HEIGHT_TILES || col >= ROOM_WIDTH_TILES) {
        return TILE_TREE;
    }

    is_border_row = (row == 0) || (row == ROOM_HEIGHT_TILES - 1);
    is_border_col = (col == 0) || (col == ROOM_WIDTH_TILES - 1);
    on_v_path = is_v_path_col(col);
    on_h_path = is_h_path_row(row);

    if (layout->interior_width != 0 &&
        (col < (ROOM_WIDTH_TILES - layout->interior_width) / 2 ||
         col >= (ROOM_WIDTH_TILES + layout->interior_width) / 2 ||
         row < (ROOM_HEIGHT_TILES - layout->interior_height) / 2 ||
         row >= (ROOM_HEIGHT_TILES + layout->interior_height) / 2)) {
        return TILE_VOID;
    }

    if (layout->interior_width != 0 && layout->interior_has_walls &&
        (row == (ROOM_HEIGHT_TILES - layout->interior_height) / 2 ||
         row == (ROOM_HEIGHT_TILES + layout->interior_height) / 2 - 1 ||
         col == (ROOM_WIDTH_TILES - layout->interior_width) / 2 ||
         col == (ROOM_WIDTH_TILES + layout->interior_width) / 2 - 1)) {
        if (row == (ROOM_HEIGHT_TILES + layout->interior_height) / 2 - 1 &&
            col == layout->interior_door_col) return TILE_PATH;
        return TILE_TREE;
    }

    if (layout->interior_margin != 0 &&
        (row < layout->interior_margin || row >= ROOM_HEIGHT_TILES - layout->interior_margin ||
         col < layout->interior_margin || col >= ROOM_WIDTH_TILES - layout->interior_margin)) {
        /* Keep a centered doorway passage through the otherwise dark frame. */
        if ((on_v_path && (layout->exit_n || layout->exit_s)) ||
            (on_h_path && (layout->exit_w || layout->exit_e))) return TILE_PATH;
        return TILE_VOID;
    }

    if (is_border_row || is_border_col) {
        if (row == 0 && on_v_path && layout->exit_n) return TILE_PATH;
        if (row == ROOM_HEIGHT_TILES - 1 && on_v_path && layout->exit_s) return TILE_PATH;
        if (col == 0 && on_h_path && layout->exit_w) return TILE_PATH;
        if (col == ROOM_WIDTH_TILES - 1 && on_h_path && layout->exit_e) return TILE_PATH;
        return TILE_TREE;
    }

    if (layout->has_path && (on_v_path || on_h_path)) {
        return TILE_PATH;
    }

    if (is_decoration(layout, row, col)) {
        return TILE_TREE;
    }

    return TILE_FLOOR;
}

unsigned char room_tile_blocks(const RoomLayout *layout, unsigned char row, unsigned char col)
{
    RoomTile tile = room_tile_at(layout, row, col);
    return tile == TILE_TREE || tile == TILE_VOID;
}
