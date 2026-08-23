/*
 * text.h -- low-level nametable text/attribute drawing helpers.
 *
 * Unlike movement.c/room.c/font.c, this module touches PPU hardware
 * directly (it has to, to draw into the nametable), so it isn't part of
 * the hardware-independent/testable set. It's deliberately free of
 * game-specific concepts (rooms, biomes) so it stays reusable -- the
 * caller decides what a row means outside of a text box.
 *
 * Every function here must run with rendering off (PPU.mask's show
 * bits clear), the same discipline as main.c's draw_room(): writing to
 * $2007 outside vblank/forced-blank corrupts whatever's on screen.
 */

#ifndef TEXT_H
#define TEXT_H

#define NAMETABLE_WIDTH_TILES 32

/* Fills one full nametable row (32 tiles) with a single tile index. */
void fill_row(unsigned char row, unsigned char tile);

/* Writes a single tile at (row, col). */
void set_tile(unsigned char row, unsigned char col, unsigned char tile);

/* Draws a string starting at (row, col), one glyph tile per character
 * via font_glyph_index() (font.h) offset by tile_base. Does not wrap or
 * clip -- the caller keeps col + strlen(str) within the 32-tile row. */
void draw_text(unsigned char row, unsigned char col, unsigned char tile_base, const char *str);

/* Sets every attribute byte covering attribute-row attr_row (0-7, each
 * covering 4 tile-rows) so all 4 quadrants of each 16x16 block in it
 * select background palette `palette` (0-3). */
void set_attribute_row(unsigned char attr_row, unsigned char palette);

/* Sets one 4x4-tile attribute-table block (attr_row/attr_col are 0-7). */
void set_attribute_block(unsigned char attr_row, unsigned char attr_col, unsigned char palette);

#endif
