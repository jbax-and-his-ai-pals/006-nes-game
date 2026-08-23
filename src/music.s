;
; music.s -- a longer, NMI-clocked neon-night theme.
;
; The state is private to this module and the NMI never calls C. This keeps
; the 60 Hz song clock independent of room rendering and game-loop load.
;

        .export _music_start, _music_play_jukebox_fanfare, _music_sfx_swing, _music_sfx_pickup
        .export _music_sfx_dialogue, _music_sfx_hit, _music_sfx_hurt
        .export _music_sfx_door, music_nmi_tick

        .include "nes.inc"

MUSIC_STEPS = 32

.segment "RODATA"
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
jukebox_state:  .res 1       ; 0 = inactive, 1 = phrase, 2 = quiet pause
jukebox_step:   .res 1
jukebox_frames: .res 1
pickup_phase:   .res 1
pickup_frames:  .res 1

.segment "CODE"
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
        sta jukebox_state
        jsr music_load_step
        lda #1
        sta music_enabled
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
        bne jukebox_tick
        lda music_enabled
        bne music_tick
        jmp music_done
music_tick:
        dec music_frames
        beq music_advance
        jmp music_done
music_advance:
        inc music_step
        lda music_step
        cmp #MUSIC_STEPS
        bcc music_load_step
        lda #0
        sta music_step

music_load_step:
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
