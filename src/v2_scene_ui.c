#include <nes.h>

#include "font.h"
#include "v2_scene_ui.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

#define FONT_TILE_BASE 8
#define TILE_BORDER_H 52
#define TILE_BORDER_V 53

void reset_scroll(void)
{
    /* $2005 and $2006 share a write latch. Every scene tile update uses
     * $2006, so reset that latch before restoring the camera. */
    (void)PPU.status;
    PPU.scroll = 0;
    PPU.scroll = 0;
}

void set_tile(unsigned char row, unsigned char col, unsigned char tile)
{
    PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
    PPU.vram.address = (unsigned char)((row << 5) + col);
    PPU.vram.data = tile;
}

void fill_title_rect(unsigned char first_row, unsigned char first_col,
                     unsigned char width, unsigned char height,
                     unsigned char tile)
{
    unsigned char row, col;

    for (row = first_row; row < first_row + height; ++row) {
        PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
        PPU.vram.address = (unsigned char)((row << 5) + first_col);
        for (col = 0; col < width; ++col) PPU.vram.data = tile;
    }
}

void fill_row(unsigned char row, unsigned char tile)
{
    fill_title_rect(row, 0, 32, 1, tile);
}

void set_attribute_block(unsigned char row, unsigned char col,
                         unsigned char palette)
{
    PPU.vram.address = 0x23;
    PPU.vram.address = (unsigned char)(0xC0 + row * 8 + col);
    PPU.vram.data = (unsigned char)(palette * 0x55);
}

void draw_text(unsigned char row, unsigned char col, const char *text)
{
    PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
    PPU.vram.address = (unsigned char)((row << 5) + col);
    while (*text) PPU.vram.data = (unsigned char)(FONT_TILE_BASE + font_glyph_index(*text++));
}

void draw_title_plate(unsigned char top_row, unsigned char left_col,
                      unsigned char width, unsigned char height)
{
    unsigned char row;

    fill_title_rect(top_row, left_col, width, height, FONT_TILE_BASE);
    fill_title_rect(top_row, left_col, width, 1, TILE_BORDER_H);
    fill_title_rect(top_row + height - 1, left_col, width, 1, TILE_BORDER_H);
    for (row = top_row + 1; row < top_row + height - 1; ++row) {
        set_tile(row, left_col, TILE_BORDER_V);
        set_tile(row, left_col + width - 1, TILE_BORDER_V);
    }
}

void draw_storefront_sign(void)
{
    /* Tile 204 used to be the plate's own dedicated "fill" tile, but it
     * was byte-for-byte identical to FONT_TILE_BASE (both are just solid
     * index 2) -- that slot now holds part of the casino lobby's
     * blackjack table art instead (see neon_avenue.s), so the fill here
     * uses FONT_TILE_BASE directly. Same pixels either way. */
    set_tile(8, 8, 200); fill_title_rect(8, 9, 14, 1, 201); set_tile(8, 23, 202);
    set_tile(9, 8, 203); fill_title_rect(9, 9, 14, 1, FONT_TILE_BASE); set_tile(9, 23, 205);
    set_tile(10, 8, 203); fill_title_rect(10, 9, 14, 1, FONT_TILE_BASE); set_tile(10, 23, 205);
    set_tile(11, 8, 206); fill_title_rect(11, 9, 14, 1, 207); set_tile(11, 23, 208);
}

void draw_storefront_window(unsigned char top_row, unsigned char left_col)
{
    unsigned char row;
    fill_title_rect(top_row, left_col, 4, 1, 209);
    fill_title_rect(top_row + 3, left_col, 4, 1, 209);
    for (row = top_row + 1; row < top_row + 3; ++row) {
        set_tile(row, left_col, 210);
        fill_title_rect(row, left_col + 1, 2, 1, 211);
        set_tile(row, left_col + 3, 210);
    }
}
