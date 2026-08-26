#ifndef V2_SCENE_UI_H
#define V2_SCENE_UI_H

/* These drawing helpers are intentionally scene-agnostic. They are the first
 * rendering surface extracted from the monolithic scene file and will move to
 * the fixed engine before PRG-banked scenes become callable. */
void reset_scroll(void);
void set_tile(unsigned char row, unsigned char col, unsigned char tile);
void fill_title_rect(unsigned char first_row, unsigned char first_col,
                     unsigned char width, unsigned char height,
                     unsigned char tile);
void fill_row(unsigned char row, unsigned char tile);
void set_attribute_block(unsigned char row, unsigned char col,
                         unsigned char palette);
void draw_text(unsigned char row, unsigned char col, const char *text);
void draw_title_plate(unsigned char top_row, unsigned char left_col,
                      unsigned char width, unsigned char height);
void draw_storefront_sign(void);
void draw_storefront_sign_at(unsigned char top_row, unsigned char left_col,
                             unsigned char width);
void draw_storefront_window(unsigned char top_row, unsigned char left_col);

#endif
