; title_tongue.s -- a 16px open-mouth tongue overlay for the title only.
; Palette index 1 is red in show_title_screen()'s sprite palette 0.

.segment "CHARS"

.byte $00, $3F, $7F, $7F, $3F, $1F, $00, $00
.byte $00, $04, $02, $00, $00, $00, $00, $00
.byte $00, $E0, $F0, $F0, $E0, $C0, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00

; A separate two-pixel uvula sits above the tongue, with a black gap between.
.byte $1E, $00, $00, $00, $00, $00, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00
