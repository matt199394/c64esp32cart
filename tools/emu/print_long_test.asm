fakeio = $02fa
; test: parte dalla stampa del testo news con 1180 byte gia' a $A000
!to "test8.prg", cbm
MENU_INIT = $0c00
* = $0801
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"
        !byte 0
.basic_end
        !word 0
        lda #$36
        sta $01
        jmp entry
        jmp sam_news
!src "sam_player_test.inc"

; finto "menu" a $0c00
* = $0c00
        jsr $e544
        ldx #0
.l      lda msg,x
        beq .h
        jsr $ffd2
        inx
        bne .l
.h      jmp .h
msg     !pet "menu ok", 0

; ingresso del test (testo gia' a $A000, 1180 byte)
* = $0d00
entry   lda #$00
        sta $fb
        lda #$a0
        sta $fc
        lda #<1180
        sta rxlen
        lda #>1180
        sta rxlen+1
        ; NMI audio a ~11 kHz durante la stampa (come sull'hardware)
        sei
        lda #<sam_nmi
        sta $0318
        lda #>sam_nmi
        sta $0319
        lda #$81
        sta $dd0d
        lda #88
        sta $dd04
        lda #0
        sta $dd05
        lda #$11
        sta $dd0e
        cli
        jsr $e544
        jsr print_news
        jmp $0c00

; testo di prova a $A000 (screen code) e contatore gia' impostato
* = $a000
        !scr "riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. riga uno del testo delle news. ri"
        !byte 0
