!to "drv.prg", cbm
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
        lda #0
        sta cnt
        sei
        lda #<nmi
        sta $0318
        lda #>nmi
        sta $0319
        cli
        lda #$73
        jsr sendbyte
        lda #$00
        jsr sendbyte
        lda #$00
        jsr sendbyte
        lda #$a0
        jsr sendbyte
        ldx #0
.l      txa
        clc
        adc #$41
        jsr sendbyte
        inx
        cpx #20
        bcc .l
        jsr $e544
        lda cnt
        jsr hex
        lda #32
        jsr $ffd2
        ldx #0
.fd     lda buf,x
        jsr hex
        inx
        cpx #8
        bcc .fd
        lda #32
        jsr $ffd2
        ldx #20
.f2     lda buf,x
        jsr hex
        inx
        cpx #26
        bcc .f2
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
cnt     !byte 0
nmi
        bit $dd0d
        pha
        tya
        pha
        ldy cnt
        lda fakeio
        sta buf,y
        inc cnt
        pla
        tay
        pla
        rti
sendbyte
        sta fakeio
        pha
        lda #$81
        sta $dd0d
        lda #$02
        sta $dd04
        lda #$00
        sta $dd05
        lda #$19
        sta $dd0e
        ldy #60
.d      dey
        bne .d
        lda #$00
        sta $dd0e
        pla
        rts
fakeio = $02fa
buf = $c000
