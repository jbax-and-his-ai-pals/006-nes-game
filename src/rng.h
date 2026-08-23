/*
 * rng.h -- a tiny 8-bit pseudo-random generator.
 *
 * Hardware-independent (no <nes.h>, no register access) so it's
 * unit-testable under sim65 like movement.c/room.c/font.c. The NES has
 * no true entropy source; real games usually seed from something that
 * varies at boot (a free-running counter during a title screen, the
 * player's first input timing, etc.) -- this project doesn't have a
 * title screen yet, so the quest generator is currently called with a fixed
 * compile-time value (see main.c's generate_quests()), making quest
 * assignment deterministic across runs rather than truly random. That's
 * a known, acceptable limitation for proving the generation pipeline
 * itself; wiring up a real entropy source is separate, later work.
 */

#ifndef RNG_H
#define RNG_H

typedef struct { unsigned char state; } RngState;

/* Resets one independent sequence. 0 is remapped to 1 -- an all-zero state
 * is a fixed point for the LFSR below (it would never produce anything but
 * more zeroes). */
void rng_seed(RngState *rng, unsigned char seed);

/* Returns the next byte in the sequence (0-255, every non-zero value
 * exactly once every 255 calls -- an 8-bit Galois LFSR is a maximal
 * length generator, not uniform-random, but good enough for picking
 * among a handful of quest items). */
unsigned char rng_next(RngState *rng);

#endif
