#include "v2_casino.h"

static void casino_add_credits(CasinoState *casino, unsigned char amount)
{
    unsigned int credits = (unsigned int)casino->credits + amount;
    casino->credits = credits > CASINO_MAX_CREDITS ? CASINO_MAX_CREDITS :
                       (unsigned char)credits;
}

/* Standard American-wheel order. Pocket 37 encodes 00 because the game
 * stores every pocket in one byte while 0 remains the ordinary single zero. */
const unsigned char casino_roulette_wheel[CASINO_ROULETTE_POCKET_COUNT] = {
    0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1,
    37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2
};

static unsigned char casino_card(CasinoState *casino)
{
    return (unsigned char)((rng_next(&casino->rng) % 13) + 1);
}

/* Draws one (rank, suit) pair for poker that doesn't match any of the
 * first existing_count cards already in the hand -- poker deals from a
 * single 52-card deck, so the same card can't turn up twice in one hand,
 * unlike blackjack's casino_card() above which draws with replacement
 * (blackjack never tracks suit, so a repeated rank there is invisible
 * anyway). Card identity is folded into one 0-51 value (rank-1)*4+suit
 * so "already in the hand" is a single comparison, not two.
 *
 * Rejects and rerolls raw >= 52 instead of using rng_next() % 52 -- 52
 * isn't a power of two, so a real modulo would pull in cc65's runtime
 * divide helper, which claims a zero-page temp that collides with the
 * one the fixed-engine API trampoline already aliases into this bank
 * (see casino_blackjack_resolve()'s natural_payout comment for the same
 * issue). Masking with 63 and rejecting the 12 out-of-range values above
 * 51 needs only a compare and an AND. */
static void casino_deal_unique_card(CasinoState *casino, unsigned char *out_rank,
                                     unsigned char *out_suit,
                                     const unsigned char *hand_rank,
                                     const unsigned char *hand_suit,
                                     unsigned char existing_count)
{
    unsigned char raw, rank, suit, i, dup;

    do {
        do {
            raw = (unsigned char)(rng_next(&casino->rng) & 63);
        } while (raw >= 52);
        rank = (unsigned char)((raw >> 2) + 1);
        suit = (unsigned char)(raw & 3);
        dup = 0;
        for (i = 0; i < existing_count; ++i) {
            if (hand_rank[i] == rank && hand_suit[i] == suit) {
                dup = 1;
                break;
            }
        }
    } while (dup);
    *out_rank = rank;
    *out_suit = suit;
}

static unsigned char casino_hand_total(const unsigned char *cards, unsigned char count)
{
    unsigned char i, total = 0, aces = 0, value;

    for (i = 0; i < count; ++i) {
        value = cards[i];
        if (value == 1) {
            ++aces;
            total = (unsigned char)(total + 11);
        } else {
            total = (unsigned char)(total + (value > 10 ? 10 : value));
        }
    }
    while (total > 21 && aces) {
        total = (unsigned char)(total - 10);
        --aces;
    }
    return total;
}

static void casino_blackjack_resolve(CasinoState *casino)
{
    unsigned char player = casino_hand_total(casino->blackjack_player,
                                              casino->blackjack_player_count);
    unsigned char dealer = casino_hand_total(casino->blackjack_dealer,
                                              casino->blackjack_dealer_count);
    /* A "natural" is 21 on the first two cards -- the only way to reach
     * 21 with exactly two cards is an ace plus a ten-value card, so the
     * count check alone is enough; no need to inspect which cards they
     * actually are. casino_blackjack_deal() already auto-stands on a
     * player natural (see its own comment), so blackjack_player_count is
     * still 2 here whenever this fires, never inflated by a hit. */
    unsigned char player_natural = (unsigned char)(casino->blackjack_player_count == 2 && player == 21);
    unsigned char dealer_natural = (unsigned char)(casino->blackjack_dealer_count == 2 && dealer == 21);

    casino->blackjack_payout = 0;
    if (player_natural && dealer_natural) {
        casino->blackjack_payout = casino->bet;   /* both natural: push */
    } else if (player_natural) {
        /* 3:2 -- bet back plus 1.5x profit, i.e. bet*5/2. bet is always
         * 1-10 (the UI clamps it), so a tiny table sidesteps an actual
         * division: cc65's runtime divide routine pulls in a zero-page
         * temp that collides with the one the fixed-engine API already
         * aliases into this bank (see gen_fixed_api.py's SHARED_ZP) --
         * multiplication doesn't need that routine, division does. */
        static const unsigned char natural_payout[11] = {0, 2, 5, 7, 10, 12, 15, 17, 20, 22, 25};
        casino->blackjack_payout = natural_payout[casino->bet];
    } else if (dealer_natural) {
        /* A dealer natural beats any player total, even a matching 21
         * reached by hitting -- stays 0, but written out rather than
         * just falling through, since "why doesn't a 21 push here" is
         * exactly the kind of rule a reader can't be expected to infer
         * from an absence of code. */
    } else if (player <= 21 && (dealer > 21 || player > dealer)) {
        casino->blackjack_payout = (unsigned char)(casino->bet * 2);
    } else if (player <= 21 && player == dealer) {
        casino->blackjack_payout = casino->bet;
    }
    casino_add_credits(casino, casino->blackjack_payout);
    casino->blackjack_state = CASINO_GAME_RESOLVED;
}

void casino_reset(CasinoState *casino)
{
    casino->credits = 10;
    casino->bet = 1;
    casino->reel[0] = 0;
    casino->reel[1] = 0;
    casino->reel[2] = 0;
    casino->blackjack_state = CASINO_GAME_READY;
    casino->poker_state = CASINO_GAME_READY;
    casino->roulette_pick = 1;
    casino->roulette_result = CASINO_ROULETTE_ZERO;
    casino->roulette_payout = 0;
    casino->roulette_state = CASINO_GAME_READY;
    rng_seed(&casino->rng, 0x2A);
}

/* BAR is wild -- it stands in for whichever other symbol the remaining
 * reels show, so e.g. 7-7-BAR wins as three 7s instead of only ever
 * winning by matching itself. Tracks the first non-wild symbol seen and
 * rejects the spin the moment a second, different one turns up; three
 * BARs (no non-wild symbol at all) is still a win in its own right. */
unsigned char casino_is_winner(const CasinoState *casino)
{
    unsigned char i;
    unsigned char real = CASINO_SYMBOL_COUNT; /* no non-wild symbol pinned down yet */

    for (i = 0; i < CASINO_REEL_COUNT; ++i) {
        if (casino->reel[i] == CASINO_SYMBOL_BAR) continue;
        if (real == CASINO_SYMBOL_COUNT) real = casino->reel[i];
        else if (real != casino->reel[i]) return 0;
    }
    return 1;
}

unsigned char casino_payout(const CasinoState *casino)
{
    /* Indexed by reel value, matching casino_slot_symbols[] in
     * scene_casino_data.c ("7", "BAR", "CHERRY" in that order) -- "7"
     * pays the most, standard slot-machine convention, not a flat rate
     * regardless of which symbol actually lined up. A wild-completed win
     * (see casino_is_winner()) pays at the rate of whichever non-wild
     * symbol it matched; three BARs pay BAR's own rate. */
    static const unsigned char multiplier[CASINO_SYMBOL_COUNT] = {10, 7, 5};
    unsigned int raw;
    unsigned char i, symbol = CASINO_SYMBOL_BAR;

    if (!casino_is_winner(casino)) return 0;
    for (i = 0; i < CASINO_REEL_COUNT; ++i) {
        if (casino->reel[i] != CASINO_SYMBOL_BAR) {
            symbol = casino->reel[i];
            break;
        }
    }
    raw = (unsigned int)multiplier[symbol] * casino->bet;
    return raw > 255 ? 255 : (unsigned char)raw;
}

unsigned char casino_spin(CasinoState *casino)
{
    unsigned char i;
    unsigned char payout;
    unsigned int credits;

    if (casino->credits < casino->bet) return 0;

    casino->credits = (unsigned char)(casino->credits - casino->bet);
    for (i = 0; i < CASINO_REEL_COUNT; ++i) {
        casino->reel[i] = (unsigned char)(rng_next(&casino->rng) % CASINO_SYMBOL_COUNT);
    }

    payout = casino_payout(casino);
    credits = (unsigned int)casino->credits + payout;
    casino->credits = credits > CASINO_MAX_CREDITS ? CASINO_MAX_CREDITS : (unsigned char)credits;
    return payout;
}

unsigned char casino_blackjack_player_total(const CasinoState *casino)
{
    return casino_hand_total(casino->blackjack_player, casino->blackjack_player_count);
}

unsigned char casino_blackjack_dealer_total(const CasinoState *casino)
{
    return casino_hand_total(casino->blackjack_dealer, casino->blackjack_dealer_count);
}

unsigned char casino_blackjack_deal(CasinoState *casino)
{
    if (casino->credits < casino->bet) return 0;
    casino->credits = (unsigned char)(casino->credits - casino->bet);
    casino->blackjack_player_count = 2;
    casino->blackjack_dealer_count = 2;
    casino->blackjack_player[0] = casino_card(casino);
    casino->blackjack_dealer[0] = casino_card(casino);
    casino->blackjack_player[1] = casino_card(casino);
    casino->blackjack_dealer[1] = casino_card(casino);
    casino->blackjack_payout = 0;
    casino->blackjack_state = CASINO_GAME_ACTIVE;
    if (casino_blackjack_player_total(casino) == 21) casino_blackjack_stand(casino);
    return 1;
}

unsigned char casino_blackjack_hit(CasinoState *casino)
{
    if (casino->blackjack_state != CASINO_GAME_ACTIVE ||
        casino->blackjack_player_count >= CASINO_HAND_SIZE) return 0;
    casino->blackjack_player[casino->blackjack_player_count++] = casino_card(casino);
    if (casino_blackjack_player_total(casino) > 21) casino_blackjack_resolve(casino);
    return 1;
}

unsigned char casino_blackjack_stand(CasinoState *casino)
{
    if (casino->blackjack_state != CASINO_GAME_ACTIVE) return 0;
    while (casino_blackjack_dealer_total(casino) < 17 &&
           casino->blackjack_dealer_count < CASINO_HAND_SIZE) {
        casino->blackjack_dealer[casino->blackjack_dealer_count++] = casino_card(casino);
    }
    casino_blackjack_resolve(casino);
    return 1;
}

/* counts[] is 14 entries (index 0 unused, 1-13 = ace..king) so callers can
 * index it directly by a card's own rank value with no offset math. */
static void casino_poker_rank_counts(const CasinoState *casino, unsigned char *counts)
{
    unsigned char i;

    for (i = 0; i <= 13; ++i) counts[i] = 0;
    for (i = 0; i < CASINO_HAND_SIZE; ++i) ++counts[casino->poker_rank[i]];
}

static unsigned char casino_poker_is_flush(const CasinoState *casino)
{
    unsigned char i;

    for (i = 1; i < CASINO_HAND_SIZE; ++i) {
        if (casino->poker_suit[i] != casino->poker_suit[0]) return 0;
    }
    return 1;
}

/* Five distinct, consecutive ranks. Ace (stored as 1) can sit at either
 * end -- low for A-2-3-4-5, high for 10-J-Q-K-A -- so those are each
 * their own check; every other straight is exactly "highest rank minus
 * lowest rank is 4" once duplicates are ruled out. */
static unsigned char casino_poker_is_straight(const unsigned char *counts)
{
    unsigned char i, low = 0, high = 0, seen = 0;

    for (i = 1; i <= 13; ++i) {
        if (counts[i] > 1) return 0;
        if (counts[i]) {
            if (!seen) low = i;
            high = i;
            seen = 1;
        }
    }
    if ((unsigned char)(high - low) == 4) return 1;
    return (unsigned char)(counts[1] && counts[10] && counts[11] && counts[12] && counts[13]);
}

/* Full standard video-poker hand ranking. A "tens or better" pay table
 * (see POKER_RESULT_PAIR's own comment) is the only house-rule choice
 * here; everything else is just how poker hands actually rank. */
unsigned char casino_poker_result(const CasinoState *casino)
{
    unsigned char counts[14];
    unsigned char i;
    unsigned char pairs = 0, qualifying_pair = 0, has_triple = 0, has_quad = 0;
    unsigned char flush, straight;

    casino_poker_rank_counts(casino, counts);
    for (i = 1; i <= 13; ++i) {
        if (counts[i] == 4) has_quad = 1;
        else if (counts[i] == 3) has_triple = 1;
        else if (counts[i] == 2) {
            ++pairs;
            if (i >= 10 || i == 1) qualifying_pair = 1;
        }
    }

    flush = casino_poker_is_flush(casino);
    straight = casino_poker_is_straight(counts);

    if (flush && straight) {
        /* Specifically the ace-high run -- every other straight flush is
         * just a straight flush. */
        return (unsigned char)((counts[1] && counts[13]) ?
                                POKER_RESULT_ROYAL_FLUSH : POKER_RESULT_STRAIGHT_FLUSH);
    }
    if (has_quad) return POKER_RESULT_QUAD;
    if (has_triple && pairs) return POKER_RESULT_FULL_HOUSE;
    if (flush) return POKER_RESULT_FLUSH;
    if (straight) return POKER_RESULT_STRAIGHT;
    if (has_triple) return POKER_RESULT_TRIPLE;
    if (pairs >= 2) return POKER_RESULT_TWO_PAIR;
    if (pairs == 1 && qualifying_pair) return POKER_RESULT_PAIR;
    return POKER_RESULT_NONE;
}

unsigned char casino_poker_payout(const CasinoState *casino)
{
    static const unsigned char multiplier[10] = {
        0,   /* NONE */
        2,   /* PAIR (10s or better) */
        3,   /* TWO_PAIR */
        4,   /* TRIPLE */
        5,   /* STRAIGHT */
        6,   /* FLUSH */
        9,   /* FULL_HOUSE */
        15,  /* QUAD */
        30,  /* STRAIGHT_FLUSH */
        100  /* ROYAL_FLUSH */
    };
    /* bet (max 10) * multiplier (max 100) can reach 1000, which doesn't fit
     * an unsigned char -- computed as a wider int and clamped, the same
     * saturate-don't-wrap rule casino_add_credits() already applies to
     * the running total, rather than silently wrapping to a small,
     * wrong-looking payout on a big win. */
    unsigned int raw = (unsigned int)multiplier[casino_poker_result(casino)] * casino->bet;
    return raw > 255 ? 255 : (unsigned char)raw;
}

unsigned char casino_poker_deal(CasinoState *casino)
{
    unsigned char i;
    if (casino->credits < casino->bet) return 0;
    casino->credits = (unsigned char)(casino->credits - casino->bet);
    for (i = 0; i < CASINO_HAND_SIZE; ++i) {
        casino_deal_unique_card(casino, &casino->poker_rank[i], &casino->poker_suit[i],
                                 casino->poker_rank, casino->poker_suit, i);
    }
    casino->poker_held = 0;
    casino->poker_payout = 0;
    casino->poker_state = CASINO_GAME_ACTIVE;
    return 1;
}

unsigned char casino_poker_draw(CasinoState *casino)
{
    unsigned char i, count = 0;
    unsigned char hand_rank[CASINO_HAND_SIZE], hand_suit[CASINO_HAND_SIZE];

    if (casino->poker_state != CASINO_GAME_ACTIVE) return 0;

    /* Held cards seed the uniqueness check; each newly drawn replacement
     * is appended too, so a later draw in this same pass can't collide
     * with one just dealt a moment ago -- one 52-card deck for the whole
     * round, not a fresh one per slot. */
    for (i = 0; i < CASINO_HAND_SIZE; ++i) {
        if (casino->poker_held & (1 << i)) {
            hand_rank[count] = casino->poker_rank[i];
            hand_suit[count] = casino->poker_suit[i];
            ++count;
        }
    }
    for (i = 0; i < CASINO_HAND_SIZE; ++i) {
        if (!(casino->poker_held & (1 << i))) {
            casino_deal_unique_card(casino, &casino->poker_rank[i], &casino->poker_suit[i],
                                     hand_rank, hand_suit, count);
            hand_rank[count] = casino->poker_rank[i];
            hand_suit[count] = casino->poker_suit[i];
            ++count;
        }
    }
    casino->poker_payout = casino_poker_payout(casino);
    casino_add_credits(casino, casino->poker_payout);
    casino->poker_state = CASINO_GAME_RESOLVED;
    return casino->poker_payout;
}

unsigned char casino_roulette_spin(CasinoState *casino, unsigned char pocket)
{
    unsigned char roll;
    unsigned char payout = 0;

    if (pocket >= CASINO_ROULETTE_BET_COUNT) return 0;
    if (casino->credits < casino->bet) return 0;

    casino->credits = (unsigned char)(casino->credits - casino->bet);
    casino->roulette_pick = pocket;
    /* Reject the 26 unused values from a masked byte. This keeps every one
     * of the 38 wheel pockets equally likely without a modulo bias. */
    do {
        roll = (unsigned char)(rng_next(&casino->rng) & 63);
    } while (roll >= CASINO_ROULETTE_POCKET_COUNT);
    casino->roulette_result = casino_roulette_wheel[roll];

    if (pocket == CASINO_ROULETTE_RED && (casino->roulette_result & 1)) {
        payout = (unsigned char)(casino->bet * 2);
    } else if (pocket == CASINO_ROULETTE_BLACK &&
               casino->roulette_result != CASINO_ROULETTE_ZERO &&
               casino->roulette_result != CASINO_ROULETTE_DOUBLE_ZERO &&
               !(casino->roulette_result & 1)) {
        payout = (unsigned char)(casino->bet * 2);
    } else if (casino->roulette_result == pocket) {
        unsigned int raw = (unsigned int)casino->bet * 36;
        payout = raw > CASINO_MAX_CREDITS ? CASINO_MAX_CREDITS : (unsigned char)raw;
    }
    casino->roulette_payout = payout;
    casino_add_credits(casino, payout);
    casino->roulette_state = CASINO_GAME_RESOLVED;
    return payout;
}
