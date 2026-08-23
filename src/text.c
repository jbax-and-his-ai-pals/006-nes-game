#include <nes.h>

#include "text.h"
#include "font.h"

static void set_vram_addr(unsigned int addr)
{
    PPU.vram.address = (unsigned char)(addr >> 8);
    PPU.vram.address = (unsigned char)(addr & 0xFF);
}

void fill_row(unsigned char row, unsigned char tile)
{
    unsigned char col;

    set_vram_addr(0x2000 + (unsigned int)row * NAMETABLE_WIDTH_TILES);
    for (col = 0; col < NAMETABLE_WIDTH_TILES; ++col) {
        PPU.vram.data = tile;
    }
}

void set_tile(unsigned char row, unsigned char col, unsigned char tile)
{
    set_vram_addr(0x2000 + (unsigned int)row * NAMETABLE_WIDTH_TILES + col);
    PPU.vram.data = tile;
}

void draw_text(unsigned char row, unsigned char col, unsigned char tile_base, const char *str)
{
    set_vram_addr(0x2000 + (unsigned int)row * NAMETABLE_WIDTH_TILES + col);
    while (*str) {
        PPU.vram.data = (unsigned char)(tile_base + font_glyph_index(*str));
        ++str;
    }
}

void set_attribute_row(unsigned char attr_row, unsigned char palette)
{
    /* Each attribute byte packs 4 quadrants (2 bits each) covering a
     * 16x16-pixel block; setting all 4 quadrants the same makes the
     * whole block select one palette uniformly. */
    unsigned char value = (unsigned char)(palette | (palette << 2) | (palette << 4) | (palette << 6));
    unsigned char i;

    /* Attribute table follows its nametable at a fixed offset; 8 bytes
     * per attribute-row (32 tile columns / 4 tiles-per-byte). */
    set_vram_addr(0x23C0 + (unsigned int)attr_row * 8);
    for (i = 0; i < 8; ++i) {
        PPU.vram.data = value;
    }
}

void set_attribute_block(unsigned char attr_row, unsigned char attr_col, unsigned char palette)
{
    unsigned char value = (unsigned char)(palette | (palette << 2) | (palette << 4) | (palette << 6));

    set_vram_addr(0x23C0 + (unsigned int)attr_row * 8 + attr_col);
    PPU.vram.data = value;
}
