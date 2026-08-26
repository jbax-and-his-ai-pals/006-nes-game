#include <stdio.h>

#include "../src/v2_drink.h"

static int failures = 0;

static void check_eq(const char *name, int actual, int expected)
{
    if (actual != expected) {
        printf("FAIL %s: got %d, expected %d\n", name, actual, expected);
        ++failures;
    } else {
        printf("PASS %s\n", name);
    }
}

/* 1 = drink, 2 = settle. Simulates the whole sequence and returns the
 * final DRINK_RESULT_*. */
static unsigned char run_sequence(const unsigned char *actions,
                                  unsigned char count, DrinkState *out)
{
    unsigned char i;

    drink_init(out);
    for (i = 0; i < count; ++i) {
        if (actions[i] == 1) {
            (void)drink_take(out);
        } else {
            (void)drink_settle(out);
        }
    }
    return out->result;
}

int main(void)
{
    DrinkState d;
    unsigned char actions[7];
    unsigned char i;
    unsigned int mask;
    unsigned char wins = 0;

    drink_init(&d);
    check_eq("init liquid", d.liquid, 5);
    check_eq("init foam", d.foam, 0);
    check_eq("init warmth", d.warmth, 0);
    check_eq("init result", d.result, DRINK_RESULT_ACTIVE);

    /* The canonical and alternative winning lines. */
    {
        static const unsigned char line1[] = {1, 1, 2, 1, 1, 2, 1};
        static const unsigned char line2[] = {1, 1, 2, 1, 2, 1, 1};
        static const unsigned char line3[] = {1, 2, 1, 1, 2, 1, 1};
        check_eq("canonical DDSDDSD wins", run_sequence(line1, 7, &d),
                 DRINK_RESULT_WIN);
        check_eq("DDSDSDD wins", run_sequence(line2, 7, &d), DRINK_RESULT_WIN);
        check_eq("DSDDSDD wins", run_sequence(line3, 7, &d), DRINK_RESULT_WIN);
    }

    /* Exhaustive proof over all 128 seven-action sequences: every play ends
     * within 7 rounds, and exactly three of them are wins (the ones above).
     * This locks the rule constants: if tuning ever makes the game
     * unwinnable or trivially winnable, this test catches it. */
    for (mask = 0; mask < 128; ++mask) {
        for (i = 0; i < 7; ++i) {
            actions[i] = (unsigned char)(((mask >> i) & 1u) ? 1 : 2);
        }
        if (run_sequence(actions, 7, &d) == DRINK_RESULT_WIN) ++wins;
        if (d.result == DRINK_RESULT_ACTIVE) {
            printf("FAIL sequence still active after 7 actions\n");
            ++failures;
        }
    }
    check_eq("exactly three winning sequences", wins, 3);

    /* Failure shapes. */
    {
        static const unsigned char greedy[] = {1, 1, 1};
        static const unsigned char hesitant[] = {2, 2, 2};
        static const unsigned char sloppy[] = {1, 1, 2, 1, 1, 1};
        check_eq("greedy triple drink spills", run_sequence(greedy, 3, &d),
                 DRINK_RESULT_SPILL);
        check_eq("hesitant triple settle snatched", run_sequence(hesitant, 3, &d),
                 DRINK_RESULT_SNATCH);
        /* Five drinks with only one settle: the 5th gulp empties the glass
         * but sloshes the rim first -- a spill, not a win. */
        check_eq("sloppy final-gulp spill beats win", run_sequence(sloppy, 6, &d),
                 DRINK_RESULT_SPILL);
    }

    /* Settle edge cases. */
    drink_init(&d);
    (void)drink_settle(&d);
    check_eq("settle from zero clamps foam", d.foam, 0);
    check_eq("settle warms once", d.warmth, 1);

    /* Actions after the game ends are no-ops; the canonical line's ending
     * state is worth pinning (foam one from the rim, warmth at the budget). */
    {
        static const unsigned char post[] = {1, 1, 2, 1, 1, 2, 1, 2, 2};
        check_eq("post-win actions are no-ops", run_sequence(post, 9, &d),
                 DRINK_RESULT_WIN);
        check_eq("winning line ends at foam 4", d.foam, 4);
        check_eq("winning line ends at warmth 2", d.warmth, 2);
        check_eq("winning line ends with liquid 0", d.liquid, 0);
    }

    /* The canonical line never touches the rim on the way to the win. */
    {
        static const unsigned char line1[] = {1, 1, 2, 1, 1, 2, 1};
        drink_init(&d);
        for (i = 0; i < 7; ++i) {
            if (line1[i] == 1) {
                (void)drink_take(&d);
            } else {
                (void)drink_settle(&d);
            }
            if (d.foam >= DRINK_SPILL_FOAM) {
                printf("FAIL canonical line touched the rim at action %d\n", i);
                ++failures;
            }
        }
    }

    if (failures) {
        printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("All checks passed\n");
    return 0;
}
