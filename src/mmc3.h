#ifndef MMC3_H
#define MMC3_H

/* CHR changes are safe from ordinary C code because they never replace the
 * caller's CPU-visible PRG window. PRG switches require fixed-bank stubs. */
void mmc3_use_common_chr(void);
void mmc3_use_title_chr(void);
void mmc3_use_park_chr(void);
void mmc3_use_sewer_chr(void);
/* CHR bank 4: font + box border + suit icons + card back, for the
 * blackjack/poker card screens. See casino_cards.s. */
void mmc3_use_casino_cards_chr(void);

/* Fixed-engine only: maps a contiguous 16 KiB scene pair into $8000-$BFFF.
 * The fastcall ABI passes first_bank in A, so no scene-stack data is touched
 * while the mapper registers are updated. */
void __fastcall__ mmc3_select_prg_pair(unsigned char first_bank);

/* Fixed-engine trampoline for a selected scene image. Each scene owns a
 * callable entry stub at $8000 and returns here before another bank switch. */
/* Returns the scene entry's A-register result to the fixed dispatcher. */
unsigned char mmc3_call_mapped_scene(void);

#endif
