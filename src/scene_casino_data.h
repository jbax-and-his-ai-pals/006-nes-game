/* Casino-owned, read-only presentation data. This is deliberately separate
 * from the fixed engine so the segment can move to PRG bank 2 unchanged. */
#ifndef SCENE_CASINO_DATA_H
#define SCENE_CASINO_DATA_H

extern const char casino_sign_text[];
extern const char casino_blackjack_text[];
extern const char casino_roulette_text[];
extern const char casino_exit_text[];
extern const char casino_slots_text[];
extern const char casino_credits_text[];
extern const char casino_bet_text[];
extern const char casino_controls_text[];
extern const char casino_pull_text[];
extern const char * const casino_slot_symbols[];

#endif
