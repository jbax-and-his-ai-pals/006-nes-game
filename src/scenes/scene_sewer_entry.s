.import _run_sewer
.export scene_sewer_entry

.segment "CODE"
scene_sewer_entry:
        jsr _run_sewer
        rts
