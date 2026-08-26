/* v2_drink.c -- see v2_drink.h for the rules contract. */

#include "v2_drink.h"

void drink_init(DrinkState *drink)
{
    drink->liquid = DRINK_GLASS_UNITS;
    drink->foam = 0;
    drink->warmth = 0;
    drink->result = DRINK_RESULT_ACTIVE;
}

unsigned char drink_take(DrinkState *drink)
{
    if (drink->result != DRINK_RESULT_ACTIVE) return drink->result;
    if (drink->liquid) --drink->liquid;
    /* Five drinks at most, so foam never approaches the 8-bit ceiling. */
    drink->foam = (unsigned char)(drink->foam + DRINK_FOAM_PER_DRINK);
    /* Spill takes precedence over emptying the glass: the 5th gulp that
     * sloshes the rim is a spill, not a win (see test_v2_drink.c's
     * "sloppy final-gulp spill" case). */
    if (drink->foam >= DRINK_SPILL_FOAM) {
        drink->result = DRINK_RESULT_SPILL;
    } else if (!drink->liquid) {
        drink->result = DRINK_RESULT_WIN;
    }
    return drink->result;
}

unsigned char drink_settle(DrinkState *drink)
{
    if (drink->result != DRINK_RESULT_ACTIVE) return drink->result;
    if (drink->foam >= DRINK_FOAM_PER_SETTLE) {
        drink->foam = (unsigned char)(drink->foam - DRINK_FOAM_PER_SETTLE);
    } else {
        /* Settling an already-low head wastes the relief: clamped at zero
         * so a wasted settle cannot bank foam credit for later. */
        drink->foam = 0;
    }
    if (drink->warmth < DRINK_SNATCH_WARMTH) ++drink->warmth;
    if (drink->warmth >= DRINK_SNATCH_WARMTH) {
        drink->result = DRINK_RESULT_SNATCH;
    }
    return drink->result;
}
