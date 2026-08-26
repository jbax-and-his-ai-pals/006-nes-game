.import _run_arcade_exterior
.export scene_arcade_entry

.segment "CODE"
scene_arcade_entry:
        jsr _run_arcade_exterior
        rts
