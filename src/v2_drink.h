#ifndef V2_DRINK_H
#define V2_DRINK_H

/* Hardware-independent rules for Jimbles's chugging challenge in the bar
 * scene. Turn-based: each round the player either drinks (lowering the
 * liquid but raising the foam head) or lets the head settle (relieving
 * foam but warming the beer). Both failure clocks are symmetric with the
 * win requirement: five drinks need at least two settles to keep foam
 * under the rim, and warmth allows at most two settles before Jimbles
 * snatches the glass -- so a winning play uses exactly its settle budget.
 *
 * Like v2_casino.c this is pure logic with no <nes.h>, so it compiles for
 * the host test harness (sim6502) and the bar scene bank alike. */

#define DRINK_GLASS_UNITS 5
#define DRINK_FOAM_PER_DRINK 2
#define DRINK_FOAM_PER_SETTLE 3
#define DRINK_SPILL_FOAM 6
#define DRINK_SNATCH_WARMTH 3

enum {
    DRINK_RESULT_ACTIVE,   /* the chug is still in progress */
    DRINK_RESULT_WIN,      /* glass emptied before either failure */
    DRINK_RESULT_SPILL,    /* foam overflowed the rim */
    DRINK_RESULT_SNATCH    /* beer went too warm; Jimbles took it */
};

typedef struct {
    unsigned char liquid;   /* units left to drink (5..0) */
    unsigned char foam;     /* head size; >= DRINK_SPILL_FOAM spills */
    unsigned char warmth;   /* settles taken; >= DRINK_SNATCH_WARMTH snatches */
    unsigned char result;   /* DRINK_RESULT_* */
} DrinkState;

void drink_init(DrinkState *drink);
/* One round action. Returns the current DRINK_RESULT_* (the caller should
 * keep looping while it is DRINK_RESULT_ACTIVE). Actions after the game
 * has ended are no-ops. */
unsigned char drink_take(DrinkState *drink);
unsigned char drink_settle(DrinkState *drink);

#endif
