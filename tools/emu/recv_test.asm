; ===========================================================================
;  test in VICE: finto ESP32 che manda un blocco (4 byte header + testo) al
;  player SAM con un NMI per byte (CIA2 timer A one-shot), poi stampa il testo
;  con print_news.  $DF00 e' sostituito da una locazione RAM (fakeio).
; ===========================================================================
!to "test9.prg", cbm
fakeio = $02fa
MENU_INIT = $1400
* = $0801
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"
        !byte 0
.basic_end
        !word 0
        jmp run

; --- finto ESP32: A = byte da consegnare, un NMI (CIA2 timer A one-shot) ---
sendbyte
        sta fakeio
        pha
        lda #$81                            ; azzera i flag e abilita timer A
        sta $dd0d
        lda #$02                            ; conta 2 cicli
        sta $dd04
        lda #$00
        sta $dd05
        lda #$19                            ; one-shot + force load + start
        sta $dd0e
        ldy #60
.d      dey
        bne .d                              ; ~300 cicli: NMI servito
        lda #$00
        sta $dd0e                           ; ferma il timer
        pla
        rts

run
        sei
        lda #<nmi_recv
        sta $0318
        lda #>nmi_recv
        sta $0319
        cli
        lda #$36
        sta $01
        lda #0
        sta $a800                           ; marcatore
        lda #1
        sta rxstate
        lda #0
        sta rxidx
        sta rxlen
        sta rxlen+1
        lda #<len                           ; header: lunghezza
        jsr sendbyte
        lda #>len
        jsr sendbyte
        lda #$00                            ; header: indirizzo $a000
        jsr sendbyte
        lda #$a0
        jsr sendbyte
        ldx #0                              ; dati
.l      lda txt,x
        jsr sendbyte
        inx
        cpx #len
        bcc .l
        ; 8 NMI spuri (glitch): NON devono iniziare un nuovo blocco
        lda #$12
        jsr sendbyte
        lda #$34
        jsr sendbyte
        lda #$56
        jsr sendbyte
        lda #$78
        jsr sendbyte
        lda #$9a
        jsr sendbyte
        lda #$bc
        jsr sendbyte
        lda #$de
        jsr sendbyte
        lda #$f0
        jsr sendbyte
        lda $a800                           ; marcatore: deve essere ancora 0
        bne .fail
        lda rxstate
        bne .fail
        lda rxstate                         ; ricezione finita?
        bne .fail
        ldx #0                              ; confronto $a000 con il testo
.c      lda $a000,x
        cmp txt,x
        bne .fail
        inx
        cpx #len
        bcc .c
        lda #$36                            ; come nel menu
        sta $01
        lda #0
        sta $fb
        lda #$a0
        sta $fc
        lda #<len
        sta rxlen
        lda #>len
        sta rxlen+1
        jsr $e544
        jsr print_news
        jmp $1400
.fail   jsr $e544
        lda rxstate
        jsr hex
        lda rxidx
        jsr hex
        lda rxlen
        jsr hex
        lda rxlen+1
        jsr hex
        lda #32
        jsr $ffd2
        ldx #0
.fd     lda $a000,x
        jsr hex
        inx
        cpx #8
        bcc .fd
.h      jmp .h
hex     pha
        lsr
        lsr
        lsr
        lsr
        jsr hd
        pla
        and #$0f
hd      ora #$30
        cmp #$3a
        bcc .p
        clc
        adc #$07
.p      jmp $ffd2

txt     !scr "prova di ricezione del testo delle news. se leggi questa scritta il blocco e arrivato a $a000 e la stampa funziona."
txtend = *
len = txtend - txt
!if len > 255 { !error "testo troppo lungo per il test" }

; --- player (stesso codice del menu, $DF00 -> fakeio) ---
* = $1000
        jmp sam_talk
        jmp sam_news
!src "sam_player_test.inc"

; --- finto "menu" a $0e00 ---
* = $1400
        ldx #0
.m      lda msg,x
        beq .h
        jsr $ffd2
        inx
        bne .m
.h      jmp .h
msg     !pet "menu ok", 0
