; ------------------------------------------------------------
; scene_music_tables.s -- hand-composed scene song period tables.
;
; The six scene themes used to live inside src/music.s and were read
; back through `git show HEAD:src/music.s` when building the music bank.
; That is fragile (it only works if the committed copy still carries
; them). They now live here, committed and read directly by
; tools/gen_music_songs.py, so the bank always regenerates.
;
; Each table is 32 bytes (one engine step per byte); the generator
; pads each song up to the bank stride.
; ------------------------------------------------------------

melody_lo:
        .byte $1E,$54,$7D,$54,$1E,$54,$7D,$AA
        .byte $D6,$BE,$AA,$BE,$D6,$E2,$1E,$54
        .byte $7D,$54,$41,$54,$7D,$AA,$BE,$AA
        .byte $7D,$54,$1E,$E2,$D6,$BE,$AA,$FE

melody_hi:
        .byte $F9,$F9,$F9,$F9,$F9,$F9,$F9,$F8
        .byte $F8,$F8,$F8,$F8,$F8,$F8,$F9,$F9
        .byte $F9,$F9,$F9,$F9,$F9,$F8,$F8,$F8
        .byte $F9,$F9,$F9,$F8,$F8,$F8,$F8,$F8

bass_lo:
        .byte $57,$57,$57,$57,$85,$85,$85,$85
        .byte $3D,$3D,$3D,$3D,$A9,$A9,$A9,$A9
        .byte $57,$57,$57,$57,$C7,$C7,$C7,$C7
        .byte $3D,$3D,$85,$85,$A9,$A9,$57,$57

bass_hi:
        .byte $FB,$FB,$FB,$FB,$FA,$FA,$FA,$FA
        .byte $FA,$FA,$FA,$FA,$F9,$F9,$F9,$F9
        .byte $FB,$FB,$FB,$FB,$F9,$F9,$F9,$F9
        .byte $FA,$FA,$FA,$FA,$F9,$F9,$FB,$FB

step_frames:
        .byte $04,$04,$04,$04,$04,$04,$04,$07
        .byte $05,$04,$04,$04,$05,$07,$04,$06
        .byte $04,$04,$04,$04,$05,$04,$06,$04
        .byte $04,$04,$05,$04,$06,$04,$05,$14

variant_melody_hi:
        .byte $F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F9,$F8
        .byte $F8,$F8,$F9,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8,$F9

mart_melody_lo:
        .byte $54,$41,$54,$7D,$54,$41,$54,$1E,$41,$54,$7D,$AA,$7D,$54,$41,$1E
        .byte $54,$7D,$AA,$7D,$54,$41,$54,$1E,$41,$54,$7D,$54,$41,$1E,$54,$7D

mart_bass_lo:
        .byte $57,$57,$3D,$3D,$57,$57,$85,$85,$3D,$3D,$57,$57,$A9,$A9,$85,$85
        .byte $57,$57,$85,$85,$3D,$3D,$57,$57,$A9,$A9,$57,$57,$3D,$3D,$85,$85

mart_bass_hi:
        .byte $FB,$FB,$FA,$FA,$FB,$FB,$FA,$FA,$FA,$FA,$FB,$FB,$F9,$F9,$FA,$FA
        .byte $FB,$FB,$FA,$FA,$FA,$FA,$FB,$FB,$F9,$F9,$FB,$FB,$FA,$FA,$FA,$FA

mart_step_frames:
        .byte $05,$05,$05,$10,$05,$05,$07,$14,$04,$06,$05,$09,$07,$05,$08,$13
        .byte $05,$07,$06,$10,$04,$06,$05,$15,$06,$05,$08,$05,$07,$12,$05,$18

sewer_melody_lo:
        .byte $E2,$FE,$D6,$FE,$BE,$FE,$D6,$FE,$AA,$FE,$BE,$FE,$D6,$FE,$E2,$FE
        .byte $D6,$FE,$BE,$FE,$AA,$FE,$BE,$FE,$D6,$FE,$E2,$FE,$D6,$FE,$BE,$FE

sewer_bass_lo:
        .byte $3D,$3D,$FE,$FE,$A9,$A9,$FE,$FE,$57,$57,$FE,$FE,$85,$85,$FE,$FE
        .byte $3D,$3D,$FE,$FE,$57,$57,$FE,$FE,$A9,$A9,$FE,$FE,$85,$85,$FE,$FE

sewer_bass_hi:
        .byte $FA,$FA,$FA,$FA,$F9,$F9,$FA,$FA,$FB,$FB,$FA,$FA,$FA,$FA,$FA,$FA
        .byte $FA,$FA,$FA,$FA,$FB,$FB,$FA,$FA,$F9,$F9,$FA,$FA,$FA,$FA,$FA,$FA

sewer_step_frames:
        .byte $18,$08,$14,$08,$20,$08,$12,$16,$16,$07,$18,$09,$14,$08,$22,$10
        .byte $15,$08,$20,$08,$13,$09,$17,$07,$21,$08,$14,$10,$18,$08,$16,$26

park_melody_lo:
        .byte $1E,$54,$7D,$54,$1E,$54,$41,$54,$7D,$AA,$7D,$54,$41,$54,$1E,$54
        .byte $54,$7D,$AA,$BE,$AA,$7D,$54,$41,$1E,$54,$7D,$54,$41,$54,$1E,$54

park_bass_lo:
        .byte $85,$85,$57,$57,$3D,$3D,$57,$57,$A9,$A9,$57,$57,$85,$85,$3D,$3D
        .byte $57,$57,$85,$85,$A9,$A9,$57,$57,$3D,$3D,$57,$57,$85,$85,$57,$57

park_bass_hi:
        .byte $FA,$FA,$FB,$FB,$FA,$FA,$FB,$FB,$F9,$F9,$FB,$FB,$FA,$FA,$FA,$FA
        .byte $FB,$FB,$FA,$FA,$F9,$F9,$FB,$FB,$FA,$FA,$FB,$FB,$FA,$FA,$FB,$FB

park_step_frames:
        .byte $08,$06,$10,$06,$12,$06,$08,$16,$06,$07,$09,$05,$11,$07,$10,$14
        .byte $07,$08,$06,$12,$05,$09,$07,$11,$10,$06,$08,$06,$12,$07,$09,$22

casino_melody_lo:
        .byte $54,$1E,$54,$7D,$41,$54,$1E,$54,$D6,$BE,$D6,$54,$41,$54,$7D,$54
        .byte $1E,$54,$7D,$AA,$7D,$54,$41,$54,$D6,$E2,$D6,$BE,$AA,$7D,$54,$1E

casino_bass_lo:
        .byte $57,$3D,$85,$57,$A9,$85,$57,$3D,$57,$A9,$85,$57,$3D,$57,$85,$A9
        .byte $3D,$57,$85,$A9,$57,$3D,$57,$85,$A9,$85,$57,$3D,$57,$85,$A9,$57

casino_bass_hi:
        .byte $FB,$FA,$FA,$FB,$F9,$FA,$FB,$FA,$FB,$F9,$FA,$FB,$FA,$FB,$FA,$F9
        .byte $FA,$FB,$FA,$F9,$FB,$FA,$FB,$FA,$F9,$FA,$FB,$FA,$FB,$FA,$F9,$FB

casino_step_frames:
        .byte $04,$04,$06,$04,$08,$04,$04,$10,$04,$04,$06,$04,$08,$04,$06,$14
        .byte $05,$04,$07,$05,$08,$04,$06,$10,$04,$05,$06,$04,$09,$05,$07,$16

zoo_melody_lo:
        .byte $7D,$54,$41,$54,$7D,$AA,$7D,$54,$41,$54,$7D,$AA,$BE,$AA,$7D,$54
        .byte $54,$7D,$AA,$BE,$D6,$BE,$AA,$7D,$54,$41,$54,$7D,$AA,$7D,$54,$1E

zoo_bass_lo:
        .byte $85,$57,$85,$3D,$57,$85,$A9,$85,$57,$3D,$57,$85,$A9,$85,$57,$3D
        .byte $85,$A9,$85,$57,$3D,$57,$85,$A9,$85,$57,$3D,$57,$85,$A9,$85,$57

zoo_bass_hi:
        .byte $FA,$FB,$FA,$FA,$FB,$FA,$F9,$FA,$FB,$FA,$FB,$FA,$F9,$FA,$FB,$FA
        .byte $FA,$F9,$FA,$FB,$FA,$FB,$FA,$F9,$FA,$FB,$FA,$FB,$FA,$F9,$FA,$FB

zoo_step_frames:
        .byte $10,$08,$12,$08,$14,$08,$10,$16,$09,$07,$11,$08,$13,$09,$12,$15
        .byte $08,$10,$07,$12,$14,$08,$10,$09,$11,$08,$10,$07,$13,$09,$11,$24

