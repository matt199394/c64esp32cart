!to "tbltest.prg", cbm
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
        ; riempi la tabella a $B800: entry k -> samples = k (offset a 256, 256+k)
        ldx #0
        lda #0
        sta $fb
        lda #$b8
        sta $fc
        lda #0
        sta $fd          ; dispOff lo
        sta $fe          ; dispOff hi
.tbl    ldy #0
        lda $fd
        sta ($fb),y
        iny
        lda $fe
        sta ($fb),y
        iny
        txa              ; samples lo = k
        sta ($fb),y
        iny
        lda #0
        sta ($fb),y
        ; avanza ptr di 4
        clc
        lda $fb
        adc #4
        sta $fb
        lda $fc
        adc #0
        sta $fc
        ; dispOff += 256
        inc $fe
        inx
        cpx #0           ; 256 voci (0..255)
        bne .tbl
        ; cur_piece = 100 -> load_cur -> piece_lo deve valere 100
        lda #100
        sta cur_piece
        jsr load_cur
        lda piece_lo
        cmp #100
        bne fail1
        lda piece_hi
        bne fail2
        ; cur_piece = 255 -> piece_lo = 255
        lda #255
        sta cur_piece
        jsr load_cur
        lda piece_lo
        cmp #255
        bne fail3
        ; OK verde
        lda #5
        sta $d020
.halt   jmp .halt
fail1   lda #2
        sta $d020
        jmp .halt
fail2   lda #10
        sta $d020
        jmp .halt
fail3   lda #8
        sta $d020
.halt2  jmp .halt

* = $0c00
!src "sam_player_test.inc"
* = $1600
        jmp *
