#ifndef V2_GAME_STATE_H
#define V2_GAME_STATE_H

#include "v2_casino.h"

/* Fixed-engine route state. Scene modules return a local exit code; the
 * dispatcher translates it into one of these location/spawn pairs. */
typedef enum {
    V2_SCENE_AVENUE,
    V2_SCENE_BAR,
    V2_SCENE_MART,
    V2_SCENE_SEWER,
    V2_SCENE_PARK,
    V2_SCENE_CASINO,
    V2_SCENE_ZOO,
    V2_SCENE_COMMENTARIUM
} V2SceneId;

typedef struct {
    V2SceneId scene;
    unsigned char spawn;
} V2SceneRoute;

/* Persistent state survives changing v2 scenes. It deliberately contains no
 * PPU or mapper state, so it can move into the fixed bank unchanged later. */
typedef struct {
    unsigned char token_taken;
    unsigned char has_token;
    unsigned char door_unlocked;
    unsigned char has_beer;
    unsigned char jimbles_limit_given;
    /* First proper Mart inventory pickup. Kept separate from presentation so
     * a future rebuilt inventory screen can list it directly. */
    unsigned char has_paper_clip;
    unsigned char has_sewer_key;
    unsigned char has_flamethrower;
    unsigned char zoo_slugs_cleared;
    unsigned char zoo_gate_open;
    unsigned char has_zoo_map;
    unsigned char jimbles_x;
    /* Dedicated pixel coordinate for a scene-to-scene side passage. This is
     * intentionally separate from route.spawn, which the dispatcher owns. */
    unsigned char transfer_y;
    unsigned char transfer_y_valid;
    V2SceneRoute route;
    CasinoState casino;
} V2GameState;

extern V2GameState game_state;
void v2_game_reset(void);
void v2_set_route(V2SceneId scene, unsigned char spawn);

#endif
