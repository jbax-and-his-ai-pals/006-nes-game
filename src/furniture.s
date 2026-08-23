; ------------------------------------------------------------------------
; furniture.s -- reserved background tiles, now used as Jimbles's sprite art.
;
; The current adventure slice no longer shows the old generic house furniture.
; Reusing these two slots keeps the fixed 8 KiB CHR bank stable while giving
; Jimbles an explicitly drawn face/body. The top tile is mirrored by OAM;
; the lower-right half uses chr.s tile 1 so its bubbles need not mirror.
;
; Linked after portal.s, so these land at
; FURNITURE_TABLE_TILE = PORTAL_DOOR_TILE_BASE + 1 and
; FURNITURE_BED_TILE = FURNITURE_TABLE_TILE + 1 (see main.c).
; ------------------------------------------------------------------------

.segment "CHARS"

; --- Tile 0: upper-left face/body, with a full-width three-pixel foam cap ---
; Palette index 1 is beer yellow, 2 is the black glass edge/pupil, and 3 is
; white. This is authored as the left half; OAM mirrors it for the right.
.byte $7F, $7F, $7F, $7F, $6F, $67, $7F, $7F
.byte $FF, $FF, $FF, $80, $98, $98, $80, $80

; --- Tile 1: left half of the solid beer glass, smile, and two bubbles. ---
; The first bit-plane stays yellow except for the smile; the second adds the
; black glass edge/smile and a few isolated white carbonation pixels.
.byte $7F, $7F, $77, $78, $7F, $7F, $7F, $7F
.byte $A0, $80, $88, $87, $90, $80, $80, $80
