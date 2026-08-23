#include "v2_casino.h"

void casino_reset(CasinoState *casino)
{
    casino->credits = 10;
    casino->bet = 1;
    casino->reel[0] = 0;
    casino->reel[1] = 0;
    casino->reel[2] = 0;
    rng_seed(&casino->rng, 0x2A);
}

unsigned char casino_is_winner(const CasinoState *casino)
{
    return casino->reel[0] == casino->reel[1] &&
           casino->reel[1] == casino->reel[2];
}

unsigned char casino_payout(const CasinoState *casino)
{
    if (!casino_is_winner(casino)) return 0;
    return (unsigned char)(casino->bet * 5);
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
