; ------------------------------------------------------------------------
; box_border.s -- text box frame tiles.
;
; Two tiles: a horizontal rule (top/bottom edge) and a vertical rule
; (left/right edge), both a 2px-thick line in color index 1 over a
; color-index-2 field -- index 2 matches the box's existing black fill
; (see main.c's show_text_box()), so the line reads as a frame drawn on
; top of the box rather than a separate opaque tile. Deliberately avoids
; color index 0: any background tile's index-0 pixels always show
; palette 0 entry 0 (the room's real backdrop color) regardless of which
; palette the attribute table selects, the same quirk font.s's glyphs
; work around -- see main.c's room_palettes comment.
;
; Linked immediately after npc.s, so these land at
; BORDER_TILE_BASE = NPC_TILE_BASE + 1 (see main.c).
; ------------------------------------------------------------------------

.segment "CHARS"

; --- Horizontal rule: a 2px line across rows 3-4 of 8 ---
.byte $00, $00, $00, $FF, $FF, $00, $00, $00    ; bit-plane 0
.byte $FF, $FF, $FF, $00, $00, $FF, $FF, $FF    ; bit-plane 1

; --- Vertical rule: a 2px line down columns 3-4 of 8 ---
.repeat 8
        .byte $18               ; bit-plane 0: ...XX... every row
.endrepeat
.repeat 8
        .byte $E7               ; bit-plane 1: complement of plane 0
.endrepeat
