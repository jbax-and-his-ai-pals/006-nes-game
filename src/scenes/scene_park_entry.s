; Entry point for the Park image mapped at $8000-$BFFF.
; The fixed dispatcher owns the cross-bank return address.

.import _run_park
.export scene_park_entry

.segment "CODE"
scene_park_entry:
        jsr _run_park
        rts
