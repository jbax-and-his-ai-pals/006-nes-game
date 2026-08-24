; Entry point for the Bar image mapped at $8000-$BFFF.

.import _run_bar_interior
.import _banked_scene_arg
.export scene_bar_entry

.segment "CODE"
scene_bar_entry:
        lda _banked_scene_arg
        jsr _run_bar_interior
        rts
