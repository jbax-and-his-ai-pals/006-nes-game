; taxi.s -- a 32x24 yellow side-view taxi and three fixed red impact flecks.
; Linked after player_avatar.s; the first twelve tiles form a 4x3 cab.

.segment "CHARS"

; Row 1: raised yellow cabin. Plane 1 is yellow; setting both planes
; creates the white trim used below.
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $00, $00, $00, $00, $FF, $FF, $FF, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $00, $00, $00, $00, $FF, $FF, $FF, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00

; Row 2: yellow body framing two large black window bays. The tiny white
; lamp at each end survives horizontal mirroring cleanly.
.byte $00, $00, $03, $03, $03, $00, $00, $00
.byte $00, $07, $7F, $FF, $FF, $FF, $FF, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $FF, $FF, $81, $81, $81, $81, $FF, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $FF, $FF, $81, $81, $81, $81, $FF, $FF
.byte $00, $00, $C0, $C0, $C0, $00, $00, $00
.byte $00, $E0, $FE, $FF, $FF, $FF, $FF, $FF

; Row 3: solid yellow skirt beneath a white stripe and alternating
; yellow/white checker band. Only tiles 8 and 9 (this pair) are actually
; drawn -- v2_traffic.c's row-2 case is "TAXI_TILE_BASE + 8 + (col & 1)",
; which only ever selects these two slots, repeating them across the
; width. The checker pattern's natural period-2 alternation made the next
; pair (tiles 10/11, formerly here) byte-for-byte identical to this one,
; so they were dead weight -- see the casino lobby's blackjack table for
; what replaced them (v2_scene_casino.c's rounded table corners).
.byte $FF, $AA, $AA, $55, $55, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $FF, $55, $55, $AA, $AA, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF

; Blackjack table, top-left corner (see v2_scene_casino.c) -- a bolder
; stepped quarter-circle cut (4/2/1 px across 3 rows) than a thin trim
; line, so the curve reads clearly at this resolution. Index 0 (outside
; the curve) is the shared universal backdrop, which the casino lobby's
; own palette 0 already sets to the same warm wood as its floor, so the
; cut corner blends into the floor around the table with no separate
; tile needed underneath. Index 1 (rail) bands the top 2 rows AND the
; left 2 columns -- a full corner of rim, not just the horizontal run --
; index 2 (felt) fills the rest. See v2_scene_casino.c's table palette.
.byte $0F, $3F, $40, $C0, $C0, $C0, $C0, $C0
.byte $00, $00, $3F, $3F, $3F, $3F, $3F, $3F

; Blackjack table, top-right corner -- horizontal mirror of the above
; (background tiles can't be flipped in hardware, so this is authored
; separately rather than reusing the left corner's tile).
.byte $F0, $FC, $02, $03, $03, $03, $03, $03
.byte $00, $00, $FC, $FC, $FC, $FC, $FC, $FC

; red impact flecks (palette index 1), deliberately static after impact.
.byte $00, $42, $18, $24, $18, $42, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $10, $00, $44, $00, $01, $00, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
; A compact rounded dot also doubles as Jimbles's small square-ish door knob.
.byte $00, $18, $3C, $3C, $3C, $18, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
