; ============================================================================
;  bootstrap_sd.asm - lo stesso bootstrap della EPROM, ma avviabile da SD2IEC
; ----------------------------------------------------------------------------
;  Serve solo per PROVARE il menu di avvio e il caricamento dei PRG dall'ESP32
;  senza programmare la 27C512. Il loader e' lo stesso (boot_loader.inc), solo
;  che qui viene copiato a $C000 da un piccolo stub BASIC invece che dalla
;  EPROM a $8000.
;
;  Build:
;       acme bootstrap_sd.asm        -> boot_sd.prg
;
;  Uso:
;       LOAD"BOOT_SD.PRG",8
;       RUN
;  (il comando 112 e' innocuo senza cartuccia: $8000-$9FFF e' gia' RAM)
;
;  Requisiti sulla SD dell'ESP32: espnewsauto.prg (+ espsamauto.prg e
;  samplayer.prg per la scelta 2) nella root.
; ============================================================================

!to "boot_sd.prg", cbm

!addr ptr1 = $fd
!addr ptr2 = $fb
PRG_ENTRY = $0339

!macro pushreg {
        php
        pha
        txa
        pha
        tya
        pha
}
!macro popreg {
        pla
        tay
        pla
        tax
        pla
        plp
}

* = $0801
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"                        ; SYS 2061
        !byte 0
.basic_end
        !word 0
!if * != 2061 {
        !error "l'indirizzo SYS non coincide con il codice"
}

; copia il loader a $C000 e lo avvia (come il warmstart della EPROM)
sd_start
        sei
        ldx #$ff
        txs
        cli
        lda #<payload_rom
        sta ptr1
        lda #>payload_rom
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$c0
        sta ptr2+1
        ldy #0
        ldx #>BOOT_LEN
        beq .tail
.pages  lda (ptr1),y
        sta (ptr2),y
        iny
        bne .pages
        inc ptr1+1
        inc ptr2+1
        dex
        bne .pages
.tail   ldx #<BOOT_LEN
        beq .go
.rest   lda (ptr1),y
        sta (ptr2),y
        iny
        dex
        bne .rest
.go     jmp $c000

payload_rom = *
!pseudopc $c000 {
!src "boot_loader.inc"
}

payload_end = *
BOOT_LEN = payload_end - payload_rom

; schermata del menu di avvio (1024 byte screen code + 1024 colori), generata
; da tools/make_boot_menu.py. Sta FUORI dal blocco pseudopc: e' un dato che il
; loader legge dall'indirizzo in cui viene assemblato (EPROM o PRG su SD).
!src "boot_menu.inc"
