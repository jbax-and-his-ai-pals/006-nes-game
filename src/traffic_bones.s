; traffic_bones.s -- a small white bone scatter for the traffic death.
; Palette index 3 is white in the Neon Avenue taxi sprite palette.

.segment "CHARS"

; left half: round joint, thin shaft, round joint
.byte $00, $00, $03, $7F, $7F, $03, $00, $00
.byte $00, $00, $03, $7F, $7F, $03, $00, $00

; right half mirrors the shaft and joint profile
.byte $00, $00, $C0, $FE, $FE, $C0, $00, $00
.byte $00, $00, $C0, $FE, $FE, $C0, $00, $00

; wishbone
.byte $81, $42, $24, $18, $18, $18, $3C, $00
.byte $81, $42, $24, $18, $18, $18, $3C, $00

; diagonal femur, with rounded ends
.byte $C3, $66, $3C, $18, $18, $3C, $66, $C3
.byte $C3, $66, $3C, $18, $18, $3C, $66, $C3

; oversized caveman club-bone
.byte $E0, $F8, $7E, $3C, $1E, $0F, $03, $00
.byte $E0, $F8, $7E, $3C, $1E, $0F, $03, $00

; compact skull and crossed bones
.byte $C3, $66, $3C, $7E, $DB, $7E, $24, $42
.byte $C3, $66, $3C, $7E, $DB, $7E, $24, $42
