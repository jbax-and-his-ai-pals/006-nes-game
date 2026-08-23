; player_avatar.s -- six-sprite (16x24) playable actor for the adventure scenes.
;
; The title portrait is too large and background-only to reuse verbatim, but
; this compact sprite carries its important silhouette: bald head, loud glasses,
; pale beard, and dark jacket. The final two tile pairs alternate as feet.

.segment "CHARS"

; head, left / right: a rounded scalp and red glasses, with transparent corners
.byte $0F, $1F, $3F, $20, $20, $21, $21, $20
.byte $00, $00, $00, $1F, $1F, $11, $1F, $00
.byte $F0, $F8, $FC, $04, $04, $84, $84, $04
.byte $00, $00, $00, $F8, $F8, $88, $F8, $00

; face and beard, left / right: skin above a white beard and red jacket collar
.byte $3F, $27, $3F, $3F, $3E, $3C, $3C, $3C
.byte $00, $18, $3F, $3F, $3E, $1C, $00, $00
.byte $FC, $E4, $FC, $FC, $7C, $3C, $3C, $3C
.byte $00, $18, $FC, $FC, $7C, $38, $00, $00

; walk frame A: a compact jacket and a wide, readable step
.byte $18, $3C, $3C, $18, $08, $00, $00, $00
.byte $00, $00, $00, $00, $10, $10, $30, $30
.byte $18, $3C, $3C, $18, $10, $00, $00, $00
.byte $00, $00, $00, $00, $08, $08, $0C, $0C

; walk frame B: feet pass underneath the same jacket instead of mirroring it
.byte $18, $3C, $3C, $18, $08, $00, $00, $10
.byte $00, $00, $00, $00, $10, $10, $10, $10
.byte $18, $3C, $3C, $18, $10, $00, $00, $08
.byte $00, $00, $00, $00, $08, $08, $08, $08
