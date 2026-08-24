/* scene_abi.h -- contract between the fixed MMC3 engine and banked scenes.
 *
 * A scene runs only while its PRG bank is mapped. It may mutate GameState
 * and queue rendering work, but it must return to the fixed-bank dispatcher
 * before another bank can be selected. */
#ifndef SCENE_ABI_H
#define SCENE_ABI_H

#define SCENE_PRG_BANK_COUNT 2

typedef struct {
    unsigned char room;
    unsigned char player_x;
    unsigned char player_y;
    unsigned char flags;
} SceneGameState;

typedef struct {
    /* MMC3 maps this contiguous pair into $8000-$BFFF. A scene never changes
     * it itself; only the fixed-bank dispatcher may select or restore it via
     * mmc3_select_prg_pair(). */
    unsigned char prg_bank_first;
    unsigned char chr_bank;
    void (*enter)(SceneGameState *state);
    void (*update)(SceneGameState *state, unsigned char pad);
    void (*render)(const SceneGameState *state);
    void (*leave)(SceneGameState *state);
} SceneModule;

#endif
