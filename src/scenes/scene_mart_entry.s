.import _run_mart_exterior, _run_mart_interior
.import _banked_scene_arg
.export scene_mart_entry

.segment "CODE"
scene_mart_entry:
        lda _banked_scene_arg
        cmp #$02
        beq @interior
        jsr _run_mart_exterior
        rts
@interior:
        jsr _run_mart_interior
        rts
