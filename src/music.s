;
; music.s -- a longer, NMI-clocked neon-night theme.
;
; The state is private to this module and the NMI never calls C. This keeps
; the 60 Hz song clock independent of room rendering and game-loop load.
;

        .export _music_start, _music_select_song, _music_play_jukebox_fanfare, _music_sfx_swing, _music_sfx_pickup
        .export _music_sfx_dialogue, _music_sfx_hit, _music_sfx_hurt
        .export _music_sfx_door, music_nmi_tick
        .export _music_sfx_confirm, _music_sfx_cancel, _music_sfx_bell
        ; C reads/writes music_scene_bank (the fixed_scene_stubs set it to the
        ; bank a scene just mapped, so music_select_song can restore it).
        .export _music_scene_bank
        ; music_select_song maps the music bank to copy the selected song into
        ; the fixed-RAM cache, then restores the scene bank.
        .import _mmc3_select_prg_pair

        .include "nes.inc"

MUSIC_STEPS = 32
MUSIC_STEPS_BAR = 96
MUSIC_SONG_BAR = 1
MUSIC_SONG_COUNT = 8
MUSIC_SONG_TITLE = 7
; First bank of the PRG pair holding the music bank (see
; assets/banks/manifest.json and src/music_songs.s).
MUSIC_BANK_BASE = 19
; Every song block in the music bank is this many bytes (see gen_music_songs.py).
; Each song holds up to 96 steps (6 * 96 + two 1-byte flags); scenes and the
; title pad their shorter loops so the stride stays uniform for the copy loop.
SONG_BLOCK_SIZE = 578

.segment "FIXED_ENGINE_RODATA"
; Song data now lives in the music bank (src/music_songs.s).

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
_music_step:     .res 1
_music_frames:   .res 1
_music_steps:    .res 1          ; loop length of the current song (title: 16)
music_enabled:  .res 1
music_song:     .res 1
jukebox_state:  .res 1       ; 0 = inactive, 1 = phrase, 2 = quiet pause
jukebox_step:   .res 1
jukebox_frames: .res 1
pickup_phase:   .res 1
pickup_frames:  .res 1
; Fixed-RAM cache of the current song's block (578 bytes), copied from the
; music bank on music_select_song so the NMI never touches a PRG bank.
; Every song reserves 96 step slots; scenes and the title use fewer.
music_ram_melody_lo:   .res 96
music_ram_melody_hi:   .res 96
music_ram_bass_lo:     .res 96
music_ram_bass_hi:     .res 96
music_ram_step_frames: .res 96
music_ram_noise:       .res 96
music_ram_has_bass:    .res 1
music_ram_has_noise:   .res 1
; First PRG bank of the scene currently mapped at $8000, so music_select_song
; can restore it after briefly mapping the music bank to copy the song.
_music_scene_bank:     .res 1

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
        sta _music_step
        sta music_song
        sta jukebox_state
        ; No song is loaded here: the RAM cache is empty until the title (or a
        ; scene) calls music_select_song, which copies the song's block in.
        lda #1
        sta music_enabled
        rts

; Select a loop and cache it. The NMI reads only the fixed-RAM cache, so we
; briefly map the music bank (just to copy the current song's block), then
; restore whichever scene bank was mapped when we were called.
_music_select_song:
        cmp #MUSIC_SONG_COUNT
        bcc :+
        lda #0
:
        cmp music_song              ; if the requested song is the one currently
        beq @keep_playing           ; loaded, keep playing it instead of restarting
        sta music_song              ; A holds the clamped song here
        lda #$BF                    ; pulse 1 at full volume for the new loop
        sta APU_PULSE1CTRL
        lda #$9A                    ; continuous triangle bass;
        sta APU_TRICTRL1            ; the loader gates it per song
        lda #MUSIC_BANK_BASE
        jsr _mmc3_select_prg_pair
        lda music_song
        asl a
        tax
        lda $8000,x                 ; pointer-table low byte of this song's block
        sta $0020
        lda $8001,x                 ; pointer-table high byte
        sta $0021
        ; Song blocks are now 386 bytes (up to 64 steps), which the 8-bit
        ; Y/X register loop could not span, so copy via 16-bit source ($0020)
        ; and destination ($0022) pointers and a 16-bit count ($0024/$0025).
        lda #<music_ram_melody_lo
        sta $0022
        lda #>music_ram_melody_lo
        sta $0023
        lda #<SONG_BLOCK_SIZE
        sta $0024
        lda #>SONG_BLOCK_SIZE
        sta $0025
        ldy #0
@copy:
        lda ($0020),y
        sta ($0022),y
        iny
        bne @copy_next
        inc $0021                   ; source moved past a 256-byte page
        inc $0023                   ; destination too
@copy_next:
        lda $0024
        bne @copy_dec_lo
        dec $0025                   ; low count byte wrapped: borrow the high
@copy_dec_lo:
        dec $0024
        lda $0024
        ora $0025
        bne @copy
@copy_done:
        lda _music_scene_bank
        jsr _mmc3_select_prg_pair
        ; Per-song loop length. The bar interior runs its full 96-step arc;
        ; scenes use the first 32 steps of their padded block; the title is a
        ; single 16-step phrase (its block repeats the phrase to fill the stride).
        lda music_song
        cmp #MUSIC_SONG_BAR
        bne @not_bar
        lda #MUSIC_STEPS_BAR
        jmp @steps0
@not_bar:
        cmp #MUSIC_SONG_TITLE
        bne @loc32
        lda #16
        jmp @steps0
@loc32:
        lda #MUSIC_STEPS
@steps0:
        sta _music_steps
        lda #0
        sta _music_step
        sta jukebox_state
        jsr music_load_step
        rts

; Same song already playing (the bar, casino, and commentarium share one loop
; so walking between them keeps the music going instead of restarting it).
@keep_playing:
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
        dec _music_frames
        beq music_advance
        jmp music_done
music_advance:
        inc _music_step
        lda _music_step
        cmp _music_steps
        bcc :+
        lda #0
        sta _music_step
:
        jmp music_load_step

; The single step loader: reads the current song's cached block from fixed
; RAM (music_ram_*) and writes the voices. No bank access, safe in NMI.
; A $00 melody period low byte is a rest (pulse 1 silenced); has_bass/has_noise
; gate the triangle and noise voices.
music_load_step:
        ldx _music_step
        lda music_ram_step_frames,x
        sta _music_frames
        lda music_ram_melody_lo,x
        sta APU_PULSE1FTUNE
        lda music_ram_melody_hi,x
        sta APU_PULSE1CTUNE
        lda music_ram_melody_lo,x
        beq @rest
        lda #$BF
        sta APU_PULSE1CTRL
        jmp @triangle
@rest:
        lda #$30
        sta APU_PULSE1CTRL
@triangle:
        lda music_ram_has_bass
        beq @triangle_off
        lda music_ram_bass_lo,x
        beq @triangle_off       ; $00 bass low byte is a rest: keep it silent
        lda #$9A
        sta APU_TRICTRL1
        lda music_ram_bass_lo,x
        sta APU_TRIFREQ1
        lda music_ram_bass_hi,x
        sta APU_TRIFREQ2
        jmp @noise
@triangle_off:
        lda #$00
        sta APU_TRICTRL1
@noise:
        lda music_ram_has_noise
        beq @noise_off
        lda music_ram_noise,x
        cmp #$FF
        beq @noise_off
        sta APU_NOISEFREQ1
        lda #$58
        sta APU_NOISEFREQ2
        lda #$18
        sta APU_NOISECTRL
        rts
@noise_off:
        lda #$00
        sta APU_NOISECTRL
        rts

; The fanfare takes over pulse 1 while the title loop is suspended. Triangle
; is silenced too, keeping the requested note phrase clean and unmistakable.
; Pulse 1 is armed here rather than trusted to the loop's last volume, since
; the shared theme can leave it at a rest (silenced) when the fanfare fires.
jukebox_load_step:
        ldx jukebox_step
        lda jukebox_melody_lo,x
        sta APU_PULSE1FTUNE
        lda jukebox_melody_hi,x
        sta APU_PULSE1CTUNE
        lda #$BF                    ; 50% duty, constant volume 15
        sta APU_PULSE1CTRL
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
        sta _music_step
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

; Generic one-shots: bright accept, low deny, and a single ringing bell.
; All pulse-2 so they never collide with the title theme's pulse 1/triangle.
_music_sfx_confirm:
        lda #$9F                    ; bright, constant volume 15
        sta APU_PULSE2CTRL
        lda #$00
        sta APU_PULSE2RAMP
        lda #$4A                    ; crisp mid-high note
        sta APU_PULSE2FTUNE
        lda #$50                    ; 10 frames, then hardware silences it
        sta APU_PULSE2STUNE
        rts

_music_sfx_cancel:
        lda #$96                    ; softened, constant volume 10
        sta APU_PULSE2CTRL
        lda #$00
        sta APU_PULSE2RAMP
        lda #$A0                    ; lower, "no" note
        sta APU_PULSE2FTUNE
        lda #$50                    ; 10 frames, then hardware silences it
        sta APU_PULSE2STUNE
        rts

_music_sfx_bell:
        lda #$9F                    ; bright, constant volume 15
        sta APU_PULSE2CTRL
        lda #$00
        sta APU_PULSE2RAMP
        lda #$30                    ; high, clean ding
        sta APU_PULSE2FTUNE
        lda #$78                    ; 15 frames: a slightly longer ring
        sta APU_PULSE2STUNE
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
