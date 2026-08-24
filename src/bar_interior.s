; bar_interior.s -- background tiles for Jimbles's Bar.
; Linked after taxi.s so existing title, street, actor, and taxi tile IDs stay
; stable. The six tiles use the bar room's warm wood palette.

.segment "CHARS"

; floorboards
.byte $00, $00, $FF, $00, $00, $00, $FF, $00
.byte $00, $00, $00, $00, $00, $00, $00, $00

; dark wood wall
.repeat 8
        .byte $00
.endrepeat
.byte $FF, $EF, $FF, $DF, $FF, $BF, $FF, $F7

; polished bar counter, bright edge over a dark face
.byte $FF, $FF, $00, $00, $00, $00, $00, $FF
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF

; bottle shelf: lowered silhouettes leave breathing room above the counter
.byte $00, $00, $00, $00, $24, $6D, $6D, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $FF

; back-room right plank: brown wood contained by black vertical borders
.byte $7E, $7E, $7E, $7E, $7E, $7E, $7E, $7E
.byte $00, $00, $00, $00, $00, $00, $00, $00

; jukebox crown: a rounded top spanning the three-tile silhouette
.byte $3C, $7E, $FF, $FF, $FF, $FF, $FF, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00

; jukebox panel: simple rectangular cabinet with a glowing record window
.byte $FF, $81, $BD, $A5, $A5, $BD, $81, $FF
.byte $00, $00, $18, $3C, $3C, $18, $00, $00

; This used to be a second "back-room plank" tile, byte-for-byte identical
; to BAR_TILE_PLANK_R above -- v2_scene_bar.c now draws both plank
; positions with that one tile (see BAR_TILE_PLANK_L there), freeing this
; slot for the blackjack table's bottom-left corner (see taxi.s's top-left
; corner for the shared design notes -- full rail band on both the bottom
; 2 rows and left 2 columns; this is its vertical mirror).
.byte $C0, $C0, $C0, $C0, $C0, $40, $3F, $0F
.byte $3F, $3F, $3F, $3F, $3F, $3F, $00, $00

; Compact bar stool: a warm wooden seat on narrow dark legs. Two instances
; beside the seated regular make the counter read as a place to sit.
.byte $00, $00, $7E, $FF, $7E, $42, $42, $42
.byte $00, $00, $00, $00, $00, $00, $00, $00

; A 48x24 southern exit built from a bright jamb and a deep doorway fill.
; The compact two-tile composite respects the current fixed 8 KiB CHR bank.
.byte $FF, $81, $81, $81, $81, $81, $81, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00
.byte $FF, $00, $18, $3C, $3C, $18, $00, $FF
.byte $00, $00, $00, $00, $00, $00, $00, $00
