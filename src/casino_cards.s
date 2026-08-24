; casino_cards.s -- suit icons and a card-back pattern for blackjack/poker's
; dedicated CHR bank (see mmc3_use_casino_cards_chr() in startup.s and
; build_casino_cards_chr.bat). That bank links chr.s, house.s, font.s,
; box_border.s, then this file, in that exact order -- the first two exist
; purely as 8 tiles of filler so font.s lands at index 8, matching the
; shared draw_text()'s hardcoded FONT_TILE_BASE (see v2_scene_ui.c), the
; same way the common bank's own tile 0-7 prefix does. That puts these
; tiles at indices 53 (spade), 54 (club), 55 (heart), 56 (diamond), 57
; (card back); box_border.s's own two tiles land at 51/52. These are
; genuinely new tiles, not a reuse of the shared common bank's already-
; spent 256-tile budget -- see REBUILD_ROADMAP.md's "CHR Budget" section.
;
; Text tiles (font, card back) keep the established convention: plane 1
; constant $FF, plane 0 the shape, landing filled pixels on index 3 (ink)
; and blank on index 2. Spades and clubs instead land filled pixels on
; index 0 (the universal backdrop, already black, and not otherwise visible
; anywhere on this screen -- see show_casino_game_screen()'s palette
; comment) and blank on index 2; hearts and diamonds land filled pixels on
; index 1 (repurposed there to red -- same comment) and blank on index 2.
; That's a real black/red split at only the cost of an otherwise-unused
; palette entry, no new tile budget and no attribute-block gymnastics,
; since all four still share the one palette group the whole screen uses.

.segment "CHARS"

; --- SPADE: index 0 (black) filled, index 2 (green) blank -- plane 0 is
; all zero, plane 1 is the shape's bitwise complement. ---
.byte %00000000, %00000000, %00000000, %00000000, %00000000, %00000000, %00000000, %00000000
.byte %11100111, %11000011, %10000001, %00000000, %10000001, %11100111, %11100111, %11000011

; --- CLUB: same index 0 / index 2 split as spade. ---
.byte %00000000, %00000000, %00000000, %00000000, %00000000, %00000000, %00000000, %00000000
.byte %11000011, %10000001, %00000000, %10000001, %11100111, %11100111, %11000011, %11111111

; --- HEART: index 1 (repurposed to red) filled, index 2 (green) blank --
; plane 0 is the shape (unchanged from the original single-color version),
; plane 1 is its bitwise complement instead of a constant $FF. ---
.byte %00000000, %01101100, %11111110, %11111110, %01111100, %00111000, %00010000, %00000000
.byte %11111111, %10010011, %00000001, %00000001, %10000011, %11000111, %11101111, %11111111

; --- DIAMOND: same index 1 / index 2 split as heart. ---
.byte %00010000, %00111000, %01111100, %11111110, %11111110, %01111100, %00111000, %00010000
.byte %11101111, %11000111, %10000011, %00000001, %00000001, %10000011, %11000111, %11101111

; --- CARD BACK: a woven lattice, solid enough to read as a face-down card
; from across the table, not just "blank tile with a question mark." ---
.byte %10101010, %01010101, %10101010, %01010101, %10101010, %01010101, %10101010, %01010101
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF

; --- SLOT SEVEN: a bold numeral, distinct from font.s's plain digit glyph
; so the slots' reel windows read as a real slot machine, not a text
; field. Same index 3 (white) filled / index 2 (green) blank split as the
; card-back and font tiles -- plane 0 the shape, plane 1 constant $FF. ---
.byte %11111100, %00001100, %00001100, %00011000, %00011000, %00110000, %00110000, %01100000
.byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF

; --- SLOT CHERRY: two round cherries side by side on a forking black stem,
; landing on index 1 (red, the same repurposed slot hearts/diamonds
; already use) and index 0 (black, the shared backdrop) against index 2
; (green) blank -- three live colors in one tile, so plane 0/1 are built
; from the red and black shapes directly rather than either single-color
; convention above (plane 0 marks red pixels only, plane 1 marks blank
; pixels only, leaving black pixels as 0 on both planes). Each cherry is a
; 3-wide/4-tall blob tapered to a single pixel top and bottom (1-3-3-1)
; rather than the flatter 2-3-3-2 box the first pass used -- pointed caps
; read rounder than flat ones at this size, and the top pixel lands right
; where the stem's branch already ends, so the two connect cleanly. ---
.byte %00000000, %00000000, %00000000, %00000000, %01000010, %11100111, %11100111, %01000010
.byte %11100111, %11100111, %11011011, %10111101, %10111101, %00011000, %00011000, %10111101
