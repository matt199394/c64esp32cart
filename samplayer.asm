; ============================================================================
;  samplayer.asm -> samplayer.prg  (PRG a $0801, caricato dal menu SAM)
;
;  Due punti d'ingresso (il menu somma 3 all'entry per le news):
;       $080D = SAM TALK   (SYS 2061, quello che l'ESP annuncia al menu)
;       $0810 = SAM NEWS
;  All'uscita torna al menu con $C003 ("jmp restart" del menu, che rimetta
;  $01 = $36: nessun passaggio dal BASIC).
; ============================================================================
!to "samplayer.prg", cbm
MENU_INIT = $c000                          ; uscita -> menu (main_init)
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
        jmp sam_talk
        jmp sam_news                        ; ingresso +3 = news
!src "sam_player.inc"
