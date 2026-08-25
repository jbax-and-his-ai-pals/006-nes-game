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
    /* Reel index 0 is "7" (see casino_slot_symbols[] in
     * scene_casino_data.c), the highest-paying symbol at 10x. */
    check_eq("default jackpot is three 7s, paying ten bets", casino_payout(&casino), 10);

    casino.reel[0] = 0;
    casino.reel[1] = 2;
    casino.reel[2] = 0;
    check_eq("mixed reels lose", casino_is_winner(&casino), 0);
    check_eq("mixed reels pay nothing", casino_payout(&casino), 0);

    /* BAR (index 1) is wild -- it stands in for whichever other symbol
     * the remaining reels show. */
    casino.reel[0] = 0;
    casino.reel[1] = 1;
    casino.reel[2] = 0;
    check_eq("a wild completes a match", casino_is_winner(&casino), 1);
    check_eq("a wild-completed win pays the matched symbol's rate", casino_payout(&casino), 10);

    casino.reel[0] = 0;
    casino.reel[1] = 1;
    casino.reel[2] = 2;
    check_eq("a wild can't reconcile two different real symbols", casino_is_winner(&casino), 0);
    check_eq("two different real symbols pay nothing even with a wild", casino_payout(&casino), 0);

    casino.reel[0] = 1;
    casino.reel[1] = 1;
    casino.reel[2] = 1;
    check_eq("three wilds still win", casino_is_winner(&casino), 1);
    check_eq("three wilds pay BAR's own rate", casino_payout(&casino), 7);

    casino.reel[0] = 2;
    casino.reel[1] = 2;
    casino.reel[2] = 2;
    casino.bet = 5;
    check_eq("any three matching symbols win", casino_is_winner(&casino), 1);
    check_eq("cherry pays five times at max bet", casino_payout(&casino), 25);

    /* Middle symbol ("BAR", reel index 1) at a smaller bet, proving the
     * payout genuinely varies by symbol and isn't a flat rate that just
     * happens to match cherry's. */
    casino.reel[0] = 1;
    casino.reel[1] = 1;
    casino.reel[2] = 1;
    casino.bet = 2;
    check_eq("bar pays seven times the bet", casino_payout(&casino), 14);

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

    casino_reset(&casino);
    casino.credits = 20;
    casino.bet = 2;
    check_eq("blackjack deal spends bet", casino_blackjack_deal(&casino), 1);
    check_eq("blackjack deal debits credits", casino.credits, 18);
    check_eq("blackjack starts active", casino.blackjack_state, CASINO_GAME_ACTIVE);
    casino_blackjack_stand(&casino);
    check_eq("blackjack stand resolves hand", casino.blackjack_state, CASINO_GAME_RESOLVED);

    casino_reset(&casino);
    casino.credits = 10;
    casino.bet = 2;
    payout = 0;
    /* Find a deterministic straight-up hit without coupling this test to a
     * particular internal step of the shared LFSR sequence. */
    for (seed = 1; seed < 256 && !payout; ++seed) {
        casino_reset(&casino);
        casino.credits = 10;
        casino.bet = 2;
        casino.rng.state = (unsigned char)seed;
        payout = casino_roulette_spin(&casino, 1);
    }
    check_eq("roulette accepts a covered straight-up bet", payout, 72);
    check_eq("roulette winning bet returns wager plus 35:1 profit", casino.credits, 80);
    check_eq("roulette records its resolved result", casino.roulette_state, CASINO_GAME_RESOLVED);

    casino_reset(&casino);
    casino.credits = 10;
    casino.bet = 2;
    payout = 0;
    for (seed = 1; seed < 256 && !payout; ++seed) {
        casino_reset(&casino);
        casino.credits = 10;
        casino.bet = 2;
        casino.rng.state = (unsigned char)seed;
        payout = casino_roulette_spin(&casino, CASINO_ROULETTE_RED);
    }
    check_eq("roulette accepts a red outside bet", payout, 4);
    check_eq("roulette red pays even money", casino.credits, 12);

    casino_reset(&casino);
    casino.credits = 1;
    casino.bet = 2;
    check_eq("roulette rejects an uncovered bet", casino_roulette_spin(&casino, 1), 0);
    check_eq("failed roulette bet preserves credits", casino.credits, 1);

    /* Player natural: ace + king dealt straight up. The dealer's own hand
     * starts at 11, well under 17, so casino_blackjack_stand()'s own draw
     * loop is guaranteed to add at least one more dealer card -- which
     * rules out a dealer natural (that needs exactly two cards) by
     * construction, regardless of which card the RNG actually deals. */
    casino_reset(&casino);
    casino.bet = 2;
    casino.blackjack_state = CASINO_GAME_ACTIVE;
    casino.blackjack_player_count = 2;
    casino.blackjack_player[0] = 1;
    casino.blackjack_player[1] = 13;
    casino.blackjack_dealer_count = 2;
    casino.blackjack_dealer[0] = 5;
    casino.blackjack_dealer[1] = 6;
    casino_blackjack_stand(&casino);
    check_eq("natural blackjack resolves", casino.blackjack_state, CASINO_GAME_RESOLVED);
    check_eq("natural blackjack pays 3:2", casino.blackjack_payout, 5);

    /* Both dealt a natural: the dealer's own 21 on two cards means the
     * stand-loop's own "under 17" condition is already false, so no third
     * card gets added and the dealer natural survives intact. */
    casino_reset(&casino);
    casino.bet = 3;
    casino.blackjack_state = CASINO_GAME_ACTIVE;
    casino.blackjack_player_count = 2;
    casino.blackjack_player[0] = 1;
    casino.blackjack_player[1] = 13;
    casino.blackjack_dealer_count = 2;
    casino.blackjack_dealer[0] = 1;
    casino.blackjack_dealer[1] = 12;
    casino_blackjack_stand(&casino);
    check_eq("two naturals push", casino.blackjack_payout, 3);

    /* Player reaches 21 by hitting (three cards, so never natural) against
     * a genuine dealer natural -- the dealer natural wins outright, it
     * does not push against a same-value 21. */
    casino_reset(&casino);
    casino.bet = 2;
    casino.blackjack_state = CASINO_GAME_ACTIVE;
    casino.blackjack_player_count = 3;
    casino.blackjack_player[0] = 7;
    casino.blackjack_player[1] = 7;
    casino.blackjack_player[2] = 7;
    casino.blackjack_dealer_count = 2;
    casino.blackjack_dealer[0] = 1;
    casino.blackjack_dealer[1] = 10;
    casino_blackjack_stand(&casino);
    check_eq("dealer natural beats a non-natural 21", casino.blackjack_payout, 0);

    casino_reset(&casino);
    casino.credits = 20;
    casino.bet = 2;
    check_eq("poker deal spends bet", casino_poker_deal(&casino), 1);
    check_eq("poker starts active", casino.poker_state, CASINO_GAME_ACTIVE);
    /* A pair of 10s -- the "tens or better" cutoff's own boundary, so this
     * also proves the cutoff is inclusive, not just "above 10." */
    casino.poker_rank[0] = 10;
    casino.poker_rank[1] = 10;
    casino.poker_rank[2] = 3;
    casino.poker_rank[3] = 4;
    casino.poker_rank[4] = 5;
    /* Deliberately mixed, not left as whatever casino_poker_deal() rolled --
     * a same-suit deal here would resolve as a flush instead of a pair and
     * make this assertion depend on the RNG sequence, not the scenario. */
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    casino.poker_held = 0x1F;
    check_eq("pair of 10s qualifies", casino_poker_result(&casino), POKER_RESULT_PAIR);
    check_eq("qualifying pair has a payout", casino_poker_draw(&casino), 4);
    check_eq("poker resolves after draw", casino.poker_state, CASINO_GAME_RESOLVED);

    /* A pair of 9s -- one rank below the cutoff -- pays nothing, same as
     * no pair at all. This is the "tens or better" rule's whole point. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 9;
    casino.poker_rank[1] = 9;
    casino.poker_rank[2] = 3;
    casino.poker_rank[3] = 4;
    casino.poker_rank[4] = 5;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    casino.poker_held = 0x1F;
    check_eq("a pair below the cutoff does not qualify", casino_poker_result(&casino), POKER_RESULT_NONE);
    check_eq("a low pair pays nothing", casino_poker_draw(&casino), 0);

    /* A pair of aces -- aces count as high for the cutoff, not as 1. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 1;
    casino.poker_rank[1] = 1;
    casino.poker_rank[2] = 3;
    casino.poker_rank[3] = 4;
    casino.poker_rank[4] = 6;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    check_eq("a pair of aces qualifies", casino_poker_result(&casino), POKER_RESULT_PAIR);

    /* Standard mid-range straight, distinct ranks, mixed suits so it can't
     * also read as a flush. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 5;
    casino.poker_rank[1] = 6;
    casino.poker_rank[2] = 7;
    casino.poker_rank[3] = 8;
    casino.poker_rank[4] = 9;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    casino.poker_held = 0x1F;
    check_eq("five consecutive ranks are a straight", casino_poker_result(&casino), POKER_RESULT_STRAIGHT);
    check_eq("straight pays five times the bet", casino_poker_draw(&casino), 10);

    /* Ace-low straight (the "wheel": A-2-3-4-5). */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 1;
    casino.poker_rank[1] = 2;
    casino.poker_rank[2] = 3;
    casino.poker_rank[3] = 4;
    casino.poker_rank[4] = 5;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    check_eq("ace-low wheel is a straight", casino_poker_result(&casino), POKER_RESULT_STRAIGHT);

    /* Ace-high straight (10-J-Q-K-A, "broadway"), mixed suits -- proves the
     * ace-high special case works independently of the flush check. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 1;
    casino.poker_rank[1] = 10;
    casino.poker_rank[2] = 11;
    casino.poker_rank[3] = 12;
    casino.poker_rank[4] = 13;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    check_eq("ace-high broadway is a straight", casino_poker_result(&casino), POKER_RESULT_STRAIGHT);

    /* Full house: three of one rank, two of another. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 8;
    casino.poker_rank[1] = 8;
    casino.poker_rank[2] = 8;
    casino.poker_rank[3] = 4;
    casino.poker_rank[4] = 4;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    casino.poker_held = 0x1F;
    check_eq("triple plus a pair is a full house", casino_poker_result(&casino), POKER_RESULT_FULL_HOUSE);
    check_eq("full house pays nine times the bet", casino_poker_draw(&casino), 18);

    /* Four of a kind. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 9;
    casino.poker_rank[1] = 9;
    casino.poker_rank[2] = 9;
    casino.poker_rank[3] = 9;
    casino.poker_rank[4] = 2;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    casino.poker_held = 0x1F;
    check_eq("four matching ranks is quad", casino_poker_result(&casino), POKER_RESULT_QUAD);
    check_eq("quad pays fifteen times the bet", casino_poker_draw(&casino), 30);

    /* Straight flush: consecutive ranks, all one suit. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 5;
    casino.poker_rank[1] = 6;
    casino.poker_rank[2] = 7;
    casino.poker_rank[3] = 8;
    casino.poker_rank[4] = 9;
    casino.poker_suit[0] = 2;
    casino.poker_suit[1] = 2;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 2;
    casino.poker_suit[4] = 2;
    casino.poker_held = 0x1F;
    check_eq("consecutive ranks all one suit is a straight flush",
             casino_poker_result(&casino), POKER_RESULT_STRAIGHT_FLUSH);
    check_eq("straight flush pays thirty times the bet", casino_poker_draw(&casino), 60);

    /* Royal flush: the ace-high straight flush specifically. */
    casino_reset(&casino);
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 1;
    casino.poker_rank[1] = 10;
    casino.poker_rank[2] = 11;
    casino.poker_rank[3] = 12;
    casino.poker_rank[4] = 13;
    casino.poker_suit[0] = 3;
    casino.poker_suit[1] = 3;
    casino.poker_suit[2] = 3;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 3;
    casino.poker_held = 0x1F;
    check_eq("ace-high straight flush is specifically a royal flush",
             casino_poker_result(&casino), POKER_RESULT_ROYAL_FLUSH);
    check_eq("royal flush pays a hundred times the bet", casino_poker_draw(&casino), 200);

    casino_reset(&casino);
    casino.credits = 20;
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 2;
    casino.poker_rank[1] = 5;
    casino.poker_rank[2] = 9;
    casino.poker_rank[3] = 11;
    casino.poker_rank[4] = 13;
    casino.poker_suit[0] = 1;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 1;
    casino.poker_suit[3] = 1;
    casino.poker_suit[4] = 1;
    casino.poker_held = 0x1F;
    check_eq("flush result with no matching ranks", casino_poker_result(&casino), POKER_RESULT_FLUSH);
    check_eq("flush pays six times the bet", casino_poker_draw(&casino), 12);

    casino_reset(&casino);
    casino.credits = 20;
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 6;
    casino.poker_rank[1] = 6;
    casino.poker_rank[2] = 6;
    casino.poker_rank[3] = 2;
    casino.poker_rank[4] = 9;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    check_eq("triple beats a mixed-suit hand", casino_poker_result(&casino), POKER_RESULT_TRIPLE);

    casino_reset(&casino);
    casino.credits = 20;
    casino.bet = 2;
    casino_poker_deal(&casino);
    casino.poker_rank[0] = 1;
    casino.poker_rank[1] = 2;
    casino.poker_rank[2] = 3;
    casino.poker_rank[3] = 4;
    casino.poker_rank[4] = 6;
    casino.poker_suit[0] = 0;
    casino.poker_suit[1] = 1;
    casino.poker_suit[2] = 2;
    casino.poker_suit[3] = 3;
    casino.poker_suit[4] = 0;
    casino.poker_held = 0x1F;
    check_eq("no pair, no flush is a losing hand", casino_poker_result(&casino), POKER_RESULT_NONE);
    check_eq("losing hand pays nothing", casino_poker_draw(&casino), 0);

    /* Poker deals from a single 52-card deck -- across a wide spread of
     * seeds, a freshly dealt hand (and a hand redrawn after holding an
     * arbitrary subset) should never contain the same card twice. */
    {
        unsigned char dup_after_deal = 0, dup_after_draw = 0;
        unsigned char i, j;

        for (seed = 1; seed < 256; ++seed) {
            casino_reset(&casino);
            casino.credits = 20;
            casino.bet = 1;
            casino.rng.state = (unsigned char)seed;
            casino_poker_deal(&casino);
            for (i = 0; i < CASINO_HAND_SIZE && !dup_after_deal; ++i) {
                for (j = (unsigned char)(i + 1); j < CASINO_HAND_SIZE; ++j) {
                    if (casino.poker_rank[i] == casino.poker_rank[j] &&
                        casino.poker_suit[i] == casino.poker_suit[j]) {
                        dup_after_deal = 1;
                        break;
                    }
                }
            }
            /* Hold every other card (a fixed, arbitrary pattern) so the
             * draw actually replaces some cards, not zero. */
            casino.poker_held = 0x15;
            casino_poker_draw(&casino);
            for (i = 0; i < CASINO_HAND_SIZE && !dup_after_draw; ++i) {
                for (j = (unsigned char)(i + 1); j < CASINO_HAND_SIZE; ++j) {
                    if (casino.poker_rank[i] == casino.poker_rank[j] &&
                        casino.poker_suit[i] == casino.poker_suit[j]) {
                        dup_after_draw = 1;
                        break;
                    }
                }
            }
        }
        check_eq("no duplicate card across any dealt hand", dup_after_deal, 0);
        check_eq("no duplicate card across any redrawn hand", dup_after_draw, 0);
    }

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
