#include "rng.h"

#ifdef __NES__
#pragma code-name ("FIXED_ENGINE")
#pragma rodata-name ("FIXED_ENGINE_RODATA")
#endif

/* 0xB8 (binary 10111000) is a maximal-length tap set for an 8-bit
 * Galois LFSR -- every non-zero starting state visits all 255 non-zero
 * values before repeating. */
#define LFSR_TAPS 0xB8

void rng_seed(RngState *rng, unsigned char seed)
{
    rng->state = seed ? seed : 1;
}

unsigned char rng_next(RngState *rng)
{
    unsigned char lsb = rng->state & 1;
    rng->state >>= 1;
    if (lsb) {
        rng->state ^= LFSR_TAPS;
    }
    return rng->state;
}
