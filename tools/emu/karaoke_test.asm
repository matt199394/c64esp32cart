!to "kfinal.prg", cbm
fakeio = $02fa
MENU_INIT = $1600
* = $0801
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"
        !byte 0
.basic_end
        !word 0
        jmp run

run
        sei
        lda #$36
        sta $01
        jsr $e544
        lda #14
        sta 646
        ldx #4
        ldy #0
        sec
        jsr $fff0
        ; testo "AAAABBBBCCCC" a $A000 (screen code 1)
        ldx #0
        lda #1
.tl     sta $a000,x
        inx
        cpx #12
        bne .tl
        ; tabella pezzi (offset dispOff, campioni): (0,100) (4,200) (8,300)
        lda #0
        sta $a500
        lda #0
        sta $a501
        lda #100
        sta $a502
        lda #0
        sta $a503
        lda #4
        sta $a504
        lda #0
        sta $a505
        lda #200
        sta $a506
        lda #0
        sta $a507
        lda #8
        sta $a508
        lda #0
        sta $a509
        lda #<300
        sta $a50a
        lda #>300
        sta $a50b
        ; variabili
        lda #12
        sta text_len
        lda #0
        sta text_len+1
        lda #3
        sta num_pieces
        ; stampa + evidenzia pezzo 0
        jsr print_text
        lda #0
        sta cur_piece
        jsr hl_set
        jsr load_cur
        ; verifica pezzo 0: d8a0..d8a3 giallo, d8a4 azzurro
        lda $d8a0
        cmp #7
        bne fail1
        lda $d8a3
        cmp #7
        bne fail2
        lda $d8a4
        cmp #14
        bne fail3
        ; avanza al pezzo 1
        lda #100
        sta samp_cnt
        lda #0
        sta samp_cnt+1
        jsr hl_advance
        ; verifica pezzo 1: d8a0 azzurro, d8a4..d8a7 giallo, d8a8 azzurro
        lda $d8a0
        cmp #14
        bne fail4
        lda $d8a4
        cmp #7
        bne fail5
        lda $d8a7
        cmp #7
        bne fail6
        lda $d8a8
        cmp #14
        bne fail7
        ; OK
        lda #5
        sta $d020
.h      jmp .h
fail1   lda #2
        sta $d020
        jmp .halt
fail2   lda #10
        sta $d020
        jmp .halt
fail3   lda #8
        sta $d020
        jmp .halt
fail4   lda #9
        sta $d020
        jmp .halt
fail5   lda #7
        sta $d020
        jmp .halt
fail6   lda #6
        sta $d020
        jmp .halt
fail7   lda #4
        sta $d020
.halt   jmp .halt

* = $0c00
        jmp sam_talk
        jmp sam_news
!src "sam_player_test.inc"
* = $1600
.h3     jmp .h3
