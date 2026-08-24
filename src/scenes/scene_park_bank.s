; scene_park_bank.s -- first paired PRG scene module.
;
; This is deliberately a no-op scene for now. Its 16 KiB link contract,
; bank-pair manifest allocation, and stable $8000 entry are the format future
; callable scene modules use. The fixed trampoline may safely JSR this entry
; once fixed PRG relocation is live.

.export scene_park_bank_entry, scene_park_bank_signature

.segment "CODE"
scene_park_bank_entry:
        rts

.segment "RODATA"
scene_park_bank_signature:
        .byte "PARK-BANK-01", 0
        .byte "MOONLIGHT PARK", 0
        .byte "BANK PAIR 03-04", 0
