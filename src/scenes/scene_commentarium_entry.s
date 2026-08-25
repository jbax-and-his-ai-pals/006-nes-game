; Entry point for the Commentarium image mapped at $8000-$BFFF.

.import _run_commentarium_scene
.export scene_commentarium_entry

.segment "CODE"
scene_commentarium_entry:
        jsr _run_commentarium_scene
        rts
