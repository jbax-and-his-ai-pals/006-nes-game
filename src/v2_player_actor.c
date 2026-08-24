#include "v2_player_actor.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif
#include "v2_runtime.h"

#define PLAYER_TILE_BASE 212

void draw_player_sprite(unsigned char x, unsigned char y, unsigned char walking)
{
    unsigned char left = x - 8;
    unsigned char top = y - 24;
    unsigned char legs = (unsigned char)(PLAYER_TILE_BASE + (walking ? 6 : 4));

    set_oam_sprite(24, left, top, PLAYER_TILE_BASE, 1);
    set_oam_sprite(25, left + 8, top, PLAYER_TILE_BASE + 1, 1);
    set_oam_sprite(26, left, top + 8, PLAYER_TILE_BASE + 2, 1);
    set_oam_sprite(27, left + 8, top + 8, PLAYER_TILE_BASE + 3, 1);
    set_oam_sprite(28, left, top + 16, legs, 1);
    set_oam_sprite(29, left + 8, top + 16, legs + 1, 1);
}

void hide_player_sprite(void)
{
    unsigned char i;
    for (i = 24; i < 30; ++i) oam_buf[(unsigned int)i * 4] = 0xFF;
}
