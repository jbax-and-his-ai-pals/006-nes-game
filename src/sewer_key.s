; sewer_key.s -- sewer-only replacement for token.s at tile 255. The key is
; white over the local blue field, giving the dry-tunnel pickup a clear bow,
; shaft, and two teeth without affecting the jukebox token in other scenes.

.segment "CHARS"

.byte $3C, $66, $66, $3C, $18, $18, $1C, $18
.repeat 8
        .byte $FF
.endrepeat
