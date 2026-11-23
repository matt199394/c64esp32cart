; ============================================================================
;  sam_talk.prg - player SAM come PRG normale (test da BASIC o dal menu)
;
;       LOAD"SAM_TALK.PRG",8  +  RUN
;
;  All'uscita (RUN/STOP) ripristina NMI e SID e torna al BASIC (warm start).
;  Comandi ESP32: 114 stop, 115 riga, 116 news, 117 fonte, 118 testo news.
; ============================================================================
!to "sam_talk.prg", cbm
MENU_INIT = $e37b                          ; uscita -> BASIC warm start
EXIT_TO_BASIC = 1                          ; servono le ROM (BASIC warm start)
* = $0801
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"
        !byte 0
.basic_end
        !word 0
!if * != 2061 {
        !error "l'indirizzo SYS non coincide con il codice"
}
!src "sam_player.inc"
