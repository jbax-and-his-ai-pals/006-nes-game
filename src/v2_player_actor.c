#include "v2_player_actor.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif
#include "v2_runtime.h"

#define PLAYER_TILE_BASE 212

void draw_player_sprite(unsigned char x, unsigned char y, unsigned char walking,
                        unsigned char facing)
{
    unsigned char left = x - 8;
    unsigned char top = y - 24;
    unsigned char legs = PLAYER_TILE_BASE + 4;
    unsigned char palette = facing == PLAYER_FACING_UP ? 3 : 1;
    unsigned char body_top = top;
    unsigned char left_leg_y = top + 16;
    unsigned char right_leg_y = top + 16;
    if (facing == PLAYER_FACING_UP) {
        /* Back view deliberately reuses the forward tiles. Only its palette
         * changes, keeping the actor's proportions pixel-for-pixel intact. */
        set_oam_sprite(24, left, body_top, PLAYER_TILE_BASE, palette);
        set_oam_sprite(25, left + 8, body_top, PLAYER_TILE_BASE + 1, palette);
        set_oam_sprite(26, left, body_top + 8, PLAYER_TILE_BASE + 2, palette);
        set_oam_sprite(27, left + 8, body_top + 8, PLAYER_TILE_BASE + 3, palette);
    } else if (facing == PLAYER_FACING_LEFT || facing == PLAYER_FACING_RIGHT) {
        unsigned char side_attr = palette;

        if (facing == PLAYER_FACING_LEFT) side_attr |= 0x40;
        /* The side profile is intentionally a centered 8px column. The
         * authored right-facing nose becomes a left-facing nose via OAM flip. */
        set_oam_sprite(24, left + 4, body_top, PLAYER_TILE_BASE + 6, side_attr);
        hide_oam_sprite(25);
        set_oam_sprite(26, left + 4, body_top + 8, PLAYER_TILE_BASE + 7, side_attr);
        hide_oam_sprite(27);
        /* Two narrow leg tiles overlap just enough to read as a slim profile.
         * Only the leading foot advances in the direction of travel. */
        if (facing == PLAYER_FACING_LEFT) {
            set_oam_sprite(28, left + 3 - walking, top + 16, legs, side_attr);
            set_oam_sprite(29, left + 5, top + 16, legs + 1, side_attr);
        } else {
            set_oam_sprite(28, left + 3, top + 16, legs, side_attr);
            set_oam_sprite(29, left + 5 + walking, top + 16, legs + 1, side_attr);
        }
        return;
    } else {
        set_oam_sprite(24, left, body_top, PLAYER_TILE_BASE, palette);
        set_oam_sprite(25, left + 8, body_top, PLAYER_TILE_BASE + 1, palette);
        set_oam_sprite(26, left, body_top + 8, PLAYER_TILE_BASE + 2, palette);
        set_oam_sprite(27, left + 8, body_top + 8, PLAYER_TILE_BASE + 3, palette);
    }

    /* One foot moves at a time: down-facing steps extend downward, while
     * back-facing steps extend upward. The other foot remains planted. */
    if (facing == PLAYER_FACING_UP) {
        if (walking) --left_leg_y;
        else --right_leg_y;
    } else {
        /* A lowered entire tile leaves a distracting one-pixel body gap.
         * Hold the leading foot flush and pull its partner back instead. */
        if (walking) --right_leg_y;
        else --left_leg_y;
    }
    set_oam_sprite(28, left, left_leg_y, legs, palette);
    set_oam_sprite(29, left + 8, right_leg_y, legs + 1, palette);
}

void hide_player_sprite(void)
{
    unsigned char i;
    for (i = 24; i < 30; ++i) oam_buf[(unsigned int)i * 4] = 0xFF;
}
