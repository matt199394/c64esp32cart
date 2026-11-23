!to "scroll.prg", cbm
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
        jsr $e544                       ; schermo pulito
        lda #14
        sta 646

        ; testo: 896 caratteri (screen code 1) a $A000..$A37F
        ldx #0
        lda #1
.tl1    sta $a000,x
        inx
        bne .tl1
.tl2    sta $a100,x
        inx
        bne .tl2
.tl3    sta $a200,x
        inx
        bne .tl3
.tl4    sta $a300,x
        inx
        cpx #128                        ; 768 + 128 = 896
        bne .tl4

        ; tabella pezzi a $B800: 56 pezzi, offset = k*16, campioni = 100
        lda #<$b800
        sta $fb
        lda #>$b800
        sta $fc
        lda #0
        sta $fd                         ; offset (lo)
        sta $fe                         ; offset (hi)
        ldx #0
.tbl    ldy #0
        lda $fd
        sta ($fb),y                     ; dispOff lo
        iny
        lda $fe
        sta ($fb),y                     ; dispOff hi
        iny
        lda #100
        sta ($fb),y                     ; samples lo
        iny
        lda #0
        sta ($fb),y                     ; samples hi
        clc
        lda $fb
        adc #4
        sta $fb
        lda $fc
        adc #0
        sta $fc
        clc
        lda $fd
        adc #16
        sta $fd
        lda $fe
        adc #0
        sta $fe
        inx
        cpx #56
        bne .tbl

        ; variabili
        lda #<896
        sta text_len
        lda #>896
        sta text_len+1
        lda #56
        sta num_pieces
        lda #0
        sta cur_piece
        sta scroll_line
        sta scroll_off
        sta scroll_off+1

        ; pezzo 0
        jsr hl_setup
        jsr render_window
        jsr load_cur
        lda scroll_line
        bne fail1

        ; avanza 52 volte -> cur_piece = 52 (offset 832, riga 20)
        ldx #52
.adv    jsr hl_advance
        dex
        bne .adv
        lda scroll_line
        bne fail2                       ; ancora in prima finestra

        ; pezzo 53 (offset 848, riga 21): deve scorrere di una riga
        jsr hl_advance
        lda scroll_line
        cmp #1
        bne fail3
        lda scroll_off
        cmp #40
        bne fail4
        lda scroll_off+1
        bne fail5

        ; evidenza del pezzo 53: color RAM = col_base + (848 - 40) = $DBA0
        lda $dba0
        and #$0f                        ; il nibble alto della color RAM
        cmp #7                          ; non e' definito: mascheralo sempre
        bne fail6
        lda $dbaf                       ; fine del pezzo (16 char)
        and #$0f
        cmp #7
        bne fail7
        lda $db9f                       ; prima del pezzo: azzurro
        and #$0f
        cmp #14
        bne fail8

        lda #5                          ; verde = OK
        sta $d020
.halt   jmp .halt

fail1   lda #2
        jmp .done
fail2   lda #10
        jmp .done
fail3   lda #8
        jmp .done
fail4   lda #9
        jmp .done
fail5   lda #7
        jmp .done
fail6   lda #6
        jmp .done
fail7   lda #4
        jmp .done
fail8   lda #3
.done   sta $d020
        jmp .halt

* = $0c00
!src "sam_player_test.inc"
* = $1600
        jmp *
