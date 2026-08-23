; ------------------------------------------------------------------------
; item.s -- repurposed v2 manhole-center tile.
;
; The retired item renderer is not linked into the active v2 game logic.
; This slot remains tile 54 (after the two border tiles) and provides the
; dark center of the widened manhole cover. Its top and bottom rows remain
; transparent/backdrop while the six middle rows join the tapered house.s
; caps into one continuous 32x8 football oval.
; ------------------------------------------------------------------------

.segment "CHARS"

.byte %00000000
.byte %11111111
.byte %11111111
.byte %11111111
.byte %11111111
.byte %11111111
.byte %11111111
.byte %00000000
.repeat 8
        .byte $00
.endrepeat
