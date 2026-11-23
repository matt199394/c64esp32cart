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

; NB: le letture della color RAM vanno mascherate con AND #$0f: il nibble
; alto non e' definito (sia su VICE sia sull'hardware vero).

run
        sei
        lda #$36
        sta $01
        jsr $e544
        lda #14
        sta 646
        ; testo "AAAABBBBCCCC" a $A000 (screen code 1)
        ldx #0
        lda #1
.tl     sta $a000,x
        inx
        cpx #12
        bne .tl
        ; tabella pezzi (offset dispOff, campioni): (0,100) (4,200) (8,300)
        ; a $B800 (nuovo indirizzo della tabella)
        lda #0
        sta $b800
        sta $b801
        lda #100
        sta $b802
        lda #0
        sta $b803
        lda #4
        sta $b804
        lda #0
        sta $b805
        lda #200
        sta $b806
        lda #0
        sta $b807
        lda #8
        sta $b808
        lda #0
        sta $b809
        lda #<300
        sta $b80a
        lda #>300
        sta $b80b
        ; variabili
        lda #12
        sta text_len
        lda #0
        sta text_len+1
        lda #3
        sta num_pieces
        lda #0
        sta cur_piece
        sta scroll_line
        sta scroll_off
        sta scroll_off+1
        ; disegna la finestra ed evidenzia il pezzo 0
        jsr hl_setup
        jsr render_window
        jsr load_cur
        ; verifica pezzo 0: d878..d87b giallo, d87c azzurro
        lda $d878
        and #$0f
        cmp #7
        bne fail1
        lda $d87b
        and #$0f
        cmp #7
        bne fail2
        lda $d87c
        and #$0f
        cmp #14
        bne fail3
        ; avanza al pezzo 1
        lda #100
        sta samp_cnt
        lda #0
        sta samp_cnt+1
        jsr hl_advance
        ; verifica pezzo 1: d878 azzurro, d87c..d87f giallo, d880 azzurro
        lda $d878
        and #$0f
        cmp #14
        bne fail4
        lda $d87c
        and #$0f
        cmp #7
        bne fail5
        lda $d87f
        and #$0f
        cmp #7
        bne fail6
        lda $d880
        and #$0f
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
