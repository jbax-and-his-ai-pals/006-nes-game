#ifndef MMC3_H
#define MMC3_H

/* CHR changes are safe from ordinary C code because they never replace the
 * caller's CPU-visible PRG window. PRG switches require fixed-bank stubs. */
void mmc3_use_common_chr(void);
void mmc3_use_title_chr(void);

#endif
