#include <stdio.h>

#include "../src/v2_casino.h"

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

int main(void)
{
    CasinoState casino;
    unsigned int seed;
    unsigned char payout = 0;

    casino_reset(&casino);
    check_eq("reset credits", casino.credits, 10);
    check_eq("reset bet", casino.bet, 1);
    check_eq("default reels start as a jackpot", casino_is_winner(&casino), 1);
    check_eq("default jackpot pays five bets", casino_payout(&casino), 5);

    casino.reel[0] = 0;
    casino.reel[1] = 1;
    casino.reel[2] = 0;
    check_eq("mixed reels lose", casino_is_winner(&casino), 0);
    check_eq("mixed reels pay nothing", casino_payout(&casino), 0);

    casino.reel[0] = 2;
    casino.reel[1] = 2;
    casino.reel[2] = 2;
    casino.bet = 5;
    check_eq("any three matching symbols win", casino_is_winner(&casino), 1);
    check_eq("max bet jackpot payout", casino_payout(&casino), 25);

    casino.credits = 3;
    casino.bet = 5;
    check_eq("cannot spin without credits", casino_spin(&casino), 0);
    check_eq("failed spin preserves credits", casino.credits, 3);

    /* Find a deterministic winning LFSR seed, then prove a $255 balance
     * saturates rather than wrapping after its five-times-bet payout. */
    for (seed = 1; seed < 256 && !payout; ++seed) {
        casino_reset(&casino);
        casino.credits = 255;
        casino.bet = 5;
        casino.rng.state = (unsigned char)seed;
        payout = casino_spin(&casino);
    }
    check_eq("a deterministic seed reaches a winning spin", payout != 0, 1);
    check_eq("winning at maximum credits does not wrap", casino.credits, 255);

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
