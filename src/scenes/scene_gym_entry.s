.import _run_gym_exterior, _run_gym_interior
.import _banked_scene_arg
.export scene_gym_entry

.segment "CODE"
scene_gym_entry:
        lda _banked_scene_arg
        cmp #$02
        beq @interior
        jsr _run_gym_exterior
        rts
@interior:
        jsr _run_gym_interior
        rts
