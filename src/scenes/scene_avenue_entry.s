; Entry point for the Neon Avenue image mapped at $8000-$BFFF.
; The validation dispatcher uses arrival mode zero for a fresh street entry.

.import _run_neon_avenue
.import _banked_scene_arg
.export scene_avenue_entry

.segment "CODE"
scene_avenue_entry:
        lda _banked_scene_arg
        jsr _run_neon_avenue
        rts
