; sewer_frame.s -- sewer-only flow-fixture tile plus the shared text-border
; tiles (51-53). The sewer keeps its custom fixture while messages retain the
; common smooth border used everywhere else.

.segment "CHARS"

; Fully framed cell: dark-blue top, bottom, left, and right edges over blue
; glass. Used at both left corners so the grid never protrudes or breaks.
.byte $FF, $81, $81, $81, $81, $81, $81, $FF
.byte $00, $7E, $7E, $7E, $7E, $7E, $7E, $00

; Shared horizontal rule: a 2px line across rows 3-4 over the plate fill.
.byte $00, $00, $00, $FF, $FF, $00, $00, $00
.byte $FF, $FF, $FF, $00, $00, $FF, $FF, $FF

; Shared vertical rule: a 2px line down columns 3-4 over the plate fill.
.repeat 8
        .byte $18
.endrepeat
.repeat 8
        .byte $E7
.endrepeat
