; title_messages.s -- generated from assets/title_messages.txt.
; Regenerate with: python tools/gen_title_messages.py

.segment "RODATA"
.export title_message_0, title_message_1, title_message_2

; PLAY OR ELSE
title_message_0:
        .byte 40,36,25,49,8,39,42,8,29,36,43,29,8,8,8,8,8

; DON'T BE AN IDIOT
title_message_1:
        .byte 28,39,38,10,44,8,26,29,8,25,38,8,33,28,33,39,44

; YOU WON'T WIN
title_message_2:
        .byte 49,39,45,8,47,39,38,10,44,8,47,33,38,8,8,8,8
