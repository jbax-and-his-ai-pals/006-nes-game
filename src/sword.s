; ------------------------------------------------------------------------
; sword.s -- Piss Corner streak and puddle tiles.
;
; The legacy attack renderer is not active in the rebuilt adventure. These
; two stable slots now form another short floor-streak variant and a low,
; irregular puddle for the bar's Piss Corner.
;
; Linked immediately after enemy.s, so these land at
; SWORD_TILE_VERTICAL = ENEMY_TILE_BASE + 1 and
; SWORD_TILE_HORIZONTAL = ENEMY_TILE_BASE + 2 (see main.c).
; ------------------------------------------------------------------------

.segment "CHARS"

; --- A second, offset cluster of short yellow streaks. ---
.byte %00011000
.byte %00000000
.byte %01100000
.byte %00000000
.byte %00001110
.byte %00000000
.byte %00110000
.byte %00000000
.repeat 8
        .byte $00
.endrepeat

; --- Low, irregular puddle, also palette index 1. ---
.byte %00000000
.byte %00111100
.byte %01111110
.byte %11111111
.byte %11111111
.byte %01111110
.byte %00111100
.byte %00000000
.repeat 8
        .byte $00
.endrepeat
