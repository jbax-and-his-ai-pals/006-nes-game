#ifndef V2_CASINO_H
#define V2_CASINO_H

#include "rng.h"

#define CASINO_REEL_COUNT 3
#define CASINO_SYMBOL_COUNT 3
#define CASINO_MAX_CREDITS 255
#define CASINO_HAND_SIZE 5

/* Indices into a reel/casino_slot_symbols[] slot -- named so
 * casino_is_winner()/casino_payout()'s wild handling (BAR stands in for
 * whichever other symbol the remaining reels show) doesn't read as a
 * magic "1" scattered through the logic. */
enum {
    CASINO_SYMBOL_SEVEN,
    CASINO_SYMBOL_BAR,
    CASINO_SYMBOL_CHERRY
};

enum {
    CASINO_GAME_READY,
    CASINO_GAME_ACTIVE,
    CASINO_GAME_RESOLVED
};

enum {
    CASINO_ROULETTE_ZERO = 0,
    CASINO_ROULETTE_DOUBLE_ZERO = 37,
    CASINO_ROULETTE_POCKET_COUNT = 38,
    CASINO_ROULETTE_RED = 38,
    CASINO_ROULETTE_BLACK = 39,
    CASINO_ROULETTE_BET_COUNT = 40
};

/* Ranked worst to best (standard video poker hand order); casino_poker_
 * result() returns the best one the current hand qualifies for, and
 * casino_poker_payout()'s multiplier table (see v2_casino.c) follows this
 * same order. This is a "tens or better" table: POKER_RESULT_PAIR only
 * ever comes back for a pair of 10s, jacks, queens, kings, or aces --
 * anything lower is POKER_RESULT_NONE, same as real tens-or-better video
 * poker paying nothing for a low pair. */
enum {
    POKER_RESULT_NONE,
    POKER_RESULT_PAIR,
    POKER_RESULT_TWO_PAIR,
    POKER_RESULT_TRIPLE,
    POKER_RESULT_STRAIGHT,
    POKER_RESULT_FLUSH,
    POKER_RESULT_FULL_HOUSE,
    POKER_RESULT_QUAD,
    POKER_RESULT_STRAIGHT_FLUSH,
    POKER_RESULT_ROYAL_FLUSH
};

typedef struct {
    unsigned char credits;
    unsigned char bet;
    unsigned char reel[CASINO_REEL_COUNT];
    unsigned char blackjack_player[CASINO_HAND_SIZE];
    unsigned char blackjack_dealer[CASINO_HAND_SIZE];
    unsigned char blackjack_player_count;
    unsigned char blackjack_dealer_count;
    unsigned char blackjack_state;
    unsigned char blackjack_payout;
    unsigned char poker_rank[CASINO_HAND_SIZE];
    /* 0-3, arbitrarily spade/heart/club/diamond (see v2_scene_casino.c's
     * suit_to_tile()) -- only ever compared to each other for a flush,
     * never against a fixed meaning, so the mapping is just convention. */
    unsigned char poker_suit[CASINO_HAND_SIZE];
    unsigned char poker_held;
    unsigned char poker_state;
    unsigned char poker_payout;
    unsigned char roulette_pick;
    unsigned char roulette_result;
    unsigned char roulette_payout;
    unsigned char roulette_state;
    RngState rng;
} CasinoState;

void casino_reset(CasinoState *casino);
unsigned char casino_is_winner(const CasinoState *casino);
unsigned char casino_payout(const CasinoState *casino);
/* Spins only when the player can cover the selected bet. Returns the payout. */
unsigned char casino_spin(CasinoState *casino);

unsigned char casino_blackjack_player_total(const CasinoState *casino);
unsigned char casino_blackjack_dealer_total(const CasinoState *casino);
unsigned char casino_blackjack_deal(CasinoState *casino);
unsigned char casino_blackjack_hit(CasinoState *casino);
unsigned char casino_blackjack_stand(CasinoState *casino);

unsigned char casino_poker_deal(CasinoState *casino);
unsigned char casino_poker_draw(CasinoState *casino);
unsigned char casino_poker_payout(const CasinoState *casino);
/* The best result the current hand qualifies for -- a POKER_RESULT_*
 * value, see its enum for the ranking. casino_poker_payout()'s multiplier
 * follows this same result, just expressed as a number instead of a name;
 * the two never disagree about which hand won. */
unsigned char casino_poker_result(const CasinoState *casino);

/* Spins the American 0/00/1-36 wheel. A straight-up hit returns 36 times the
 * wager (original stake plus 35:1 profit); red/black wins return 2 times the
 * wager (original stake plus 1:1 profit). */
unsigned char casino_roulette_spin(CasinoState *casino, unsigned char pocket);
extern const unsigned char casino_roulette_wheel[CASINO_ROULETTE_POCKET_COUNT];

#endif
