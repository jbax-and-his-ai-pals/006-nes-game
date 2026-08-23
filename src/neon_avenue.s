; neon_avenue.s -- background tiles for the opening Neon Avenue scene.
; Linked after title_portrait.s so the title's fixed NMI tile tables remain
; stable. Palette index 1 is asphalt/sidewalk detail, 2 is facade neon, and
; 3 is the brightest trim or window light.

.segment "CHARS"

; asphalt: sparse wet-street glints
.byte $00, $04, $00, $20, $00, $00, $01, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00

; sidewalk: a pale curb with dark breaks
.byte $FF, $00, $00, $00, $FF, $00, $00, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00

; brick facade: real coursed brickwork -- a full dark mortar line every
; 4th row, with a staggered vertical joint (offset half a tile on
; alternating courses) so adjacent tiles read as a continuous running
; bond instead of a repeating grid. Plane 1 stays all zero, same as the
; sparse version this replaces -- only two colors per tile either way,
; just arranged as visible coursing instead of a couple of stray dots.
.byte $7F, $7F, $7F, $00, $F7, $F7, $F7, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00

; striped marquee/awning
.byte $CC, $33, $CC, $33, $CC, $33, $CC, $33
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF

; taxi post: slim pole and glowing cap
.byte $18, $18, $18, $18, $18, $18, $3C, $18
.byte $3C, $3C, $18, $18, $18, $18, $3C, $18

; bar door, left / center / right: a plain solid-red panel (index 1, the
; wall's own red) with a continuous white frame line (index 3) down the
; door's outer edges only -- left tile's left column, right tile's right
; column. Repeated vertically with no per-tile pattern that would
; otherwise repeat into a "venetian blind" look, so it reads as one tall,
; uninterrupted door instead of stacked identical tiles. The doorknob is
; drawn separately in C code (see set_tile(...,TOKEN_TILE) in
; show_neon_avenue()) rather than baked in here, since a mark on any of
; these three tiles would print once per vertical repeat instead of once
; on the door.
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $80, $80, $80, $80, $80, $80, $80, $80
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $01, $01, $01, $01, $01, $01, $01, $01

; Rounded black sign plate. In background palette 1, index 1 is pink trim,
; index 2 is black fill, and index 3 is the font's white ink.
; top-left / top / top-right
.byte $3C, $7E, $C0, $C0, $C0, $C0, $C0, $C0
.byte $C3, $81, $3F, $3F, $3F, $3F, $3F, $3F
.byte $FF, $FF, $00, $00, $00, $00, $00, $00
.byte $00, $00, $FF, $FF, $FF, $FF, $FF, $FF
.byte $3C, $7E, $03, $03, $03, $03, $03, $03
.byte $C3, $81, $FC, $FC, $FC, $FC, $FC, $FC
; left / fill / right
.byte $C0, $C0, $C0, $C0, $C0, $C0, $C0, $C0
.byte $3F, $3F, $3F, $3F, $3F, $3F, $3F, $3F
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
.byte $03, $03, $03, $03, $03, $03, $03, $03
.byte $FC, $FC, $FC, $FC, $FC, $FC, $FC, $FC
; bottom-left / bottom / bottom-right
.byte $C0, $C0, $C0, $C0, $C0, $C0, $7E, $3C
.byte $3F, $3F, $3F, $3F, $3F, $3F, $81, $C3
.byte $00, $00, $00, $00, $00, $00, $FF, $FF
.byte $FF, $FF, $FF, $FF, $FF, $FF, $00, $00
.byte $03, $03, $03, $03, $03, $03, $7E, $3C
.byte $FC, $FC, $FC, $FC, $FC, $FC, $81, $C3

; Storefront windows: shared horizontal edge, vertical edge, and fill.
; Reusing these three tiles keeps the outline clean without spending a large
; part of the fixed 8KB CHR budget on rounded-corner variants. Both edge
; tiles produce a white line (index 3) top/bottom or left/right, black
; (index 0, the shared backdrop) everywhere else.
.byte $FF, $00, $00, $00, $00, $00, $00, $FF
.byte $FF, $00, $00, $00, $00, $00, $00, $FF
.repeat 8
        .byte $81
.endrepeat
.repeat 8
        .byte $81
.endrepeat

; Glass fill: blue (index 2) with a dark-blue edge mark (index 1) down its
; own right column and along its own bottom row. Placed 2x2 for one
; window's interior (see draw_window()), those edges land at the same
; two seams every time -- the left pair's right-edge marks and the top
; pair's bottom-edge marks -- so together they form one continuous cross
; through the middle of the glass, with no separate mullion tile spent.
; The window's own border tiles cover the redundant marks this leaves on
; the two outer edges.
.byte $01, $01, $01, $01, $01, $01, $01, $FF
.byte $FE, $FE, $FE, $FE, $FE, $FE, $FE, $00
