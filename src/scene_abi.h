/* scene_abi.h -- contract between the fixed MMC3 engine and banked scenes.
 *
 * A scene runs only while its PRG bank is mapped. It may mutate GameState
 * and queue rendering work, but it must return to the fixed-bank dispatcher
 * before another bank can be selected. */
#ifndef SCENE_ABI_H
#define SCENE_ABI_H

typedef struct {
    unsigned char room;
    unsigned char player_x;
    unsigned char player_y;
    unsigned char flags;
} SceneGameState;

typedef struct {
    unsigned char prg_bank;
    unsigned char chr_bank;
    void (*enter)(SceneGameState *state);
    void (*update)(SceneGameState *state, unsigned char pad);
    void (*render)(const SceneGameState *state);
    void (*leave)(SceneGameState *state);
} SceneModule;

#endif
