#include "movement.h"

unsigned char boxes_overlap(unsigned char ax, unsigned char ay,
                             unsigned char bx, unsigned char by, unsigned char pad)
{
    /* Widen to a type that can't overflow: SCREEN_MAX_X/Y mean a/bx/y
     * can be up to 248/232, and +8+pad could otherwise wrap an
     * unsigned char (max 255) right when a real box is near the edge. */
    unsigned int axi = ax, ayi = ay, bxi = bx, byi = by, w = 8 + pad;

    return (axi + w > bxi) && (axi < bxi + w) &&
           (ayi + w > byi) && (ayi < byi + w);
}

unsigned char box_hits_tiles(const RoomLayout *layout, unsigned char x, unsigned char y)
{
    return room_tile_blocks(layout, y >> 3, x >> 3) || room_tile_blocks(layout, y >> 3, (x + 7) >> 3) ||
           room_tile_blocks(layout, (y + 7) >> 3, x >> 3) || room_tile_blocks(layout, (y + 7) >> 3, (x + 7) >> 3);
}

unsigned char knockback_direction_away(unsigned char source_x, unsigned char source_y,
                                       unsigned char target_x, unsigned char target_y)
{
    unsigned char dx = target_x >= source_x ? target_x - source_x : source_x - target_x;
    unsigned char dy = target_y >= source_y ? target_y - source_y : source_y - target_y;

    if (dx >= dy) return target_x >= source_x ? PAD_RIGHT_MASK : PAD_LEFT_MASK;
    return target_y >= source_y ? PAD_DOWN_MASK : PAD_UP_MASK;
}

void knockback_step(unsigned char *x, unsigned char *y, unsigned char direction,
                    const RoomLayout *layout)
{
    unsigned char nx = *x;
    unsigned char ny = *y;

    if (direction == PAD_LEFT_MASK && *x > 0) {
        --nx;
    } else if (direction == PAD_RIGHT_MASK && *x < SCREEN_MAX_X) {
        ++nx;
    } else if (direction == PAD_UP_MASK && *y > SCREEN_MIN_Y) {
        --ny;
    } else if (direction == PAD_DOWN_MASK && *y < SCREEN_MAX_Y) {
        ++ny;
    } else {
        return;
    }

    if (!box_hits_tiles(layout, nx, ny)) {
        *x = nx;
        *y = ny;
    }
}

static unsigned char box_blocked(const RoomLayout *layout, unsigned char x, unsigned char y, const Obstacle *extra)
{
    return (extra != 0 && boxes_overlap(x, y, extra->x, extra->y, 0)) || box_hits_tiles(layout, x, y);
}

unsigned char update_position(unsigned char *x, unsigned char *y, unsigned char pad,
                               const RoomLayout *layout, const Obstacle *extra)
{
    unsigned char nx, ny;
    unsigned char exit_flags = EXIT_NONE;

    if (pad & PAD_LEFT_MASK) {
        if (*x == 0) {
            exit_flags |= EXIT_WEST;
        } else {
            nx = *x - 1;
            if (!box_blocked(layout, nx, *y, extra)) *x = nx;
        }
    }
    if (pad & PAD_RIGHT_MASK) {
        if (*x == SCREEN_MAX_X) {
            exit_flags |= EXIT_EAST;
        } else {
            nx = *x + 1;
            if (!box_blocked(layout, nx, *y, extra)) *x = nx;
        }
    }
    if (pad & PAD_UP_MASK) {
        if (*y == SCREEN_MIN_Y) {
            exit_flags |= EXIT_NORTH;
        } else {
            ny = *y - 1;
            if (!box_blocked(layout, *x, ny, extra)) *y = ny;
        }
    }
    if (pad & PAD_DOWN_MASK) {
        if (*y == SCREEN_MAX_Y) {
            exit_flags |= EXIT_SOUTH;
        } else {
            ny = *y + 1;
            if (!box_blocked(layout, *x, ny, extra)) *y = ny;
        }
    }

    return exit_flags;
}
