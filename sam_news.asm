; ============================================================================
;  sam_news.prg - player SAM (solo SAM NEWS) come PRG normale
;
;       LOAD"SAM_NEWS.PRG",8  +  RUN
;
;  All'uscita (un tasto) ripristina NMI e SID e torna al BASIC.
; ============================================================================
!to "sam_news.prg", cbm
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
        jmp sam_news                       ; entra diretto nella parte news
!src "sam_player.inc"
