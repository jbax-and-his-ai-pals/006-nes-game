; ------------------------------------------------------------------------
; portal.s -- back-facing bar regular sprite tiles.
;
; The legacy portal renderer is not active in the rebuilt adventure. These
; stable slots now draw the left half of a seated regular from behind; OAM
; mirrors them to make the right half. Tile 0 is the bald/capped head and
; shoulders, tile 1 the jacket back below it.
;
; Linked after critter.s, so these land at
; PORTAL_TILE_BASE = CRITTER_TILE_BASE + 1 and
; PORTAL_DOOR_TILE_BASE = PORTAL_TILE_BASE + 1 (see main.c).
; ------------------------------------------------------------------------

.segment "CHARS"

; --- Tile 0: the full, rounded red hair silhouette from the back. ---
.byte $1F, $3F, $7F, $FF, $FF, $FF, $FF, $FF
.repeat 8
        .byte $00
.endrepeat

; --- Tile 1: hair trailing into a white shirt, with a dark center seam. ---
.byte $FF, $FF, $FF, $FF, $FF, $FE, $FE, $C0
.byte $00, $00, $00, $00, $00, $FE, $FE, $00
