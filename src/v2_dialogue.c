#include <nes.h>

#include "font.h"
#include "v2_dialogue.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif
#include "v2_runtime.h"
#include "v2_scene_ui.h"
#include "v2_vram_queue.h"

#define FONT_TILE_BASE 8
#define TILE_BORDER_H 52
#define TILE_BORDER_V 53
#define MSG_BOX_TOP 16
#define MSG_BOX_LEFT 4
#define MSG_BOX_WIDTH 24
#define MSG_BOX_HEIGHT 8
#define MSG_BOX_RIGHT ((MSG_BOX_LEFT + MSG_BOX_WIDTH) * 8)
#define MSG_BOX_BOTTOM ((MSG_BOX_TOP + MSG_BOX_HEIGHT) * 8)

static unsigned char message_palette = 3;

void dialogue_set_palette(unsigned char palette)
{
    message_palette = palette & 3;
}

unsigned char dialogue_get_palette(void)
{
    return message_palette;
}

static void hide_sprites_under_message_box(void)
{
    unsigned char i;

    for (i = 0; i < 64; ++i) {
        unsigned int offset = (unsigned int)i * 4;
        unsigned char y = oam_buf[offset];
        unsigned char x = oam_buf[offset + 3];

        if (y != 0xFF && x < MSG_BOX_RIGHT && y < MSG_BOX_BOTTOM &&
            (unsigned int)x + 8 > MSG_BOX_LEFT * 8 &&
            (unsigned int)y + 8 > MSG_BOX_TOP * 8) {
            oam_buf[offset] = 0xFF;
        }
    }
}

static void queue_message_panel_row(unsigned char row)
{
    unsigned char col;
    unsigned char tile;

    for (col = MSG_BOX_LEFT; col < MSG_BOX_LEFT + MSG_BOX_WIDTH; ++col) {
        tile = FONT_TILE_BASE;
        if ((row == MSG_BOX_TOP || row == MSG_BOX_TOP + MSG_BOX_HEIGHT - 1) &&
            col > MSG_BOX_LEFT && col < MSG_BOX_LEFT + MSG_BOX_WIDTH - 1) {
            tile = TILE_BORDER_H;
        } else if (row > MSG_BOX_TOP && row < MSG_BOX_TOP + MSG_BOX_HEIGHT - 1 &&
                   (col == MSG_BOX_LEFT || col == MSG_BOX_LEFT + MSG_BOX_WIDTH - 1)) {
            tile = TILE_BORDER_V;
        }
        (void)vram_queue_batch_tile(row, col, tile);
    }

    if (row == MSG_BOX_TOP || row == MSG_BOX_TOP + 4) {
        unsigned char attr_row = (unsigned char)(row >> 2);
        for (col = 1; col < 7; ++col) {
            (void)vram_queue_batch_address((unsigned int)0x23C0 +
                                            (unsigned int)attr_row * 8 + col,
                                            (unsigned char)(message_palette * 0x55));
        }
    }
    (void)vram_queue_batch_commit();
}

void show_message_box(const char *line1, const char *line2, const char *line3)
{
    unsigned char r, pad;
    unsigned char release_frames;
    unsigned char text_row = 18;
    unsigned char text_col = 6;
    unsigned char text_index = 0;
    const char *text = line1;

    /* Dialogue always uses the shared inventory palette. Scene redraws after
     * dismissal restore their own palette assignments. */
    PPU.mask = 0;
    PPU.vram.address = 0x3F;
    PPU.vram.address = (unsigned char)(message_palette << 2);
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x2A;
    PPU.vram.data = 0x01;
    PPU.vram.data = 0x30;
    reset_scroll();
    PPU.mask = 0x1E;

    hide_sprites_under_message_box();
    while (vram_queue_count) {
        waitvsync();
        oam_flush();
        reset_scroll();
    }
    waitvsync();
    oam_flush();
    reset_scroll();
    for (r = MSG_BOX_TOP; r < MSG_BOX_TOP + MSG_BOX_HEIGHT; ++r) {
        while (vram_queue_count) {
            waitvsync();
            oam_flush();
            reset_scroll();
        }
        (void)vram_queue_batch_begin();
        queue_message_panel_row(r);
        waitvsync();
        oam_flush();
        reset_scroll();
    }

    while (text) {
        waitvsync();
        oam_flush();
        reset_scroll();
        if (text[text_index]) {
            if (!vram_queue_count) {
                (void)vram_queue_tile(text_row, (unsigned char)(text_col + text_index),
                                      (unsigned char)(FONT_TILE_BASE + font_glyph_index(text[text_index])));
                ++text_index;
            }
        } else if (text == line1 && line2) {
            text = line2;
            text_row = 20;
            text_index = 0;
        } else if (text == line2 && line3) {
            text = line3;
            text_row = 22;
            text_index = 0;
        } else {
            text = 0;
        }
    }

    while (vram_queue_count) {
        waitvsync();
        oam_flush();
        reset_scroll();
    }

    release_frames = 0;
    while (release_frames < 10) {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();
        if (pad & PAD_A) {
            release_frames = 0;
        } else {
            ++release_frames;
        }
    }
    do {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();
    } while (!(pad & PAD_A));
}
