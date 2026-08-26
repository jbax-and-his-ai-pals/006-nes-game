.import _run_sidestreet_exterior
.export scene_sidestreet_entry

.segment "CODE"
scene_sidestreet_entry:
        jsr _run_sidestreet_exterior
        rts
