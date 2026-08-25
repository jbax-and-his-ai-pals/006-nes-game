#include <nes.h>

#include "v2_dialogue.h"
#include "v2_runtime.h"
#include "v2_scene_ui.h"
#include "v2_traffic.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

#define PLAYER_TILE_BASE 212
#define TAXI_TILE_BASE 220
#define TAXI_PARTICLE_TILE 232
#define TAXI_PARTICLE_SMALL_TILE 233
#define TAXI_PARTICLE_DOT_TILE 234
#define TRAFFIC_BONE_TILE_BASE 249
#define PARTICLE_OAM_BASE 48

static unsigned char traffic_crash_oam_base = PARTICLE_OAM_BASE;

static void set_oam_sprite_offset(unsigned char slot, unsigned char x, int dx,
                                  unsigned char y, unsigned char tile,
                                  unsigned char attr)
{
    int px = (int)x + dx;
    if (px < 0) px = 0;
    if (px > 247) px = 247;
    set_oam_sprite(slot, (unsigned char)px, y, tile, attr);
}

void draw_traffic_particles(unsigned char x, unsigned char moving_left)
{
    int direction = moving_left ? -1 : 1;
    unsigned char base = traffic_crash_oam_base;

    set_oam_sprite_offset(base, x, direction * 8, 220, TAXI_PARTICLE_TILE, 2);
    set_oam_sprite_offset(base + 1, x, direction * 24, 216, TAXI_PARTICLE_SMALL_TILE, 2);
    set_oam_sprite_offset(base + 2, x, direction * 38, 228, TAXI_PARTICLE_DOT_TILE, 2);
    set_oam_sprite_offset(base + 11, x, direction * 52, 212, TAXI_PARTICLE_SMALL_TILE, 2);
    set_oam_sprite_offset(base + 12, x, direction * 66, 222, TAXI_PARTICLE_DOT_TILE, 2);
    set_oam_sprite_offset(base + 13, x, direction * 80, 230, TAXI_PARTICLE_SMALL_TILE, 2);
    set_oam_sprite_offset(base + 14, x, direction * 92, 218, TAXI_PARTICLE_DOT_TILE, 2);
}

void draw_crash_tableau(unsigned char x, unsigned char moving_left)
{
    unsigned char base = traffic_crash_oam_base + 3;

    if (moving_left) {
        set_oam_sprite_offset(base, x, -66, 220, TRAFFIC_BONE_TILE_BASE, 3);
        set_oam_sprite_offset(base + 1, x, -58, 220, TRAFFIC_BONE_TILE_BASE + 1, 3);
        set_oam_sprite_offset(base + 2, x, -18, 216, TRAFFIC_BONE_TILE_BASE + 2, 3);
        set_oam_sprite_offset(base + 3, x, -38, 228, TRAFFIC_BONE_TILE_BASE + 3, 3);
        set_oam_sprite_offset(base + 4, x, -8, 224, TRAFFIC_BONE_TILE_BASE + 4, 3);
        set_oam_sprite_offset(base + 5, x, -45, 210, TRAFFIC_BONE_TILE_BASE + 5, 3);
        set_oam_sprite_offset(base + 6, x, -34, 208, PLAYER_TILE_BASE, 1);
        set_oam_sprite_offset(base + 7, x, -26, 208, PLAYER_TILE_BASE + 1, 1);
    } else {
        set_oam_sprite_offset(base, x, 34, 220, TRAFFIC_BONE_TILE_BASE, 3);
        set_oam_sprite_offset(base + 1, x, 42, 220, TRAFFIC_BONE_TILE_BASE + 1, 3);
        set_oam_sprite_offset(base + 2, x, 70, 216, TRAFFIC_BONE_TILE_BASE + 2, 3);
        set_oam_sprite_offset(base + 3, x, 46, 228, TRAFFIC_BONE_TILE_BASE + 3, 3);
        set_oam_sprite_offset(base + 4, x, 76, 224, TRAFFIC_BONE_TILE_BASE + 4, 3);
        set_oam_sprite_offset(base + 5, x, 30, 210, TRAFFIC_BONE_TILE_BASE + 5, 3);
        set_oam_sprite_offset(base + 6, x, 50, 208, PLAYER_TILE_BASE, 1);
        set_oam_sprite_offset(base + 7, x, 58, 208, PLAYER_TILE_BASE + 1, 1);
    }
}

void set_traffic_crash_oam_base(unsigned char first_slot)
{
    traffic_crash_oam_base = first_slot;
}

void draw_taxi_sprite(int x, unsigned char moving_left)
{
    unsigned char row, col;
    unsigned char slot = 32;
    /* Sprite palette 2 carries the cab's yellow body, red accents, and
     * white checker stripe. Palette 3 is reserved for bones/crash art. */
    unsigned char attr = moving_left ? 2 : 0x42;
    unsigned char base_y = 190;

    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 4; ++col) {
            int col_x = x + col * 8;
            if (col_x < 0 || col_x > 247) {
                oam_buf[(unsigned int)slot * 4] = 0xFF;
                ++slot;
            } else {
                set_oam_sprite(slot++, (unsigned char)col_x,
                               (unsigned char)(base_y + row * 8),
                               (unsigned char)(row == 2 ? TAXI_TILE_BASE + 8 + (col & 1)
                                                        : TAXI_TILE_BASE + row * 4 + col),
                               attr);
            }
        }
    }
}

void hide_taxi_sprite(void)
{
    unsigned char i;
    for (i = 32; i < 44; ++i) oam_buf[(unsigned int)i * 4] = 0xFF;
}

static unsigned char centered_text_col(const char *text)
{
    unsigned char length = 0;

    while (text[length]) ++length;
    return (unsigned char)((32 - length) >> 1);
}

void show_traffic_death_message(const char *line1, const char *line2)
{
    unsigned char i;
    unsigned char palette = dialogue_get_palette();

    PPU.mask = 0;
    /* The active dialogue palette only draws inside this aligned plate. */
    PPU.vram.address = 0x3F;
    PPU.vram.address = (unsigned char)(palette << 2);
    PPU.vram.data = 0x0F;
    PPU.vram.data = 0x2A;
    PPU.vram.data = 0x01;
    PPU.vram.data = 0x30;
    draw_title_plate(12, 4, 24, 8);
    for (i = 1; i < 7; ++i) set_attribute_block(3, i, palette);
    for (i = 1; i < 7; ++i) set_attribute_block(4, i, palette);
    draw_text(14, centered_text_col(line1), line1);
    draw_text(16, centered_text_col(line2), line2);
    reset_scroll();
    PPU.mask = 0x1E;
}

void show_traffic_death(void)
{
    show_traffic_death_message("YOU CAN'T JUST WALK", "INTO TRAFFIC.");
}
