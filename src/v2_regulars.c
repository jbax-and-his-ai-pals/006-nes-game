#include "rng.h"
#include "v2_regulars.h"
#include "v2_runtime.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

#define PLAYER_TILE_BASE 212
#define PISS_FACE_FEATURE_TILE 2
#define PISS_FACE_OAM_BASE 20
#define BAR_PATRON_BACK_TOP 59
#define BAR_PATRON_BACK_BOTTOM 60

static unsigned char one_x, one_y, two_x, two_y;
static unsigned char one_timer, two_timer, one_steps, two_steps;
static unsigned char one_direction, two_direction, one_facing_up, two_facing_up;
static unsigned char seed = 0x6D;
static RngState rng;

static void update_regular(unsigned char *x, unsigned char *y, unsigned char *timer,
                           unsigned char *steps, unsigned char *direction,
                           unsigned char *facing_up, unsigned char min_x,
                           unsigned char max_x, unsigned char min_y, unsigned char max_y)
{
    unsigned char roll;
    if (*timer) --*timer;
    else if (*steps) {
        switch (*direction) {
            case 0: if (*x < max_x) ++*x; else --*x; *facing_up = 0; break;
            case 1: if (*x > min_x) --*x; else ++*x; *facing_up = 0; break;
            case 2: if (*y < max_y) ++*y; else --*y; *facing_up = 0; break;
            default: if (*y > min_y) { --*y; *facing_up = 1; } else { ++*y; *facing_up = 0; } break;
        }
        --*steps;
        *timer = (unsigned char)(2 + (rng_next(&rng) & 3));
    } else {
        roll = rng_next(&rng);
        *direction = roll & 3;
        *steps = (unsigned char)(2 + ((roll >> 2) & 3));
        *timer = (unsigned char)(45 + (rng_next(&rng) & 0x7F));
    }
}

void reset_piss_corner_regulars(void)
{
    one_x = 200; one_y = 168; two_x = 224; two_y = 184;
    one_timer = 74; two_timer = 141; one_steps = 0; two_steps = 0;
    one_facing_up = 0; two_facing_up = 0;
    seed = (unsigned char)(seed + 0x3D);
    rng_seed(&rng, seed);
}

void update_piss_corner_regulars(void)
{
    update_regular(&one_x, &one_y, &one_timer, &one_steps, &one_direction, &one_facing_up, 200, 216, 168, 184);
    update_regular(&two_x, &two_y, &two_timer, &two_steps, &two_direction, &two_facing_up, 224, 232, 176, 192);
}

void reset_park_regulars(void)
{
    one_x = 72; one_y = 152; two_x = 184; two_y = 168;
    one_timer = 35; two_timer = 96; one_steps = 0; two_steps = 0;
    one_facing_up = 0; two_facing_up = 0;
    rng_seed(&rng, 0xC7);
}

void update_park_regulars(void)
{
    update_regular(&one_x, &one_y, &one_timer, &one_steps, &one_direction, &one_facing_up, 56, 104, 136, 184);
    update_regular(&two_x, &two_y, &two_timer, &two_steps, &two_direction, &two_facing_up, 160, 208, 136, 184);
}

unsigned char park_regular_is_near(unsigned char regular, unsigned char x,
                                   unsigned char y)
{
    unsigned char regular_x = regular ? two_x : one_x;
    unsigned char regular_y = regular ? two_y : one_y;

    return x >= regular_x - 12 && x <= regular_x + 20 &&
           y >= regular_y - 12 && y <= regular_y + 24;
}

static void draw_regular(unsigned char slot, unsigned char face_slot, unsigned char x,
                         unsigned char y, unsigned char facing_up, unsigned char mirrored)
{
    unsigned char attr = mirrored ? 0x41 : 1;
    if (facing_up) {
        set_oam_sprite(slot, x, y, BAR_PATRON_BACK_TOP, 1);
        set_oam_sprite(slot + 1, (unsigned char)(x + 8), y, BAR_PATRON_BACK_TOP, 0x41);
        set_oam_sprite(slot + 2, x, (unsigned char)(y + 8), BAR_PATRON_BACK_BOTTOM, 1);
        set_oam_sprite(slot + 3, (unsigned char)(x + 8), (unsigned char)(y + 8), BAR_PATRON_BACK_BOTTOM, 0x41);
        hide_oam_sprite(face_slot); hide_oam_sprite(face_slot + 1);
    } else {
        set_oam_sprite(slot, x, y, mirrored ? PLAYER_TILE_BASE + 1 : PLAYER_TILE_BASE, attr);
        set_oam_sprite(slot + 1, (unsigned char)(x + 8), y, mirrored ? PLAYER_TILE_BASE : PLAYER_TILE_BASE + 1, attr);
        set_oam_sprite(slot + 2, x, (unsigned char)(y + 8), mirrored ? PLAYER_TILE_BASE + 3 : PLAYER_TILE_BASE + 2, attr);
        set_oam_sprite(slot + 3, (unsigned char)(x + 8), (unsigned char)(y + 8), mirrored ? PLAYER_TILE_BASE + 2 : PLAYER_TILE_BASE + 3, attr);
        set_oam_sprite(face_slot, x, y, PISS_FACE_FEATURE_TILE, 3);
        set_oam_sprite(face_slot + 1, (unsigned char)(x + 8), y, PISS_FACE_FEATURE_TILE, 0x43);
    }
}

void draw_regulars(void)
{
    draw_regular(30, PISS_FACE_OAM_BASE, one_x, one_y, one_facing_up, 0);
    draw_regular(34, PISS_FACE_OAM_BASE + 2, two_x, two_y, two_facing_up, 1);
}

void draw_bar_patron(void)
{
    set_oam_sprite(38, 80, 72, BAR_PATRON_BACK_TOP, 1);
    set_oam_sprite(39, 88, 72, BAR_PATRON_BACK_TOP, 0x41);
    set_oam_sprite(40, 80, 80, BAR_PATRON_BACK_BOTTOM, 1);
    set_oam_sprite(41, 88, 80, BAR_PATRON_BACK_BOTTOM, 0x41);
}
