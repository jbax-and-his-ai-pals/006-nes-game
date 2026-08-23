; ------------------------------------------------------------------------
; house.s -- retired town-house slots, repurposed for the v2 manhole cover.
;
; These two slots remain at tile IDs 6 and 7, immediately before the font,
; but the active v2 build does not use the retired town-house renderer. They
; now form the left and right caps of a 32x8 manhole: a flat horizontal
; oval that contrasts with the sidewalk without needing new CHR capacity.
; ------------------------------------------------------------------------

.segment "CHARS"

; --- Tile 0: football-oval left half, palette index 1 (dark navy) ---
.byte $00, $03, $0F, $1F, $1F, $0F, $03, $00    ; bit-plane 0
.byte $00, $00, $00, $00, $00, $00, $00, $00    ; bit-plane 1

; --- Tile 1: football-oval right half (mirror of tile 0) ---
.byte $00, $C0, $F0, $F8, $F8, $F0, $C0, $00    ; bit-plane 0
.byte $00, $00, $00, $00, $00, $00, $00, $00    ; bit-plane 1
