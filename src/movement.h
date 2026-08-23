/*
 * movement.h -- hardware-independent player movement/collision logic.
 *
 * Deliberately has no dependency on <nes.h> or any PPU/joypad register
 * (it depends only on room.h, which is equally hardware-independent).
 * That's what lets it be compiled and unit-tested under cc65's sim6502
 * target (see tests/), not just the real nes target -- the pad-bit
 * masks below are just the standard NES controller shift-register bit
 * order, not an NES-specific API.
 */

#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "room.h"

/* Matches the bit positions read_pad() (src/main.c) assembles its result
 * into, which in turn matches nes.h's JOY_UP_MASK/JOY_DOWN_MASK/etc. */
#define PAD_UP_MASK     0x10
#define PAD_DOWN_MASK   0x20
#define PAD_LEFT_MASK   0x40
#define PAD_RIGHT_MASK  0x80

/* The player sprite is 8x8; the room fills the whole 256x240 screen.
 * These are the outer limits on the sprite's top-left corner so it never
 * leaves the visible screen. Reaching one of these is only possible via
 * an exit gap (every other position along an edge gets stopped earlier
 * by a tree tile), which is exactly what makes them double as the
 * exit-crossing signal in update_position()'s return value below.
 *
 * SCREEN_MIN_Y is 1, not 0: the NES sprite-Y-is-delayed-by-one-scanline
 * quirk means the hardware OAM Y byte has to be written as (true Y - 1)
 * (see set_sprite() in main.c), which underflows if true Y is ever 0.
 * Losing the top pixel row of movement is imperceptible; there's no
 * equivalent restriction on X. */
#define SCREEN_MIN_Y   1
#define SCREEN_MAX_X   (256 - 8)
#define SCREEN_MAX_Y   (240 - 8)

/* update_position()'s return value: which edge(s) the player pressed
 * into while already flush against them this frame -- i.e. tried to
 * walk through an exit. main.c uses this to trigger a room change. */
#define EXIT_NONE    0x00
#define EXIT_NORTH   0x01
#define EXIT_EAST    0x02
#define EXIT_SOUTH   0x04
#define EXIT_WEST    0x08

/* An 8x8-sprite-sized position -- used for anything that blocks/is
 * checked against the player like a room tile does, but isn't part of
 * the room's static layout (room.h's decorations), e.g. an NPC. */
typedef struct {
    unsigned char x, y;
} Obstacle;

/* Whether two 8x8 boxes at (ax,ay) and (bx,by) are within `pad` pixels
 * of touching or overlapping (pad=0 for exact overlap -- what collision
 * uses; a larger pad gives a "close enough to interact with" check). */
unsigned char boxes_overlap(unsigned char ax, unsigned char ay,
                             unsigned char bx, unsigned char by, unsigned char pad);

/* Whether an 8x8 box at (x, y) overlaps any blocking room tile. */
unsigned char box_hits_tiles(const RoomLayout *layout, unsigned char x, unsigned char y);

/* Returns the cardinal PAD_* direction that moves target away from source.
 * A horizontal tie wins, keeping contact knockback stable and predictable. */
unsigned char knockback_direction_away(unsigned char source_x, unsigned char source_y,
                                       unsigned char target_x, unsigned char target_y);

/* Moves an 8x8 box one pixel in a knockback direction, stopping at room
 * tiles or screen bounds. Unlike update_position(), it never reports exits:
 * combat recoil must not accidentally trigger a room transition. */
void knockback_step(unsigned char *x, unsigned char *y, unsigned char direction,
                    const RoomLayout *layout);

/* Applies one frame of D-pad input to (*x, *y), blocked by layout's tree
 * tiles, clamped to the screen, and additionally blocked by `extra` if
 * non-null (pass NULL when the current room has no such obstacle).
 * Returns an EXIT_* bitmask (usually EXIT_NONE) for any edge the player
 * pressed into while already at it. */
unsigned char update_position(unsigned char *x, unsigned char *y, unsigned char pad,
                               const RoomLayout *layout, const Obstacle *extra);

#endif
