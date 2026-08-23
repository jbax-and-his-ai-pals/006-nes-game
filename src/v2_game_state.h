#ifndef V2_GAME_STATE_H
#define V2_GAME_STATE_H

#include "v2_casino.h"

/* Persistent state survives changing v2 scenes. It deliberately contains no
 * PPU or mapper state, so it can move into the fixed bank unchanged later. */
typedef struct {
    unsigned char token_taken;
    unsigned char has_token;
    unsigned char door_unlocked;
    unsigned char has_beer;
    /* First proper Mart inventory pickup. Kept separate from presentation so
     * a future rebuilt inventory screen can list it directly. */
    unsigned char has_paper_clip;
    unsigned char jimbles_x;
    CasinoState casino;
} V2GameState;

extern V2GameState game_state;
void v2_game_reset(void);

#endif
