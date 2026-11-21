; RAMTAS tocca $CAxx / $9800 / $A000 ?  Codice a $C000, esito a video
!to "ramtest5.prg", cbm
* = $0801
        !word .be
        !word 2026
        !byte $9e
        !text "49152"
        !byte 0
.be     !word 0
        !fill $c000 - *, 0
* = $c000
        sei
        ldx #$ff
        txs
        lda #$37
        sta $01
        lda #$ab
        sta $ca00
        sta $ca7f
        sta $9800
        sta $a000
        sta $c400
        stx $d020
        jsr $ff87                   ; RAMTAS (quella di main_init)
        jsr $ff81                   ; CINT
        jsr $e453                   ; init vettori BASIC
        jsr $e3bf                   ; init RAM BASIC (come main_init)
        ldx #0
        lda $ca00
        jsr rep
        lda $ca7f
        jsr rep
        lda $9800
        jsr rep
        lda $a000
        jsr rep
        lda $c400
        jsr rep
.h      jmp .h
rep     ldx idx
        cmp #$ab
        bne .z
        lda #$31
        jmp .p
.z      lda #$30
.p      sta $0400,x
        lda #1
        sta $d800,x
        inc idx
        rts
idx     !byte 0
