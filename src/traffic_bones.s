; traffic_bones.s -- a small white bone scatter for the traffic death.
; Palette index 3 is white in the Neon Avenue taxi sprite palette.
; Both bitplanes are identical, so every set pixel is white (value 3).
; No skull: the crash's detached head already reads as the victim.
; Tile order is fixed by draw_crash_tableau(): +0/+1 bone halves, +2
; wishbone, +3 femur, +4 club, +5 upright bone.

.segment "CHARS"

; +0 bone, left half: a rounded left knob feeding a two-row shaft; a little
; divot is notched into the far-left end at the vertical centre.
.byte $00, $E0, $F0, $3F, $3F, $F0, $E0, $00
.byte $00, $E0, $F0, $3F, $3F, $F0, $E0, $00

; +1 bone, right half: the shaft finishes in a rounded right knob, with the
; matching divot notched into the far-right end at the vertical centre.
.byte $00, $07, $0F, $FC, $FC, $0F, $07, $00
.byte $00, $07, $0F, $FC, $FC, $0F, $07, $00

; +2 wishbone
.byte $81, $42, $24, $18, $18, $18, $3C, $00
.byte $81, $42, $24, $18, $18, $18, $3C, $00

; +3 diagonal femur: a single bone running top-left to bottom-right, with a
; rounded knob at each end -- distinctly a long bone, not an X.
.byte $E0, $E0, $70, $38, $1C, $0E, $07, $07
.byte $E0, $E0, $70, $38, $1C, $0E, $07, $07

; +4 oversized caveman club-bone
.byte $E0, $F8, $7E, $3C, $1E, $0F, $03, $00
.byte $E0, $F8, $7E, $3C, $1E, $0F, $03, $00

; +5 small upright bone (replaces the old skull-and-crossbones): rounded
; ends top and bottom, a narrow shaft between.
.byte $18, $3C, $3C, $18, $18, $3C, $3C, $18
.byte $18, $3C, $3C, $18, $18, $3C, $3C, $18
