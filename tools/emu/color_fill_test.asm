!to "cftest.prg", cbm
ptr2 = $f9
hl_lo = $104b
hl_hi = $104c
hl_len = $104d
* = $0801
        !word nextln
        !word 2026
        !byte $9e
        !text "2061"
        !byte 0
nextln  !word 0
        sei
        lda #$36
        sta $01
        ; setup: hl_lo=$a0, hl_hi=$d8, hl_len=4, colore=7
        lda #$a0
        sta hl_lo
        lda #$d8
        sta hl_hi
        lda #4
        sta hl_len
        lda #7
        jsr color_fill
        ; verifica $d8a0..$d8a3 = 7, $d8a4 = 0 (nero, non toccato)
        lda $d8a0
        cmp #7
        bne fail
        lda $d8a3
        cmp #7
        bne fail
        lda #5
        sta $d020
ok      jmp ok
fail    lda #2
        sta $d020
.h      jmp .h

color_fill
        tax
        lda hl_lo
        sta ptr2
        lda hl_hi
        sta ptr2+1
        ldy #0
.cf     cpy hl_len
        beq .cdone
        txa
        sta (ptr2),y
        iny
        bne .cf
.cdone  rts
