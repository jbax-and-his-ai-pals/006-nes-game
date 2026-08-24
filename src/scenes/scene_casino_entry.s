.import _run_casino
.export scene_casino_entry

.segment "CODE"
scene_casino_entry:
        jsr _run_casino
        rts
