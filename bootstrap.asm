; ============================================================================
;  C64 ESP32 Cart - bootstrap EPROM 8k
; ----------------------------------------------------------------------------
;  Scopo:
;    - la C64 parte dalla EPROM a $8000 (CBM80);
;    - copia un piccolo loader a $C000;
;    - il loader chiede all'ESP32 di alzare EXROM (GPIO2 -> transistor);
;    - carica "espnewsauto.prg" dalla SD dell'ESP32 a $0801;
;    - salta all'entry point del PRG (stub BASIC SYS ...).
;
;  Build:
;       acme bootstrap.asm          -> c64esp8kBOOT.bin
;
;       acme -DDBGBOOT=1 -o c64esp8kBOOT_dbg.bin bootstrap.asm
;                                   -> build di debug: bordo bianco = EPROM
;                                      partita, poi caratteri/colori
;                                      '2'..'6' indicano fino a dove arriva
;                                      (2=loader a $C000, 3=112, 4=nome,
;                                      5=PRG ricevuto, 6=entry point)
;
;  Il file prodotto e' 8192 byte, da inserire in una 27C512.
;  Firmware ESP32 richiesto: comando 112 = EXROM alto.
;
;  Banco della 27C512 (scheda c64esp32crt1.1):
;    A13/A14/A15 hanno pull-up a +5V (R9/R10/R11) e jumper a GND
;    (J4=JA13, J5=JA14, J6=JA15).
;    Senza jumper -> A13=A14=A15=1 -> banco 7 (offset 0xE000).
;    Con tutti e tre i jumper -> banco 0 (offset 0x0000).
;    Per non dipendere dai jumper: replicare gli 8K in tutti i banchi con
;    "tools/make_eprom_image.sh all <bin>".
; ============================================================================

!to "c64esp8kBOOT.bin", plain

!addr basout = $ffd2
PRG_ENTRY = $0339

!addr ptr1 = $fd            ; puntatore dati / destinazione
!addr ptr2 = $fb            ; lunghezza / puntatore riga

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
!macro inc16 .a {
        inc .a
        bne +
        inc .a + 1
+
}
!macro dec16 .a {
        lda .a
        bne +
        dec .a + 1
+       dec .a
}

; ---------------------------------------------------------------------------
;  Vettori CBM80 a $8000
; ---------------------------------------------------------------------------
        * = $8000
        !word coldstart
        !word warmstart
        !byte $c3,$c2,$cd,$38,$30           ; "CBM80"

coldstart
        sei
        stx $d016
        jsr $fda3                           ; IOINIT
        jsr $fd50                           ; RAMTAS
        jsr $fd15                           ; RESTOR
        jsr $ff5b                           ; CINT
        cli

!ifdef DBGBOOT {
        ; DEBUG: segnale visivo che la EPROM e' mappata e il coldstart gira
        lda #$01
        sta $d020                           ; bordo bianco
        lda #$00
        sta $d021                           ; sfondo nero
}

; ---------------------------------------------------------------------------
;  Copia il loader da EPROM a $C000 e lo avvia
; ---------------------------------------------------------------------------
warmstart
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

; ---------------------------------------------------------------------------
;  Loader: viene eseguito da $C000
; ---------------------------------------------------------------------------
payload_rom = *
!pseudopc $c000 {
!src "boot_loader.inc"
}

payload_end = *
BOOT_LEN = payload_end - payload_rom

; riempie fino a 8k
        * = $9fff
        !byte 0
