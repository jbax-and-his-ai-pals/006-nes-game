; ------------------------------------------------------------------------
; chr.s -- CHR-ROM pattern data (the game's "graphics")
;
; The NES stores tile graphics as 2 bits per pixel, split into two
; bit-planes of 8 bytes each (one byte per row of the 8x8 tile).
; The final color index for a pixel is (plane1_bit << 1) | plane0_bit.
;
; This file defines six tiles. Tiles 0/2/3 are reused across every
; room/biome -- only the loaded palette changes what colors they end up
; as (grass green vs. sand tan, etc.), which is how "floor" and "path"
; get a different look per biome without needing separate CHR data for
; each. Obstacles get a distinct shape per biome (tile 2 for the meadow's
; tree line, tile 4 for the desert's rocks) since a recolored circle
; wouldn't read as a different kind of thing the way a different
; silhouette does.
;
;   tile 0 - floor: mostly transparent (background palette entry 0, the
;            "backdrop" color) with a few index-1 pixels for texture
;   tile 1 - Jimbles's lower-right glass half. The rebuilt player uses the
;            dedicated player_avatar.s tiles, leaving this legacy slot free
;            for asymmetric bubble details without shifting any later IDs.
;   tile 2 - black-only facial overlay for the Piss Corner regulars. Their
;            base sprite keeps its red/yellow/white palette while this tile
;            supplies eyes and a mouth without transparent facial holes.
;   tile 3 - solid color index 3; the path connecting a room's exits
;   tile 4 - a rounded-trapezoid boulder in color index 2; the desert's
;            border obstacle. Same "mostly solid so it connects
;            edge-to-edge" idea as tile 2, different silhouette.
;   tile 5 - the left half of a peaked roof, color index 2: a diagonal
;            rising left-to-right, empty ("sky") above it. Paired with
;            house.s's mirrored right half so two side-by-side copies
;            meet in the middle as one roof peak instead of two separate
;            triangles -- see house.s for why the town's houses need
;            more than one repeated tile the way every other room's
;            obstacle does.
; ------------------------------------------------------------------------

.segment "CHARS"

; --- Tile 0: grass floor (mostly backdrop, with a little texture) ---
.byte $00, $00, $20, $04, $00, $00, $10, $00    ; bit-plane 0
.byte $00, $00, $00, $00, $00, $00, $00, $00    ; bit-plane 1

; --- Tile 1: Jimbles's lower-right glass, matching furniture.s tile 62. ---
; Two left-side bubbles live in tile 62; this single right-side bubble keeps
; the glass lively without duplicating marks around the smile's center.
.byte $FE, $FE, $EE, $1E, $FE, $FE, $FE, $FE    ; bit-plane 0
.byte $01, $05, $11, $E1, $01, $01, $01, $01    ; bit-plane 1

; --- Tile 2: left eye and the left half of a simple black smile. ---
; Sprite palette 3 has black at index 1, so plane 0 alone is black while
; every other pixel remains transparent.
.byte $00, $00, $00, $00, $08, $00, $03, $00    ; bit-plane 0
.repeat 8
        .byte $00
.endrepeat

; --- Tile 3: path (solid color index 3 -> both planes set) ---
.repeat 8
        .byte $FF
.endrepeat
.repeat 8
        .byte $FF
.endrepeat

; --- Tile 4: boulder (color index 2 -> plane0 clear, plane1 set) ---
.byte $00, $00, $00, $00, $00, $00, $00, $00    ; bit-plane 0
.byte $7C, $FE, $FF, $FF, $FF, $FF, $7E, $3C    ; bit-plane 1

; --- Tile 5: roof, left half (color index 2 -> plane0 clear, plane1 set) ---
.byte $00, $00, $00, $00, $00, $00, $00, $00    ; bit-plane 0
.byte $00, $01, $03, $07, $0F, $1F, $3F, $7F    ; bit-plane 1
