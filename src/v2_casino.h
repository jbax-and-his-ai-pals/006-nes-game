#ifndef V2_CASINO_H
#define V2_CASINO_H

#include "rng.h"

#define CASINO_REEL_COUNT 3
#define CASINO_SYMBOL_COUNT 3
#define CASINO_MAX_CREDITS 255

typedef struct {
    unsigned char credits;
    unsigned char bet;
    unsigned char reel[CASINO_REEL_COUNT];
    RngState rng;
} CasinoState;

void casino_reset(CasinoState *casino);
unsigned char casino_is_winner(const CasinoState *casino);
unsigned char casino_payout(const CasinoState *casino);
/* Spins only when the player can cover the selected bet. Returns the payout. */
unsigned char casino_spin(CasinoState *casino);

#endif
