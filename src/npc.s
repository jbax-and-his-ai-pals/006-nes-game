; ------------------------------------------------------------------------
; npc.s -- repurposed v2 background door-knob tile.
;
; This retired prototype slot remains tile 51 (after the font and before the
; border tiles). The active v2 build has no use for its former NPC sprite, so
; it now carries a white circular knob on an index-1 red door field. Keeping
; plane 0 set across the whole tile is important: background tile index 0 is
; the universal black backdrop, not transparency.
; ------------------------------------------------------------------------

.segment "CHARS"

; Plane 0: red door field everywhere. Plane 1: the compact white circle
; (index 3 in the exterior's brick/door palette) at the normal handle point.
.repeat 8
        .byte $FF
.endrepeat
.byte %00000000
.byte %00011000
.byte %00111100
.byte %00111100
.byte %00111100
.byte %00011000
.byte %00000000
.byte %00000000
