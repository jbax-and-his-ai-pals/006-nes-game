#include <nes.h>

#include "v2_runtime.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

#define JOYPAD1 (*(unsigned char volatile *)0x4016)

#pragma bss-name(push, "OAM")
unsigned char oam_buf[256];
#pragma bss-name(pop)

unsigned char read_pad(void)
{
    unsigned char result = 0;
    unsigned char i;

    JOYPAD1 = 1;
    JOYPAD1 = 0;
    for (i = 0; i < 8; ++i) result |= (unsigned char)((JOYPAD1 & 1) << i);
    return result;
}

void clear_sprites(void)
{
    unsigned int i;

    for (i = 0; i < 256; ++i) oam_buf[i] = 0xFF;
}

void set_oam_sprite(unsigned char slot, unsigned char x, unsigned char y,
                    unsigned char tile, unsigned char attr)
{
    unsigned char *entry = &oam_buf[(unsigned int)slot * 4];

    entry[0] = y - 1;
    entry[1] = tile;
    entry[2] = attr;
    entry[3] = x;
}

void hide_oam_sprite(unsigned char slot)
{
    oam_buf[(unsigned int)slot * 4] = 0xFF;
}

void oam_flush(void)
{
    /* DMA copies all 256 bytes, but the destination latch still matters. */
    PPU.sprite.address = 0;
    SPRITE_DMA = 0x60;
}
