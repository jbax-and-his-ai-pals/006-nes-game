#include <nes.h>

#include "font.h"
#include "v2_dialogue.h"
#include "v2_game_state.h"
#include "v2_input.h"
#include "v2_inventory.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif
#include "v2_runtime.h"
#include "v2_scene_ui.h"

#define FONT_TILE_BASE 8
#define TILE_BORDER_V 53
#define BAR_TILE_WALL 236

enum {
    INVENTORY_ITEM_PAPER_CLIP,
    INVENTORY_ITEM_BEER,
    INVENTORY_ITEM_TOKEN,
    INVENTORY_ITEM_SEWER_KEY,
    INVENTORY_ITEM_FLAMETHROWER,
    INVENTORY_ITEM_ZOO_MAP
};

static unsigned char inventory_item_count(void)
{
    unsigned char count = 0;

    if (game_state.has_paper_clip) ++count;
    if (game_state.has_beer) ++count;
    if (game_state.has_token) ++count;
    if (game_state.has_sewer_key) ++count;
    if (game_state.has_flamethrower) ++count;
    if (game_state.has_zoo_map) ++count;
    return count;
}

static unsigned char inventory_item_at(unsigned char index)
{
    if (game_state.has_paper_clip) {
        if (index == 0) return INVENTORY_ITEM_PAPER_CLIP;
        --index;
    }
    if (game_state.has_beer) {
        if (index == 0) return INVENTORY_ITEM_BEER;
        --index;
    }
    if (game_state.has_token) {
        if (index == 0) return INVENTORY_ITEM_TOKEN;
        --index;
    }
    if (game_state.has_sewer_key) {
        if (index == 0) return INVENTORY_ITEM_SEWER_KEY;
        --index;
    }
    if (game_state.has_flamethrower) {
        if (index == 0) return INVENTORY_ITEM_FLAMETHROWER;
        --index;
    }
    return INVENTORY_ITEM_ZOO_MAP;
}

static const char *inventory_item_name(unsigned char item)
{
    if (item == INVENTORY_ITEM_PAPER_CLIP) return "PAPER CLIP";
    if (item == INVENTORY_ITEM_BEER) return "JIMBLES'S BEER";
    if (item == INVENTORY_ITEM_TOKEN) return "JUKEBOX TOKEN";
    if (item == INVENTORY_ITEM_SEWER_KEY) return "SEWER KEY";
    if (item == INVENTORY_ITEM_FLAMETHROWER) return "FLAMETHROWER";
    return "ZOO MAP";
}

static void show_inventory_description(unsigned char item)
{
    if (item == INVENTORY_ITEM_PAPER_CLIP) {
        show_message_box("A BENT PAPER CLIP.", "IT COULD BE USEFUL.", 0);
    } else if (item == INVENTORY_ITEM_BEER) {
        show_message_box("JIMBLES'S HOUSE BEER.", "WARM, BUT FREE.", 0);
    } else if (item == INVENTORY_ITEM_TOKEN) {
        show_message_box("A JUKEBOX TOKEN.", "GOOD FOR ONE SONG.", 0);
    } else if (item == INVENTORY_ITEM_SEWER_KEY) {
        show_message_box("A RUSTY SEWER KEY.", "SMELLS LIKE A DRAIN.", 0);
    } else if (item == INVENTORY_ITEM_FLAMETHROWER) {
        show_message_box("A VERY REAL", "FLAMETHROWER.", "FOR VERY REAL SLUGS.");
    } else {
        show_message_box("A POCKET ZOO MAP.", "IT HAS A DUCK", "CIRCLED IN RED.");
    }
}

static void show_inventory_screen(unsigned char selected)
{
    unsigned char row, col;
    unsigned char count = inventory_item_count();

    PPU.mask = 0;
    fill_title_rect(0, 0, 32, 30, BAR_TILE_WALL);
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x04;
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x2A;
    PPU.vram.data = 0x01;
    PPU.vram.data = 0x30;
    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) set_attribute_block(row, col, 1);
    }
    draw_title_plate(2, 2, 28, 26);
    draw_text(5, 11, "INVENTORY");
    if (count == 0) {
        draw_text(12, 5, "YOUR POCKETS ARE EMPTY.");
    } else {
        for (row = 0; row < count; ++row) {
            unsigned char item_row = (unsigned char)(8 + row * 3);
            set_tile(item_row, 5, row == selected ? TILE_BORDER_V : FONT_TILE_BASE);
            draw_text(item_row, 8, inventory_item_name(inventory_item_at(row)));
        }
    }
    draw_text(22, 7, "A: DESCRIBE");
    draw_text(24, 5, "START OR B: CLOSE");
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

static void update_inventory_selector(unsigned char old_selected,
                                      unsigned char new_selected)
{
    PPU.mask = 0;
    set_tile((unsigned char)(8 + old_selected * 3), 5, FONT_TILE_BASE);
    set_tile((unsigned char)(8 + new_selected * 3), 5, TILE_BORDER_V);
    reset_scroll();
    PPU.mask = 0x1E;
}

static void wait_for_inventory_close_release(void)
{
    unsigned char released = 0;

    while (released < DEBOUNCE_RELEASE_FRAMES) {
        waitvsync();
        oam_flush();
        reset_scroll();
        if (read_pad() & (PAD_START | PAD_B)) {
            released = 0;
        } else {
            ++released;
        }
    }
}

void run_inventory(void)
{
    unsigned char selected = 0;
    unsigned char count = inventory_item_count();
    unsigned char pad;
    unsigned char start_held = 1, start_run = 0;
    unsigned char b_held = 0, b_run = 0;
    unsigned char a_held = 0, a_run = 0;
    unsigned char up_held = 0, up_run = 0;
    unsigned char down_held = 0, down_run = 0;
    unsigned char left_held = 0, left_run = 0;
    unsigned char right_held = 0, right_run = 0;
    unsigned char up_press, down_press, left_press, right_press;

    show_inventory_screen(selected);
    for (;;) {
        waitvsync();
        oam_flush();
        reset_scroll();
        pad = read_pad();

        if (debounce_press((unsigned char)(pad & PAD_START), &start_held, &start_run) ||
            debounce_press((unsigned char)(pad & PAD_B), &b_held, &b_run)) {
            wait_for_inventory_close_release();
            return;
        }

        up_press = debounce_press((unsigned char)(pad & PAD_UP), &up_held, &up_run);
        down_press = debounce_press((unsigned char)(pad & PAD_DOWN), &down_held, &down_run);
        left_press = debounce_press((unsigned char)(pad & PAD_LEFT), &left_held, &left_run);
        right_press = debounce_press((unsigned char)(pad & PAD_RIGHT), &right_held, &right_run);
        if (count > 1 && (up_press || left_press)) {
            unsigned char old_selected = selected;
            selected = selected ? (unsigned char)(selected - 1) : (unsigned char)(count - 1);
            update_inventory_selector(old_selected, selected);
        } else if (count > 1 && (down_press || right_press)) {
            unsigned char old_selected = selected;
            selected = (unsigned char)((selected + 1) % count);
            update_inventory_selector(old_selected, selected);
        }

        if (count && debounce_press((unsigned char)(pad & PAD_A), &a_held, &a_run)) {
            show_inventory_description(inventory_item_at(selected));
            show_inventory_screen(selected);
        }
    }
}
