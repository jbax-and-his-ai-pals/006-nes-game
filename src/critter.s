; ------------------------------------------------------------------------
; critter.s -- All-Nite Mart checkout register.
;
; The legacy ambient wildlife renderer is not part of the active v2 game.
; Its tile slot is reused as a small cash register: a bright housing with a
; dark display/key break, readable above the Mart's checkout counter.
;
; Linked after sword.s, so this tile lands at
; CRITTER_TILE_BASE = SWORD_TILE_HORIZONTAL + 1 (see main.c).
; ------------------------------------------------------------------------

.segment "CHARS"

; housing / keypad silhouette (palette index 1)
.byte $00, $3C, $7E, $FF, $FF, $DB, $FF, $00
; display and key highlights (palette index 3 where both planes are set).
; The upper-right index-2 pixel is the tiny gray paper clip beside it.
.byte $01, $00, $00, $3C, $24, $24, $00, $00
