; player_avatar.s -- six-sprite (16x24) playable actor for the adventure scenes.
;
; The title portrait is too large and background-only to reuse verbatim, but
; this compact sprite carries its important silhouette: bald head, loud glasses,
; pale beard, and dark jacket. The final tile pair is a mirrored back view.

.segment "CHARS"

; head, left / right: a broad swept hair cap, skin, and two dark square eyes.
.byte $0F, $3F, $7F, $F0, $0C, $0C, $00, $00
.byte $00, $00, $00, $0F, $F3, $F3, $FF, $7F
.byte $F0, $FC, $FE, $0F, $30, $30, $00, $00
.byte $00, $00, $00, $F0, $CF, $CF, $FF, $FE

; torso, left / right: dark jacket shoulders around a bright shirt front.
.byte $FF, $FF, $FF, $FF, $FF, $FF, $7E, $7E
.byte $0F, $0F, $0F, $0F, $0F, $00, $81, $81
.byte $FF, $FF, $FF, $FF, $FF, $FF, $7E, $7E
.byte $F0, $F0, $F0, $F0, $F0, $00, $81, $81

; walk frame A: square trouser legs and bright shoe tips.
.byte $3C, $3C, $3C, $3C, $18, $18, $3C, $3C
.byte $00, $00, $00, $00, $00, $00, $18, $18
.byte $3C, $3C, $3C, $3C, $18, $18, $3C, $3C
.byte $00, $00, $00, $00, $00, $00, $18, $18

; Side-view head: dark swept hair over a face which projects one pixel to
; the right as a deliberately readable NES-scale nose. OAM mirrors it left.
.byte $7E, $FF, $FF, $FF, $FE, $FE, $FC, $78
.byte $00, $00, $00, $00, $01, $01, $03, $07
; Side-view jacket and hand. This tile and the head above are the only new
; art; the regular trouser/shoe tile completes the slim 8x24 profile.
.byte $FE, $FE, $7E, $7E, $7E, $3C, $3C, $7E
.byte $00, $00, $00, $00, $00, $00, $00, $18
