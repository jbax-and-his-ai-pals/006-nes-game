#include "v2_game_state.h"
#include "rng.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

V2GameState game_state;

void v2_game_reset(void)
{
    game_state.token_taken = 0;
    game_state.has_token = 0;
    game_state.door_unlocked = 0;
    game_state.has_beer = 0;
    game_state.jimbles_limit_given = 0;
    game_state.has_paper_clip = 0;
    game_state.has_sewer_key = 0;
    game_state.has_flamethrower = 0;
    game_state.zoo_slugs_cleared = 0;
    game_state.zoo_gate_open = 0;
    game_state.has_zoo_map = 0;
    game_state.jimbles_x = 116;
    game_state.transfer_y = 0;
    game_state.transfer_y_valid = 0;
    v2_set_route(V2_SCENE_AVENUE, 0);
    /* casino_reset()'s own logic, inlined -- v2_casino.c moved out of the
     * fixed engine into the casino scene's own bank (see
     * build_scene_casino.bat) once blackjack/poker's flush support made
     * it too big to keep duplicating there via fixed_scene_stubs.c's
     * reachability trick (see REBUILD_ROADMAP.md's "CHR Budget" section
     * for the same fixed-engine-budget pressure in general). This is the
     * one casino_* call the fixed engine still needs -- v2_game_reset()
     * runs before any scene is mapped, so it can't reach into a scene
     * bank the way scene code reaches into the fixed engine. */
    game_state.casino.credits = 5;   /* real money now; start with $5 */
    game_state.casino.bet = 1;
    game_state.casino.reel[0] = 0;
    game_state.casino.reel[1] = 0;
    game_state.casino.reel[2] = 0;
    game_state.casino.blackjack_state = CASINO_GAME_READY;
    game_state.casino.poker_state = CASINO_GAME_READY;
    rng_seed(&game_state.casino.rng, random_seed());
}

void v2_set_route(V2SceneId scene, unsigned char spawn)
{
    game_state.route.scene = scene;
    game_state.route.spawn = spawn;
}
