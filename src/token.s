; token.s -- the jukebox token: a round coin with a small punched hole.
; This is the very last tile slot available before the 256-tile boundary
; both sprites and backgrounds currently share (PPU.control never selects
; the far CHR half) -- see v2_title_main.c's TOKEN_TILE comment.

.segment "CHARS"

.byte $3C, $7E, $FF, $E7, $E7, $FF, $7E, $3C
.repeat 8
        .byte $00
.endrepeat
