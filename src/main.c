/*
 * main.c -- a closed loop of 4 biomes, each with one dead-end room
 * branching off it.
 *
 *              [Tundra]
 *                 |
 *   [Swamp] -- [Forest] -- [Snow]
 *                 |           |
 *              [Meadow] -- [Desert]
 *                 |           |
 *              [Plains]    [Canyon]
 *
 * The original 4-room loop (Meadow/Forest/Snow/Desert, walkable
 * clockwise or counterclockwise) is unchanged; Plains/Canyon/Swamp/
 * Tundra each hang off one of those 4 by a single new exit, one per
 * room, so every original room gained exactly one more exit and every
 * new room has exactly one. Every room shares the same room-shape logic
 * (room.c) with its own decorations, obstacle shape, palette, and exit
 * configuration -- each room only has exits where it actually connects
 * to something, using room.h's per-exit open/closed support.
 */

#include <nes.h>
#include <string.h>

#include "movement.h"
#include "room.h"
#include "text.h"
#include "font.h"
#include "music.h"
#include "rng.h"
#include "quest.h"
#include "world.h"
#include "mmc3.h"
#include "scene_casino_data.h"
#include "scene_title_data.h"

/* nes.h's JOYPAD macro casts a constant to an array type, which cc65's
 * compiler rejects as an lvalue. Address controller port 1 directly. */
#define JOYPAD1 (*(unsigned char volatile *)0x4016)

/* CHR-ROM tile indices (see chr.s). Floor/path/player are shared across
 * every room -- only the loaded palette changes how they look. Obstacles
 * get one of two shapes per biome (room_obstacle_tile[] below): the
 * meadow and forest are both "trees" (just recolored), the desert and
 * snow are both "rocks" (also just recolored) -- no new CHR data needed
 * to give all 4 rooms a distinct feel. Font glyphs (font.s) follow
 * immediately after in the same pattern table. */
#define CHR_TILE_FLOOR   0
#define CHR_TILE_PLAYER  1
#define CHR_TILE_TREE    2
#define CHR_TILE_PATH    3
#define CHR_TILE_ROCK    4
/* house.s is linked immediately after chr.s -- the town's 2x2 house
 * cluster needs 3 different tiles (roof-left is chr.s tile 5, above;
 * roof-right and wall are house.s's), not one repeated tile like every
 * other room's obstacle -- see house.s and chr_tile_for() below for how
 * that's picked per-cell without room.c needing to know. */
#define CHR_TILE_HOUSE_ROOF_L 5
#define CHR_TILE_HOUSE_ROOF_R 6
#define CHR_TILE_HOUSE_WALL   7
#define FONT_TILE_BASE   8

/* npc.s is linked immediately after font.s, so the NPC tile lands right
 * after the last font glyph -- no hardcoded tile number to keep in sync
 * if the font ever grows. */
#define NPC_TILE_BASE (FONT_TILE_BASE + FONT_GLYPH_COUNT)
#define NPC_SPRITE_PALETTE 1

/* box_border.s is linked immediately after npc.s, so its 2 tiles land
 * right after the NPC's. */
#define BORDER_TILE_BASE (NPC_TILE_BASE + 1)
#define TILE_BORDER_H BORDER_TILE_BASE
#define TILE_BORDER_V (BORDER_TILE_BASE + 1)

/* item.s is linked immediately after box_border.s. */
#define ITEM_TILE_BASE (BORDER_TILE_BASE + 2)
#define ITEM_SPRITE_PALETTE 2

/* enemy.s is linked immediately after item.s. */
#define ENEMY_TILE_BASE (ITEM_TILE_BASE + 1)

/* Only 4 sprite palettes exist in hardware, already spoken for by role
 * (0 player, 1 NPC normal color, 2 item color, 3 NPC reward color) --
 * ENEMY_SPRITE_PALETTE reuses slot 2's number, same as items do. Safe
 * for now because no room has both an item and an enemy; the day one
 * does, that room needs its own slot-conflict resolution (a per-room
 * remap, most likely), not a bigger PALETTE_COUNT that doesn't exist. */
#define ENEMY_SPRITE_PALETTE 2

/* The NPC's color while the quest is QUEST_COMPLETE -- a distinct
 * "reward granted" tell, separate from its normal color (sprite
 * palette 1) so the player can see the quest resolved without opening
 * the text box. */
#define NPC_REWARD_PALETTE 3

/* How close (in boxes_overlap()'s padding sense) the player needs to be
 * to an NPC for the A button to start dialogue -- a few pixels of slack
 * beyond exact-touching, since box_blocked() already prevents walking
 * fully onto the NPC's tile. */
#define INTERACT_RANGE 4

/* Both buttons that open/close the text box, and how many consecutive
 * released frames are required before a new press is allowed to fire
 * again -- see main()'s talk_release_frames for why this needs to be
 * more than a single frame. */
#define TALK_MASK JOY_BTN_A_MASK
#define TALK_DEBOUNCE_FRAMES 4

#define MENU_DEBOUNCE_FRAMES 4
#define MENU_ITEM_INVENTORY 0
#define MENU_ITEM_QUIT      1
#define MENU_ITEM_COUNT     2

/* Phase 6's first slice: real-time combat, Zelda verbs (B swings,
 * direction comes from movement) with Morrowind-style stat resolution
 * underneath (see roll_hit() below) rather than pure position/hitbox
 * skill. B was free -- A is talk, Start is the room-name toggle.
 * Debounced the same way and for the same reason as TALK_MASK. */
#define ATTACK_MASK JOY_BTN_B_MASK
#define ATTACK_DEBOUNCE_FRAMES 4

/* How far in front of the player (in the direction they're facing) an
 * attack reaches -- one tile, so you have to be genuinely adjacent to
 * an enemy, not swinging from across the room. */
#define ATTACK_REACH 8
#define ATTACK_PAD   2

/* sword.s is linked immediately after enemy.s and has 2 tiles (vertical
 * blade, horizontal blade) -- see its header comment for why 2 covers
 * all 4 facing directions via OAM flip bits instead of needing 4.
 * SWORD_SPRITE_PALETTE reuses NPC_REWARD_PALETTE's slot number (3) for
 * the same reason ENEMY_SPRITE_PALETTE reuses slot 2 -- only 4 sprite
 * palettes exist. Meadow (which has both the enemy and, via the NPC,
 * slot 3 already set to gold) is the only room that matters for this
 * right now; other rooms show the sword in whatever slot 3 already
 * holds. */
#define SWORD_TILE_VERTICAL   (ENEMY_TILE_BASE + 1)
#define SWORD_TILE_HORIZONTAL (ENEMY_TILE_BASE + 2)
#define SWORD_SPRITE_PALETTE 3

/* critter.s is linked immediately after sword.s -- the town's ambient
 * wildlife. Reuses slot 2, same reasoning as ENEMY_SPRITE_PALETTE: safe
 * because no room has both a critter and an item/enemy. */
#define CRITTER_TILE_BASE (SWORD_TILE_HORIZONTAL + 1)
#define CRITTER_SPRITE_PALETTE 2

/* portal.s is linked immediately after critter.s. Reuses slot 3, same
 * reasoning as SWORD_SPRITE_PALETTE -- the sword only ever shows in
 * meadow (which has no portal), and everywhere else slot 3 is either
 * a still-unclaimed reward color or, in town, the same slot the
 * portal shares with nothing else. */
#define PORTAL_TILE_BASE (CRITTER_TILE_BASE + 1)
/* portal.s's second tile -- a hollow doorway instead of a solid house,
 * for a portal that sits recessed into an actual house's corner (see
 * that file's header) rather than standing alone. */
#define PORTAL_DOOR_TILE_BASE (PORTAL_TILE_BASE + 1)
#define PORTAL_SPRITE_PALETTE 3

/* furniture.s is linked immediately after portal.s -- house-interior
 * background tiles, color index 1 (each room's accent color) instead of
 * the walls' index 2, so furniture reads as different from a wall
 * without needing its own palette slot. See chr_tile_for()'s
 * house_furniture_tile() for how a (room, row, col) picks one of these
 * over the plain wall tile. */
#define FURNITURE_TABLE_TILE (PORTAL_DOOR_TILE_BASE + 1)
#define FURNITURE_BED_TILE   (FURNITURE_TABLE_TILE + 1)
#define JIMBLES_FACE_TILE    FURNITURE_TABLE_TILE
#define JIMBLES_BODY_TILE    FURNITURE_BED_TILE
#define JIMBLES_SPRITE_PALETTE 2

/* title_portrait.s is linked after furniture.s: a 10x12-tile face used only
 * by the title screen, generated from assets/title_portrait_reference.png. */
#define TITLE_PORTRAIT_TILE_BASE (FURNITURE_BED_TILE + 1)
#define TITLE_PORTRAIT_WIDTH  10
#define TITLE_PORTRAIT_HEIGHT 12
#define TITLE_PORTRAIT_TOP    8
#define TITLE_PORTRAIT_LEFT   2
#define TITLE_CLOSED_MOUTH_TILE_BASE (TITLE_PORTRAIT_TILE_BASE + TITLE_PORTRAIT_WIDTH * TITLE_PORTRAIT_HEIGHT)
#define TITLE_MOUTH_TILE_ROW 7
#define TITLE_MOUTH_TILE_COL 4
#define TITLE_MOUTH_WIDTH    3
#define TITLE_MOUTH_HEIGHT   3
#define TITLE_PORTRAIT_TILE_COUNT (TITLE_PORTRAIT_WIDTH * TITLE_PORTRAIT_HEIGHT + TITLE_MOUTH_WIDTH * TITLE_MOUTH_HEIGHT)

/* neon_avenue.s follows title_portrait.s, deliberately preserving title
 * tile numbers used by startup.s's NMI animation tables. */
#define NEON_TILE_ASPHALT   (TITLE_PORTRAIT_TILE_BASE + TITLE_PORTRAIT_TILE_COUNT)
#define NEON_TILE_SIDEWALK  (NEON_TILE_ASPHALT + 1)
#define NEON_TILE_BRICK     (NEON_TILE_ASPHALT + 2)
#define NEON_TILE_AWNING    (NEON_TILE_ASPHALT + 3)
#define NEON_TILE_TAXI_POST (NEON_TILE_ASPHALT + 4)
#define NEON_TILE_BAR_DOOR_L (NEON_TILE_TAXI_POST + 1)
#define NEON_TILE_BAR_DOOR_C (NEON_TILE_BAR_DOOR_L + 1)
#define NEON_TILE_BAR_DOOR_R (NEON_TILE_BAR_DOOR_L + 2)
#define NEON_TILE_SIGN_TL    (NEON_TILE_BAR_DOOR_L + 3)
#define NEON_TILE_SIGN_T     (NEON_TILE_SIGN_TL + 1)
#define NEON_TILE_SIGN_TR    (NEON_TILE_SIGN_TL + 2)
#define NEON_TILE_SIGN_L     (NEON_TILE_SIGN_TL + 3)
#define NEON_TILE_SIGN_FILL  (NEON_TILE_SIGN_TL + 4)
#define NEON_TILE_SIGN_R     (NEON_TILE_SIGN_TL + 5)
#define NEON_TILE_SIGN_BL    (NEON_TILE_SIGN_TL + 6)
#define NEON_TILE_SIGN_B     (NEON_TILE_SIGN_TL + 7)
#define NEON_TILE_SIGN_BR    (NEON_TILE_SIGN_TL + 8)
#define NEON_TILE_WINDOW_H   (NEON_TILE_SIGN_BR + 1)
#define NEON_TILE_WINDOW_V   (NEON_TILE_WINDOW_H + 1)
#define NEON_TILE_WINDOW_FILL (NEON_TILE_WINDOW_H + 2)

/* player_avatar.s follows the opening-scene background tiles. Keeping it
 * after title_portrait.s preserves the title NMI's fixed portrait indices. */
#define PLAYER_AVATAR_TILE_BASE (NEON_TILE_WINDOW_FILL + 1)
#define PLAYER_AVATAR_HEAD_L    PLAYER_AVATAR_TILE_BASE
#define PLAYER_AVATAR_HEAD_R    (PLAYER_AVATAR_HEAD_L + 1)
#define PLAYER_AVATAR_FACE_L    (PLAYER_AVATAR_HEAD_L + 2)
#define PLAYER_AVATAR_FACE_R    (PLAYER_AVATAR_HEAD_L + 3)
#define PLAYER_AVATAR_LEGS_A_L  (PLAYER_AVATAR_HEAD_L + 4)
#define PLAYER_AVATAR_LEGS_A_R  (PLAYER_AVATAR_HEAD_L + 5)
#define PLAYER_AVATAR_LEGS_B_L  (PLAYER_AVATAR_HEAD_L + 6)
#define PLAYER_AVATAR_LEGS_B_R  (PLAYER_AVATAR_HEAD_L + 7)

/* taxi.s follows the avatar and contributes a 32x24 side-view cab plus
 * three splatters. The twelve cab tiles are arranged in a 4x3 grid. */
#define TAXI_TILE_BASE          (PLAYER_AVATAR_TILE_BASE + 8)
#define TAXI_PARTICLE_TILE      (TAXI_TILE_BASE + 12)
#define TAXI_PARTICLE_TILE_SMALL (TAXI_PARTICLE_TILE + 1)
#define TAXI_PARTICLE_TILE_DOT  (TAXI_PARTICLE_TILE + 2)

/* bar_interior.s follows the taxi art. */
#define BAR_TILE_FLOOR           (TAXI_PARTICLE_TILE_DOT + 1)
#define BAR_TILE_WALL            (BAR_TILE_FLOOR + 1)
#define BAR_TILE_COUNTER         (BAR_TILE_FLOOR + 2)
#define BAR_TILE_BOTTLES         (BAR_TILE_FLOOR + 3)
#define BAR_TILE_STOOL           (BAR_TILE_FLOOR + 4)
#define BAR_TILE_BOOTH           (BAR_TILE_STOOL + 1)
#define BAR_TILE_JUKEBOX         (BAR_TILE_BOOTH + 1)
#define BAR_TILE_BACK_DOOR       (BAR_TILE_JUKEBOX + 1)
#define BAR_TILE_BACK_DOOR_OPEN  (BAR_TILE_BACK_DOOR + 1)
#define BAR_TILE_DOOR            BAR_TILE_STOOL
#define BAR_TILE_EXIT_EDGE       (BAR_TILE_BACK_DOOR_OPEN + 1)
#define BAR_TILE_EXIT_FILL       (BAR_TILE_EXIT_EDGE + 1)
#define BAR_TILE_EXIT_TL         BAR_TILE_EXIT_EDGE
#define BAR_TILE_EXIT_T          BAR_TILE_EXIT_FILL
#define BAR_TILE_EXIT_TR         BAR_TILE_EXIT_EDGE
#define BAR_TILE_EXIT_L          BAR_TILE_EXIT_EDGE
#define BAR_TILE_EXIT_R          BAR_TILE_EXIT_EDGE
#define BAR_TILE_EXIT_BL         BAR_TILE_EXIT_EDGE
#define BAR_TILE_EXIT_B          BAR_TILE_EXIT_FILL
#define BAR_TILE_EXIT_BR         BAR_TILE_EXIT_EDGE

/* A title-only sprite overlay isolates the tongue from the lower jaw's
 * shared background palette. title_tongue.s follows the bar tiles. */
#define TITLE_TONGUE_TILE        (BAR_TILE_EXIT_FILL + 1)
#define TITLE_TONGUE_TILE_RIGHT  (TITLE_TONGUE_TILE + 1)
#define TITLE_UVULA_TILE         (TITLE_TONGUE_TILE_RIGHT + 1)
#define TITLE_TONGUE_SLOT        0
#define TITLE_TONGUE_SLOT_RIGHT  1
#define TITLE_UVULA_SLOT         2
#define TITLE_TONGUE_X           56
#define TITLE_TONGUE_Y           132
#define TITLE_UVULA_Y            129

/* traffic_bones.s follows the title-only mouth overlays. */
#define TRAFFIC_BONE_TILE         (TITLE_UVULA_TILE + 1)
#define TRAFFIC_WISHBONE_TILE     (TRAFFIC_BONE_TILE + 2)
#define TRAFFIC_FEMUR_TILE        (TRAFFIC_BONE_TILE + 3)
#define TRAFFIC_CAVEMAN_TILE      (TRAFFIC_BONE_TILE + 4)
#define TRAFFIC_SKULL_TILE        (TRAFFIC_BONE_TILE + 5)

/* The NMI consumes this one-entry queue at the start of vblank. C publishes
 * only which of the assembly-owned open/closed frames to use. */
volatile unsigned char title_mouth_dirty;
volatile unsigned char title_mouth_open;
volatile unsigned char title_message_dirty;
volatile unsigned char title_message_index;

#define TITLE_MESSAGE_COUNT 3
#define TITLE_MESSAGE_ROW   12
#define TITLE_MESSAGE_COL   14
#define TITLE_MESSAGE_WIDTH 17
#define TITLE_MESSAGE_HIDDEN TITLE_MESSAGE_COUNT

/* OAM attribute byte bits 6/7 -- horizontal/vertical sprite flip. The
 * only two of the 8 attribute bits this project has needed before now;
 * the low 2 bits are still the sprite palette number. */
#define OAM_FLIP_H 0x40
#define OAM_FLIP_V 0x80

/* A swing shows for this many frames total, split into two equal
 * halves: SWORD_THRUST_REACH out from the player for the first half,
 * then the full ATTACK_REACH for the second -- a small two-position
 * "thrust" instead of one static frame, closer to how a Zelda-style
 * swing actually reads (the blade extends *out*, not just appears).
 * The hit-check itself is untouched by any of this -- it still happens
 * once, instantly, at ATTACK_REACH, on the frame the button is pressed;
 * this only changes what the swing looks like. Was 12 (200ms) -- read as
 * sluggish in playtest, so trimmed to 8 (133ms, still splitting evenly
 * into two 4-frame halves) for a snappier swing. */
#define SWORD_VISIBLE_FRAMES 8
#define SWORD_THRUST_REACH (ATTACK_REACH / 2)

/* How long (in frames) a non-lethal hit makes the enemy blink, and the
 * on/off toggle rate within that window -- replaces the original
 * per-swing text box (see ROADMAP.md's Phase 6 note on why that
 * wouldn't have scaled): a killing blow just removes the sprite, no
 * blink needed, since disappearing is already unambiguous feedback. */
#define ENEMY_FLASH_FRAMES 16
#define FLASH_TOGGLE_BIT 2   /* bit of the countdown that drives blink on/off */

/* A wandering enemy picks a new direction (or stands still) every
 * ENEMY_WANDER_MIN_FRAMES-ENEMY_WANDER_MAX_FRAMES, or immediately if the
 * current direction turns out to be blocked -- see update_enemy_wander().
 * No AI beyond that yet: it doesn't chase or flee, just ambles. */
#define ENEMY_MOVE_SPEED 1
#define ENEMY_WANDER_MIN_FRAMES 30
#define ENEMY_WANDER_MAX_FRAMES 90

/* Touching a live enemy costs the player HP, once, then a period of
 * invulnerability (CONTACT_INVULN_FRAMES) before it can happen again --
 * without that, standing inside an enemy's box would drain HP every
 * single frame. No death handling yet (hp just clamps at 0 and nothing
 * further happens) -- a deliberate, documented gap for this slice, not
 * an oversight; see ROADMAP.md. */
#define PLAYER_MAX_HP 10
#define CONTACT_DAMAGE 1
#define CONTACT_INVULN_FRAMES 60
#define PLAYER_FLASH_FRAMES 16
#define DEATH_RESPAWN_FRAMES 75
/* Six frames at two pixels each produces a visible 12-pixel shove without
 * losing the snappy pace of the existing eight-frame sword animation. */
#define KNOCKBACK_FRAMES 6
#define KNOCKBACK_STEPS_PER_FRAME 2

/* The text box occupies tile rows 24-27 -- deliberately exactly one
 * attribute-table row (each covers 4 tile rows, at 4-row-aligned
 * boundaries: 0-3, 4-7, ..., 24-27), so recoloring it to the text box's
 * own palette can't bleed into a neighboring room row that should stay
 * in the room's normal palette. */
#define TEXT_BOX_TOP_ROW  24
#define TEXT_BOX_ROWS     4
#define TEXT_BOX_ATTR_ROW (TEXT_BOX_TOP_ROW / 4)
#define TEXT_BOX_PALETTE  1
#define TEXT_BOX_TEXT_ROW (TEXT_BOX_TOP_ROW + 1)
#define TEXT_BOX_MESSAGE_WIDTH (NAMETABLE_WIDTH_TILES - 2)

/* Traffic deaths use a dedicated, screen-centered card; normal dialogue
 * remains anchored at the bottom where it does not cover the adventure art. */
#define DEATH_BOX_TOP_ROW  12
#define DEATH_BOX_ROWS     8
/* Two-line messages occupy rows 15-16, centered in the taller card. */
#define DEATH_BOX_TEXT_ROW (DEATH_BOX_TOP_ROW + 3)
#define DEATH_BOX_LEFT_COL 4
#define DEATH_BOX_WIDTH    24
#define DEATH_BOX_TEXT_WIDTH (DEATH_BOX_WIDTH - 2)
#define DEATH_BOX_FIRST_ATTR_COL (DEATH_BOX_LEFT_COL / 4)
#define DEATH_BOX_LAST_ATTR_COL ((DEATH_BOX_LEFT_COL + DEATH_BOX_WIDTH - 1) / 4)
#define DEATH_BOX_FIRST_ATTR_ROW (DEATH_BOX_TOP_ROW / 4)
#define DEATH_BOX_LAST_ATTR_ROW ((DEATH_BOX_TOP_ROW + DEATH_BOX_ROWS - 1) / 4)

#define MENU_TOP_ROW 6
#define MENU_ROWS 14
#define MENU_BOTTOM_ROW (MENU_TOP_ROW + MENU_ROWS - 1)
#define MENU_FIRST_ATTR_ROW (MENU_TOP_ROW / 4)
#define MENU_LAST_ATTR_ROW (MENU_BOTTOM_ROW / 4)

/* OAM attributes for the portrait-inspired adventure actor. */
#define SPRITE_PALETTE  0       /* sprite palette 0 */
#define SPRITE_ATTR     SPRITE_PALETTE
#define PLAYER_SPRITE_SLOTS 6
#define PLAYER_WALK_FRAME_PERIOD 8
#define TAXI_SPRITE_SLOTS 12
#define TAXI_WIDTH 32
#define TRAFFIC_PARTICLE_COUNT 8
#define TRAFFIC_BONE_COUNT 6
#define TRAFFIC_HEAD_SPRITE_COUNT 2
#define TAXI_SLOT(i) (PLAYER_SPRITE_SLOTS + (i))
#define TRAFFIC_PARTICLE_SLOT(i) (PLAYER_SPRITE_SLOTS + TAXI_SPRITE_SLOTS + (i))
#define TRAFFIC_BONE_SLOT(i) (PLAYER_SPRITE_SLOTS + TAXI_SPRITE_SLOTS + TRAFFIC_PARTICLE_COUNT + (i))
#define TRAFFIC_HEAD_SLOT(i) (PLAYER_SPRITE_SLOTS + TAXI_SPRITE_SLOTS + TRAFFIC_PARTICLE_COUNT + TRAFFIC_BONE_COUNT + (i))
#define ENTITY_OAM_BASE (PLAYER_SPRITE_SLOTS + TAXI_SPRITE_SLOTS + TRAFFIC_PARTICLE_COUNT + TRAFFIC_BONE_COUNT + TRAFFIC_HEAD_SPRITE_COUNT)
#define NPC_LARGE_SPRITE_SLOTS 6
#define NPC_LARGE_SLOT(i) (ENTITY_OAM_BASE + (i))
#define NPC_OAM_BASE (ENTITY_OAM_BASE + NPC_LARGE_SPRITE_SLOTS)
#define BAR_COUNTER_OVERLAY_SPRITE_SLOTS 4
/* Lower OAM indices render in front on the NES. Keep the striped counter
 * before Jimbles so it genuinely occludes his lower body. */
#define BAR_COUNTER_OVERLAY_SLOT(i) (NPC_OAM_BASE + NPC_COUNT + (i))
#define BAR_BARTENDER_SPRITE_SLOTS 6
#define BAR_BARTENDER_SLOT(i) (NPC_OAM_BASE + NPC_COUNT + BAR_COUNTER_OVERLAY_SPRITE_SLOTS + (i))
#define BAR_DOORKNOB_SLOT (NPC_OAM_BASE + NPC_COUNT + BAR_COUNTER_OVERLAY_SPRITE_SLOTS + BAR_BARTENDER_SPRITE_SLOTS)
#define ITEM_OAM_BASE (BAR_DOORKNOB_SLOT + 1)
#define BAR_COUNTER_SPRITE_PALETTE 3
#define TAXI_SPRITE_PALETTE 3
/* The bar facade ends at pixel row 152. The 16x24 actor is drawn with its
 * top at (player_y - 16); allowing a 16px overlap makes it read as walking
 * in front of the facade, while its feet remain on the pavement. */
#define NEON_AVENUE_WALK_TOP 152
/* Let the actor overlap the front of the counter/door by one tile, matching
 * the exterior facade's depth treatment instead of treating head height as
 * the collision boundary. */
#define JIMBLES_BAR_WALK_TOP 124
#define CASINO_WALK_TOP 144

/* How many 1px steps of movement.c's (already unit-tested) per-frame
 * update_position() the player gets each frame -- calling it twice
 * doubles ground speed using the exact same, unmodified collision logic
 * as a single step, rather than teaching movement.c a step size (which
 * would mean rewriting most of test_movement.c's expected pixel values
 * for no real benefit). Bumped from 1 to make hand-testing this now-
 * 8-room world faster to walk around; not a gameplay-balance choice. */
#define PLAYER_MOVE_STEPS 2

#define TITLE_START_MASK (JOY_BTN_A_MASK | JOY_START_MASK)
#define TITLE_RELEASE_FRAMES 4

typedef enum {
    ROOM_MEADOW, ROOM_DESERT, ROOM_FOREST, ROOM_SNOW,
    ROOM_PLAINS, ROOM_CANYON, ROOM_SWAMP, ROOM_TUNDRA,
    ROOM_TOWN,
    ROOM_SEWER,
    ROOM_CASINO,
    ROOM_HOUSE,
    ROOM_HOUSE2,
    ROOM_COUNT
} RoomId;

/* The fixed-camera Neon Avenue renderer owns its large scenic facades. Keep
 * room-layout collision sparse here: scanning a huge decoration list for
 * every tile made the forced-blank room load visibly slow. */
static const RoomDecoration meadow_decorations[] = {
    {18,5}, {19,5}, {20,5},
    {0xFF, 0xFF}
};

/* Subset of meadow_decorations drawn with bg palette 2 instead of the
 * room's default palette 0 -- same backdrop/accent/path (see
 * room_palettes' comment on this), just a slightly different tree
 * color, so the south clump doesn't read as a solid-identical copy of
 * the north one. See draw_room()'s attribute-table pass for how a
 * (row, col) here gets turned into the right attribute-byte bits. */
static const RoomDecoration meadow_variant_decorations[] = {
    {0xFF, 0xFF}
};

static const RoomDecoration desert_decorations[] = {
    { 9, 10}, {20, 21},
    {0xFF, 0xFF}
};

static const RoomDecoration desert_variant_decorations[] = {
    {20, 21},
    {0xFF, 0xFF}
};

/* Forest is meant to feel denser than the meadow -- noticeably more
 * trees scattered around, not just a recolor. */
static const RoomDecoration forest_decorations[] = {
    { 3,  4}, { 4,  5}, { 5,  4}, { 3, 25}, { 4, 26}, { 5, 25},
    {10,  3}, {11,  4}, {10, 27}, {11, 26},
    {20,  5}, {21,  6}, {20, 25}, {21, 24},
    {25, 10}, {25, 20},
    {0xFF, 0xFF}
};

/* The east-side clusters (those five-ish columns further right) get the
 * variant green -- an arbitrary but consistent split, so the forest
 * reads as having two stands of trees instead of one uniform block. */
static const RoomDecoration forest_variant_decorations[] = {
    { 3, 25}, { 4, 26}, { 5, 25}, {10, 27}, {11, 26}, {20, 25}, {21, 24},
    {0xFF, 0xFF}
};

static const RoomDecoration snow_decorations[] = {
    { 8,  8}, { 9,  9}, {19, 22}, {20, 21},
    {0xFF, 0xFF}
};

static const RoomDecoration plains_decorations[] = {
    { 6, 20}, { 7, 21}, {22, 10}, {23,  9},
    {0xFF, 0xFF}
};

static const RoomDecoration canyon_decorations[] = {
    { 5,  5}, { 6,  6}, {24, 25}, {23, 24},
    {0xFF, 0xFF}
};

static const RoomDecoration swamp_decorations[] = {
    { 4, 10}, { 5, 11}, {20, 20}, {21, 21}, {10,  5}, {11,  6},
    {0xFF, 0xFF}
};

static const RoomDecoration tundra_decorations[] = {
    { 5, 25}, { 6, 24}, {22,  7}, {23,  8},
    {0xFF, 0xFF}
};

/* Each house is a deliberately compact 8x8 room in the center of an
 * otherwise black nametable. Two furniture cells make the small footprint
 * read as lived-in without needing a full-screen floor plan. */
static const RoomDecoration house1_decorations[] = {
    {12, 13}, {14, 18},
    {0xFF, 0xFF}
};

/* Same compact footprint, with furniture mirrored for a different silhouette. */
static const RoomDecoration house2_decorations[] = {
    {13, 18}, {15, 13},
    {0xFF, 0xFF}
};

/* Each room's exits match its actual connections below -- a room with
 * no neighbor to the north keeps a solid border there instead of an
 * opening to nowhere. */
static const RoomLayout room_layouts[ROOM_COUNT] = {
    /* ROOM_MEADOW: north (forest), east (desert), south (plains) */
    {1, 1, 1, 0, meadow_decorations, 1},
    /* ROOM_DESERT: north (snow), west (meadow), south (canyon) */
    {1, 0, 1, 1, desert_decorations, 1},
    /* ROOM_FOREST: east (snow), south (meadow), west (swamp) */
    {0, 1, 1, 1, forest_decorations, 1},
    /* ROOM_SNOW: north (tundra), south (desert), west (forest) */
    {1, 0, 1, 1, snow_decorations, 1},
    /* ROOM_PLAINS: north (meadow) */
    {1, 0, 0, 0, plains_decorations, 1},
    /* ROOM_CANYON: north (desert) */
    {1, 0, 0, 0, canyon_decorations, 1},
    /* ROOM_SWAMP: east (forest) */
    {0, 1, 0, 0, swamp_decorations, 1},
    /* ROOM_TUNDRA: south (snow) */
    {0, 0, 1, 0, tundra_decorations, 1},
    /* Jimbles's Bar owns its scenery in draw_jimbles_bar_scene(). Keep its
     * inherited layout empty so invisible former-town decorations cannot
     * block the stage floor; only the south edge's centered exit remains. */
    {0, 0, 1, 0, 0, 0},
    /* The sewer is a sealed, direct-rendered room. The ordinary room border
     * supplies solid collision around its visible masonry walls. */
    {0, 0, 0, 0, 0, 1},
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, house1_decorations, 0, 0, 8, 8, 1, 15},
    {0, 0, 0, 0, house2_decorations, 0, 0, 8, 8, 1, 15},
};

/* Per-room list of which decorations (if any) draw with bg palette 2
 * instead of the room's usual palette 0 -- see draw_room()'s attribute-
 * table pass. 0 (a null RoomDecoration*, not a real list) means "no
 * variation in this room," which is most of them for now; the NES only
 * gives a room 4 background palettes total (0 = own tiles, 1 = the
 * constant text box, 2/3 unused everywhere until this), so this is
 * building on real spare capacity rather than adding anything new to
 * the hardware side. */
static const RoomDecoration * const room_variant[ROOM_COUNT] = {
    meadow_variant_decorations,   /* ROOM_MEADOW */
    desert_variant_decorations,   /* ROOM_DESERT */
    forest_variant_decorations,   /* ROOM_FOREST */
    0,                             /* ROOM_SNOW   */
    0,                             /* ROOM_PLAINS */
    0,                             /* ROOM_CANYON */
    0,                             /* ROOM_SWAMP  */
    0,                             /* ROOM_TUNDRA */
    0,                             /* ROOM_TOWN: direct-rendered bar */
    0,                             /* ROOM_SEWER: direct-rendered */
    0,                             /* ROOM_CASINO: direct-rendered */
    0,                             /* ROOM_HOUSE  */
    0,                             /* ROOM_HOUSE2 */
};

/* Town's south exit lands the player at Meadow's north edge (the
 * ordinary EXIT_SOUTH -> destination's SCREEN_MIN_Y rule everything
 * else in the room graph already follows) -- the same landing spot
 * Forest's south exit already uses, since Meadow's north edge is
 * Forest's, not Town's. Meadow's own north exit still leads to Forest,
 * not back to Town, so this pairing is intentionally one-way: unlike
 * every other room pair here, Town <-> Meadow doesn't round-trip
 * through the same two edges. That's fine -- getting back in is what
 * the portal tile by the meadow's house icon is for. Both houses <->
 * town are the same kind of one-way pairing, one level down: each
 * house's south exit lands at a hand-picked spot by its own door in
 * town (main() special-cases this the same way it does town's), not
 * town's generic north edge -- town doesn't even have a north exit to
 * land at. */
static const RoomConnections room_connections[ROOM_COUNT] = {
    /* ROOM_MEADOW */ {ROOM_FOREST, ROOM_DESERT, ROOM_PLAINS, ROOM_NONE},
    /* ROOM_DESERT */ {ROOM_SNOW,   ROOM_NONE,   ROOM_CANYON, ROOM_MEADOW},
    /* ROOM_FOREST */ {ROOM_NONE,   ROOM_SNOW,   ROOM_MEADOW, ROOM_SWAMP},
    /* ROOM_SNOW   */ {ROOM_TUNDRA, ROOM_NONE,   ROOM_DESERT, ROOM_FOREST},
    /* ROOM_PLAINS */ {ROOM_MEADOW, ROOM_NONE,   ROOM_NONE,   ROOM_NONE},
    /* ROOM_CANYON */ {ROOM_DESERT, ROOM_NONE,   ROOM_NONE,   ROOM_NONE},
    /* ROOM_SWAMP  */ {ROOM_NONE,   ROOM_FOREST, ROOM_NONE,   ROOM_NONE},
    /* ROOM_TUNDRA */ {ROOM_NONE,   ROOM_NONE,   ROOM_SNOW,   ROOM_NONE},
    /* ROOM_TOWN   */ {ROOM_NONE,   ROOM_NONE,   ROOM_MEADOW, ROOM_NONE},
    /* ROOM_SEWER  */ {ROOM_NONE,   ROOM_NONE,   ROOM_NONE,   ROOM_NONE},
    /* ROOM_CASINO */ {ROOM_NONE,   ROOM_NONE,   ROOM_NONE,   ROOM_NONE},
    /* ROOM_HOUSE  */ {ROOM_NONE,   ROOM_NONE,   ROOM_NONE,   ROOM_NONE},
    /* ROOM_HOUSE2 */ {ROOM_NONE,   ROOM_NONE,   ROOM_NONE,   ROOM_NONE},
};

/* Entry positions are authored beside the room graph. Grid transitions keep
 * the perpendicular coordinate; one-way town/house exits name exact spawns. */
static const RoomSpawn room_entry_spawns[ROOM_COUNT][4] = {
    {{0, SCREEN_MAX_Y, WORLD_SPAWN_KEEP_X}, {0, 0, WORLD_SPAWN_KEEP_Y}, {0, SCREEN_MIN_Y, WORLD_SPAWN_KEEP_X}, {0, 0, 0}},
    {{0, SCREEN_MAX_Y, WORLD_SPAWN_KEEP_X}, {0, 0, 0}, {0, SCREEN_MIN_Y, WORLD_SPAWN_KEEP_X}, {SCREEN_MAX_X, 0, WORLD_SPAWN_KEEP_Y}},
    {{0, 0, 0}, {0, 0, WORLD_SPAWN_KEEP_Y}, {0, SCREEN_MIN_Y, WORLD_SPAWN_KEEP_X}, {SCREEN_MAX_X, 0, WORLD_SPAWN_KEEP_Y}},
    {{0, SCREEN_MAX_Y, WORLD_SPAWN_KEEP_X}, {0, 0, 0}, {0, SCREEN_MIN_Y, WORLD_SPAWN_KEEP_X}, {SCREEN_MAX_X, 0, WORLD_SPAWN_KEEP_Y}},
    {{0, SCREEN_MAX_Y, WORLD_SPAWN_KEEP_X}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, SCREEN_MAX_Y, WORLD_SPAWN_KEEP_X}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, WORLD_SPAWN_KEEP_Y}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, SCREEN_MIN_Y, WORLD_SPAWN_KEEP_X}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {120, 168, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {48, 64, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {176, 176, 0}, {0, 0, 0}},
};
static const unsigned char room_one_way_flags[ROOM_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, WORLD_EXIT_SOUTH, 0, 0, 0, 0
};

static const unsigned char room_obstacle_tile[ROOM_COUNT] = {
    CHR_TILE_TREE,   /* ROOM_MEADOW */
    CHR_TILE_ROCK,   /* ROOM_DESERT */
    CHR_TILE_TREE,   /* ROOM_FOREST */
    CHR_TILE_ROCK,   /* ROOM_SNOW   */
    CHR_TILE_TREE,   /* ROOM_PLAINS */
    CHR_TILE_ROCK,   /* ROOM_CANYON */
    CHR_TILE_TREE,   /* ROOM_SWAMP  */
    CHR_TILE_ROCK,   /* ROOM_TUNDRA */
    /* Fallback only -- every actual town house cell is special-cased in
     * chr_tile_for() below, by exact (row, col), to one of the three
     * house tiles instead of using this uniform value. */
    CHR_TILE_HOUSE_WALL, /* ROOM_TOWN */
    CHR_TILE_TREE,       /* ROOM_SEWER */
    CHR_TILE_TREE,       /* ROOM_CASINO */
    CHR_TILE_HOUSE_WALL, /* ROOM_HOUSE  */
    CHR_TILE_HOUSE_WALL, /* ROOM_HOUSE2 */
};

/* Full 32-byte PPU palette per room: 4 background palettes + 4 sprite
 * palettes. Background palette 0 is what any room's own tiles use: entry
 * 0 is the shared backdrop/floor-base color, entry 1 is floor texture
 * accent, entry 2 is the obstacle color, entry 3 is the path. Background
 * palette 1 is the text box's, constant across every room: entry 1 is
 * the border frame's accent color (box_border.s), entry 2 is the box
 * fill/glyph background, entry 3 is the text ink; entry 0 is unused on
 * purpose (see the NES quirk below -- font.s and box_border.s both
 * deliberately avoid color index 0 in their tiles). Background palette 2
 * is unused (left at the 0x0F filler) in most rooms, but in the four
 * that have a room_variant[] list (see that array's comment), it's
 * identical to palette 0 except entry 2 -- a close but distinct obstacle
 * color, so draw_room()'s attribute-table pass can recolor just the
 * listed decorations without disturbing that quadrant's floor/path
 * pixels (which never use color index 2). Sprite palette 1's
 * entry 1 is whichever NPC's normal color, sprite palette 2's entry 1 is
 * whichever quest item's color, sprite palette 3's entry 1 is the
 * "reward granted" color an NPC switches to once its quest is complete
 * (NPC_REWARD_PALETTE) -- each only set in the room(s) that actually
 * have one (meadow has an NPC and its reward color; forest has an item;
 * desert has the second NPC and its reward color; snow has both the
 * second item and the third NPC/its reward color; swamp has the fourth
 * NPC and its reward color; tundra has the third item; meadow also has
 * the enemy, reusing sprite palette 2's "item color" slot since meadow
 * has no item of its own -- see ENEMY_SPRITE_PALETTE. Canyon's slot 2/3
 * entries are stale leftovers from when the enemy briefly lived there
 * during early testing -- harmless (nothing reads them while nothing's
 * placed in canyon), left as-is rather than re-churned for no reason;
 * town has the fifth NPC and its reward color, plus the critters,
 * reusing slot 2's "item color" slot the same way meadow's enemy does,
 * since town has neither an item nor an enemy -- see
 * CRITTER_SPRITE_PALETTE; house has the sixth NPC and its reward color;
 * house2 has the seventh NPC and its reward color).
 *
 * NES hardware quirks:
 *  - Only palette 0 entry 0 is ever the real backdrop. A background
 *    tile's index-0 pixels always show *that* color, regardless of
 *    which of the 4 background palettes the attribute table selects for
 *    that tile -- so palette 1's own entry 0 can never give the text
 *    box an independent color, which is why it's left as a placeholder
 *    here and never used by any text-box tile.
 *  - Entry 0 of each background palette shares the exact same storage
 *    as entry 0 of the *corresponding* sprite palette ($3F00
 *    background-palette-0-entry-0 IS $3F10 sprite-palette-0-entry-0, not
 *    just visually similar -- one write location, whichever happens
 *    last wins). Since palette 0 is the one both the room and the player
 *    sprite actually use, sprite palette 0's entry 0 (index 16 in each
 *    array below) has to match that room's backdrop color, or it
 *    silently overwrites it. */
static const unsigned char room_palettes[ROOM_COUNT][32] = {
    /* ROOM_MEADOW is now Neon Avenue: near-black midnight, blue wet asphalt,
     * cyan sidewalks, and pink storefront neon. Background palette 2 keeps
     * the same asphalt color while giving the sign text a cream-white ink. */
    {
        0x0F, 0x01, 0x16, 0x21,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x01, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x11, 0x0F, 0x0F,
        0x0F, 0x24, 0x0F, 0x0F,
        0x0F, 0x16, 0x27, 0x30
    },
    /* ROOM_DESERT: sand backdrop, pale sand accent, grey rock obstacle,
     * dark worn-trail path (plus a lighter grey, 0x10, in bg palette 2
     * for desert_variant_decorations' second rock cluster). */
    {
        0x27, 0x37, 0x00, 0x17,
        0x0F, 0x28, 0x0F, 0x30,
        0x27, 0x37, 0x10, 0x17,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x27, 0x16, 0x27, 0x30,
        0x0F, 0x2C, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x17, 0x07, 0x30
    },
    /* ROOM_FOREST: dark, shaded-canopy green backdrop, medium-green
     * accent, bright sunlit-green treetop obstacle, brown path (plus the
     * same distinctly cooler teal-green, 0x0C, meadow uses -- see its
     * comment -- in bg palette 2 for forest_variant_decorations' east-
     * side trees). Darker floor than the meadow is what sells
     * "denser forest" alongside the extra decoration count. */
    {
        0x09, 0x19, 0x29, 0x07,
        0x0F, 0x28, 0x0F, 0x30,
        0x09, 0x19, 0x0C, 0x07,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x09, 0x16, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x24, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F
    },
    /* ROOM_SNOW: pale icy-blue backdrop, brighter blue accent, grey
     * snow-drift obstacle, dark packed-snow trail. */
    {
        0x31, 0x21, 0x10, 0x2D,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x31, 0x16, 0x27, 0x30,
        0x0F, 0x22, 0x0F, 0x0F,
        0x0F, 0x16, 0x0F, 0x0F,
        0x0F, 0x28, 0x0F, 0x0F
    },
    /* ROOM_PLAINS: sunny bright-green backdrop, pale grass accent, dark
     * green shrub obstacle, dirt path. */
    {
        0x2A, 0x3A, 0x19, 0x17,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x2A, 0x16, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F
    },
    /* ROOM_CANYON: dark rust-orange backdrop, bright orange accent, deep
     * red-brown rock obstacle, red trail. Gets the first enemy -- bright
     * green stands out hard against the orange, on purpose -- and the
     * sword's color (white, slot 3 -- free here since canyon has no NPC
     * reward color to hold instead). */
    {
        0x17, 0x27, 0x06, 0x16,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x17, 0x16, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x2A, 0x0F, 0x0F,
        0x0F, 0x30, 0x0F, 0x0F
    },
    /* ROOM_SWAMP: dark murky-olive backdrop, muddy green accent, near-
     * black tree obstacle, brown path. Gets the fourth NPC (stationary,
     * QUEST_FETCH) and its reward color. */
    {
        0x08, 0x18, 0x09, 0x17,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x08, 0x16, 0x27, 0x30,
        0x0F, 0x1C, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x28, 0x0F, 0x0F
    },
    /* ROOM_TUNDRA: pale grey-white backdrop, medium grey accent, near-
     * black rock obstacle, blue-grey trail. Gets the third item (its
     * color, cyan, is set below). */
    {
        0x20, 0x10, 0x00, 0x2D,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x20, 0x16, 0x27, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x2C, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F
    },
    /* ROOM_TOWN is now Jimbles's Bar: walnut walls, amber floorboards, a
     * bright counter, and warm sprite colors for the bartender and player. */
    {
        0x0F, 0x17, 0x07, 0x30,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x08, 0x16, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x27, 0x16, 0x30,
        0x0F, 0x16, 0x27, 0x30,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x17, 0x07, 0x30
    },
    /* ROOM_SEWER: black water, blue-green masonry, and a pale ladder. */
    {
        0x0F, 0x0C, 0x02, 0x30,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0C, 0x21, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x27, 0x16, 0x30,
        0x0F, 0x16, 0x27, 0x30,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0C, 0x21, 0x30
    },
    /* ROOM_CASINO: deep burgundy carpet, gold trim, and bright machine lamps. */
    {
        0x0F, 0x16, 0x07, 0x30,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x19, 0x0C, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x27, 0x16, 0x30,
        0x0F, 0x16, 0x27, 0x30,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x17, 0x07, 0x30
    },
    /* ROOM_HOUSE: warm hardwood-floor backdrop, lighter wood-grain
     * accent, dark plank wall (the reused tree tile, see
     * room_obstacle_tile), red rug path. Gets the sixth NPC (the
     * homeowner, blue) and its reward color; nothing needs slot 2 here
     * (no item, enemy, or critter indoors), so it's left at the filler. */
    {
        0x07, 0x17, 0x08, 0x16,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x07, 0x16, 0x27, 0x30,
        0x0F, 0x21, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x28, 0x0F, 0x0F
    },
    /* ROOM_HOUSE2: same idea as ROOM_HOUSE, varied so the two don't feel
     * copy-pasted -- lighter orange-brown floor, blue rug instead of
     * red, and the seventh NPC in green instead of blue. */
    {
        0x17, 0x27, 0x08, 0x21,
        0x0F, 0x28, 0x0F, 0x30,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x17, 0x16, 0x27, 0x30,
        0x0F, 0x2A, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x28, 0x0F, 0x0F
    }
};

/* A fixed waypoint: standing on it (no button, same "just touch it" feel
 * as an item pickup) instantly moves the player to dest_room at
 * (dest_x, dest_y). Unlike the room graph (room_connections), this isn't
 * a matched pair of opposite edges the player walks through -- it's an
 * arbitrary point-to-point link, which is what makes "a town tile
 * tucked into the meadow" possible without the town needing an edge
 * exit of its own to get *into* it. One-way (into the destination) --
 * getting back out is an ordinary south-edge exit instead (see
 * ROOM_TOWN/ROOM_HOUSE/ROOM_HOUSE2's layouts/room_connections above),
 * not a second portal, so there's nothing here for the return trip to
 * bounce off of.
 *
 * palette lets each portal pick its own sprite color rather than all
 * sharing one constant -- the two house doors deliberately match their
 * house's own brown (see town_decorations' comment) instead of the
 * meadow entrance's gold, and different rooms hold different colors in
 * any given slot anyway. tile picks between portal.s's two looks
 * (standalone house vs. hollow doorway) -- see that file's header for
 * why a house door can't just reuse the standalone house's fully-solid
 * silhouette: sitting right next to three solid wall tiles of the same
 * color, it would be invisible as a distinct, interactive spot rather
 * than just another wall cell. */
typedef struct {
    RoomId room;
    unsigned char x, y;
    RoomId dest_room;
    unsigned char dest_x, dest_y;
    unsigned char palette;
    unsigned char tile;
} Portal;

/* A trap's data stays separate from its animation state so later scene
 * hazards can share the same approach/impact/message lifecycle. */
typedef struct {
    RoomId room;
    unsigned char trigger_y;
    unsigned char lane_y;
    const char *message;
} TrafficTrap;

static const TrafficTrap neon_avenue_traffic = {
    ROOM_MEADOW, 184, 168, "YOU CAN'T JUST WALK INTO TRAFFIC!"
};

/* All three land just above the destination's south exit (SCREEN_MAX_Y,
 * the same "just inside the border" landing spot an ordinary south-edge
 * entry would use -- see EXIT_NORTH's handling below) on the crossing
 * path's column, so arriving puts the way back out in view right away
 * instead of somewhere unrelated. The two house entries sit right in
 * their house's own bottom-left corner (see town_decorations), not
 * beside or below it -- CRITTER_SPRITE_PALETTE is reused for the color
 * match (both a critter and a house-door tile are just "whatever this
 * room's second accent color is"; nothing in town needs both at once in
 * a way that conflicts). */
static const Portal portals[] = {
    { ROOM_MEADOW, 124, 152, ROOM_TOWN, 120, 200, PORTAL_SPRITE_PALETTE, PORTAL_TILE_BASE },
    { ROOM_MEADOW, 64, 168, ROOM_SEWER, 120, 40, PORTAL_SPRITE_PALETTE, BAR_TILE_JUKEBOX },
    { ROOM_SEWER,  120, 32, ROOM_MEADOW, 64, 178, PORTAL_SPRITE_PALETTE, NEON_TILE_TAXI_POST },
};
#define PORTAL_COUNT (sizeof(portals) / sizeof(portals[0]))

/* A quest item: sits in its room until walked over (no button), then
 * hides and waits to be turned in to whichever NPC wants it. Not const
 * any more -- `taken` is mutable per-item state, where a single global
 * quest_state used to do this job for the one quest Phase 4 shipped
 * with. request_message/turnin_message live here rather than on the
 * NPC, since generate_quests() (below) now assigns items to NPCs at
 * boot -- the dialogue about "bring me a flower" has to follow whichever
 * NPC ends up wanting the flower, and the item is the one thing that
 * doesn't change. */
typedef struct {
    RoomId room;
    unsigned char x, y;
    unsigned char taken;          /* 0 = still sitting in the room, 1 = picked up */
    const char *pickup_message;
    const char *request_message;  /* shown by whichever NPC wants this item, before it's taken */
    const char *turnin_message;   /* shown once, when turned in to that NPC -- completes its quest */
} Item;

static const char jukebox_token_request[] = "THE JUKEBOX NEEDS A TOKEN.";
static const char jimbles_bar_title[] = "JIMBLES'S BAR";

/* Both hand-placed on open floor, clear of the crossing path, the
 * border, and every decoration in that room's own decoration list. */
static Item items[] = {
    { ROOM_FOREST, 160, 144, 0, "GOT A FLOWER!",  "BRING ME A FLOWER!", "THANK YOU!" },
    { ROOM_SNOW,   160, 80,  0, "GOT A GEM!",     "I NEED A GEM!",      "MANY THANKS!" },
    { ROOM_TUNDRA, 80,  80,  0, "GOT A CRYSTAL!", "I WANT A CRYSTAL!",  "AT LAST!" },
    { ROOM_TOWN,   64, 160,  0, "GOT SOME BREAD!", "COULD USE SOME BREAD.", "THANKS FOR THE BREAD!" },
    { ROOM_TOWN,   216, 176, 0, "GOT A JUKEBOX TOKEN!", jukebox_token_request, "THE JUKEBOX IS READY!" },
};
#define ITEM_COUNT (sizeof(items) / sizeof(items[0]))
#define BAR_TOKEN_ITEM 4
static unsigned char bar_backroom_unlocked;

static const char * const item_names[ITEM_COUNT] = {
    "FLOWER", "GEM", "CRYSTAL", "BREAD", "JUKEBOX TOKEN"
};

/* Phase 5's third slice added a second quest verb. QUEST_FETCH needs an
 * item turned in before it's ready to complete; QUEST_TALK needs nothing
 * but reaching the NPC at all -- item_id is meaningless for it (left 0,
 * never read). npc_quest_ready() is the one place that distinction
 * lives, so npc_dialogue() and the turn-in check in main() don't need to
 * know it exists. ROADMAP.md's Quests & NPCs recap lists
 * fetch/deliver/kill/find-and-talk as the eventual verb set -- QUEST_TALK
 * here is that last one. */
/* Seven NPCs, each carrying its own quest progress (quest_complete),
 * rather than the single global quest_state Phase 4 shipped with.
 *
 * The meadow NPC keeps its two-room schedule (home/work, swapping on a
 * global tick counter -- see schedule_tick/npc_current_room() below, a
 * minimal stand-in for a real day/night clock); the other six all have
 * home == work, i.e. they never move, proving the same walk state
 * machine degenerates cleanly to "stationary" rather than needing a
 * separate code path -- so they're just placed on open floor like any
 * other hand-placed obstacle. The town greeter and the cottage's
 * occupant are QUEST_TALK like the snow NPC -- free-standing "flavor"
 * NPCs with no item tied to them. The first house's homeowner, though,
 * is QUEST_FETCH like the meadow/desert/swamp NPCs -- wants the loaf of
 * bread sitting out in the town square, which is what actually gives
 * the village a reason to walk into more than one house instead of
 * every door leading to the same kind of empty flavor text. The meadow
 * NPC's rest spot, in contrast,
 * sits ON the crossing path deliberately, so its walk
 * (update_npc_walk() below) between rooms is a straight line to/from
 * the exit with no separate sideways leg needed to reach it.
 *
 * item_id is left 0 here (a placeholder) for every QUEST_FETCH entry --
 * generate_quests() (below, called once at boot) assigns the real value
 * from the pool of items[], picked via rng.c, rather than it being
 * hand-authored data. That's Phase 5's fourth slice: the first thing in
 * this file actually generated at runtime instead of typed in by hand,
 * proving the quest-as-data pipeline can be driven by a rule instead of
 * an author, even though the rule and the pool are both still tiny. */
typedef struct {
    RoomId home, work;
    unsigned char x, y;
    unsigned char verb;
    unsigned char item_id;          /* QUEST_FETCH only -- assigned by generate_quests() */
    unsigned char quest_complete;
    const char *dialogue_greet;     /* QUEST_TALK only -- shown once, completes the quest immediately */
    const char *dialogue_after;     /* quest already complete, either verb */
} Npc;

static Npc npcs[] = {
    { ROOM_MEADOW, ROOM_MEADOW, 200, 176, QUEST_TALK, 0, 0,
      "WELCOME TO NEON AVENUE!", "THE NIGHT IS YOUNG!" },
    { ROOM_DESERT, ROOM_DESERT, 80, 144, QUEST_FETCH, 0, 0,
      0, "FAREWELL!" },
    { ROOM_SNOW, ROOM_SNOW, 160, 40, QUEST_TALK, 0, 0,
      "HAVE YOU SEEN THE YETI?", "TAKE CARE!" },
    { ROOM_SWAMP, ROOM_SWAMP, 200, 144, QUEST_FETCH, 0, 0,
      0, "STAY DRY!" },
    { ROOM_TOWN, ROOM_TOWN, 150, 116, QUEST_TALK, 0, 0,
      "WELCOME TO JIMBLES'S!", "COME BACK ANYTIME!" },
    { ROOM_HOUSE, ROOM_HOUSE, 112, 104, QUEST_FETCH, 0, 0,
      0, "SEE YOU AROUND!" },
    { ROOM_HOUSE2, ROOM_HOUSE2, 136, 104, QUEST_TALK, 0, 0,
      "NICE DAY, ISN'T IT?", "COME BACK SOON!" },
};
#define NPC_COUNT (sizeof(npcs) / sizeof(npcs[0]))
#define NPC_NONE 0xFF
#define JIMBLES_BARTENDER_NPC 4

/* OAM slot assignment: the actor owns six slots for a 16x24 silhouette;
 * each NPC and item gets a fixed later slot so no role can overwrite it. */
#define NPC_SLOT(i)  (NPC_OAM_BASE + (i))
#define ITEM_SLOT(i) (ITEM_OAM_BASE + (i))

static unsigned char npc_quest_ready(unsigned char idx)
{
    const Npc *npc = &npcs[idx];
    if (idx == JIMBLES_BARTENDER_NPC) return 0;
    return npc->verb == QUEST_TALK || items[npc->item_id].taken;
}

static const char *npc_dialogue(unsigned char idx)
{
    const Npc *npc = &npcs[idx];

    if (idx == JIMBLES_BARTENDER_NPC) {
        if (!items[BAR_TOKEN_ITEM].taken) return "WHAT CAN OL' JIMBLES GET YOU TO DRINK?";
        if (bar_backroom_unlocked) return "BACK ROOM IS OPEN.";
        return "GIVE THAT TOKEN TO THE JUKEBOX.";
    }
    if (npc->quest_complete) return npc->dialogue_after;
    if (npc->verb == QUEST_TALK) return npc->dialogue_greet;

    return items[npc->item_id].taken ? items[npc->item_id].turnin_message
                                      : items[npc->item_id].request_message;
}

/* Assigns each QUEST_FETCH NPC an item to want, at random from the
 * items no other NPC has already claimed -- npcs[]' item_id fields
 * start out as placeholders (see the Npc comment above); this is what
 * actually fills them in, once, at boot. A local `pool` (item indices
 * not yet claimed) is shrunk by swap-remove as picks are made, so every
 * remaining item is equally likely on each pick and no two NPCs can
 * ever end up wanting the same one -- important with a pool this size
 * (4 items, 4 fetch NPCs as of the town/house content pass), where a
 * naive independent-random-pick-per-NPC could otherwise double up.
 *
 * rng_seed()'s argument is a fixed compile-time constant, not real
 * entropy (see rng.h) -- so today this reliably produces the same
 * assignment every run. That's fine for proving the pipeline; a real
 * seed source is separate work for whenever it matters that replays
 * differ. */
#define QUEST_RNG_SEED 0x2A

static unsigned char generate_quests(void)
{
    unsigned char pool[ITEM_COUNT];
    unsigned char verbs[NPC_COUNT], item_ids[NPC_COUNT], i;
    RngState quest_rng;

    for (i = 0; i < NPC_COUNT; ++i) {
        verbs[i] = npcs[i].verb;
        item_ids[i] = QUEST_ITEM_NONE;
    }
    rng_seed(&quest_rng, QUEST_RNG_SEED);
    if (!quest_assign_fetch_items(verbs, NPC_COUNT, ITEM_COUNT, item_ids, pool, &quest_rng)) return 0;
    for (i = 0; i < NPC_COUNT; ++i) if (npcs[i].verb == QUEST_FETCH) npcs[i].item_id = item_ids[i];
    return 1;
}

/* Phase 6's second slice: the enemy wanders (update_enemy_wander()
 * below) and touching it costs the player HP (main()'s contact-damage
 * check) -- hp/evasion are the "small stat block" the combat design
 * section calls for; alive tracks whether it's been defeated (parallel
 * to Item's `taken` -- once false, it's gone for the rest of the
 * session, no respawn). flash_timer/wander_pad/wander_timer are all
 * runtime state, not authored data -- every enemy starts them at 0. */
typedef struct {
    RoomId room;
    unsigned char x, y;
    unsigned char hp;
    unsigned char max_hp;
    unsigned char evasion;
    unsigned char alive;
    unsigned char flash_timer;    /* >0 while blinking from a recent non-lethal hit */
    unsigned char wander_pad;     /* current move direction: a PAD_*_MASK, or 0 = standing still */
    unsigned char wander_timer;   /* frames left before picking a new wander direction */
    unsigned char knockback_pad;
    unsigned char knockback_timer;
} Enemy;

static Enemy enemies[] = {
    { ROOM_CANYON, 80, 160, 3, 3, 20, 1, 0, 0, 0, 0, 0 },
};
#define ENEMY_COUNT (sizeof(enemies) / sizeof(enemies[0]))

/* SWORD_SLOT deliberately gets the lower OAM index (higher render
 * priority) here, before ENEMY_SLOT -- NES sprite-vs-sprite priority is
 * purely OAM slot order, lowest wins, with no per-sprite "draw in
 * front" bit for that (only a bg-vs-sprite priority bit exists). A
 * successful hit lands the sword's visual position almost exactly on
 * top of the enemy's (that's what boxes_overlap() being true *means*),
 * so with the enemy slot first, the enemy -- solidly filled, unlike the
 * sword's thin diagonal line -- was drawing over it completely on every
 * hit, which is exactly the case that matters most to see. */
/* The bar's bartender/counter claim the slots before ITEM_OAM_BASE. Keep
 * gameplay sprites after the fixed item range so a sword hide can never
 * erase part of Jimbles. */
#define SWORD_SLOT    (ITEM_OAM_BASE + ITEM_COUNT)
#define WORLD_ENTITY_OAM_BASE (SWORD_SLOT + 1)
#define ENEMY_SLOT(i) (WORLD_ENTITY_OAM_BASE + (i))

/* Whether an enemy's 8x8 box at (x, y) overlaps a blocking tile --
 * movement.c's equivalent (box_blocked()) is static/private and also
 * checks an NPC-style Obstacle the enemy doesn't need, so this is its
 * own small copy rather than changing movement.c's public API for one
 * caller. Deliberately does *not* also check the player as an obstacle
 * -- walking into the player is how contact damage happens, not
 * something to path around. */
static RngState simulation_rng;

/* Advances one enemy's wander state by one frame: counts down to its
 * next direction change, and steps ENEMY_MOVE_SPEED pixels that way if
 * nothing blocks it. Hitting a wall or the screen edge cuts the current
 * direction short (wander_timer forced to 0) instead of standing there
 * doing nothing until the timer naturally expires -- reads as "changed
 * its mind" rather than "got stuck". No pathing, no chase/flee -- pure
 * ambient movement, same spirit as an NES enemy that just paces. */
static void update_enemy_wander(unsigned char idx)
{
    Enemy *e = &enemies[idx];
    const RoomLayout *layout;
    unsigned char nx, ny;
    unsigned char pick;

    if (!e->alive) {
        return;
    }

    if (e->wander_timer == 0) {
        pick = rng_next(&simulation_rng) % 5;   /* 0 = stand still, 1-4 = one direction each */
        switch (pick) {
            case 1:  e->wander_pad = PAD_UP_MASK;    break;
            case 2:  e->wander_pad = PAD_DOWN_MASK;  break;
            case 3:  e->wander_pad = PAD_LEFT_MASK;  break;
            case 4:  e->wander_pad = PAD_RIGHT_MASK; break;
            default: e->wander_pad = 0;              break;
        }
        e->wander_timer = ENEMY_WANDER_MIN_FRAMES +
                           (rng_next(&simulation_rng) % (ENEMY_WANDER_MAX_FRAMES - ENEMY_WANDER_MIN_FRAMES));
    } else {
        --e->wander_timer;
    }

    if (e->wander_pad == 0) {
        return;
    }

    nx = e->x;
    ny = e->y;
    if (e->wander_pad == PAD_UP_MASK && e->y > SCREEN_MIN_Y) {
        ny = e->y - ENEMY_MOVE_SPEED;
    } else if (e->wander_pad == PAD_DOWN_MASK && e->y < SCREEN_MAX_Y) {
        ny = e->y + ENEMY_MOVE_SPEED;
    } else if (e->wander_pad == PAD_LEFT_MASK && e->x > 0) {
        nx = e->x - ENEMY_MOVE_SPEED;
    } else if (e->wander_pad == PAD_RIGHT_MASK && e->x < SCREEN_MAX_X) {
        nx = e->x + ENEMY_MOVE_SPEED;
    } else {
        e->wander_timer = 0;   /* flush against an edge -- pick again next frame */
        return;
    }

    layout = &room_layouts[e->room];
    if (box_hits_tiles(layout, nx, ny)) {
        e->wander_timer = 0;
        return;
    }
    e->x = nx;
    e->y = ny;
}

/* Ambient wildlife: the town's wandering animals. Same wander state
 * machine as Enemy (reusing box_hits_tiles() and the
 * ENEMY_MOVE_SPEED/ENEMY_WANDER_*_FRAMES constants -- the pacing has
 * nothing enemy-specific about it), minus everything combat-related
 * (hp/evasion/alive/flash_timer). Purely decorative -- nothing checks
 * for the player touching one. */
typedef struct {
    RoomId room;
    unsigned char x, y;
    unsigned char wander_pad;
    unsigned char wander_timer;
} Critter;

static Critter critters[] = {
    { ROOM_PLAINS, 48, 176, 0, 0 },
    { ROOM_PLAINS, 232, 64, 0, 0 },
};
#define CRITTER_COUNT (sizeof(critters) / sizeof(critters[0]))
/* Enemies, critters, and portals live in mutually exclusive rooms, so they
 * deliberately share the two remaining OAM slots rather than exceed 64. */
#define CRITTER_SLOT(i) (WORLD_ENTITY_OAM_BASE + (i))
#define PORTAL_SLOT(i) (WORLD_ENTITY_OAM_BASE + (i))

static void update_critter_wander(unsigned char idx)
{
    Critter *c = &critters[idx];
    const RoomLayout *layout;
    unsigned char nx, ny;
    unsigned char pick;

    if (c->wander_timer == 0) {
        pick = rng_next(&simulation_rng) % 5;   /* 0 = stand still, 1-4 = one direction each */
        switch (pick) {
            case 1:  c->wander_pad = PAD_UP_MASK;    break;
            case 2:  c->wander_pad = PAD_DOWN_MASK;  break;
            case 3:  c->wander_pad = PAD_LEFT_MASK;  break;
            case 4:  c->wander_pad = PAD_RIGHT_MASK; break;
            default: c->wander_pad = 0;              break;
        }
        c->wander_timer = ENEMY_WANDER_MIN_FRAMES +
                           (rng_next(&simulation_rng) % (ENEMY_WANDER_MAX_FRAMES - ENEMY_WANDER_MIN_FRAMES));
    } else {
        --c->wander_timer;
    }

    if (c->wander_pad == 0) {
        return;
    }

    nx = c->x;
    ny = c->y;
    if (c->wander_pad == PAD_UP_MASK && c->y > SCREEN_MIN_Y) {
        ny = c->y - ENEMY_MOVE_SPEED;
    } else if (c->wander_pad == PAD_DOWN_MASK && c->y < SCREEN_MAX_Y) {
        ny = c->y + ENEMY_MOVE_SPEED;
    } else if (c->wander_pad == PAD_LEFT_MASK && c->x > 0) {
        nx = c->x - ENEMY_MOVE_SPEED;
    } else if (c->wander_pad == PAD_RIGHT_MASK && c->x < SCREEN_MAX_X) {
        nx = c->x + ENEMY_MOVE_SPEED;
    } else {
        c->wander_timer = 0;   /* flush against an edge -- pick again next frame */
        return;
    }

    layout = &room_layouts[c->room];
    if (box_hits_tiles(layout, nx, ny)) {
        c->wander_timer = 0;
        return;
    }
    c->x = nx;
    c->y = ny;
}

/* The player's weapon, for now: a single fixed stat block, no
 * inventory or switching yet -- ROADMAP.md's weapon variety (dagger/
 * sword/blunt with different speed/damage/accuracy) is later work this
 * is standing in for with one hardcoded "starting dagger" profile. */
#define PLAYER_ACCURACY 70
#define PLAYER_DAMAGE   1

/* Stat-driven hit resolution: accuracy vs. evasion decides hit or miss.
 * This is the actual "Morrowind resolution" the combat design section
 * describes -- a visually clean swing (the attack box overlapping the
 * enemy, checked by the caller before this ever runs) can still miss.
 * Clamped to 5..95% so no matchup is ever a guaranteed hit or a
 * guaranteed miss, however lopsided the stats get once there's more
 * than one weapon/enemy to combine. */
static unsigned char roll_hit(unsigned char accuracy, unsigned char evasion)
{
    unsigned char chance = accuracy > evasion ? accuracy - evasion : 0;
    if (chance < 5)  chance = 5;
    if (chance > 95) chance = 95;
    return (rng_next(&simulation_rng) % 100) < chance;
}

/* Short enough to see the NPC move between rooms within a normal test
 * session, not meant to represent a real in-game day yet. The walk
 * itself (update_npc_walk()) takes well under this either direction, so
 * the NPC always finishes and rests before the next swap is due. */
#define NPC_SCHEDULE_PERIOD 300
#define NPC_WALK_SPEED 1

/* schedule_tick counts frames up to NPC_SCHEDULE_PERIOD and wraps,
 * flipping schedule_phase -- tracked incrementally instead of the more
 * obvious "frames-since-boot counter divided by the period" because the
 * 6502 has no hardware divide; cc65 has to call a software division
 * routine for it, and npc_current_room() used to do exactly that, once
 * per NPC, every single frame (5 divisions/frame at the current NPC
 * count, all to recompute the same 1-bit answer 5 times). An increment
 * and a compare each frame is enough to track the identical schedule --
 * same swap timing, just without paying for a division nothing actually
 * needed. */
static unsigned int schedule_tick = 0;
static unsigned char schedule_phase = 0;

static RoomId npc_current_room(const Npc *npc)
{
    return schedule_phase ? npc->work : npc->home;
}

/* Where an NPC actually is on screen right now -- may lag behind
 * npc_current_room() while mid-walk between the two. Room is whichever
 * of home/work it's currently in or crossing through; (x, y) is only
 * meaningful together with that room. */
typedef struct {
    RoomId room;
    unsigned char x, y;
} NpcState;

static NpcState npc_state[NPC_COUNT];
static unsigned char bartender_walk_timer;
static unsigned char bartender_walk_right;
static unsigned char bartender_is_walking;

#define BARTENDER_MIN_X 96
#define BARTENDER_MAX_X 184

static void npc_state_init(void)
{
    unsigned char i;
    for (i = 0; i < NPC_COUNT; ++i) {
        npc_state[i].room = npcs[i].home;
        npc_state[i].x = npcs[i].x;
        npc_state[i].y = npcs[i].y;
    }
    bartender_walk_timer = 0;
    bartender_walk_right = 1;
    bartender_is_walking = 0;
}

/* Jimbles spends most of his time still behind the counter, occasionally
 * taking a short random shift left or right before settling again. */
static void update_bartender_pace(void)
{
    NpcState *bartender = &npc_state[JIMBLES_BARTENDER_NPC];

    if (bartender_walk_timer == 0) {
        if (bartender_is_walking) {
            bartender_is_walking = 0;
            bartender_walk_timer = 120 + (rng_next(&simulation_rng) % 180);
        } else {
            bartender_is_walking = 1;
            bartender_walk_right = rng_next(&simulation_rng) & 1;
            bartender_walk_timer = 24 + (rng_next(&simulation_rng) % 48);
        }
    } else {
        --bartender_walk_timer;
    }
    if (!bartender_is_walking) return;
    if (bartender_walk_right) {
        if (bartender->x < BARTENDER_MAX_X) ++bartender->x;
        else {
            bartender_is_walking = 0;
            bartender_walk_timer = 120 + (rng_next(&simulation_rng) % 180);
        }
    } else {
        if (bartender->x > BARTENDER_MIN_X) --bartender->x;
        else {
            bartender_is_walking = 0;
            bartender_walk_timer = 120 + (rng_next(&simulation_rng) % 180);
        }
    }
}

/* Which exit direction leads from `from` toward `to`, assuming they're
 * directly connected -- true of every home/work pair this schedule
 * system supports so far (the desert NPC's home == work, so it never
 * even reaches this function). A pair that isn't directly connected is
 * a data bug (not something to route around at runtime): direction_to()
 * would fall through to EXIT_WEST, silently sending the NPC the wrong
 * way, rather than getting stuck anywhere obviously wrong -- fine for
 * now with hand-placed NPCs, worth a real check once routes are
 * generated instead. */
static unsigned char direction_to(RoomId from, RoomId to)
{
    const RoomConnections *c = &room_connections[from];
    if (c->north == to) return EXIT_NORTH;
    if (c->east  == to) return EXIT_EAST;
    if (c->south == to) return EXIT_SOUTH;
    return EXIT_WEST;
}

/* The point (in the room being left) an NPC walks to before crossing,
 * and the point (in the room being entered) it appears at right after
 * -- mirrors main()'s own room-transition coordinates (crossing north
 * lands the player at SCREEN_MAX_Y, etc.) so an NPC's crossing lines up
 * with the same exit gap the player uses. */
static unsigned char exit_edge_x(unsigned char dir, unsigned char x)
{
    if (dir == EXIT_EAST) return SCREEN_MAX_X;
    if (dir == EXIT_WEST) return 0;
    return x;
}
static unsigned char exit_edge_y(unsigned char dir, unsigned char y)
{
    if (dir == EXIT_NORTH) return SCREEN_MIN_Y;
    if (dir == EXIT_SOUTH) return SCREEN_MAX_Y;
    return y;
}
static unsigned char entry_edge_x(unsigned char dir, unsigned char x)
{
    if (dir == EXIT_EAST) return 0;
    if (dir == EXIT_WEST) return SCREEN_MAX_X;
    return x;
}
static unsigned char entry_edge_y(unsigned char dir, unsigned char y)
{
    if (dir == EXIT_NORTH) return SCREEN_MAX_Y;
    if (dir == EXIT_SOUTH) return SCREEN_MIN_Y;
    return y;
}

static unsigned char step_toward(unsigned char cur, unsigned char target)
{
    if (cur < target) return cur + NPC_WALK_SPEED;
    if (cur > target) return cur - NPC_WALK_SPEED;
    return cur;
}

/* Advances one NPC's on-screen position by one frame's worth of
 * walking, if its schedule currently disagrees with where it actually
 * is. Runs every frame for every NPC regardless of which room the
 * player is currently viewing, so a crossing always takes the same
 * number of frames whether or not anyone's watching it happen. */
static void update_npc_walk(unsigned char idx)
{
    const Npc *npc = &npcs[idx];
    NpcState *st = &npc_state[idx];
    RoomId scheduled_room = npc_current_room(npc);
    unsigned char dir, exit_x, exit_y;

    if (st->room == scheduled_room) {
        /* Already in the right room -- walk the last leg to its actual
         * rest spot there, if it hasn't reached it yet. */
        st->x = step_toward(st->x, npc->x);
        st->y = step_toward(st->y, npc->y);
        return;
    }

    dir = direction_to(st->room, scheduled_room);
    exit_x = exit_edge_x(dir, npc->x);
    exit_y = exit_edge_y(dir, npc->y);

    if (st->x != exit_x || st->y != exit_y) {
        st->x = step_toward(st->x, exit_x);
        st->y = step_toward(st->y, exit_y);
        return;
    }

    /* Reached the exit -- cross into the connected room, appearing at
     * the matching entrance; the next call walks the remaining leg to
     * its rest spot there (handled by the branch above). */
    switch (dir) {
        case EXIT_NORTH: st->room = room_connections[st->room].north; break;
        case EXIT_EAST:  st->room = room_connections[st->room].east;  break;
        case EXIT_SOUTH: st->room = room_connections[st->room].south; break;
        default:         st->room = room_connections[st->room].west;  break;
    }
    st->x = entry_edge_x(dir, npc->x);
    st->y = entry_edge_y(dir, npc->y);
}

/* Returns the index into npcs[]/npc_state[] of the NPC actually
 * standing in `room` right now (NPC_NONE if none is), based on its
 * current on-screen position, not just its schedule -- mid-walk, it may
 * still be in the room it's leaving, or already in the one it's
 * entering, independent of which side of the swap schedule_phase is on. */
static unsigned char npc_in_room(RoomId room)
{
    unsigned char i;
    for (i = 0; i < NPC_COUNT; ++i) {
        if (npc_state[i].room == room) return i;
    }
    return NPC_NONE;
}

static RoomId current_room = ROOM_MEADOW;

#define BAR_JUKEBOX_X 208
#define BAR_JUKEBOX_Y 160
#define BAR_BACKROOM_X 32
/* With INTERACT_RANGE=4 and strict box edges, this reaches from the bar's
 * northernmost legal player Y (124) while remaining centered on the door. */
#define BAR_BACKROOM_Y 116
#define CASINO_SLOT_X 120
#define CASINO_SLOT_Y 136
#define CASINO_SLOT_RANGE 20
#define CASINO_EXIT_LEFT 104
#define CASINO_EXIT_RIGHT 144
#define CASINO_EXIT_Y 216

typedef struct {
    unsigned char x, y;
    const char *dialogue;
} BarRegular;

/* These patrons deliberately use the normal NPC slots while the other
 * adventuring NPCs are off-screen. They are seated scenery, not quest NPCs. */
static const BarRegular bar_regulars[] = {
    {104, 128, "LONG STORY."},
    {176, 128, "PEANUTS."},
};
#define BAR_REGULAR_COUNT (sizeof(bar_regulars) / sizeof(bar_regulars[0]))

/* Resets mutable session state without touching authored room/entity data.
 * Both the title screen's New Game action and the pause menu's Quit action
 * use this path, so a new run never inherits collected items or defeated
 * enemies from the one before it. */
static unsigned char begin_new_game(unsigned char *x, unsigned char *y)
{
    unsigned char i;

    for (i = 0; i < ITEM_COUNT; ++i) items[i].taken = 0;
    bar_backroom_unlocked = 0;
    for (i = 0; i < NPC_COUNT; ++i) {
        npcs[i].item_id = 0;
        npcs[i].quest_complete = 0;
    }
    for (i = 0; i < ENEMY_COUNT; ++i) {
        enemies[i].hp = enemies[i].max_hp;
        enemies[i].alive = 1;
        enemies[i].flash_timer = 0;
        enemies[i].wander_pad = 0;
        enemies[i].wander_timer = 0;
        enemies[i].knockback_pad = 0;
        enemies[i].knockback_timer = 0;
    }
    for (i = 0; i < CRITTER_COUNT; ++i) {
        critters[i].wander_pad = 0;
        critters[i].wander_timer = 0;
    }
    schedule_tick = 0;
    schedule_phase = 0;
    if (!generate_quests()) return 0;
    rng_seed(&simulation_rng, 0xA5);
    npc_state_init();
    current_room = ROOM_MEADOW;
    *x = 72;
    *y = 176;
    return 1;
}

/* Strobe the controller and shift in 8 bits: A,B,Select,Start,Up,Down,Left,Right */
static unsigned char read_pad(void)
{
    unsigned char result = 0;
    unsigned char i;

    JOYPAD1 = 1;
    JOYPAD1 = 0;

    for (i = 0; i < 8; ++i) {
        unsigned char bit = JOYPAD1 & 0x01;
        result |= (unsigned char)(bit << i);
    }

    return result;
}

static void load_palette(RoomId room)
{
    unsigned char i;

    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x00;
    for (i = 0; i < 32; ++i) {
        PPU.vram.data = room_palettes[room][i];
    }
}

/* Town's two houses each need 3 different tiles at 3 exact cells (see
 * house.s) rather than the one uniform obstacle tile every other room's
 * decorations use -- this is the one place that distinction is made, so
 * room.c's model of "obstacle cells" stays exactly as generic as every
 * other room needs it to be. The 4th cell of each 2x2 footprint (the
 * door) isn't listed here at all, because it isn't in town_decorations
 * either -- it's ordinary floor with a Portal sprite drawn over it (see
 * portals[]), not a background tile. */
static unsigned char town_house_tile(unsigned char row, unsigned char col)
{
    if (row == 6  && col ==  6) return CHR_TILE_HOUSE_ROOF_L;
    if (row == 6  && col ==  7) return CHR_TILE_HOUSE_ROOF_R;
    if (row == 7  && col ==  7) return CHR_TILE_HOUSE_WALL;
    if (row == 20 && col == 22) return CHR_TILE_HOUSE_ROOF_L;
    if (row == 20 && col == 23) return CHR_TILE_HOUSE_ROOF_R;
    if (row == 21 && col == 23) return CHR_TILE_HOUSE_WALL;
    return CHR_TILE_HOUSE_WALL;
}

/* Same idea as town_house_tile() -- most of a house interior's blocking
 * cells are just its wall (room_obstacle_tile[room], the reused tree
 * tile), except the exact cells house1_decorations/house2_decorations
 * marked as furniture, which get table/bed art instead. */
static unsigned char house_furniture_tile(RoomId room, unsigned char row, unsigned char col,
                                           unsigned char wall_tile)
{
    if (room == ROOM_HOUSE) {
        if (row == 12 && col == 13) return FURNITURE_TABLE_TILE;
        if (row == 14 && col == 18) return FURNITURE_BED_TILE;
    } else {
        if (row == 13 && col == 18) return FURNITURE_TABLE_TILE;
        if (row == 15 && col == 13) return FURNITURE_BED_TILE;
    }
    return wall_tile;
}

static unsigned char neon_avenue_tile(RoomTile tile, unsigned char row, unsigned char col)
{
    if (tile == TILE_FLOOR) return NEON_TILE_ASPHALT;
    if (tile == TILE_PATH) return NEON_TILE_SIDEWALK;
    if (tile == TILE_TREE) {
        if (col == 5 && row >= 18 && row <= 20) return NEON_TILE_TAXI_POST;
        if (row == 2) return NEON_TILE_AWNING;
        if (row == 3) return NEON_TILE_BRICK;
        return NEON_TILE_BRICK;
    }
    return NEON_TILE_ASPHALT;
}

static unsigned char chr_tile_for(RoomId room, RoomTile tile, unsigned char row, unsigned char col)
{
    if (room == ROOM_MEADOW) return neon_avenue_tile(tile, row, col);

    switch (tile) {
        case TILE_PATH: return CHR_TILE_PATH;
        case TILE_VOID: return CHR_TILE_PATH;
        case TILE_TREE:
            if (room == ROOM_TOWN) return town_house_tile(row, col);
            if (room == ROOM_HOUSE || room == ROOM_HOUSE2) {
                return house_furniture_tile(room, row, col, room_obstacle_tile[room]);
            }
            return room_obstacle_tile[room];
        case TILE_FLOOR:
        default:        return CHR_TILE_FLOOR;
    }
}

/* Writes room's normal tiles for rows [first_row, first_row+row_count),
 * from room.c's layout. Used both for the initial full-room draw and to
 * restore just the text box's rows when it closes (hide_text_box()) --
 * pulling this out as its own function means "what do these rows
 * normally look like" has exactly one implementation, so the room and
 * the text box can never disagree about it.
 *
 * Must run while rendering is off (PPU.mask's show-bg/show-sprite bits
 * clear). Unlike per-frame OAM updates, this isn't vblank-limited -- with
 * rendering off the PPU never scans out, so there's no deadline; it's
 * fine for this to take a while. */
static void draw_room_rows(RoomId room, unsigned char first_row, unsigned char row_count)
{
    unsigned char row, col;
    const RoomLayout *layout = &room_layouts[room];

    PPU.vram.address = (unsigned char)(0x20 + (first_row >> 3));
    PPU.vram.address = (unsigned char)(first_row << 5);

    for (row = first_row; row < first_row + row_count; ++row) {
        for (col = 0; col < ROOM_WIDTH_TILES; ++col) {
            PPU.vram.data = chr_tile_for(room, room_tile_at(layout, row, col), row, col);
        }
    }
}

/* Writes one 4-tile-high attribute row. Keeping this independently
 * drawable is important for dialogue dismissal: its four tile rows map to
 * exactly one attribute row, so closing the box need not rebuild palette
 * state for the other 896 background tiles while the screen is blanked. */
static void draw_room_attribute_row(RoomId room, unsigned char attr_row)
{
    unsigned char row, col;
    unsigned char attr[8];
    const RoomDecoration *d;

    for (col = 0; col < 8; ++col) {
        attr[col] = 0;
    }
    d = room_variant[room];
    if (d != 0) {
        while (!(d->row == 0xFF && d->col == 0xFF)) {
            if ((d->row >> 2) == attr_row) {
                unsigned char shift = ((d->row & 2) ? 4 : 0) + ((d->col & 2) ? 2 : 0);
                attr[d->col >> 2] |= (unsigned char)(2 << shift);
            }
            ++d;
        }
    }

    /* Palette 3 is all black in every room palette. Void uses the solid
     * path tile, whose index-3 pixels therefore become the dark surround. */
    for (row = (unsigned char)(attr_row << 2); row < (unsigned char)((attr_row + 1) << 2); ++row) {
        for (col = 0; col < ROOM_WIDTH_TILES; ++col) {
            if (room_tile_at(&room_layouts[room], row, col) == TILE_VOID) {
                unsigned char shift = ((row & 2) ? 4 : 0) + ((col & 2) ? 2 : 0);
                unsigned char *entry = &attr[col >> 2];
                *entry = (unsigned char)((*entry & ~(unsigned char)(3 << shift)) | (3 << shift));
            }
        }
    }

    PPU.vram.address = 0x23;
    PPU.vram.address = (unsigned char)(0xC0 + attr_row * 8);
    for (col = 0; col < 8; ++col) {
        PPU.vram.data = attr[col];
    }
}

/* Full room draw: every row, plus the 64-byte attribute table. Defaults
 * to background palette 0, applies palette 2 to decorations, and palette 3
 * to compact-interior void. See draw_room_attribute_row() for the exact
 * quadrant packing and why a single row can safely be restored on its own. */
static void draw_room_attributes(RoomId room)
{
    unsigned char attr_row;

    for (attr_row = 0; attr_row < 8; ++attr_row) {
        draw_room_attribute_row(room, attr_row);
    }
}

static void draw_room(RoomId room)
{
    draw_room_rows(room, 0, ROOM_HEIGHT_TILES);
    draw_room_attributes(room);
}

static void draw_neon_rect(unsigned char top_row, unsigned char left_col,
                           unsigned char width, unsigned char height,
                           unsigned char tile)
{
    unsigned char row;
    unsigned char col;

    for (row = top_row; row < top_row + height; ++row) {
        PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
        PPU.vram.address = (unsigned char)((row << 5) + left_col);
        for (col = 0; col < width; ++col) {
            PPU.vram.data = tile;
        }
    }
}

static void draw_neon_window(unsigned char top_row, unsigned char left_col,
                             unsigned char width, unsigned char height)
{
    unsigned char row;

    draw_neon_rect(top_row, left_col, width, 1, NEON_TILE_WINDOW_H);
    for (row = top_row + 1; row < top_row + height - 1; ++row) {
        set_tile(row, left_col, NEON_TILE_WINDOW_V);
        draw_neon_rect(row, left_col + 1, width - 2, 1, NEON_TILE_WINDOW_FILL);
        set_tile(row, left_col + width - 1, NEON_TILE_WINDOW_V);
    }
    draw_neon_rect(top_row + height - 1, left_col, width, 1, NEON_TILE_WINDOW_H);
}

static void draw_title_plate(unsigned char top_row, unsigned char left_col,
                             unsigned char width, unsigned char height);
static void reset_scroll(void);

/* Fixed-camera adventure tableau: the upper half reads as storefronts against
 * a night sky while the bottom half remains a broad walkable street. It is
 * deliberately rendered directly rather than inferred from collision tiles,
 * giving future scenes freedom to be scenic and side-on. */
static void draw_neon_avenue_scene(void)
{
    unsigned char attr_row;
    unsigned char attr_col;

    for (attr_row = 0; attr_row < 8; ++attr_row) set_attribute_row(attr_row, 0);
    for (attr_row = 0; attr_row < ROOM_HEIGHT_TILES; ++attr_row) {
        fill_row(attr_row, NEON_TILE_ASPHALT);
    }

    /* A single stage-like facade: sky at the top, one low-slung bar filling
     * the middle, and just enough pavement for the actor to cross. */
    draw_neon_rect(4, 1, 30, 15, NEON_TILE_BRICK);
    draw_neon_rect(3, 3, 26, 1, NEON_TILE_AWNING);
    draw_neon_rect(4, 1, 30, 1, NEON_TILE_AWNING);
    draw_neon_window(10, 3, 4, 3);
    draw_neon_window(10, 20, 4, 3);
    draw_neon_window(10, 25, 4, 3);
    draw_neon_rect(14, 14, 1, 5, NEON_TILE_BAR_DOOR_L);
    draw_neon_rect(14, 15, 1, 5, NEON_TILE_BAR_DOOR_C);
    draw_neon_rect(14, 16, 1, 5, NEON_TILE_BAR_DOOR_R);
    set_tile(5, 7, NEON_TILE_SIGN_TL);
    draw_neon_rect(5, 8, 16, 1, NEON_TILE_SIGN_T);
    set_tile(5, 24, NEON_TILE_SIGN_TR);
    set_tile(6, 7, NEON_TILE_SIGN_L);
    draw_neon_rect(6, 8, 16, 1, NEON_TILE_SIGN_FILL);
    set_tile(6, 24, NEON_TILE_SIGN_R);
    set_tile(7, 7, NEON_TILE_SIGN_BL);
    draw_neon_rect(7, 8, 16, 1, NEON_TILE_SIGN_B);
    set_tile(7, 24, NEON_TILE_SIGN_BR);
    draw_neon_rect(19, 0, ROOM_WIDTH_TILES, 2, NEON_TILE_SIDEWALK);
    set_tile(14, 2, NEON_TILE_TAXI_POST);
    set_tile(15, 2, NEON_TILE_TAXI_POST);
    set_tile(16, 2, NEON_TILE_TAXI_POST);
    set_tile(17, 2, NEON_TILE_TAXI_POST);
    /* A recessed 16x16 manhole in the street is the sewer entrance. */
    set_tile(21, 8, BAR_TILE_JUKEBOX);
    set_tile(21, 9, BAR_TILE_JUKEBOX);
    set_tile(22, 8, BAR_TILE_JUKEBOX);
    set_tile(22, 9, BAR_TILE_JUKEBOX);

    for (attr_row = 1; attr_row < 5; ++attr_row) {
        for (attr_col = 0; attr_col < 8; ++attr_col) {
            set_attribute_block(attr_row, attr_col, 2);
        }
    }
    for (attr_col = 1; attr_col < 7; ++attr_col) set_attribute_block(1, attr_col, 1);
    set_attribute_block(3, 3, 0);
    set_attribute_block(3, 4, 0);
    set_attribute_block(4, 3, 0);
    set_attribute_block(4, 4, 0);
    draw_text(6, 9, FONT_TILE_BASE, jimbles_bar_title);
}

/* The old town map is replaced by a compact adventure interior: the
 * counter/bottle wall is scenery, while the lower floor is the stage where
 * the player can walk, talk to the bartender, and leave through the south. */
static void draw_jimbles_bar_scene(void)
{
    unsigned char attr_row;

    for (attr_row = 0; attr_row < 8; ++attr_row) set_attribute_row(attr_row, 0);
    for (attr_row = 0; attr_row < ROOM_HEIGHT_TILES; ++attr_row) {
        fill_row(attr_row, BAR_TILE_FLOOR);
    }
    draw_neon_rect(0, 0, ROOM_WIDTH_TILES, 16, BAR_TILE_WALL);
    draw_neon_rect(9, 3, 5, 1, BAR_TILE_BOTTLES);
    draw_neon_rect(9, 12, 5, 1, BAR_TILE_BOTTLES);
    draw_neon_rect(9, 22, 5, 1, BAR_TILE_BOTTLES);
    draw_neon_rect(14, 2, 28, 2, BAR_TILE_COUNTER);
    draw_neon_rect(12, 2, 1, 4, BAR_TILE_BACK_DOOR);
    draw_neon_rect(12, 3, 1, 4, BAR_TILE_BACK_DOOR);
    draw_neon_rect(12, 4, 1, 4, BAR_TILE_STOOL);
    /* Two stools sit directly below the counter, with their seated regulars
     * supplied by the sprite layer so their dialogue stays interactive. */
    set_tile(17, 13, BAR_TILE_STOOL);
    set_tile(17, 22, BAR_TILE_STOOL);
    set_tile(17, 25, BAR_TILE_DOOR);
    set_tile(17, 26, BAR_TILE_BOOTH);
    set_tile(17, 27, BAR_TILE_DOOR);
    draw_neon_rect(18, 25, 1, 3, BAR_TILE_DOOR);
    draw_neon_rect(18, 26, 1, 3, BAR_TILE_JUKEBOX);
    draw_neon_rect(18, 27, 1, 3, BAR_TILE_DOOR);
    draw_title_plate(0, 1, 30, 6);
    draw_text(3, 10, FONT_TILE_BASE, jimbles_bar_title);
    for (attr_row = 0; attr_row < 2; ++attr_row) {
        unsigned char attr_col;
        for (attr_col = 0; attr_col < 8; ++attr_col) set_attribute_block(attr_row, attr_col, 1);
    }
    set_tile(27, 13, BAR_TILE_EXIT_TL);
    draw_neon_rect(27, 14, 4, 1, BAR_TILE_EXIT_T);
    set_tile(27, 18, BAR_TILE_EXIT_TR);
    set_tile(28, 13, BAR_TILE_EXIT_L);
    draw_neon_rect(28, 14, 4, 1, BAR_TILE_EXIT_FILL);
    set_tile(28, 18, BAR_TILE_EXIT_R);
    set_tile(29, 13, BAR_TILE_EXIT_BL);
    draw_neon_rect(29, 14, 4, 1, BAR_TILE_EXIT_B);
    set_tile(29, 18, BAR_TILE_EXIT_BR);
}

/* A compact, sealed sewer chamber below Neon Avenue. The ladder is authored
 * from vertical rails and bright rungs, with a dark water channel below. */
static void draw_sewer_scene(void)
{
    unsigned char attr_row;
    unsigned char row;

    for (attr_row = 0; attr_row < 8; ++attr_row) set_attribute_row(attr_row, 0);
    for (row = 0; row < ROOM_HEIGHT_TILES; ++row) fill_row(row, BAR_TILE_FLOOR);
    draw_neon_rect(0, 0, ROOM_WIDTH_TILES, 4, BAR_TILE_WALL);
    draw_neon_rect(0, 0, 2, ROOM_HEIGHT_TILES, BAR_TILE_WALL);
    draw_neon_rect(0, 30, 2, ROOM_HEIGHT_TILES, BAR_TILE_WALL);
    draw_neon_rect(25, 2, 28, 5, NEON_TILE_ASPHALT);
    for (row = 3; row < 25; ++row) {
        set_tile(row, 14, NEON_TILE_WINDOW_V);
        set_tile(row, 17, NEON_TILE_WINDOW_V);
        if ((row & 1) == 0) {
            set_tile(row, 15, NEON_TILE_SIDEWALK);
            set_tile(row, 16, NEON_TILE_SIDEWALK);
        }
    }
}

static void draw_casino_scene(void)
{
    unsigned char attr_row;

    for (attr_row = 0; attr_row < 8; ++attr_row) set_attribute_row(attr_row, 0);
    for (attr_row = 0; attr_row < ROOM_HEIGHT_TILES; ++attr_row) fill_row(attr_row, BAR_TILE_FLOOR);
    draw_neon_rect(0, 0, ROOM_WIDTH_TILES, 8, BAR_TILE_WALL);
    draw_title_plate(1, 4, 24, 5);
    draw_text(3, 8, FONT_TILE_BASE, casino_sign_text);
    for (attr_row = 0; attr_row < 2; ++attr_row) {
        unsigned char attr_col;
        for (attr_col = 1; attr_col < 7; ++attr_col) set_attribute_block(attr_row, attr_col, 1);
    }
    draw_neon_rect(12, 13, 3, 5, BAR_TILE_DOOR);
    set_tile(12, 14, BAR_TILE_BOOTH);
    set_tile(14, 14, BAR_TILE_JUKEBOX);
    draw_neon_rect(14, 21, 8, 3, BAR_TILE_COUNTER);
    draw_neon_rect(17, 23, 4, 1, BAR_TILE_STOOL);
    draw_text(12, 21, FONT_TILE_BASE, casino_blackjack_text);
    /* Background palette 2 is dedicated to the table's green felt. */
    set_attribute_block(3, 5, 2);
    set_attribute_block(3, 6, 2);
    set_attribute_block(3, 7, 2);
    set_attribute_block(4, 5, 2);
    set_attribute_block(4, 6, 2);
    set_attribute_block(4, 7, 2);
    draw_neon_rect(27, 13, 6, 3, BAR_TILE_EXIT_FILL);
    draw_text(28, 14, FONT_TILE_BASE, casino_exit_text);
}

static void draw_slot_number(unsigned char row, unsigned char col, unsigned char value)
{
    if (value >= 100) {
        set_tile(row, col++, FONT_TILE_BASE + font_glyph_index((char)('0' + value / 100)));
        value %= 100;
    }
    if (value >= 10) {
        set_tile(row, col++, FONT_TILE_BASE + font_glyph_index((char)('0' + value / 10)));
        value %= 10;
    }
    set_tile(row, col, FONT_TILE_BASE + font_glyph_index((char)('0' + value)));
}

static void show_slot_game(unsigned char wager, unsigned char credits, const char *result,
                           const unsigned char *reels, unsigned char spinning)
{
    unsigned char row, attr_row, attr_col;

    PPU.mask = 0x00;
    for (row = 0; row < ROOM_HEIGHT_TILES; ++row) fill_row(row, FONT_TILE_BASE + FONT_GLYPH_SPACE);
    for (attr_row = 0; attr_row < 8; ++attr_row) {
        for (attr_col = 0; attr_col < 8; ++attr_col) set_attribute_block(attr_row, attr_col, 1);
    }
    draw_title_plate(3, 3, 26, 22);
    draw_text(5, 13, FONT_TILE_BASE, casino_slots_text);
    /* Three enamel-framed windows make the symbols read as physical reels. */
    draw_title_plate(8, 5, 7, 5);
    draw_title_plate(8, 13, 7, 5);
    draw_title_plate(8, 21, 7, 5);
    if (!spinning) {
        draw_text(10, (unsigned char)(8 - (reels[0] == 2 ? 2 : reels[0] == 1 ? 1 : 0)), FONT_TILE_BASE, casino_slot_symbols[reels[0]]);
        draw_text(10, (unsigned char)(16 - (reels[1] == 2 ? 2 : reels[1] == 1 ? 1 : 0)), FONT_TILE_BASE, casino_slot_symbols[reels[1]]);
        draw_text(10, (unsigned char)(24 - (reels[2] == 2 ? 2 : reels[2] == 1 ? 1 : 0)), FONT_TILE_BASE, casino_slot_symbols[reels[2]]);
    }
    draw_text(13, 7, FONT_TILE_BASE, casino_credits_text);
    draw_slot_number(13, 17, credits);
    draw_text(15, 7, FONT_TILE_BASE, casino_bet_text);
    draw_slot_number(15, 12, wager);
    draw_text(18, 7, FONT_TILE_BASE, casino_controls_text);
    draw_text(20, 7, FONT_TILE_BASE, casino_pull_text);
    if (!spinning && result != 0) draw_text(22, 6, FONT_TILE_BASE, result);
    reset_scroll();
    PPU.mask = 0x1E;
}

/* $2005 (scroll) and $2006 (VRAM address) share one internal PPU write
 * latch/register: the low byte of the *last* $2006 write becomes the
 * scroll position once rendering resumes, unless something writes
 * $2005 afterward. Every VRAM-writing function above finishes mid
 * attribute-table, not at a scroll-neutral address, so without this the
 * background visibly jumps left/up while sprites (which aren't
 * affected by scroll) stay put -- caught by exactly that symptom when
 * opening the text box, whose ending address happened to differ from
 * draw_room()'s. Must run after all VRAM writes, right before turning
 * rendering back on. */
static void reset_scroll(void)
{
    PPU.scroll = 0;
    PPU.scroll = 0;
}

/* Pause menu and inventory share one bounded panel. Inventory is an action
 * on the first menu item, not a separate game state, so B always dismisses
 * the menu cleanly and returns to the room. */
static void show_pause_menu(unsigned char selection, unsigned char show_inventory)
{
    unsigned char row, attr_row, i, inventory_row = 11;

    PPU.mask = 0x00;
    for (row = MENU_TOP_ROW; row <= MENU_BOTTOM_ROW; ++row) {
        fill_row(row, FONT_TILE_BASE + FONT_GLYPH_SPACE);
    }
    for (attr_row = MENU_FIRST_ATTR_ROW; attr_row <= MENU_LAST_ATTR_ROW; ++attr_row) {
        set_attribute_row(attr_row, TEXT_BOX_PALETTE);
    }
    fill_row(MENU_TOP_ROW, TILE_BORDER_H);
    fill_row(MENU_BOTTOM_ROW, TILE_BORDER_H);
    for (row = MENU_TOP_ROW + 1; row < MENU_BOTTOM_ROW; ++row) {
        set_tile(row, 3, TILE_BORDER_V);
        set_tile(row, 28, TILE_BORDER_V);
    }

    if (show_inventory) {
        unsigned char found = 0;
        draw_text(8, 11, FONT_TILE_BASE, "INVENTORY");
        for (i = 0; i < ITEM_COUNT; ++i) {
            if (items[i].taken) {
                draw_text(inventory_row, 8, FONT_TILE_BASE, item_names[i]);
                ++inventory_row;
                found = 1;
            }
        }
        if (!found) draw_text(12, 11, FONT_TILE_BASE, "NO ITEMS");
        draw_text(17, 11, FONT_TILE_BASE, "B CLOSE");
    } else {
        draw_text(8, 13, FONT_TILE_BASE, "PAUSE");
        draw_text(11, 9, FONT_TILE_BASE, selection == MENU_ITEM_INVENTORY ? "X INVENTORY" : "  INVENTORY");
        draw_text(14, 9, FONT_TILE_BASE, selection == MENU_ITEM_QUIT ? "X QUIT TO TITLE" : "  QUIT TO TITLE");
        draw_text(17, 9, FONT_TILE_BASE, "A SELECT");
        draw_text(18, 10, FONT_TILE_BASE, "B CLOSE");
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

static void hide_pause_menu(RoomId room)
{
    unsigned char attr_row;

    PPU.mask = 0x00;
    draw_room_rows(room, MENU_TOP_ROW, MENU_ROWS);
    for (attr_row = MENU_FIRST_ATTR_ROW; attr_row <= MENU_LAST_ATTR_ROW; ++attr_row) {
        draw_room_attribute_row(room, attr_row);
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Shadow OAM, transferred to the PPU by oam_flush() (below) via
 * hardware DMA instead of individual $2003/$2004 writes -- see its
 * comment for why. $4014 DMA always copies exactly 256 bytes starting
 * at a page-aligned ($xx00) address; oam_raw is padded with 255 spare
 * bytes so *some* 256-byte window inside it is guaranteed aligned
 * regardless of where the linker happens to place the array, and
 * oam_align() (called once at startup) points oam_buf at that window. */
#define OAM_SIZE 256
static unsigned char oam_raw[OAM_SIZE + 255];
static unsigned char *oam_buf;
static void oam_flush(void);
static void set_oam_sprite(unsigned char slot, unsigned char x, unsigned char y,
                           unsigned char tile, unsigned char attr);
static void hide_oam_sprite(unsigned char slot);

static void oam_align(void)
{
    unsigned int addr = (unsigned int)oam_raw;
    oam_buf = (unsigned char *)((addr + 255) & ~(unsigned int)255);
}

/* OAM has 64 sprite slots; we only ever use a handful. The rest start
 * out as uninitialized/garbage data, which can render as stray junk
 * pixels wherever they happen to point on screen. $FF is in the
 * conventional "off-screen" Y range, hiding them. */
static void clear_sprites(void)
{
    unsigned int i;

    for (i = 0; i < OAM_SIZE; ++i) {
        oam_buf[i] = 0xFF;
    }
}

/* Fills a compact title-screen rectangle with sequential PPU writes instead
 * of individual set_tile() calls. It is used only while rendering is off. */
static void fill_title_rect(unsigned char first_row, unsigned char first_col,
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

/* Draws a plate frame using the text-box trim tiles. The fill is the font's
 * space tile, whose opaque background becomes the plate enamel under the
 * selected attribute palette. */
static void draw_title_plate(unsigned char top_row, unsigned char left_col,
                             unsigned char width, unsigned char height)
{
    unsigned char row;

    fill_title_rect(top_row, left_col, width, height, FONT_TILE_BASE + FONT_GLYPH_SPACE);
    fill_title_rect(top_row, left_col, width, 1, TILE_BORDER_H);
    fill_title_rect(top_row + height - 1, left_col, width, 1, TILE_BORDER_H);
    for (row = top_row + 1; row < top_row + height - 1; ++row) {
        set_tile(row, left_col, TILE_BORDER_V);
        set_tile(row, left_col + width - 1, TILE_BORDER_V);
    }
}

static void draw_title_portrait(unsigned char top_row, unsigned char left_col)
{
    unsigned char row, col;
    unsigned char tile = TITLE_PORTRAIT_TILE_BASE;

    for (row = top_row; row < top_row + TITLE_PORTRAIT_HEIGHT; ++row) {
        PPU.vram.address = (unsigned char)(0x20 + (row >> 3));
        PPU.vram.address = (unsigned char)((row << 5) + left_col);
        for (col = 0; col < TITLE_PORTRAIT_WIDTH; ++col) PPU.vram.data = tile++;
    }
}

/* Draws the initial nine-tile closed mouth while title rendering is forced
 * blank. Animated frames use queue_title_mouth() instead. */
static void draw_title_mouth(unsigned char open)
{
    unsigned char row, col;
    unsigned char screen_row;
    unsigned char tile;

    for (row = 0; row < TITLE_MOUTH_HEIGHT; ++row) {
        screen_row = (unsigned char)(TITLE_PORTRAIT_TOP + TITLE_MOUTH_TILE_ROW + row);
        PPU.vram.address = (unsigned char)(0x20 + (screen_row >> 3));
        PPU.vram.address = (unsigned char)((screen_row << 5) +
                                            TITLE_PORTRAIT_LEFT + TITLE_MOUTH_TILE_COL);
        tile = open ? (unsigned char)(TITLE_PORTRAIT_TILE_BASE +
                                      (TITLE_MOUTH_TILE_ROW + row) * TITLE_PORTRAIT_WIDTH +
                                      TITLE_MOUTH_TILE_COL)
                    : (unsigned char)(TITLE_CLOSED_MOUTH_TILE_BASE + row * TITLE_MOUTH_WIDTH);
        for (col = 0; col < TITLE_MOUTH_WIDTH; ++col) PPU.vram.data = tile++;
    }

    /* $2006 changes the PPU's pending scroll state even for this small
     * nametable patch. Restore the title camera before the next scanline. */
    reset_scroll();
}

/* Direct VRAM updates from C can run out of vblank once the mouth grew to
 * nine tiles. Queue only the frame selection instead; startup.s owns the
 * tile bytes and applies the selected frame at the next NMI. */
static void queue_title_mouth(unsigned char open)
{
    title_mouth_open = open;
    title_mouth_dirty = 1;
    if (open) {
        set_oam_sprite(TITLE_TONGUE_SLOT, TITLE_TONGUE_X, TITLE_TONGUE_Y,
                       TITLE_TONGUE_TILE, 0);
        set_oam_sprite(TITLE_TONGUE_SLOT_RIGHT, TITLE_TONGUE_X + 8, TITLE_TONGUE_Y,
                       TITLE_TONGUE_TILE_RIGHT, 0);
        set_oam_sprite(TITLE_UVULA_SLOT, TITLE_TONGUE_X, TITLE_UVULA_Y,
                       TITLE_UVULA_TILE, 0);
    } else {
        hide_oam_sprite(TITLE_TONGUE_SLOT);
        hide_oam_sprite(TITLE_TONGUE_SLOT_RIGHT);
        hide_oam_sprite(TITLE_UVULA_SLOT);
    }
}

static void queue_title_message(unsigned char index)
{
    title_message_index = index;
    title_message_dirty = 1;
}

/* The title is a license-plate poster: enamel nameplate above an intentionally
 * crude four-color portrait, with the New Game plate below. The portrait uses
 * its own title-only palette for skin, yellow frames, and beard detail. */
static void show_title_screen(void)
{
    static const unsigned char title_palette[32] = {
        0x0F, 0x11, 0x01, 0x01,
        0x0F, 0x16, 0x30, 0x01,
        0x0F, 0x27, 0x28, 0x01,
        0x0F, 0x27, 0x20, 0x01,
        /* Sprite palette 0 supplies the open-mouth tongue overlay. */
        0x0F, 0x16, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F
    };
    unsigned char i;

    PPU.mask = 0x00;
    mmc3_use_title_chr();
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x00;
    for (i = 0; i < 32; ++i) PPU.vram.data = title_palette[i];
    for (i = 0; i < ROOM_HEIGHT_TILES; ++i) fill_row(i, CHR_TILE_FLOOR);
    for (i = 0; i < 8; ++i) set_attribute_row(i, 0);

    /* Offset shadow first: the path tile is solid palette-0 navy. */
    fill_title_rect(3, 4, 26, 5, CHR_TILE_PATH);
    draw_title_plate(2, 3, 26, 5);
    for (i = 0; i < 8; ++i) set_attribute_block(0, i, 1);
    for (i = 0; i < 8; ++i) set_attribute_block(1, i, 1);
    draw_text(3, 10, FONT_TILE_BASE, title_game_name);

    for (i = 0; i < 3; ++i) set_attribute_block(2, i, 2);
    for (i = 0; i < 3; ++i) set_attribute_block(3, i, 2);
    for (i = 0; i < 3; ++i) set_attribute_block(4, i, 3);
    draw_title_portrait(TITLE_PORTRAIT_TOP, TITLE_PORTRAIT_LEFT);
    title_mouth_dirty = 0;
    draw_title_mouth(0);

    title_message_dirty = 0;

    fill_title_rect(24, 6, 22, 5, CHR_TILE_PATH);
    draw_title_plate(23, 5, 22, 5);
    for (i = 1; i < 7; ++i) set_attribute_block(5, i, 1);
    for (i = 1; i < 7; ++i) set_attribute_block(6, i, 1);
    draw_text(25, 8, FONT_TILE_BASE, title_start_prompt);

    clear_sprites();
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

static void wait_for_new_game(void)
{
    static const unsigned char talk_frames[] = {7, 5, 6, 5, 8, 6};
    unsigned char released = 0;
    unsigned char pad;
    unsigned char mouth_open = 0;
    unsigned char talk_index = 0;
    unsigned char talk_timer = 0;
    /* Start with an empty speech area, then leave a real pause around each
     * line so the title has room to breathe instead of constantly talking. */
    unsigned char message_index = TITLE_MESSAGE_COUNT - 1;
    unsigned char message_visible = 0;
    unsigned char message_timer = 240;

    music_start();
    for (;;) {
        waitvsync();
        /* NMI applies the matching background-mouth patch first; flush the
         * tongue sprite immediately afterward so both frames change together. */
        oam_flush();
        if (--message_timer == 0) {
            if (message_visible) {
                queue_title_message(TITLE_MESSAGE_HIDDEN);
                message_visible = 0;
                if (mouth_open) {
                    mouth_open = 0;
                    queue_title_mouth(0);
                }
                message_timer = 120;
            } else {
                message_index = (unsigned char)((message_index + 1) % TITLE_MESSAGE_COUNT);
                queue_title_message(message_index);
                message_visible = 1;
                talk_index = 0;
                talk_timer = 1;
                message_timer = 180;
            }
        }
        if (message_visible && --talk_timer == 0) {
            mouth_open ^= 1;
            queue_title_mouth(mouth_open);
            talk_timer = talk_frames[talk_index++];
            if (talk_index >= sizeof(talk_frames)) talk_index = 0;
        }
        pad = read_pad();
        if (!(pad & TITLE_START_MASK)) {
            if (released < TITLE_RELEASE_FRAMES) ++released;
        } else if (released >= TITLE_RELEASE_FRAMES) {
            return;
        }
    }
}

/* Copies the whole shadow OAM buffer to the PPU in one hardware DMA
 * transfer -- a fixed 513-514 CPU cycles regardless of how many sprites
 * are actually in use, comfortably inside a single vblank (~2273 CPU
 * cycles on NTSC) no matter how many entities the game grows to.
 *
 * This replaces writing each sprite's 4 bytes individually via
 * $2003/$2004 every frame. That approach's cost scaled with entity
 * count -- fine with a couple of sprites, but once there were enough
 * NPCs/items/enemy/sword all doing it in the same frame, the writes
 * for whichever ones came later in the sequence started landing after
 * vblank ended and were silently dropped by the PPU (confirmed by
 * forcing a debug sprite write to the very front of the frame, which
 * rendered, versus its normal spot after the NPC loop, which didn't --
 * the same *symptom* as the earlier stale-OAMADDR bug, but a different
 * cause: this was genuinely running out of time, not corrupting the
 * PPU's read pointer). DMA sidesteps the problem entirely instead of
 * needing the sprite-write order re-tuned every time the entity count
 * grows again.
 *
 * Because sprite updates now just poke RAM (oam_buf) instead of PPU
 * registers directly, none of set_oam_sprite()/hide_oam_sprite()'s
 * callers have a timing deadline any more either -- only this flush
 * does. It runs first thing after waitvsync() each frame, flushing
 * whatever the *previous* frame's game logic computed; the game logic
 * that fills in this frame's buffer runs afterward, with no rush. */
static void oam_flush(void)
{
    SPRITE_DMA = (unsigned char)((unsigned int)oam_buf >> 8);
}

/* Shows a text box across the bottom of the screen (rows 24-27) with a
 * single, horizontally-centered line of text, framed by box_border.s's
 * rule tiles: a full-width horizontal rule on the box's top and bottom
 * row (which doubles as the corners, since it spans column 0 to 31),
 * and a vertical rule at columns 0/31 on the interior rows. Uses
 * background palette 1 (see room_palettes) instead of whichever room is
 * open, so the box looks the same everywhere. */
/* Text boxes have two interior rows. Long messages wrap at the last word
 * boundary that fits, then center each line so punchlines stay readable. */
static void draw_text_box_message(unsigned char text_row, unsigned char left_col,
                                  unsigned char message_width, const char *message)
{
    unsigned char len = (unsigned char)strlen(message);
    unsigned char first_len = len;
    unsigned char second_len = 0;
    unsigned char count;
    const char *second = 0;

    if (len > message_width) {
        first_len = message_width;
        while (first_len > 0 && message[first_len] != ' ') --first_len;
        if (first_len == 0) first_len = message_width;
        second = message + first_len + 1;
        second_len = (unsigned char)(len - first_len - 1);
    }

    PPU.vram.address = (unsigned char)(0x20 + (text_row >> 3));
    PPU.vram.address = (unsigned char)((text_row << 5) + left_col +
                                       (message_width - first_len) / 2);
    for (count = first_len; count > 0; --count) {
        PPU.vram.data = (unsigned char)(FONT_TILE_BASE + font_glyph_index(*message));
        ++message;
    }
    if (second != 0) {
        PPU.vram.address = (unsigned char)(0x20 + ((text_row + 1) >> 3));
        PPU.vram.address = (unsigned char)(((text_row + 1) << 5) + left_col +
                                           (message_width - second_len) / 2);
        for (count = second_len; count > 0; --count) {
            PPU.vram.data = (unsigned char)(FONT_TILE_BASE + font_glyph_index(*second));
            ++second;
        }
    }
}

static void show_text_box(const char *message)
{
    unsigned char row;

    PPU.mask = 0x00;

    fill_row(TEXT_BOX_TOP_ROW, TILE_BORDER_H);
    fill_row(TEXT_BOX_TOP_ROW + TEXT_BOX_ROWS - 1, TILE_BORDER_H);

    for (row = TEXT_BOX_TOP_ROW + 1; row < TEXT_BOX_TOP_ROW + TEXT_BOX_ROWS - 1; ++row) {
        fill_row(row, FONT_TILE_BASE + FONT_GLYPH_SPACE);
        set_tile(row, 0, TILE_BORDER_V);
        set_tile(row, NAMETABLE_WIDTH_TILES - 1, TILE_BORDER_V);
    }
    draw_text_box_message(TEXT_BOX_TEXT_ROW, 1, TEXT_BOX_MESSAGE_WIDTH, message);
    set_attribute_row(TEXT_BOX_ATTR_ROW, TEXT_BOX_PALETTE);

    reset_scroll();
    PPU.mask = 0x1E;
}

static void show_death_text_box(const char *message)
{
    unsigned char row, attr_col;
    unsigned char attr_row;

    PPU.mask = 0x00;
    fill_title_rect(DEATH_BOX_TOP_ROW, DEATH_BOX_LEFT_COL, DEATH_BOX_WIDTH,
                    DEATH_BOX_ROWS, FONT_TILE_BASE + FONT_GLYPH_SPACE);
    fill_title_rect(DEATH_BOX_TOP_ROW, DEATH_BOX_LEFT_COL, DEATH_BOX_WIDTH, 1,
                    TILE_BORDER_H);
    fill_title_rect(DEATH_BOX_TOP_ROW + DEATH_BOX_ROWS - 1, DEATH_BOX_LEFT_COL,
                    DEATH_BOX_WIDTH, 1, TILE_BORDER_H);
    for (row = DEATH_BOX_TOP_ROW + 1; row < DEATH_BOX_TOP_ROW + DEATH_BOX_ROWS - 1; ++row) {
        set_tile(row, DEATH_BOX_LEFT_COL, TILE_BORDER_V);
        set_tile(row, DEATH_BOX_LEFT_COL + DEATH_BOX_WIDTH - 1, TILE_BORDER_V);
    }
    draw_text_box_message(DEATH_BOX_TEXT_ROW, DEATH_BOX_LEFT_COL + 1,
                          DEATH_BOX_TEXT_WIDTH, message);
    for (attr_row = DEATH_BOX_FIRST_ATTR_ROW; attr_row <= DEATH_BOX_LAST_ATTR_ROW; ++attr_row) {
        for (attr_col = DEATH_BOX_FIRST_ATTR_COL; attr_col <= DEATH_BOX_LAST_ATTR_COL; ++attr_col) {
            set_attribute_block(attr_row, attr_col, TEXT_BOX_PALETTE);
        }
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

static void hide_death_text_box(RoomId room)
{
    PPU.mask = 0x00;
    if (room == ROOM_MEADOW) draw_neon_avenue_scene();
    else if (room == ROOM_TOWN) draw_jimbles_bar_scene();
    else if (room == ROOM_SEWER) draw_sewer_scene();
    else if (room == ROOM_CASINO) draw_casino_scene();
    else draw_room(room);
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Restores only the text box's rows and its matching room attribute row.
 * The rest of the nametable was never covered, so rewriting it only makes
 * the forced-blank transition long enough to feel like a pause. */
static void hide_text_box(RoomId room)
{
    PPU.mask = 0x00;
    if (room == ROOM_TOWN) {
        /* The deliberately bottom-aligned street exit occupies rows the
         * dialogue panel covers, so restore the authored scene as a whole. */
        draw_jimbles_bar_scene();
    } else if (room == ROOM_SEWER) {
        draw_sewer_scene();
    } else if (room == ROOM_CASINO) {
        draw_casino_scene();
    } else {
        draw_room_rows(room, TEXT_BOX_TOP_ROW, TEXT_BOX_ROWS);
        draw_room_attribute_row(room, TEXT_BOX_ATTR_ROW);
    }
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Writes one OAM slot (4 bytes each, so slot*4 is its offset) into the
 * shadow buffer -- see oam_flush() for why this is RAM, not a direct
 * PPU register write, and why that means this has no timing deadline.
 * Hardware quirk: sprite rendering is delayed by one scanline, so OAM's
 * Y byte must hold (true Y - 1) or the sprite is drawn one pixel lower
 * than every other system (collision, in particular) thinks it is.
 * There's no equivalent quirk for X. movement.c works entirely in true
 * coordinates -- this -1 is applied only here, at the point of actually
 * writing the buffer, so the quirk stays contained to this one line
 * instead of leaking into game logic. (y is guaranteed >= SCREEN_MIN_Y,
 * i.e. >= 1, so this never underflows.) */
static void set_oam_sprite(unsigned char slot, unsigned char x, unsigned char y,
                            unsigned char tile, unsigned char attr)
{
    unsigned char *entry = &oam_buf[(unsigned int)slot * 4];
    entry[0] = y - 1;   /* byte 0: Y position */
    entry[1] = tile;    /* byte 1: tile index */
    entry[2] = attr;    /* byte 2: attributes */
    entry[3] = x;       /* byte 3: X position */
}

/* Movement and collision still use the legacy 8x8 foot box. Render the
 * larger actor centered on it, with its feet ending on the old sprite's
 * baseline so gameplay does not change merely because the art grew. */
static void hide_oam_sprite(unsigned char slot);

static void set_sprite(unsigned char x, unsigned char y, unsigned char walk_frame)
{
    unsigned char left = x > 4 ? x - 4 : 0;
    unsigned char top = y > 16 ? y - 16 : 1;
    unsigned char leg_left = walk_frame ? PLAYER_AVATAR_LEGS_B_L : PLAYER_AVATAR_LEGS_A_L;
    unsigned char leg_right = walk_frame ? PLAYER_AVATAR_LEGS_B_R : PLAYER_AVATAR_LEGS_A_R;

    set_oam_sprite(0, left, top, PLAYER_AVATAR_HEAD_L, SPRITE_ATTR);
    set_oam_sprite(1, left + 8, top, PLAYER_AVATAR_HEAD_R, SPRITE_ATTR);
    set_oam_sprite(2, left, top + 8, PLAYER_AVATAR_FACE_L, SPRITE_ATTR);
    set_oam_sprite(3, left + 8, top + 8, PLAYER_AVATAR_FACE_R, SPRITE_ATTR);
    set_oam_sprite(4, left, top + 16, leg_left, SPRITE_ATTR);
    set_oam_sprite(5, left + 8, top + 16, leg_right, SPRITE_ATTR);
}

/* A pull keeps the slot face static; these three sprite highlights travel
 * through the reel windows without touching nametable VRAM. */
static void set_slot_reel_sweep(unsigned char frame)
{
    unsigned char i;
    unsigned char x = 72;
    unsigned char y = (frame & 4) ? 72 : 80;

    for (i = 0; i < 3; ++i) {
        set_oam_sprite(i, x, y, TILE_BORDER_V, SPRITE_ATTR);
        x += 64;
    }
}

static void set_taxi_sprites(unsigned char x, unsigned char y, unsigned char moving_left)
{
    unsigned char attr = TAXI_SPRITE_PALETTE;

    if (!moving_left) attr |= OAM_FLIP_H;
    set_oam_sprite(TAXI_SLOT(0), x, y,
                   TAXI_TILE_BASE + (moving_left ? 0 : 3), attr);
    set_oam_sprite(TAXI_SLOT(1), x + 8, y,
                   TAXI_TILE_BASE + (moving_left ? 1 : 2), attr);
    set_oam_sprite(TAXI_SLOT(2), x + 16, y,
                   TAXI_TILE_BASE + (moving_left ? 2 : 1), attr);
    set_oam_sprite(TAXI_SLOT(3), x + 24, y,
                   TAXI_TILE_BASE + (moving_left ? 3 : 0), attr);
    set_oam_sprite(TAXI_SLOT(4), x, y + 8,
                   TAXI_TILE_BASE + (moving_left ? 3 : 7), attr);
    set_oam_sprite(TAXI_SLOT(5), x + 8, y + 8,
                   TAXI_TILE_BASE + (moving_left ? 4 : 6), attr);
    set_oam_sprite(TAXI_SLOT(6), x + 16, y + 8,
                   TAXI_TILE_BASE + (moving_left ? 6 : 4), attr);
    set_oam_sprite(TAXI_SLOT(7), x + 24, y + 8,
                   TAXI_TILE_BASE + (moving_left ? 7 : 3), attr);
    set_oam_sprite(TAXI_SLOT(8), x, y + 16, TAXI_TILE_BASE + 8, attr);
    set_oam_sprite(TAXI_SLOT(9), x + 8, y + 16, TAXI_TILE_BASE + 8, attr);
    set_oam_sprite(TAXI_SLOT(10), x + 16, y + 16, TAXI_TILE_BASE + 8, attr);
    set_oam_sprite(TAXI_SLOT(11), x + 24, y + 16, TAXI_TILE_BASE + 8, attr);
}

static void hide_taxi_sprites(void)
{
    unsigned char i;
    for (i = 0; i < TAXI_SPRITE_SLOTS; ++i) hide_oam_sprite(TAXI_SLOT(i));
}

static void set_traffic_particles(unsigned char x, unsigned char y)
{
    unsigned char left = x > 8 ? x - 8 : 0;
    unsigned char top = y > 16 ? y - 16 : 1;

    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(0), left, top, TAXI_PARTICLE_TILE, TAXI_SPRITE_PALETTE);
    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(1), x + 8, top + 4, TAXI_PARTICLE_TILE_SMALL, TAXI_SPRITE_PALETTE);
    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(2), left + 4, y + 4, TAXI_PARTICLE_TILE_DOT, TAXI_SPRITE_PALETTE);
    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(3), x + 12, y - 4, TAXI_PARTICLE_TILE_SMALL, TAXI_SPRITE_PALETTE);
    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(4), left + 12, top + 12, TAXI_PARTICLE_TILE, TAXI_SPRITE_PALETTE);
    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(5), x + 16, y + 2, TAXI_PARTICLE_TILE_DOT, TAXI_SPRITE_PALETTE);
    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(6), left + 2, y + 14, TAXI_PARTICLE_TILE_SMALL, TAXI_SPRITE_PALETTE);
    set_oam_sprite(TRAFFIC_PARTICLE_SLOT(7), x + 10, y + 16, TAXI_PARTICLE_TILE_DOT, TAXI_SPRITE_PALETTE);
}

static void hide_traffic_particles(void)
{
    unsigned char i;
    for (i = 0; i < TRAFFIC_PARTICLE_COUNT; ++i) hide_oam_sprite(TRAFFIC_PARTICLE_SLOT(i));
}

static void set_traffic_bones(unsigned char x, unsigned char y, unsigned char mirrored)
{
    unsigned char left = x < 16 ? 0 : (x > 216 ? 216 : x - 16);
    unsigned char top = y > 216 ? 216 : y - 16;

    if (!mirrored) {
        set_oam_sprite(TRAFFIC_BONE_SLOT(0), left, top + 24, TRAFFIC_BONE_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(1), left + 8, top + 24, TRAFFIC_BONE_TILE + 1, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(2), left + 20, top + 4, TRAFFIC_WISHBONE_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(3), left + 2, top + 12, TRAFFIC_FEMUR_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(4), left + 22, top + 20, TRAFFIC_CAVEMAN_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(5), left + 10, top, TRAFFIC_SKULL_TILE, TAXI_SPRITE_PALETTE);
    } else {
        unsigned char right = x > 224 ? 224 : x;
        set_oam_sprite(TRAFFIC_BONE_SLOT(0), right, top + 24, TRAFFIC_BONE_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(1), right + 8, top + 24, TRAFFIC_BONE_TILE + 1, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(2), right - 12, top + 4, TRAFFIC_WISHBONE_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(3), right + 10, top + 12, TRAFFIC_FEMUR_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(4), right - 10, top + 20, TRAFFIC_CAVEMAN_TILE, TAXI_SPRITE_PALETTE);
        set_oam_sprite(TRAFFIC_BONE_SLOT(5), right + 2, top, TRAFFIC_SKULL_TILE, TAXI_SPRITE_PALETTE);
    }
}

static void hide_traffic_bones(void)
{
    unsigned char i;
    for (i = 0; i < TRAFFIC_BONE_COUNT; ++i) hide_oam_sprite(TRAFFIC_BONE_SLOT(i));
}

static void set_traffic_head(unsigned char x, unsigned char y, unsigned char mirrored)
{
    unsigned char left = mirrored ? (x > 216 ? 240 : x + 24)
                                  : (x < 40 ? 0 : (x > 224 ? 184 : x - 40));
    unsigned char top = y > 208 ? 208 : y - 8;

    set_oam_sprite(TRAFFIC_HEAD_SLOT(0), left, top, PLAYER_AVATAR_HEAD_L, SPRITE_ATTR);
    set_oam_sprite(TRAFFIC_HEAD_SLOT(1), left + 8, top, PLAYER_AVATAR_HEAD_R, SPRITE_ATTR);
}

static void hide_traffic_head(void)
{
    unsigned char i;
    for (i = 0; i < TRAFFIC_HEAD_SPRITE_COUNT; ++i) hide_oam_sprite(TRAFFIC_HEAD_SLOT(i));
}

/* Uses NPC_REWARD_PALETTE once the quest is complete, so re-entering the
 * room (or turning the quest in without leaving it) always shows the
 * NPC's current state, not just whatever palette it happened to load
 * with. Draws at its current on-screen position (npc_state), which may
 * be mid-walk rather than its rest spot. */
static void set_npc_sprite(unsigned char idx)
{
    unsigned char palette = npcs[idx].quest_complete ? NPC_REWARD_PALETTE : NPC_SPRITE_PALETTE;

    if (idx == 0 && npc_state[idx].room == ROOM_MEADOW) {
        unsigned char left = npc_state[idx].x > 4 ? npc_state[idx].x - 4 : 0;
        unsigned char top = npc_state[idx].y > 16 ? npc_state[idx].y - 16 : 1;

        set_oam_sprite(NPC_LARGE_SLOT(0), left, top, PLAYER_AVATAR_HEAD_L, SPRITE_ATTR);
        set_oam_sprite(NPC_LARGE_SLOT(1), left + 8, top, PLAYER_AVATAR_HEAD_R, SPRITE_ATTR);
        set_oam_sprite(NPC_LARGE_SLOT(2), left, top + 8, PLAYER_AVATAR_FACE_L, SPRITE_ATTR);
        set_oam_sprite(NPC_LARGE_SLOT(3), left + 8, top + 8, PLAYER_AVATAR_FACE_R, SPRITE_ATTR);
        set_oam_sprite(NPC_LARGE_SLOT(4), left, top + 16, PLAYER_AVATAR_LEGS_A_L, SPRITE_ATTR);
        set_oam_sprite(NPC_LARGE_SLOT(5), left + 8, top + 16, PLAYER_AVATAR_LEGS_A_R, SPRITE_ATTR);
        hide_oam_sprite(NPC_SLOT(idx));
        return;
    }
    if (idx == JIMBLES_BARTENDER_NPC && npc_state[idx].room == ROOM_TOWN) {
        unsigned char left = npc_state[idx].x > 4 ? npc_state[idx].x - 4 : 0;
        /* The third glass row begins on the counter's top row, where the
         * aligned foreground overlay hides it and makes Jimbles sit behind
         * the bar rather than perch on it. */
        unsigned char top = npc_state[idx].y > 20 ? npc_state[idx].y - 20 : 1;

        set_oam_sprite(BAR_BARTENDER_SLOT(0), left, top, JIMBLES_FACE_TILE, JIMBLES_SPRITE_PALETTE);
        set_oam_sprite(BAR_BARTENDER_SLOT(1), left + 8, top, JIMBLES_FACE_TILE,
                       JIMBLES_SPRITE_PALETTE | OAM_FLIP_H);
        set_oam_sprite(BAR_BARTENDER_SLOT(2), left, top + 8, JIMBLES_BODY_TILE, JIMBLES_SPRITE_PALETTE);
        set_oam_sprite(BAR_BARTENDER_SLOT(3), left + 8, top + 8, JIMBLES_BODY_TILE,
                       JIMBLES_SPRITE_PALETTE | OAM_FLIP_H);
        set_oam_sprite(BAR_BARTENDER_SLOT(4), left, top + 16, BAR_TILE_BACK_DOOR_OPEN,
                       JIMBLES_SPRITE_PALETTE);
        set_oam_sprite(BAR_BARTENDER_SLOT(5), left + 8, top + 16, BAR_TILE_BACK_DOOR_OPEN,
                       JIMBLES_SPRITE_PALETTE | OAM_FLIP_H);
        /* Late OAM slots redraw the counter over Jimbles's face/body. */
        set_oam_sprite(BAR_COUNTER_OVERLAY_SLOT(0), left, 112, BAR_TILE_COUNTER, BAR_COUNTER_SPRITE_PALETTE);
        set_oam_sprite(BAR_COUNTER_OVERLAY_SLOT(1), left + 8, 112, BAR_TILE_COUNTER, BAR_COUNTER_SPRITE_PALETTE);
        set_oam_sprite(BAR_COUNTER_OVERLAY_SLOT(2), left, 120, BAR_TILE_COUNTER, BAR_COUNTER_SPRITE_PALETTE);
        set_oam_sprite(BAR_COUNTER_OVERLAY_SLOT(3), left + 8, 120, BAR_TILE_COUNTER, BAR_COUNTER_SPRITE_PALETTE);
        hide_oam_sprite(NPC_SLOT(idx));
        return;
    }
    set_oam_sprite(NPC_SLOT(idx), npc_state[idx].x, npc_state[idx].y, NPC_TILE_BASE, palette);
}

static void set_bar_regular_sprites(void)
{
    unsigned char i;

    for (i = 0; i < BAR_REGULAR_COUNT; ++i) {
        set_oam_sprite(NPC_SLOT(i), bar_regulars[i].x, bar_regulars[i].y,
                       BAR_TILE_BACK_DOOR_OPEN, BAR_COUNTER_SPRITE_PALETTE);
    }
}

static void hide_npc_sprite(unsigned char idx)
{
    unsigned char i;
    hide_oam_sprite(NPC_SLOT(idx));
    if (idx == 0) {
        for (i = 0; i < NPC_LARGE_SPRITE_SLOTS; ++i) hide_oam_sprite(NPC_LARGE_SLOT(i));
    }
    if (idx == JIMBLES_BARTENDER_NPC) {
        for (i = 0; i < BAR_BARTENDER_SPRITE_SLOTS; ++i) hide_oam_sprite(BAR_BARTENDER_SLOT(i));
        for (i = 0; i < BAR_COUNTER_OVERLAY_SPRITE_SLOTS; ++i) hide_oam_sprite(BAR_COUNTER_OVERLAY_SLOT(i));
    }
}

static void set_item_sprite(unsigned char idx)
{
    set_oam_sprite(ITEM_SLOT(idx), items[idx].x, items[idx].y, ITEM_TILE_BASE, ITEM_SPRITE_PALETTE);
}

static void set_bar_door_knob(void)
{
    set_oam_sprite(BAR_DOORKNOB_SLOT, 32, 109, TAXI_PARTICLE_TILE_DOT, ITEM_SPRITE_PALETTE);
}

static void set_enemy_sprite(unsigned char idx)
{
    set_oam_sprite(ENEMY_SLOT(idx), enemies[idx].x, enemies[idx].y, ENEMY_TILE_BASE, ENEMY_SPRITE_PALETTE);
}

static void set_critter_sprite(unsigned char idx)
{
    set_oam_sprite(CRITTER_SLOT(idx), critters[idx].x, critters[idx].y, CRITTER_TILE_BASE, CRITTER_SPRITE_PALETTE);
}

static void set_portal_sprite(unsigned char idx)
{
    set_oam_sprite(PORTAL_SLOT(idx), portals[idx].x, portals[idx].y, portals[idx].tile, portals[idx].palette);
}

/* OAM Y $FF is the conventional off-screen value (see clear_sprites());
 * writing just the Y byte is enough to hide a slot, since the rest of
 * the entry doesn't matter once it's off-screen. */
static void hide_oam_sprite(unsigned char slot)
{
    oam_buf[(unsigned int)slot * 4] = 0xFF;
}

/* Full room switch: repaints the palette and nametable and repositions
 * the player, all with rendering off so the (fairly slow) redraw can't
 * tear or leave stale tiles on screen. Turning PPU.mask off is safe to
 * do at any point, not just during vblank -- once rendering is off the
 * PPU stops fetching immediately, so nothing after this point has a
 * timing deadline until mask is turned back on. */
static void load_room(RoomId room, unsigned char x, unsigned char y)
{
    /* Doesn't set NPCs' or enemies' OAM slots here -- both can change
     * whether they're in this room, or (for enemies) blink/move, while
     * the player just stands here with no room reload to hook into, so
     * main()'s per-frame loop sets/hides them every frame instead, the
     * same way it already refreshes the player's slot every frame. */
    unsigned char i;

    PPU.mask = 0x00;
    load_palette(room);
    if (room == ROOM_MEADOW) draw_neon_avenue_scene();
    else if (room == ROOM_TOWN) draw_jimbles_bar_scene();
    else if (room == ROOM_SEWER) draw_sewer_scene();
    else if (room == ROOM_CASINO) draw_casino_scene();
    else draw_room(room);
    clear_sprites();
    set_sprite(x, y, 0);
    for (i = 0; i < ITEM_COUNT; ++i) {
        if (items[i].room == room && !items[i].taken) {
            set_item_sprite(i);
        }
    }
    /* Portals are as static as items (never move, never disappear), so
     * -- same as items -- setting them once here is enough; no per-frame
     * refresh needed. Neon Avenue's first portal is authored directly into
     * its facade instead of wearing the legacy generic portal sprite. */
    for (i = 0; i < PORTAL_COUNT; ++i) {
        if (portals[i].room == room && room != ROOM_MEADOW && room != ROOM_TOWN && room != ROOM_SEWER) {
            set_portal_sprite(i);
        }
    }
    /* Flushed here, still under forced blank, so rendering never turns
     * back on with stale/uninitialized OAM showing for a frame -- the
     * main loop's own oam_flush() takes over from the next frame on. */
    oam_flush();
    reset_scroll();
    PPU.mask = 0x1E;
}

/* Which way the player is facing -- movement.c has no concept of this
 * (it's pure position math), so it's tracked here from raw pad state,
 * updated every frame a direction is held and left alone otherwise
 * (facing a wall/enemy without walking into it is normal Zelda-style
 * behavior). Drives the attack's directional reach below. */
typedef enum { FACING_DOWN, FACING_UP, FACING_LEFT, FACING_RIGHT } Facing;
typedef enum { TRAFFIC_IDLE, TRAFFIC_APPROACH, TRAFFIC_IMPACT, TRAFFIC_MESSAGE } TrafficState;

#define TRAFFIC_TAXI_SPEED 4
#define TRAFFIC_IMPACT_FRAMES 120

void main(void)
{
    unsigned char x = 72;    /* starts at the left side of Neon Avenue's street stage */
    unsigned char y = 176;
    unsigned char pad;
    unsigned char exits;
    unsigned char text_box_open = 0;
    /* text_box_dirty/pending_text_message defer the actual nametable
     * redraw to the top of the loop (see the waitvsync()/oam_flush()
     * block below) instead of doing it right where the button press is
     * detected, deep in the frame. show_text_box()/hide_text_box() turn
     * rendering off for the duration of their writes -- fine in
     * principle, but flipping PPU.mask off and back on mid active-
     * picture (as happened when they were called straight from the
     * input-handling code below) cuts a visible torn band out of
     * whichever frame that lands on. Doing it right after waitvsync()
     * instead keeps the whole off/write/on sequence at the start of the
     * frame, before this frame's active picture has begun, so there's
     * nothing to tear. */
    unsigned char text_box_dirty = 0;
    unsigned char bar_scene_dirty = 0;
    unsigned char death_text_box = 0;
    const char *pending_text_message = 0;
    Facing player_facing = FACING_DOWN;
    unsigned char player_walk_tick = 0;
    unsigned char player_walk_frame = 0;
    TrafficState traffic_state = TRAFFIC_IDLE;
    unsigned char taxi_x = 0;
    unsigned char taxi_moves_left = 0;
    unsigned char taxi_visible = 0;
    unsigned char traffic_timer = 0;
    /* Same debounce pattern as talk_release_frames, same reason. */
    unsigned char attack_release_frames = ATTACK_DEBOUNCE_FRAMES;
    /* >0 while the sword sprite is showing after a swing. sword_facing/
     * tile/attr capture the swing's orientation (direction + OAM flip
     * bits, from player_facing) once at swing time, so turning mid-swing
     * doesn't change the blade's direction -- but the sprite's *position*
     * is recomputed from the player's current x/y every frame (see the
     * render block below), so the sword travels with the player instead
     * of hanging behind if they walk mid-swing. */
    unsigned char sword_timer = 0;
    Facing sword_facing = FACING_DOWN;
    unsigned char sword_tile = 0, sword_attr = 0;
    unsigned char player_hp = PLAYER_MAX_HP;
    unsigned char player_invuln = 0;   /* >0 briefly after taking contact damage -- see CONTACT_INVULN_FRAMES */
    unsigned char player_flash = 0;    /* >0 while the player sprite is blinking from that same hit */
    unsigned char player_knockback_pad = 0;
    unsigned char player_knockback_timer = 0;
    unsigned char death_timer = 0;
    unsigned char menu_open = 0;
    unsigned char menu_inventory_open = 0;
    unsigned char menu_dirty = 0;
    unsigned char menu_selection = MENU_ITEM_INVENTORY;
    unsigned char menu_release_frames = MENU_DEBOUNCE_FRAMES;
    unsigned char start_release_frames = MENU_DEBOUNCE_FRAMES;
    unsigned char slot_game_open = 0;
    unsigned char slot_game_dirty = 0;
    unsigned char slot_release_frames = TALK_DEBOUNCE_FRAMES;
    unsigned char slot_wager = 1;
    unsigned char slot_credits = 12;
    unsigned char slot_reels[3] = {1, 2, 0};
    unsigned char slot_spin_timer = 0;
    const char *slot_result = 0;
    /* Counts consecutive frames with neither START nor A held; a new
     * press only fires once this reaches TALK_DEBOUNCE_FRAMES. Starts
     * at that value so the very first real press fires immediately.
     * Plain single-frame edge-detection (comparing against the previous
     * frame's pad state) turned out not to be enough -- some input
     * paths (e.g. a held key under OS/emulator auto-repeat) can report
     * a button as released for just one frame before re-reporting it
     * pressed, which a single-frame comparison sees as a brand new
     * press and retriggers on, producing rapid open/close "blinking"
     * while the button is physically just being held. Requiring several
     * consecutive released frames filters that out while still being
     * effectively instant for a real, deliberate release. */
    unsigned char talk_release_frames = TALK_DEBOUNCE_FRAMES;
    unsigned char npc_idx;
    unsigned char i;
    const RoomConnections *conn;
    const RoomSpawn *entry_spawn;
    Obstacle npc_obstacle;

    oam_align();

    /* Let the PPU settle after reset before touching it */
    waitvsync();
    waitvsync();

    PPU.control = 0x80;   /* 8x8 sprites, pattern table 0, vblank NMI on */
    PPU.mask = 0x00;      /* rendering off while we set things up */

    show_title_screen();
    wait_for_new_game();
    mmc3_use_common_chr();

    if (!world_validate_connections(room_layouts, room_connections, room_one_way_flags, ROOM_COUNT)) return;
    if (!begin_new_game(&x, &y)) return;
    load_room(current_room, x, y);

    for (;;) {
        waitvsync();

        /* The only part of the loop with a hard deadline: DMA the
         * shadow OAM buffer to the PPU right after entering vblank (see
         * oam_flush()). Everything below that fills in *this* frame's
         * buffer contents has no deadline of its own any more -- it's
         * just writing to RAM, flushed on the *next* trip through this
         * point. That one-frame-old-at-60Hz lag is harmless and was
         * already true of NPC positions specifically before this; DMA
         * just makes it true of every sprite uniformly, and (the actual
         * point of switching to it) removes the risk of any individual
         * sprite write landing after vblank ends as more entities get
         * added -- see oam_flush()'s comment for the bug that forced
         * this. */
        oam_flush();

        if (bar_scene_dirty) {
            PPU.mask = 0x00;
            draw_jimbles_bar_scene();
            reset_scroll();
            PPU.mask = 0x1E;
            bar_scene_dirty = 0;
        }

        if (text_box_dirty) {
            if (text_box_open) {
                if (death_text_box) show_death_text_box(pending_text_message);
                else show_text_box(pending_text_message);
            } else {
                if (death_text_box) {
                    hide_death_text_box(current_room);
                    death_text_box = 0;
                } else {
                    hide_text_box(current_room);
                }
            }
            text_box_dirty = 0;
        }
        if (menu_dirty) {
            if (menu_open) {
                show_pause_menu(menu_selection, menu_inventory_open);
            } else {
                hide_pause_menu(current_room);
            }
            menu_dirty = 0;
        }
        if (slot_game_dirty) {
            show_slot_game(slot_wager, slot_credits, slot_result, slot_reels,
                           slot_spin_timer == 18);
            slot_game_dirty = 0;
        }

        /* Death pauses the world long enough for its message to register,
         * then returns a fully healed player to the Meadow starting point. */
        if (death_timer > 0) {
            --death_timer;
            if (death_timer == 0) {
                current_room = ROOM_MEADOW;
                x = 72;
                y = 176;
                player_hp = PLAYER_MAX_HP;
                player_invuln = 0;
                player_flash = 0;
                player_knockback_timer = 0;
                text_box_open = 0;
                text_box_dirty = 0;
                load_room(current_room, x, y);
            }
            continue;
        }

        if (slot_game_open) {
            if (slot_spin_timer) {
                --slot_spin_timer;
                for (i = 0; i < PLAYER_SPRITE_SLOTS; ++i) hide_oam_sprite(i);
                set_slot_reel_sweep(slot_spin_timer);
                if (slot_spin_timer == 0) slot_game_dirty = 1;
                continue;
            }
            pad = read_pad();
            if (!(pad & (JOY_BTN_A_MASK | JOY_BTN_B_MASK | PAD_LEFT_MASK | PAD_RIGHT_MASK))) {
                if (slot_release_frames < TALK_DEBOUNCE_FRAMES) ++slot_release_frames;
            } else if (slot_release_frames >= TALK_DEBOUNCE_FRAMES) {
                if (pad & JOY_BTN_B_MASK) {
                    slot_game_open = 0;
                    slot_result = 0;
                    load_room(ROOM_CASINO, x, y);
                } else if ((pad & PAD_LEFT_MASK) && slot_wager > 1) {
                    --slot_wager;
                    slot_result = 0;
                    slot_game_dirty = 1;
                } else if ((pad & PAD_RIGHT_MASK) && slot_wager < 3) {
                    ++slot_wager;
                    slot_result = 0;
                    slot_game_dirty = 1;
                } else if (pad & JOY_BTN_A_MASK) {
                    unsigned char roll;
                    unsigned int next_credits;
                    if (slot_credits < slot_wager) slot_result = "NO CREDIT";
                    else {
                        slot_credits -= slot_wager;
                        roll = rng_next(&simulation_rng) & 7;
                        if (roll == 0) {
                            slot_reels[0] = slot_reels[1] = slot_reels[2] = 0;
                            next_credits = (unsigned int)slot_credits + (unsigned int)slot_wager * 10;
                            slot_credits = next_credits > 255 ? 255 : (unsigned char)next_credits;
                            slot_result = "JACKPOT";
                        } else if (roll < 3) {
                            slot_reels[0] = slot_reels[1] = 2;
                            slot_reels[2] = 1;
                            next_credits = (unsigned int)slot_credits + (unsigned int)slot_wager * 2;
                            slot_credits = next_credits > 255 ? 255 : (unsigned char)next_credits;
                            slot_result = "PAIR";
                        } else {
                            slot_reels[0] = roll % 3;
                            slot_reels[1] = (unsigned char)((roll + 1) % 3);
                            slot_reels[2] = (unsigned char)((roll + 2) % 3);
                            slot_result = "LOSE";
                        }
                    }
                    if (slot_result[0] == 'N') slot_game_dirty = 1;
                    else {
                        slot_spin_timer = 18;
                        slot_game_dirty = 1;
                    }
                    music_sfx_pickup();
                }
                slot_release_frames = 0;
            }
            for (i = 0; i < PLAYER_SPRITE_SLOTS; ++i) hide_oam_sprite(i);
            continue;
        }

        /* Menu input is handled before any world simulation, making pause a
         * real gameplay freeze rather than merely a visual overlay. */
        if (menu_open) {
            pad = read_pad();
            if (!(pad & (JOY_BTN_A_MASK | JOY_BTN_B_MASK | PAD_UP_MASK | PAD_DOWN_MASK))) {
                if (menu_release_frames < MENU_DEBOUNCE_FRAMES) ++menu_release_frames;
            } else if (menu_release_frames >= MENU_DEBOUNCE_FRAMES) {
                if (pad & JOY_BTN_B_MASK) {
                    menu_open = 0;
                    menu_inventory_open = 0;
                    menu_dirty = 1;
                } else if (!menu_inventory_open && (pad & PAD_UP_MASK) && menu_selection > 0) {
                    --menu_selection;
                    menu_dirty = 1;
                    music_sfx_dialogue();
                } else if (!menu_inventory_open && (pad & PAD_DOWN_MASK) && menu_selection + 1 < MENU_ITEM_COUNT) {
                    ++menu_selection;
                    menu_dirty = 1;
                    music_sfx_dialogue();
                } else if (pad & JOY_BTN_A_MASK) {
                    if (menu_selection == MENU_ITEM_INVENTORY) {
                        menu_inventory_open = 1;
                        menu_dirty = 1;
                        music_sfx_dialogue();
                    } else {
                        show_title_screen();
                        wait_for_new_game();
                        mmc3_use_common_chr();
                        if (!begin_new_game(&x, &y)) return;
                        player_hp = PLAYER_MAX_HP;
                        player_invuln = 0;
                        player_flash = 0;
                        player_knockback_timer = 0;
                        sword_timer = 0;
                        text_box_open = 0;
                        text_box_dirty = 0;
                        menu_open = 0;
                        menu_inventory_open = 0;
                        menu_dirty = 0;
                        load_room(current_room, x, y);
                    }
                }
                menu_release_frames = 0;
            }
            continue;
        }

        if (traffic_state == TRAFFIC_IMPACT || traffic_state == TRAFFIC_MESSAGE ||
            (player_flash > 0 && (player_flash & FLASH_TOGGLE_BIT))) {
            for (i = 0; i < PLAYER_SPRITE_SLOTS; ++i) hide_oam_sprite(i);
        } else {
            set_sprite(x, y, player_walk_frame);
        }
        if (traffic_state == TRAFFIC_APPROACH ||
            (traffic_state == TRAFFIC_IMPACT && taxi_visible)) {
            set_taxi_sprites(taxi_x, neon_avenue_traffic.lane_y, taxi_moves_left);
        } else {
            hide_taxi_sprites();
        }
        if (traffic_state == TRAFFIC_IMPACT || traffic_state == TRAFFIC_MESSAGE) {
            set_traffic_particles(x, y);
            set_traffic_bones(x, y, x >= 128);
            set_traffic_head(x, y, x >= 128);
        } else {
            hide_traffic_particles();
            hide_traffic_bones();
            hide_traffic_head();
        }
        for (i = 0; i < NPC_COUNT; ++i) {
            if (npc_state[i].room == current_room) {
                set_npc_sprite(i);
            } else {
                hide_npc_sprite(i);
            }
        }
        if (current_room == ROOM_TOWN) set_bar_regular_sprites();
        if (current_room == ROOM_TOWN) set_bar_door_knob();
        else hide_oam_sprite(BAR_DOORKNOB_SLOT);
        for (i = 0; i < ENEMY_COUNT; ++i) {
            if (enemies[i].room != current_room || !enemies[i].alive) {
                hide_oam_sprite(ENEMY_SLOT(i));
            } else if (enemies[i].flash_timer > 0 && (enemies[i].flash_timer & FLASH_TOGGLE_BIT)) {
                hide_oam_sprite(ENEMY_SLOT(i));
            } else {
                set_enemy_sprite(i);
            }
        }
        for (i = 0; i < CRITTER_COUNT; ++i) {
            if (critters[i].room != current_room) {
                hide_oam_sprite(CRITTER_SLOT(i));
            } else {
                set_critter_sprite(i);
            }
        }
        if (sword_timer > 0) {
            unsigned char sx = x, sy = y;
            unsigned char reach = (sword_timer > SWORD_VISIBLE_FRAMES / 2) ?
                SWORD_THRUST_REACH : ATTACK_REACH;
            switch (sword_facing) {
                case FACING_UP:    sy = y - reach; break;
                case FACING_DOWN:  sy = y + reach; break;
                case FACING_LEFT:  sx = x - reach; break;
                case FACING_RIGHT: sx = x + reach; break;
            }
            set_oam_sprite(SWORD_SLOT, sx, sy, sword_tile, sword_attr);
        } else {
            hide_oam_sprite(SWORD_SLOT);
        }

        if (++schedule_tick >= NPC_SCHEDULE_PERIOD) {
            schedule_tick = 0;
            schedule_phase ^= 1;
        }
        /* Frozen while a text box is open, same as player movement --
         * otherwise a patrolling NPC (the meadow one) could visibly
         * walk off, or even leave the room entirely, out from under a
         * conversation still on screen with it. schedule_tick/phase
         * still advance, so the schedule keeps ticking; it just catches
         * back up to wherever it's supposed to be once the box closes. */
        if (!text_box_open) {
            for (i = 0; i < NPC_COUNT; ++i) {
                if (i == JIMBLES_BARTENDER_NPC) update_bartender_pace();
                else update_npc_walk(i);
            }
        }
        /* Both loops below skip entities outside current_room -- unlike
         * NPCs (which must keep walking off-screen to finish a room
         * crossing on schedule), enemies/critters never leave their home
         * room, so there's nothing for an off-screen update to
         * accomplish except spend cycles: a wander tick plus up to 4
         * blocked-tile checks (each a room.c decoration-list scan) for
         * an entity nobody can see right now. Gating this was worth
         * roughly a wander update per idle entity per frame -- small per
         * entity, but it was happening unconditionally for every enemy
         * and critter in the game on every single frame regardless of
         * which room the player was actually in. */
        for (i = 0; i < ENEMY_COUNT; ++i) {
            if (enemies[i].room == current_room) {
                if (enemies[i].knockback_timer > 0) {
                    knockback_step(&enemies[i].x, &enemies[i].y, enemies[i].knockback_pad,
                                   &room_layouts[current_room]);
                    knockback_step(&enemies[i].x, &enemies[i].y, enemies[i].knockback_pad,
                                   &room_layouts[current_room]);
                    --enemies[i].knockback_timer;
                } else {
                    update_enemy_wander(i);
                }
            }
            if (enemies[i].flash_timer > 0) --enemies[i].flash_timer;
        }
        for (i = 0; i < CRITTER_COUNT; ++i) {
            if (critters[i].room == current_room) {
                update_critter_wander(i);
            }
        }
        if (sword_timer > 0)    --sword_timer;
        if (player_flash > 0)   --player_flash;
        if (player_invuln > 0)  --player_invuln;
        pad = read_pad();
        npc_idx = npc_in_room(current_room);

        /* Once the taxi is committed, no input can dodge or pause the gag.
         * The message state deliberately falls through so A can dismiss it. */
        if (traffic_state == TRAFFIC_APPROACH) {
            if (taxi_moves_left) {
                taxi_x -= TRAFFIC_TAXI_SPEED;
                if (taxi_x <= x) {
                    traffic_state = TRAFFIC_IMPACT;
                    traffic_timer = TRAFFIC_IMPACT_FRAMES;
                    music_sfx_hurt();
                }
            } else {
                taxi_x += TRAFFIC_TAXI_SPEED;
                if (taxi_x + TAXI_WIDTH >= x) {
                    traffic_state = TRAFFIC_IMPACT;
                    traffic_timer = TRAFFIC_IMPACT_FRAMES;
                    music_sfx_hurt();
                }
            }
            continue;
        }
        if (traffic_state == TRAFFIC_IMPACT) {
            /* The hit does not freeze the cab in place: it continues across
             * the lane until the whole 32px sprite has left the screen. */
            if (taxi_moves_left) {
                if (taxi_x >= TRAFFIC_TAXI_SPEED) taxi_x -= TRAFFIC_TAXI_SPEED;
                else taxi_visible = 0;
            } else {
                if (taxi_x < 256 - TAXI_WIDTH) taxi_x += TRAFFIC_TAXI_SPEED;
                else taxi_visible = 0;
            }
            if (--traffic_timer == 0) {
                traffic_state = TRAFFIC_MESSAGE;
                text_box_open = 1;
                death_text_box = 1;
                pending_text_message = neon_avenue_traffic.message;
                text_box_dirty = 1;
                music_sfx_dialogue();
            }
            continue;
        }

        if (!(pad & JOY_START_MASK)) {
            if (start_release_frames < MENU_DEBOUNCE_FRAMES) ++start_release_frames;
        } else if (start_release_frames >= MENU_DEBOUNCE_FRAMES) {
            menu_open = 1;
            menu_inventory_open = 0;
            menu_selection = MENU_ITEM_INVENTORY;
            menu_dirty = 1;
            menu_release_frames = 0;
            start_release_frames = 0;
            music_sfx_dialogue();
            continue;
        }

        if (pad & PAD_UP_MASK) player_facing = FACING_UP;
        else if (pad & PAD_DOWN_MASK) player_facing = FACING_DOWN;
        else if (pad & PAD_LEFT_MASK) player_facing = FACING_LEFT;
        else if (pad & PAD_RIGHT_MASK) player_facing = FACING_RIGHT;

        /* Debounced (only once talk_release_frames shows the button was
         * actually let go for a bit, not every frame it's held) so
         * holding it doesn't rapidly toggle. A closes an open box or
         * talks to a nearby NPC (if any -- boxes_overlap()'s pad
         * argument gives a little reach beyond exact-touching, since
         * collision already stops the player from standing exactly on
         * the NPC's tile). */
        if (pad & TALK_MASK) {
            if (talk_release_frames >= TALK_DEBOUNCE_FRAMES) {
                if (text_box_open) {
                    text_box_open = 0;
                    text_box_dirty = 1;
                } else if (current_room == ROOM_TOWN &&
                           boxes_overlap(x, y, BAR_JUKEBOX_X, BAR_JUKEBOX_Y, INTERACT_RANGE)) {
                    text_box_open = 1;
                    if (!items[BAR_TOKEN_ITEM].taken) {
                        pending_text_message = jukebox_token_request;
                        music_sfx_dialogue();
                    } else if (!bar_backroom_unlocked) {
                        bar_backroom_unlocked = 1;
                        bar_scene_dirty = 1;
                        pending_text_message = "THE JUKEBOX PLAYS. THE DOOR OPENS.";
                        music_play_jukebox_fanfare();
                    } else {
                        pending_text_message = "THE JUKEBOX IS PLAYING YOUR SONG.";
                        music_sfx_dialogue();
                    }
                    text_box_dirty = 1;
                } else if (current_room == ROOM_TOWN &&
                           boxes_overlap(x, y, BAR_BACKROOM_X, BAR_BACKROOM_Y, INTERACT_RANGE)) {
                    if (bar_backroom_unlocked) {
                        current_room = ROOM_CASINO;
                        x = 120;
                        y = 200;
                        music_sfx_door();
                        load_room(current_room, x, y);
                    } else {
                        text_box_open = 1;
                        pending_text_message = "THE BACK ROOM IS LOCKED TIGHT.";
                        text_box_dirty = 1;
                        music_sfx_dialogue();
                    }
                } else if (current_room == ROOM_CASINO &&
                           boxes_overlap(x, y, CASINO_SLOT_X, CASINO_SLOT_Y, CASINO_SLOT_RANGE)) {
                    slot_game_open = 1;
                    slot_game_dirty = 1;
                    slot_result = 0;
                    slot_release_frames = 0;
                } else if (current_room == ROOM_TOWN &&
                           boxes_overlap(x, y, bar_regulars[0].x, bar_regulars[0].y, INTERACT_RANGE)) {
                    text_box_open = 1;
                    pending_text_message = bar_regulars[0].dialogue;
                    text_box_dirty = 1;
                    music_sfx_dialogue();
                } else if (current_room == ROOM_TOWN &&
                           boxes_overlap(x, y, bar_regulars[1].x, bar_regulars[1].y, INTERACT_RANGE)) {
                    text_box_open = 1;
                    pending_text_message = bar_regulars[1].dialogue;
                    text_box_dirty = 1;
                    music_sfx_dialogue();
                } else if (npc_idx != NPC_NONE &&
                           boxes_overlap(x, y, npc_state[npc_idx].x, npc_state[npc_idx].y,
                                         npc_idx == JIMBLES_BARTENDER_NPC ? 22 : INTERACT_RANGE)) {
                    text_box_open = 1;
                    pending_text_message = npc_dialogue(npc_idx);
                    text_box_dirty = 1;
                    music_sfx_dialogue();
                    if (!npcs[npc_idx].quest_complete && npc_quest_ready(npc_idx)) {
                        npcs[npc_idx].quest_complete = 1;
                        set_npc_sprite(npc_idx);
                    }
                }
            }
            talk_release_frames = 0;
        } else if (talk_release_frames < TALK_DEBOUNCE_FRAMES) {
            ++talk_release_frames;
        }

        /* Frozen while reading, same as Zelda-style dialogue. */
        if (text_box_open) {
            continue;
        }

        if (traffic_state == TRAFFIC_MESSAGE) {
            traffic_state = TRAFFIC_IDLE;
            if (!begin_new_game(&x, &y)) return;
            player_hp = PLAYER_MAX_HP;
            player_invuln = 0;
            player_flash = 0;
            player_knockback_timer = 0;
            sword_timer = 0;
            load_room(current_room, x, y);
            continue;
        }

        if (npc_idx != NPC_NONE) {
            npc_obstacle.x = npc_state[npc_idx].x;
            npc_obstacle.y = npc_state[npc_idx].y;
        }
        exits = EXIT_NONE;
        if (player_knockback_timer > 0) {
            knockback_step(&x, &y, player_knockback_pad, &room_layouts[current_room]);
            knockback_step(&x, &y, player_knockback_pad, &room_layouts[current_room]);
            --player_knockback_timer;
        } else {
            for (i = 0; i < PLAYER_MOVE_STEPS; ++i) {
                exits |= update_position(&x, &y, pad, &room_layouts[current_room],
                                          npc_idx != NPC_NONE ? &npc_obstacle : 0);
            }
        }

        /* Neon Avenue is a stage-like scene, not a top-down field. Its
         * building is painted in the upper screen, so prevent the legacy
         * room grid from letting the player's feet climb into that facade. */
        if (current_room == ROOM_MEADOW && y < NEON_AVENUE_WALK_TOP) {
            y = NEON_AVENUE_WALK_TOP;
            exits &= (unsigned char)~EXIT_NORTH;
        }
        if (current_room == ROOM_TOWN && y < JIMBLES_BAR_WALK_TOP) {
            y = JIMBLES_BAR_WALK_TOP;
            exits &= (unsigned char)~EXIT_NORTH;
        }
        if (current_room == ROOM_CASINO && y < CASINO_WALK_TOP) {
            y = CASINO_WALK_TOP;
            exits &= (unsigned char)~EXIT_NORTH;
        }

        if (current_room == ROOM_CASINO && y >= CASINO_EXIT_Y &&
            x >= CASINO_EXIT_LEFT && x <= CASINO_EXIT_RIGHT) {
            current_room = ROOM_TOWN;
            x = 32;
            y = 124;
            music_sfx_door();
            load_room(current_room, x, y);
            continue;
        }

        if (traffic_state == TRAFFIC_IDLE && current_room == neon_avenue_traffic.room &&
            y >= neon_avenue_traffic.trigger_y) {
            traffic_state = TRAFFIC_APPROACH;
            taxi_moves_left = x < 128;
            /* A 32px taxi at X=248 wraps its later sprite columns back to
             * the left edge. Start fully on-screen at the far right instead. */
            taxi_x = taxi_moves_left ? (256 - TAXI_WIDTH) : 0;
            taxi_visible = 1;
        }

        if (pad & (PAD_UP_MASK | PAD_DOWN_MASK | PAD_LEFT_MASK | PAD_RIGHT_MASK)) {
            if (++player_walk_tick >= PLAYER_WALK_FRAME_PERIOD) {
                player_walk_tick = 0;
                player_walk_frame ^= 1;
            }
        } else {
            player_walk_tick = 0;
            player_walk_frame = 0;
        }

        /* Zelda-style walk-over pickup: no button needed, just touch it.
         * Doesn't block movement (unlike an NPC) since there's nothing to
         * walk around before picking it up. The text box is the only
         * feedback that anything happened -- without it, picking up an
         * item that's just quietly removed from the room is invisible,
         * since there's no inventory display yet to check afterward. At
         * most one pickup per frame -- items don't overlap each other,
         * so this never skips a real one. */
        for (i = 0; i < ITEM_COUNT; ++i) {
            if (!items[i].taken && items[i].room == current_room &&
                boxes_overlap(x, y, items[i].x, items[i].y, 0)) {
                items[i].taken = 1;
                hide_oam_sprite(ITEM_SLOT(i));
                music_sfx_pickup();
                text_box_open = 1;
                pending_text_message = items[i].pickup_message;
                text_box_dirty = 1;
                break;
            }
        }

        /* Same walk-over feel as an item pickup, but moves the player
         * instead of collecting anything -- see Portal's comment above
         * for why this needs its own mechanism rather than reusing the
         * room_connections/EXIT_* edge-crossing path below. load_room()
         * handles the redraw exactly like a normal room change does. */
        for (i = 0; i < PORTAL_COUNT; ++i) {
            if (portals[i].room == current_room &&
                boxes_overlap(x, y, portals[i].x, portals[i].y, 0)) {
                current_room = portals[i].dest_room;
                x = portals[i].dest_x;
                y = portals[i].dest_y;
                music_sfx_door();
                load_room(current_room, x, y);
                break;
            }
        }

        /* Contact damage: touching a live enemy costs HP once, then
         * player_invuln holds off the next hit for CONTACT_INVULN_FRAMES
         * -- without that, standing inside its box would drain HP every
         * single frame. Unlike an attack, this isn't debounced by a
         * button -- the cooldown itself is what prevents repeats. No
         * death handling yet; hp just clamps at 0 (see PLAYER_MAX_HP). */
        if (player_invuln == 0) {
            for (i = 0; i < ENEMY_COUNT; ++i) {
                if (enemies[i].alive && enemies[i].room == current_room &&
                    boxes_overlap(x, y, enemies[i].x, enemies[i].y, 0)) {
                    player_hp = player_hp > CONTACT_DAMAGE ? player_hp - CONTACT_DAMAGE : 0;
                    player_invuln = CONTACT_INVULN_FRAMES;
                    player_flash = PLAYER_FLASH_FRAMES;
                    player_knockback_pad = knockback_direction_away(enemies[i].x, enemies[i].y, x, y);
                    player_knockback_timer = KNOCKBACK_FRAMES;
                    music_sfx_hurt();
                    if (player_hp == 0) {
                        death_timer = DEATH_RESPAWN_FRAMES;
                        text_box_open = 1;
                        pending_text_message = "YOU FAINTED";
                        text_box_dirty = 1;
                    }
                    break;
                }
            }
        }

        if (death_timer > 0) continue;

        /* Debounced the same way as TALK_MASK, for the same reason.
         * Only runs at all while nothing else has frozen the frame
         * (text_box_open already `continue`d above), so there's no way
         * to swing mid-dialogue. The attack (hit-check) point is one
         * tile in front of the player along player_facing -- ATTACK_PAD
         * gives a little forgiveness the same way INTERACT_RANGE does
         * for talking, rather than demanding pixel-perfect alignment.
         * The sword sprite shows regardless of whether anything was
         * there to hit (you swung either way), oriented to player_facing
         * and thrust from near to far (rendered above); hit/miss/defeat
         * feedback used to be a text box per swing, which would've
         * frozen the frame on every single attack -- now a non-lethal
         * hit just sets the enemy's flash_timer and a miss is silent, so
         * combat stays real-time instead of stopping to show a message
         * each time. */
        if (pad & ATTACK_MASK) {
            if (attack_release_frames >= ATTACK_DEBOUNCE_FRAMES) {
                unsigned char ax = x, ay = y;
                unsigned char enemy_knockback_pad;
                sword_facing = player_facing;
                switch (player_facing) {
                    case FACING_UP:
                        ay = y - ATTACK_REACH;
                        enemy_knockback_pad = PAD_UP_MASK;
                        sword_tile = SWORD_TILE_VERTICAL;
                        sword_attr = SWORD_SPRITE_PALETTE;
                        break;
                    case FACING_DOWN:
                        ay = y + ATTACK_REACH;
                        enemy_knockback_pad = PAD_DOWN_MASK;
                        sword_tile = SWORD_TILE_VERTICAL;
                        sword_attr = SWORD_SPRITE_PALETTE | OAM_FLIP_V;
                        break;
                    case FACING_LEFT:
                        ax = x - ATTACK_REACH;
                        enemy_knockback_pad = PAD_LEFT_MASK;
                        sword_tile = SWORD_TILE_HORIZONTAL;
                        sword_attr = SWORD_SPRITE_PALETTE;
                        break;
                    case FACING_RIGHT:
                        ax = x + ATTACK_REACH;
                        enemy_knockback_pad = PAD_RIGHT_MASK;
                        sword_tile = SWORD_TILE_HORIZONTAL;
                        sword_attr = SWORD_SPRITE_PALETTE | OAM_FLIP_H;
                        break;
                }
                sword_timer = SWORD_VISIBLE_FRAMES;
                music_sfx_swing();
                for (i = 0; i < ENEMY_COUNT; ++i) {
                    if (!enemies[i].alive || enemies[i].room != current_room ||
                        !boxes_overlap(ax, ay, enemies[i].x, enemies[i].y, ATTACK_PAD)) {
                        continue;
                    }
                    if (roll_hit(PLAYER_ACCURACY, enemies[i].evasion)) {
                        music_sfx_hit();
                        if (enemies[i].hp <= PLAYER_DAMAGE) {
                            enemies[i].hp = 0;
                            enemies[i].alive = 0;
                        } else {
                            enemies[i].hp -= PLAYER_DAMAGE;
                            enemies[i].flash_timer = ENEMY_FLASH_FRAMES;
                            enemies[i].knockback_pad = enemy_knockback_pad;
                            enemies[i].knockback_timer = KNOCKBACK_FRAMES;
                            enemies[i].wander_timer = 0;
                        }
                    }
                    break;
                }
            }
            attack_release_frames = 0;
        } else if (attack_release_frames < ATTACK_DEBOUNCE_FRAMES) {
            ++attack_release_frames;
        }

        conn = &room_connections[current_room];

        if ((exits & EXIT_EAST) && conn->east != ROOM_NONE) {
            entry_spawn = &room_entry_spawns[current_room][1];
            current_room = conn->east;
            world_apply_spawn(entry_spawn, &x, &y);
            load_room(current_room, x, y);
        } else if ((exits & EXIT_WEST) && conn->west != ROOM_NONE) {
            entry_spawn = &room_entry_spawns[current_room][3];
            current_room = conn->west;
            world_apply_spawn(entry_spawn, &x, &y);
            load_room(current_room, x, y);
        } else if ((exits & EXIT_NORTH) && conn->north != ROOM_NONE) {
            entry_spawn = &room_entry_spawns[current_room][0];
            current_room = conn->north;
            world_apply_spawn(entry_spawn, &x, &y);
            load_room(current_room, x, y);
        } else if ((exits & EXIT_SOUTH) && conn->south != ROOM_NONE) {
            entry_spawn = &room_entry_spawns[current_room][2];
            current_room = conn->south;
            world_apply_spawn(entry_spawn, &x, &y);
            load_room(current_room, x, y);
        }
    }
}
