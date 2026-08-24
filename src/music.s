;
; music.s -- a longer, NMI-clocked neon-night theme.
;
; The state is private to this module and the NMI never calls C. This keeps
; the 60 Hz song clock independent of room rendering and game-loop load.
;

        .export _music_start, _music_select_song, _music_play_jukebox_fanfare, _music_sfx_swing, _music_sfx_pickup
        .export _music_sfx_dialogue, _music_sfx_hit, _music_sfx_hurt
        .export _music_sfx_door, music_nmi_tick

        .include "nes.inc"

MUSIC_STEPS = 32
MUSIC_SONG_COUNT = 7

.segment "FIXED_ENGINE_RODATA"
; A 32-step lounge-jazz loop: pulse 1 carries a nimble, offbeat phrase while
; triangle walks a bass line underneath. Period bytes target NTSC APU timers.
; Ten-frame steps keep the groove lively; the final held note creates a clear
; turnaround before the loop returns to its opening E-G-A lift.
melody_lo:
        .byte $54,$1E,$FE,$D6, $E2,$FE,$1E,$54
        .byte $41,$FE,$D6,$BE, $D6,$FE,$1E,$54
        .byte $7D,$41,$FE,$D6, $AA,$BE,$D6,$FE
        .byte $1E,$E2,$BE,$AA, $BE,$E2,$FE,$1E
melody_hi:
        .byte $F9,$F9,$F8,$F8, $F8,$F8,$F9,$F9
        .byte $F9,$F8,$F8,$F8, $F8,$F8,$F9,$F9
        .byte $F9,$F9,$F8,$F8, $F8,$F8,$F8,$F8
        .byte $F9,$F8,$F8,$F8, $F8,$F8,$F8,$F9
bass_lo:
        .byte $57,$3D,$FE,$3D, $85,$57,$FD,$3D
        .byte $FE,$A9,$85,$57, $3D,$FD,$A9,$FE
        .byte $FD,$FE,$C7,$85, $A9,$FD,$57,$3D
        .byte $3D,$C7,$FD,$A9, $FD,$C7,$FE,$3D
bass_hi:
        .byte $FB,$FA,$F9,$FA, $FA,$FB,$FA,$FA
        .byte $F9,$FA,$FA,$FB, $FA,$FA,$FA,$F9
        .byte $FA,$F9,$F9,$FA, $FA,$FA,$FB,$FA
        .byte $FA,$F9,$FA,$FA, $FA,$F9,$F9,$FA
step_frames:
        .byte 10,10,10,10, 10,10,10,10
        .byte 10,10,10,10, 10,10,10,10
        .byte 10,10,10,10, 10,10,10,10
        .byte 10,10,10,10, 10,10,10,20

; Each location gets a 16-note motif repeated once. The familiar Avenue
; arrangement above remains song zero; these variants keep the same light
; CPU cost while making scene changes immediately audible.
bar_melody_lo:
        .byte $D6,$BE,$AA,$BE,$D6,$FE,$E2,$BE, $D6,$BE,$AA,$BE,$D6,$FE,$E2,$BE
bar_melody_hi:
        .byte $F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8, $F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8
bar_step_frames:
        .byte 6,6,12,6, 8,6,16,8, 6,6,12,6, 8,6,10,18
mart_melody_lo:
        .byte $54,$41,$54,$7D,$54,$41,$54,$1E, $54,$41,$54,$7D,$54,$41,$54,$1E
mart_melody_hi:
        .byte $F9,$F9,$F9,$F9,$F9,$F9,$F9,$F9, $F9,$F9,$F9,$F9,$F9,$F9,$F9,$F9
mart_step_frames:
        .byte 5,5,5,10, 5,5,7,14, 5,5,5,10, 5,5,7,18
sewer_melody_lo:
        .byte $E2,$FE,$D6,$FE,$BE,$FE,$D6,$FE, $E2,$FE,$D6,$FE,$BE,$FE,$D6,$FE
sewer_melody_hi:
        .byte $F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8, $F8,$F8,$F8,$F8,$F8,$F8,$F8,$F8
sewer_step_frames:
        .byte 18,8,14,8, 20,8,12,16, 18,8,14,8, 20,8,12,24
park_melody_lo:
        .byte $1E,$54,$7D,$54,$1E,$54,$41,$54, $1E,$54,$7D,$54,$1E,$54,$41,$54
park_melody_hi:
        .byte $F9,$F9,$F9,$F9,$F9,$F9,$F9,$F9, $F9,$F9,$F9,$F9,$F9,$F9,$F9,$F9
park_step_frames:
        .byte 8,6,10,6, 12,6,8,16, 8,6,10,6, 12,6,8,20
casino_melody_lo:
        .byte $54,$1E,$54,$7D,$41,$54,$1E,$54, $D6,$BE,$D6,$54,$41,$54,$7D,$54
casino_melody_hi:
        .byte $F9,$F9,$F9,$F9,$F9,$F9,$F9,$F9, $F8,$F8,$F8,$F9,$F9,$F9,$F9,$F9
casino_step_frames:
        .byte 4,4,6,4, 8,4,4,10, 4,4,6,4, 8,4,6,14
zoo_melody_lo:
        .byte $7D,$54,$41,$54,$7D,$AA,$7D,$54, $41,$54,$7D,$AA,$BE,$AA,$7D,$54
zoo_melody_hi:
        .byte $F9,$F9,$F9,$F9,$F9,$F8,$F9,$F9, $F9,$F9,$F9,$F8,$F8,$F8,$F9,$F9
zoo_step_frames:
        .byte 10,8,12,8, 14,8,10,16, 10,8,12,8, 14,8,10,22

bar_bass_lo:   .repeat 2
        .byte $57,$57,$85,$85,$3D,$3D,$57,$57
        .endrepeat
bar_bass_hi:   .repeat 2
        .byte $FB,$FB,$FA,$FA,$FA,$FA,$FB,$FB
        .endrepeat
mart_bass_lo:  .repeat 2
        .byte $57,$57,$3D,$3D,$57,$57,$85,$85
        .endrepeat
mart_bass_hi:  .repeat 2
        .byte $FB,$FB,$FA,$FA,$FB,$FB,$FA,$FA
        .endrepeat
sewer_bass_lo: .repeat 2
        .byte $3D,$3D,$FE,$FE,$A9,$A9,$FE,$FE
        .endrepeat
sewer_bass_hi: .repeat 2
        .byte $FA,$FA,$FA,$FA,$F9,$F9,$FA,$FA
        .endrepeat
park_bass_lo:  .repeat 2
        .byte $85,$85,$57,$57,$3D,$3D,$57,$57
        .endrepeat
park_bass_hi:  .repeat 2
        .byte $FA,$FA,$FB,$FB,$FA,$FA,$FB,$FB
        .endrepeat
casino_bass_lo:.repeat 2
        .byte $57,$3D,$85,$57,$A9,$85,$57,$3D
        .endrepeat
casino_bass_hi:.repeat 2
        .byte $FB,$FA,$FA,$FB,$F9,$FA,$FB,$FA
        .endrepeat
zoo_bass_lo:   .repeat 2
        .byte $85,$57,$85,$3D,$57,$85,$A9,$85
        .endrepeat
zoo_bass_hi:   .repeat 2
        .byte $FA,$FB,$FA,$FA,$FB,$FA,$F9,$FA
        .endrepeat

; One-shot Jimbles's jukebox fanfare, transcribed directly from the requested
; A5 A5 G5 F5 F5 F5 G5 A5 F5 F5 F5 G5 G5 F5 E5 F5 phrase. Each note lasts
; sixteen NTSC frames; after the last note, the room stays quiet for one second.
JUKEBOX_NOTES = 16
jukebox_melody_lo:
        .byte $7E,$7E,$8E,$9F, $9F,$9F,$8E,$7E
        .byte $9F,$9F,$9F,$8E, $8E,$9F,$A9,$9F
jukebox_melody_hi:
        .byte $F8,$F8,$F8,$F8, $F8,$F8,$F8,$F8
        .byte $F8,$F8,$F8,$F8, $F8,$F8,$F8,$F8

.segment "BSS"
music_step:     .res 1
music_frames:   .res 1
music_enabled:  .res 1
music_song:     .res 1
jukebox_state:  .res 1       ; 0 = inactive, 1 = phrase, 2 = quiet pause
jukebox_step:   .res 1
jukebox_frames: .res 1
pickup_phase:   .res 1
pickup_frames:  .res 1

.segment "FIXED_ENGINE"
_music_start:
        lda #0
        sta music_enabled
        lda #$0F                    ; enable pulse 1/2, triangle, and noise
        sta APU_CHANCTRL
        lda #$BF                    ; 50% duty, constant volume 15
        sta APU_PULSE1CTRL
        lda #$00
        sta APU_PULSE1RAMP
        lda #$9A                    ; triangle counter runs continuously
        sta APU_TRICTRL1
        lda #$10                    ; silence effect channels without halting length counters
        sta APU_PULSE2CTRL
        sta APU_NOISECTRL
        lda #0
        sta music_step
        sta music_song
        sta jukebox_state
        jsr music_load_step
        lda #1
        sta music_enabled
        rts

; Restart a selected loop on scene entry. The NMI remains the only routine
; which advances notes, so selecting a new song cannot affect frame pacing.
_music_select_song:
        cmp #MUSIC_SONG_COUNT
        bcc :+
        lda #0
:
        sta music_song
        lda #0
        sta music_step
        sta jukebox_state
        jsr music_load_step
        rts

; Play the jukebox phrase once, then resume the original loop after a short
; intentional pause. The existing effect channels remain available throughout.
_music_play_jukebox_fanfare:
        lda #1
        sta jukebox_state
        lda #0
        sta jukebox_step
        lda #16
        sta jukebox_frames
        jsr jukebox_load_step
        rts

music_nmi_tick:
        jsr sfx_tick
        lda jukebox_state
        beq :+
        jmp jukebox_tick
:
        lda music_enabled
        beq :+
        jmp music_tick
:
        jmp music_done
music_tick:
        dec music_frames
        beq music_advance
        jmp music_done
music_advance:
        inc music_step
        lda music_step
        cmp #MUSIC_STEPS
        bcc :+
        lda #0
        sta music_step
:
        jmp music_load_step

music_load_step:
        lda music_song
        bne :+
        jmp music_load_avenue
:
        cmp #1
        bne :+
        jmp music_load_bar
:
        cmp #2
        bne :+
        jmp music_load_mart
:
        cmp #3
        bne :+
        jmp music_load_sewer
:
        cmp #4
        bne :+
        jmp music_load_park
:
        cmp #5
        bne :+
        jmp music_load_casino
:
        jmp music_load_zoo

; The loader copies a single selected step to the two music voices. Each
; branch is static and index-only, avoiding mapper reads or C work in NMI.
.macro LOAD_SONG melodylo, melodyhi, basslo, basshi
        ldx music_step
        lda step_frames,x
        sta music_frames
        lda melodylo,x
        sta APU_PULSE1FTUNE
        lda melodyhi,x
        sta APU_PULSE1CTUNE
        lda basslo,x
        sta APU_TRIFREQ1
        lda basshi,x
        sta APU_TRIFREQ2
        rts
.endmacro

.macro LOAD_VARIANT melodylo, melodyhi, basslo, basshi, frames
        ldx music_step
        txa
        and #$0F
        tax
        lda frames,x
        sta music_frames
        lda melodylo,x
        sta APU_PULSE1FTUNE
        lda melodyhi,x
        sta APU_PULSE1CTUNE
        lda basslo,x
        sta APU_TRIFREQ1
        lda basshi,x
        sta APU_TRIFREQ2
        rts
.endmacro

music_load_avenue:
        LOAD_SONG melody_lo, melody_hi, bass_lo, bass_hi
music_load_bar:
        LOAD_VARIANT bar_melody_lo, bar_melody_hi, bar_bass_lo, bar_bass_hi, bar_step_frames
music_load_mart:
        LOAD_VARIANT mart_melody_lo, mart_melody_hi, mart_bass_lo, mart_bass_hi, mart_step_frames
music_load_sewer:
        LOAD_VARIANT sewer_melody_lo, sewer_melody_hi, sewer_bass_lo, sewer_bass_hi, sewer_step_frames
music_load_park:
        LOAD_VARIANT park_melody_lo, park_melody_hi, park_bass_lo, park_bass_hi, park_step_frames
music_load_casino:
        LOAD_VARIANT casino_melody_lo, casino_melody_hi, casino_bass_lo, casino_bass_hi, casino_step_frames
music_load_zoo:
        LOAD_VARIANT zoo_melody_lo, zoo_melody_hi, zoo_bass_lo, zoo_bass_hi, zoo_step_frames

; Legacy loader body retained below for source history.
music_load_step_legacy:
        ldx music_step
        lda step_frames,x
        sta music_frames
        lda melody_lo,x
        sta APU_PULSE1FTUNE
        lda melody_hi,x             ; writing high byte restarts its length
        sta APU_PULSE1CTUNE
        lda bass_lo,x
        sta APU_TRIFREQ1
        lda bass_hi,x
        sta APU_TRIFREQ2
        rts

; The fanfare takes over pulse 1 while the title loop is suspended. Triangle
; is silenced too, keeping the requested note phrase clean and unmistakable.
jukebox_load_step:
        ldx jukebox_step
        lda jukebox_melody_lo,x
        sta APU_PULSE1FTUNE
        lda jukebox_melody_hi,x
        sta APU_PULSE1CTUNE
        lda #$00
        sta APU_TRICTRL1
jukebox_tick:
        lda jukebox_state
        cmp #1
        beq jukebox_phrase_tick
        dec jukebox_frames
        bne music_done
        lda #0
        sta jukebox_state
        sta music_step
        lda #$BF                    ; restore the normal loop's pulse voice
        sta APU_PULSE1CTRL
        lda #$9A                    ; and its continuous triangle bass
        sta APU_TRICTRL1
        jsr music_load_step
        jmp music_done

jukebox_phrase_tick:
        dec jukebox_frames
        bne music_done
        inc jukebox_step
        lda jukebox_step
        cmp #JUKEBOX_NOTES
        bcc jukebox_next_note
        lda #2
        sta jukebox_state
        lda #60
        sta jukebox_frames
        lda #$30                    ; pulse 1 volume zero during the pause
        sta APU_PULSE1CTRL
        jmp music_done

jukebox_next_note:
        lda #16
        sta jukebox_frames
        jsr jukebox_load_step
music_done:
        rts

; Effects use pulse 2 or noise, never the title theme's pulse 1/triangle.
; The APU length counters end one-shot effects without a C-side frame timer.
_music_sfx_swing:
        lda #$9A                    ; hollow pulse, constant volume 10
        sta APU_PULSE2CTRL
        lda #$08                    ; gentle downward sweep
        sta APU_PULSE2RAMP
        lda #$E0
        sta APU_PULSE2FTUNE
        lda #$F8
        sta APU_PULSE2STUNE
        rts

_music_sfx_dialogue:
        lda #$96                    ; brief, soft acknowledgement blip
        sta APU_PULSE2CTRL
        lda #$00
        sta APU_PULSE2RAMP
        lda #$80
        sta APU_PULSE2FTUNE
        lda #$F8
        sta APU_PULSE2STUNE
        rts

_music_sfx_pickup:
        lda #0                      ; do not expose partially initialized state to NMI
        sta pickup_phase
        lda #5
        sta pickup_frames
        lda #$9F                    ; bright first half of the collection chime
        sta APU_PULSE2CTRL
        lda #$4A
        sta APU_PULSE2FTUNE
        lda #$F8
        sta APU_PULSE2STUNE
        lda #1
        sta pickup_phase
        rts

_music_sfx_hit:
        lda #$1F                    ; crisp noise hit, constant volume 15
        sta APU_NOISECTRL
        lda #$0B
        sta APU_NOISEFREQ1
        lda #$58                    ; 10 frames, then hardware silences it
        sta APU_NOISEFREQ2
        rts

_music_sfx_hurt:
        lda #$1C                    ; lower, longer noise thud for player damage
        sta APU_NOISECTRL
        lda #$05
        sta APU_NOISEFREQ1
        lda #$B9                    ; 22 frames, then hardware silences it
        sta APU_NOISEFREQ2
        rts

_music_sfx_door:
        lda #$18                    ; subdued doorway/room-transition knock
        sta APU_NOISECTRL
        lda #$03
        sta APU_NOISEFREQ1
        lda #$48                    ; 8 frames, then hardware silences it
        sta APU_NOISEFREQ2
        rts

; The pickup is the only effect needing a second timed note. NMI advances it
; even if collecting the item immediately begins a text-box redraw.
sfx_tick:
        lda pickup_phase
        beq sfx_done
        dec pickup_frames
        bne sfx_done
        lda #0
        sta pickup_phase
        lda #$9F
        sta APU_PULSE2CTRL
        lda #$1C                    ; higher second note: the audible "ding"
        sta APU_PULSE2FTUNE
        lda #$F8
        sta APU_PULSE2STUNE
sfx_done:
        rts
