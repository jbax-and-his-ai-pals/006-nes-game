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
; yellow/white checker band. Tiles 8 and 9 are repeated by the renderer.
.byte $FF, $AA, $AA, $55, $55, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $FF, $55, $55, $AA, $AA, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $FF, $AA, $AA, $55, $55, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $FF, $55, $55, $AA, $AA, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF

; red impact flecks (palette index 1), deliberately static after impact.
.byte $00, $42, $18, $24, $18, $42, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $10, $00, $44, $00, $01, $00, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
; A compact rounded dot also doubles as Jimbles's small square-ish door knob.
.byte $00, $18, $3C, $3C, $3C, $18, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
