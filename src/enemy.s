; ------------------------------------------------------------------------
; enemy.s -- Piss Corner short streak tile.
;
; Combat is retired from the active v2 scenes. This stable tile slot is now
; one of two short yellow floor-streak variants in Jimbles's Piss Corner.
;
; Linked immediately after item.s, so this tile lands at
; ENEMY_TILE_BASE = ITEM_TILE_BASE + 1 (see main.c).
; ------------------------------------------------------------------------

.segment "CHARS"

; Three uneven horizontal marks, palette index 1 over the bar floor.
.byte %00000000
.byte %00111000
.byte %00000000
.byte %00000110
.byte %00000000
.byte %01110000
.byte %00000000
.byte %00000000
.repeat 8
        .byte $00
.endrepeat
