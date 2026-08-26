#include <nes.h>

#include "mmc3.h"
#include "music.h"
#include "rng.h"
#include "v2_inventory.h"
#include "v2_player_actor.h"
#include "v2_runtime.h"
#include "v2_scene_commentarium.h"
#include "v2_scene_ui.h"
#include "dumb_comments_data.h"

#define FONT_TILE_BASE 8
#define TILE_BORDER_H 52
#define TILE_BORDER_V 53

#define PLAYER_TILE_BASE 212
#define BAR_TILE_FLOOR 235
#define BAR_TILE_WALL 236
#define BAR_TILE_COUNTER 237
#define BAR_TILE_SHELF 238
#define BAR_TILE_STOOL 243
#define BAR_TILE_DOOR_JAMB 244
#define BAR_TILE_DOOR_FILL 245

#define COMMENTER_OAM_SLOT 32
#define COMMENTER_OAM_SLOT_COUNT 6
#define COMMENTER_STAGE_Y 80
#define COMMENT_DISPLAY_FRAMES 240
#define COMMENT_GAP_FRAMES 165
#define MIC_OAM_SLOT 18
#define MIC_OAM_SLOT_COUNT 4

#define ACT_ENTER 0
#define ACT_SPEAK 1
#define ACT_EXIT 2
#define ACT_GAP 3

static const unsigned char commentarium_palette[32] = {
    0x0F, 0x01, 0x11, 0x21,
    0x0F, 0x06, 0x16, 0x26,
    0x0F, 0x08, 0x18, 0x28,
    /* Shared dialogue panel: green rules around the standard deep-blue fill. */
    0x0F, 0x2A, 0x01, 0x30,
    0x0F, 0x16, 0x27, 0x30,
    /* Sprite palette 1 is the player in forward/side poses. */
    0x0F, 0x0F, 0x27, 0x30,
    /* Commenters can vary without inheriting the player palette. */
    0x0F, 0x07, 0x17, 0x27,
    /* Palette 3 collapses the front art into a mostly black back-facing
     * silhouette while retaining the established white back and shoe accents. */
    0x0F, 0x0F, 0x0F, 0x30
};

static void set_scene_attributes(void) {
    unsigned char row;
    unsigned char col;

    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            set_attribute_block(row, col, 0);
        }
    }

    /* Curtain and stage palettes are confined to the upper theater area. */
    for (row = 0; row < 3; ++row) {
        set_attribute_block(row, 0, 1);
        set_attribute_block(row, 7, 1);
        for (col = 1; col < 7; ++col) {
            set_attribute_block(row, col, 2);
        }
    }
}

static void set_lounge_attributes(void) {
    unsigned char row;
    unsigned char col;

    for (row = 3; row < 6; ++row) {
        for (col = 1; col < 7; ++col) {
            set_attribute_block(row, col, 0);
        }
    }
}

static void draw_lounge_seating(void) {
    unsigned char row;

    PPU.mask = 0;
    for (row = 13; row < 26; ++row) {
        fill_row(row, BAR_TILE_FLOOR);
    }
    /* Three shallow rows of tables make the lower half read as an audience. */
    fill_title_rect(15, 4, 7, 1, BAR_TILE_COUNTER);
    fill_title_rect(15, 22, 6, 1, BAR_TILE_COUNTER);
    fill_title_rect(16, 5, 5, 1, BAR_TILE_STOOL);
    fill_title_rect(16, 23, 4, 1, BAR_TILE_STOOL);
    fill_title_rect(19, 7, 18, 1, BAR_TILE_COUNTER);
    fill_title_rect(20, 8, 16, 1, BAR_TILE_STOOL);
    fill_title_rect(23, 4, 7, 1, BAR_TILE_COUNTER);
    fill_title_rect(23, 21, 7, 1, BAR_TILE_COUNTER);
    fill_title_rect(24, 5, 5, 1, BAR_TILE_STOOL);
    fill_title_rect(24, 22, 5, 1, BAR_TILE_STOOL);
    set_lounge_attributes();
    reset_scroll();
    PPU.mask = 0x1E;
}

static void draw_comment_panel(const DumbComment *comment) {
    unsigned char row;

    PPU.mask = 0;
    for (row = 14; row < 23; ++row) {
        fill_title_rect(row, 4, 24, 1, FONT_TILE_BASE);
    }
    fill_title_rect(13, 4, 24, 1, TILE_BORDER_H);
    fill_title_rect(23, 4, 24, 1, TILE_BORDER_H);
    for (row = 14; row < 23; ++row) {
        set_tile(row, 4, TILE_BORDER_V);
        set_tile(row, 27, TILE_BORDER_V);
    }
    draw_text(16, 6, comment->line1);
    draw_text(18, 6, comment->line2);
    draw_text(20, 6, comment->line3);
    for (row = 3; row < 6; ++row) {
        set_attribute_block(row, 1, 3);
        set_attribute_block(row, 2, 3);
        set_attribute_block(row, 3, 3);
        set_attribute_block(row, 4, 3);
        set_attribute_block(row, 5, 3);
        set_attribute_block(row, 6, 3);
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

static void show_commentarium(void) {
    unsigned char row;

    PPU.mask = 0;
    mmc3_use_common_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x00;
    for (row = 0; row < sizeof(commentarium_palette); ++row) {
        PPU.vram.data = commentarium_palette[row];
    }

    for (row = 0; row < 30; ++row) {
        fill_row(row, BAR_TILE_FLOOR);
    }

    fill_title_rect(0, 0, 32, 3, BAR_TILE_WALL);
    draw_text(1, 10, "COMMENTARIUM");

    /* The upper third is a dedicated raised stage, not another bar counter. */
    for (row = 3; row < 10; ++row) {
        fill_title_rect(row, 0, 5, 1, BAR_TILE_WALL);
        fill_title_rect(row, 27, 5, 1, BAR_TILE_WALL);
        fill_title_rect(row, 5, 22, 1, BAR_TILE_SHELF);
    }
    fill_title_rect(9, 4, 24, 1, BAR_TILE_COUNTER);
    fill_title_rect(10, 4, 24, 1, BAR_TILE_COUNTER);
    fill_title_rect(11, 4, 24, 1, BAR_TILE_SHELF);
    set_tile(7, 15, BAR_TILE_STOOL);
    set_tile(8, 15, TILE_BORDER_V);
    set_tile(9, 15, TILE_BORDER_V);
    set_tile(10, 15, TILE_BORDER_V);

    /* A clearly marked south exit returns to Jimbles's Bar. */
    for (row = 26; row < 30; ++row) {
        set_tile(row, 14, BAR_TILE_DOOR_JAMB);
        set_tile(row, 15, BAR_TILE_DOOR_FILL);
        set_tile(row, 16, BAR_TILE_DOOR_JAMB);
    }

    set_scene_attributes();
    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
    draw_lounge_seating();
}

static void draw_commenter(unsigned char x, unsigned char y, unsigned char variant, unsigned char walk_frame, unsigned char facing) {
    unsigned char attr = (unsigned char)(1 + (variant & 1));
    unsigned char left = (unsigned char)(x - 8);
    unsigned char top = (unsigned char)(y - 24);

    if (facing == PLAYER_FACING_RIGHT) {
        /* Keep a single side-facing silhouette while the performer crosses stage. */
        set_oam_sprite(COMMENTER_OAM_SLOT, (unsigned char)(left + 4), top, (unsigned char)(PLAYER_TILE_BASE + 6), attr);
        hide_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 1));
        set_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 2), (unsigned char)(left + 4), (unsigned char)(top + 8), (unsigned char)(PLAYER_TILE_BASE + 7), attr);
        hide_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 3));
        set_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 4), (unsigned char)(left + 3), (unsigned char)(top + 16), (unsigned char)(PLAYER_TILE_BASE + 4), attr);
        set_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 5), (unsigned char)(left + 5 + walk_frame), (unsigned char)(top + 16), (unsigned char)(PLAYER_TILE_BASE + 5), attr);
        return;
    }

    set_oam_sprite(COMMENTER_OAM_SLOT, left, top, PLAYER_TILE_BASE, attr);
    set_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 1), x, top, (unsigned char)(PLAYER_TILE_BASE + 1), attr);
    set_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 2), left, (unsigned char)(top + 8), (unsigned char)(PLAYER_TILE_BASE + 2), attr);
    set_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 3), x, (unsigned char)(top + 8), (unsigned char)(PLAYER_TILE_BASE + 3), attr);
    hide_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 4));
    hide_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + 5));
}

static void draw_microphone(void) {
    /* Lower OAM slots have priority, keeping the microphone in front of a
     * performer as they cross its exact center point. */
    set_oam_sprite(MIC_OAM_SLOT, 120, 56, BAR_TILE_STOOL, 2);
    set_oam_sprite((unsigned char)(MIC_OAM_SLOT + 1), 120, 64, TILE_BORDER_V, 2);
    set_oam_sprite((unsigned char)(MIC_OAM_SLOT + 2), 120, 72, TILE_BORDER_V, 2);
    set_oam_sprite((unsigned char)(MIC_OAM_SLOT + 3), 120, 80, TILE_BORDER_V, 2);
}

static void hide_commenter(void) {
    unsigned char i;

    for (i = 0; i < COMMENTER_OAM_SLOT_COUNT; ++i) {
        hide_oam_sprite((unsigned char)(COMMENTER_OAM_SLOT + i));
    }
}

void run_commentarium_scene(void) {
    unsigned char pad;
    unsigned char player_x = 120;
    unsigned char player_y = 208;
    unsigned char facing = PLAYER_FACING_DOWN;
    unsigned char walk_frame = 0;
    unsigned char walk_timer = 0;
    unsigned char actor_x = 8;
    unsigned char actor_state = ACT_ENTER;
    unsigned char actor_timer = 0;
    unsigned char actor_frame = 0;
    unsigned char actor_variant = 0;
    unsigned char comment_index;
    unsigned char comment_order[DUMB_COMMENT_COUNT];
    unsigned char comment_cursor = 0;
    unsigned char i;
    RngState comment_rng;

    music_select_song(MUSIC_SONG_BAR);
    rng_seed(&comment_rng, random_seed());
    for (i = 0; i < DUMB_COMMENT_COUNT; ++i) {
        comment_order[i] = i;
    }
    for (i = (unsigned char)(DUMB_COMMENT_COUNT - 1); i != 0; --i) {
        unsigned char pick = (unsigned char)(rng_next(&comment_rng) % (i + 1));
        unsigned char swap = comment_order[i];
        comment_order[i] = comment_order[pick];
        comment_order[pick] = swap;
    }
    comment_index = comment_order[comment_cursor++];
    show_commentarium();

    for (;;) {
        unsigned char moving = 0;

        waitvsync();
        reset_scroll();
        oam_flush();
        pad = read_pad();

        if (pad & PAD_START) {
            run_inventory();
            show_commentarium();
            continue;
        }

        if (pad & PAD_LEFT) {
            if (player_x > 12) {
                --player_x;
            }
            facing = PLAYER_FACING_LEFT;
            moving = 1;
        } else if (pad & PAD_RIGHT) {
            if (player_x < 244) {
                ++player_x;
            }
            facing = PLAYER_FACING_RIGHT;
            moving = 1;
        } else if (pad & PAD_UP) {
            if (player_y > 112) {
                --player_y;
            }
            facing = PLAYER_FACING_UP;
            moving = 1;
        } else if (pad & PAD_DOWN) {
            if (player_y < 220) {
                ++player_y;
            } else if (player_x >= 104 && player_x <= 152) {
                return;
            }
            facing = PLAYER_FACING_DOWN;
            moving = 1;
        }

        if (moving) {
            if (++walk_timer == 8) {
                walk_timer = 0;
                walk_frame ^= 1;
            }
        } else {
            walk_timer = 0;
            walk_frame = 0;
        }

        switch (actor_state) {
            case ACT_ENTER:
                if ((actor_timer++ & 1) == 0) {
                    actor_x = (unsigned char)(actor_x + 2);
                    actor_frame ^= 1;
                }
                if (actor_x >= 120) {
                    actor_x = 120;
                    actor_timer = COMMENT_DISPLAY_FRAMES;
                    actor_state = ACT_SPEAK;
                    draw_comment_panel(&dumb_comments[comment_index]);
                }
                draw_commenter(actor_x, COMMENTER_STAGE_Y, actor_variant, actor_frame, PLAYER_FACING_RIGHT);
                break;

            case ACT_SPEAK:
                if (actor_timer != 0) {
                    --actor_timer;
                    draw_commenter(actor_x, COMMENTER_STAGE_Y, actor_variant, 0, PLAYER_FACING_DOWN);
                } else {
                    actor_state = ACT_EXIT;
                    actor_timer = 0;
                    /* Restore the audience the instant the speaker leaves the mic. */
                    draw_lounge_seating();
                    draw_commenter(actor_x, COMMENTER_STAGE_Y, actor_variant, 0, PLAYER_FACING_RIGHT);
                }
                break;

            case ACT_EXIT:
                if ((actor_timer++ & 1) == 0) {
                    actor_x = (unsigned char)(actor_x + 2);
                    actor_frame ^= 1;
                }
                if (actor_x >= 248) {
                    actor_state = ACT_GAP;
                    actor_timer = COMMENT_GAP_FRAMES;
                    hide_commenter();
                } else {
                    draw_commenter(actor_x, COMMENTER_STAGE_Y, actor_variant, actor_frame, PLAYER_FACING_RIGHT);
                }
                break;

            default:
                if (actor_timer != 0) {
                    --actor_timer;
                } else {
                    if (comment_cursor >= DUMB_COMMENT_COUNT) {
                        for (i = 0; i < DUMB_COMMENT_COUNT; ++i) {
                            comment_order[i] = i;
                        }
                        for (i = (unsigned char)(DUMB_COMMENT_COUNT - 1); i != 0; --i) {
                            unsigned char pick = (unsigned char)(rng_next(&comment_rng) % (i + 1));
                            unsigned char swap = comment_order[i];
                            comment_order[i] = comment_order[pick];
                            comment_order[pick] = swap;
                        }
                        comment_cursor = 0;
                    }
                    comment_index = comment_order[comment_cursor++];
                    actor_variant ^= 1;
                    actor_x = 8;
                    actor_timer = 0;
                    actor_state = ACT_ENTER;
                }
                hide_commenter();
                break;
        }

        draw_player_sprite(player_x, player_y, walk_frame, facing);
        draw_microphone();
    }
}
