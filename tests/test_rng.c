/*
 * test_rng.c -- unit tests for src/rng.c, run under cc65's sim65 (no
 * NES hardware, no emulator).
 */

#include <stdio.h>

#include "../src/rng.h"

static int failures = 0;

/* File-scope, not stack locals -- cc65 caps how many local variables a
 * single sim6502 function can allocate, and a 256-byte array plus
 * several scalars in one function tripped it ("too many local
 * variables"). */
static unsigned char seen[256];
static unsigned char first[5];
static RngState rng_a;
static RngState rng_b;

static void check_eq(const char *name, int actual, int expected)
{
    if (actual != expected) {
        printf("FAIL %s: got %d, expected %d\n", name, actual, expected);
        ++failures;
    } else {
        printf("PASS %s\n", name);
    }
}

static void check_repeat_seed(void)
{
    unsigned char i;

    rng_seed(&rng_a, 42);
    for (i = 0; i < 5; ++i) first[i] = rng_next(&rng_a);
    rng_seed(&rng_a, 42);
    check_eq("repeat seed matches call 0", rng_next(&rng_a), first[0]);
    check_eq("repeat seed matches call 1", rng_next(&rng_a), first[1]);
    check_eq("repeat seed matches call 2", rng_next(&rng_a), first[2]);
    check_eq("repeat seed matches call 3", rng_next(&rng_a), first[3]);
    check_eq("repeat seed matches call 4", rng_next(&rng_a), first[4]);
}

/* Maximal-length property: starting from any non-zero seed, the
 * generator visits every one of the 255 non-zero byte values exactly
 * once before returning to the seed -- this is what actually verifies
 * the LFSR tap constant is correct, not just "produces some numbers."
 * A wrong tap value would show up here as a short cycle or an early
 * repeat. */
static void check_maximal_length(void)
{
    unsigned int i;
    unsigned char v, saw_zero = 0, cycle_ok = 1;

    rng_seed(&rng_a, 1);
    for (i = 0; i < 256; ++i) seen[i] = 0;
    for (i = 0; i < 255; ++i) {
        v = rng_next(&rng_a);
        if (v == 0) saw_zero = 1;
        if (seen[v]) cycle_ok = 0;
        seen[v] = 1;
    }
    check_eq("maximal length: never produces 0", saw_zero, 0);
    check_eq("maximal length: all 255 non-zero values distinct", cycle_ok, 1);
    /* v now holds the 255th call's result -- a full cycle from seed 1
     * lands back on 1 exactly there, not one call later. */
    check_eq("maximal length: cycle returns to seed after 255 steps", v, 1);
}

int main(void)
{
    check_repeat_seed();

    /* Seed 0 is remapped to 1, not left as a fixed point at 0. */
    rng_seed(&rng_a, 0);
    check_eq("seed 0 does not produce 0", rng_next(&rng_a) != 0, 1);
    rng_seed(&rng_a, 42);
    rng_seed(&rng_b, 42);
    (void)rng_next(&rng_a);
    check_eq("independent state is unaffected by another stream", rng_next(&rng_b), first[0]);

    check_maximal_length();

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
