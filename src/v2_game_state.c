#include "v2_game_state.h"

V2GameState game_state;

void v2_game_reset(void)
{
    game_state.token_taken = 0;
    game_state.has_token = 0;
    game_state.door_unlocked = 0;
    game_state.has_beer = 0;
    game_state.has_paper_clip = 0;
    game_state.jimbles_x = 116;
    casino_reset(&game_state.casino);
}
