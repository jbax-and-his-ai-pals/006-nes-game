;
; startup.s -- project-owned cc65 NES startup.
;
; This is cc65's NES startup with one deliberate extension: every vblank
; NMI clocks the assembly music sequencer. Keeping the sequencer in assembly
; prevents an interrupt from re-entering cc65's non-reentrant C runtime.
;

        .export         _exit
        .export         __STARTUP__ : absolute = 1
        .export         _mmc3_use_common_chr, _mmc3_use_title_chr, _mmc3_use_park_chr
        .export         _mmc3_use_casino_cards_chr, _mmc3_use_sewer_chr
        .export         _mmc3_select_prg_pair
        .export         _mmc3_call_mapped_scene
        .export         _banked_scene_arg
        .export         _random_seed

        .import         initlib, donelib, callmain
        .import         zerobss, copydata
        .import         music_nmi_tick, _title_mouth_dirty, _title_mouth_open
        .import         _title_message_dirty, _title_message_index
        .import         _vram_queue_count, _vram_queue_hi, _vram_queue_lo
        .import         _vram_queue_tile_data
        .import         title_message_0, title_message_1, title_message_2
        .import         __SRAM_START__, __SRAM_SIZE__
        .forceimport    NESfont

        .include        "zeropage.inc"
        .include        "nes.inc"

.segment "HEADER"
        .byte   $4e,$45,$53,$1a
        .byte   2               ; 2 x 16 KiB PRG-ROM banks
        .byte   1               ; 1 x 8 KiB CHR-ROM bank
        .byte   %00000011       ; horizontal mirroring, battery-backed RAM
        .byte   %00000000
        .byte   0,0,0,0,0,0,0,0

.segment "FIXED_ENGINE"
; These routines only alter PPU pattern-table mapping. Unlike PRG bank
; changes, they cannot unmap the C caller, so scene entry may call them
; directly while the fixed-engine PRG trampoline work is still underway.
_mmc3_use_common_chr:
        lda     #$00
        jmp     mmc3_map_chr

_mmc3_use_title_chr:
        lda     #$08
        jmp     mmc3_map_chr

_mmc3_use_park_chr:
        lda     #$10
        jmp     mmc3_map_chr

; CHR bank 4 -- font.s + box_border.s + casino_cards.s (see
; build_casino_cards_chr.bat and casino_cards.s), a genuinely new tile set
; for the blackjack/poker card screens rather than a reuse of the shared
; common bank's already-spent 256-tile budget.
_mmc3_use_casino_cards_chr:
        lda     #$20
        jmp     mmc3_map_chr

_mmc3_use_sewer_chr:
        lda     #$28
        jmp     mmc3_map_chr

; Map one contiguous 8 KiB CHR page. Registers 0 and 1 select 2 KiB each;
; registers 2 through 5 select the remaining four 1 KiB regions.
mmc3_map_chr:
        tax
        lda     #$00
        sta     $8000
        stx     $8001
        inx
        inx
        lda     #$01
        sta     $8000
        stx     $8001
        inx
        inx
        lda     #$02
        sta     $8000
        stx     $8001
        inx
        lda     #$03
        sta     $8000
        stx     $8001
        inx
        lda     #$04
        sta     $8000
        stx     $8001
        inx
        lda     #$05
        sta     $8000
        stx     $8001
        rts

; C fastcall ABI passes the first bank number in A. This routine lives in the
; fixed engine, so callers may safely replace both switchable CPU windows.
; Scene code must never call it directly because it would unmap its own return
; address before RTS; the fixed dispatcher will own that transition.
_mmc3_select_prg_pair:
        tax
        lda     #$06                ; register 6 maps $8000-$9FFF
        sta     $8000
        txa
        sta     $8001
        inx
        lda     #$07                ; register 7 maps $A000-$BFFF
        sta     $8000
        txa
        sta     $8001
        rts

; The caller and this return address are fixed at $C000-$FFFF. The selected
; scene may therefore return normally without relying on a switchable bank.
_mmc3_call_mapped_scene:
        jsr     $8000
        rts

; Returns a fresh per-game seed byte in A. The free-running vblank frame
; counter is the NES's only genuinely non-repeatable entropy source: which
; frame an action lands on depends on the player's timing, which varies
; every run. Mixing the low and high bytes spreads the 16-bit counter across
; the byte range.
_random_seed:
        lda     tickcount
        eor     tickcount+1
        bne     :+
        lda     #1              ; never hand back the LFSR's zero fixed point
:       rts

.segment "BSS"
_banked_scene_arg:
        .res    1

.segment "FIXED_ENGINE"

start:
        sei
        cld
        ; MMC3 powers up with unspecified CHR register contents. Map the
        ; common 8 KiB CHR bank before any title or gameplay PPU writes.
        jsr     _mmc3_use_common_chr
        ldx     #0
        stx     VBLANK_FLAG
        stx     ringread
        stx     ringwrite
        stx     ringcount
        txs

        lda     #$20
@clear_ring:
        sta     ringbuff,x
        sta     ringbuff+$0100,x
        sta     ringbuff+$0200,x
        inx
        bne     @clear_ring

        jsr     zerobss
        jsr     copydata

        lda     #<(__SRAM_START__ + __SRAM_SIZE__)
        ldx     #>(__SRAM_START__ + __SRAM_SIZE__)
        sta     c_sp
        stx     c_sp+1

        jsr     initlib
        jsr     callmain

_exit:  jsr     donelib
        jmp     start

; MMC3 always fixes its final 8 KiB at $E000, so reset lands in this tiny
; segment even before the switchable $8000/$A000 banks are selected.
.segment "BOOT"
mmc3_boot:
        lda     #$80                ; enable the cartridge's 8 KiB PRG RAM
        sta     $A001
        lda     #$06                ; register 6 controls the $8000 bank
        sta     $8000
        lda     #$00
        sta     $8001
        lda     #$07                ; register 7 controls the $A000 bank
        sta     $8000
        lda     #$01
        sta     $8001
        jmp     start

.segment "FIXED_ENGINE"

; The NMI owns only timing and APU writes. Room/title rendering uses direct
; PPU register writes while forced blank, so touching PPU registers here
; could interrupt a multi-byte address/data sequence and corrupt the redraw.
nmi:    pha
        tya
        pha
        txa
        pha

        lda     #1
        sta     VBLANK_FLAG
        inc     tickcount
        bne     @tick_done
        inc     tickcount+1
@tick_done:
        jsr     music_nmi_tick
        jsr     title_message_nmi_tick
        jsr     title_mouth_nmi_tick
        jsr     vram_queue_nmi_tick

        pla
        tax
        pla
        tay
        pla
irq:    rti

; These tile IDs mirror main.c's title portrait layout: base tile 63, mouth
; source rows 7-9/columns 4-6, followed by the 3x3 closed-mouth patch.
title_mouth_open_tiles:
        .byte 137,138,139, 147,148,149, 157,158,159
title_mouth_closed_tiles:
        .byte 183,184,185, 186,187,188, 189,190,191

title_speech_plate:
        .byte 52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52
        .byte 53,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,53
        .byte 53,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,53
        .byte 53,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,53
        .byte 52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52

.macro WRITE_TITLE_MOUTH tiles
        lda     #$21                ; nametable row 15, column 6
        sta     PPU_VRAM_ADDR2
        lda     #$E6
        sta     PPU_VRAM_ADDR2
        lda     tiles+0
        sta     PPU_VRAM_IO
        lda     tiles+1
        sta     PPU_VRAM_IO
        lda     tiles+2
        sta     PPU_VRAM_IO

        lda     #$22                ; row 16, column 6
        sta     PPU_VRAM_ADDR2
        lda     #$06
        sta     PPU_VRAM_ADDR2
        lda     tiles+3
        sta     PPU_VRAM_IO
        lda     tiles+4
        sta     PPU_VRAM_IO
        lda     tiles+5
        sta     PPU_VRAM_IO

        lda     #$22                ; row 17, column 6
        sta     PPU_VRAM_ADDR2
        lda     #$26
        sta     PPU_VRAM_ADDR2
        lda     tiles+6
        sta     PPU_VRAM_IO
        lda     tiles+7
        sta     PPU_VRAM_IO
        lda     tiles+8
        sta     PPU_VRAM_IO
.endmacro

.macro WRITE_TITLE_MESSAGE tiles
        lda     #$21                ; nametable row 12, column 14
        sta     PPU_VRAM_ADDR2
        lda     #$8E
        sta     PPU_VRAM_ADDR2
        lda     tiles+0
        sta     PPU_VRAM_IO
        lda     tiles+1
        sta     PPU_VRAM_IO
        lda     tiles+2
        sta     PPU_VRAM_IO
        lda     tiles+3
        sta     PPU_VRAM_IO
        lda     tiles+4
        sta     PPU_VRAM_IO
        lda     tiles+5
        sta     PPU_VRAM_IO
        lda     tiles+6
        sta     PPU_VRAM_IO
        lda     tiles+7
        sta     PPU_VRAM_IO
        lda     tiles+8
        sta     PPU_VRAM_IO
        lda     tiles+9
        sta     PPU_VRAM_IO
        lda     tiles+10
        sta     PPU_VRAM_IO
        lda     tiles+11
        sta     PPU_VRAM_IO
        lda     tiles+12
        sta     PPU_VRAM_IO
        lda     tiles+13
        sta     PPU_VRAM_IO
        lda     tiles+14
        sta     PPU_VRAM_IO
        lda     tiles+15
        sta     PPU_VRAM_IO
        lda     tiles+16
        sta     PPU_VRAM_IO
.endmacro

.macro WRITE_SPEECH_ROW low, offset
        .local copy
        lda     #$21
        sta     PPU_VRAM_ADDR2
        lda     #low
        sta     PPU_VRAM_ADDR2
        ldx     #0
copy:
        lda     title_speech_plate+offset,x
        sta     PPU_VRAM_IO
        inx
        cpx     #19
        bne     copy
.endmacro

.macro CLEAR_SPEECH_ROW low
        .local clear
        lda     #$21
        sta     PPU_VRAM_ADDR2
        lda     #low
        sta     PPU_VRAM_ADDR2
        ldx     #19
        lda     #0
clear:
        sta     PPU_VRAM_IO
        dex
        bne     clear
.endmacro

.macro WRITE_SPEECH_ATTRIBUTES value
        .local upper, lower
        lda     #$23
        sta     PPU_VRAM_ADDR2
        lda     #$D3
        sta     PPU_VRAM_ADDR2
        ldx     #5
        lda     #value
upper:
        sta     PPU_VRAM_IO
        dex
        bne     upper
        lda     #$23
        sta     PPU_VRAM_ADDR2
        lda     #$DB
        sta     PPU_VRAM_ADDR2
        ldx     #5
        lda     #value
lower:
        sta     PPU_VRAM_IO
        dex
        bne     lower
.endmacro

; Draws the static plate tiles; the caller supplies the actual text row.
draw_title_speech_plate:
        WRITE_SPEECH_ROW $4D, 0
        WRITE_SPEECH_ROW $6D, 19
        WRITE_SPEECH_ROW $8D, 38
        WRITE_SPEECH_ROW $AD, 57
        WRITE_SPEECH_ROW $CD, 76
        rts

; Restores both the 19x5 plate and its one-tile shadow to the original title
; background. The five affected attribute bytes in rows 2 and 3 return to
; palette 0, which keeps the floor texture from inheriting plate colors.
clear_title_speech_plate:
        CLEAR_SPEECH_ROW $4D
        CLEAR_SPEECH_ROW $6D
        CLEAR_SPEECH_ROW $8D
        CLEAR_SPEECH_ROW $AD
        CLEAR_SPEECH_ROW $CD
        CLEAR_SPEECH_ROW $ED
        WRITE_SPEECH_ATTRIBUTES $00
        rts

; C queues only the line index; the NMI owns the complete PPU transaction.
title_message_nmi_tick:
        lda     _title_message_dirty
        bne     @queued
        rts
@queued:
        lda     #0
        sta     _title_message_dirty
        lda     PPU_STATUS
        lda     _title_message_index
        cmp     #3
        bne     :+
        jsr     clear_title_speech_plate
        lda     #0
        sta     PPU_VRAM_ADDR1
        sta     PPU_VRAM_ADDR1
        rts
:
        WRITE_SPEECH_ATTRIBUTES $55
        jsr     draw_title_speech_plate
        lda     _title_message_index
        bne     :+
        jmp     @line_0
:
        cmp     #1
        bne     :+
        jmp     @line_1
:
        jmp     @line_2
@line_0:
        WRITE_TITLE_MESSAGE title_message_0
        jmp     @restore_scroll
@line_1:
        WRITE_TITLE_MESSAGE title_message_1
        jmp     @restore_scroll
@line_2:
        WRITE_TITLE_MESSAGE title_message_2
@restore_scroll:
        lda     #0
        sta     PPU_VRAM_ADDR1
        sta     PPU_VRAM_ADDR1
        rts

; Applies a queued 3x3 title-mouth tile patch at the start of vblank. The
; frame data lives here, so the NMI never races C while reading nine bytes.
title_mouth_nmi_tick:
        lda     _title_mouth_dirty
        bne     @queued
        rts
@queued:
        lda     #0
        sta     _title_mouth_dirty

        lda     PPU_STATUS          ; reset the shared $2005/$2006 write latch
        lda     _title_mouth_open
        bne     @open
        jmp     @closed
@open:
        WRITE_TITLE_MOUTH title_mouth_open_tiles
        jmp     @restore_scroll
@closed:
        WRITE_TITLE_MOUTH title_mouth_closed_tiles

@restore_scroll:
        lda     #0                  ; restore the title's fixed scroll
        sta     PPU_VRAM_ADDR1
        sta     PPU_VRAM_ADDR1
@done:  rts

; C publishes queue entries by writing their three bytes, then increasing
; count. The NMI claims count before consuming it, so a later frame can begin
; filling the same buffer without changing the transaction in progress.
vram_queue_nmi_tick:
        ldx     _vram_queue_count
        beq     @done
        lda     #0
        sta     _vram_queue_count
        lda     PPU_STATUS
        ldy     #0
@write:
        lda     _vram_queue_hi,y
        sta     PPU_VRAM_ADDR2
        lda     _vram_queue_lo,y
        sta     PPU_VRAM_ADDR2
        lda     _vram_queue_tile_data,y
        sta     PPU_VRAM_IO
        iny
        dex
        bne     @write
        lda     #0
        sta     PPU_VRAM_ADDR1
        sta     PPU_VRAM_ADDR1
@done:  rts

.segment "VECTORS"
        .word nmi
        .word mmc3_boot
        .word irq
