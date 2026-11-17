; ============================================================================
;  C64 <-> ESP32 - test audio 4 bit / 11 kHz
; ----------------------------------------------------------------------------
;  Build:
;       acme c64_audio_test.asm
;       LOAD"c64_audio_test.prg",8
;       RUN
;
;  L'ESP32 deve avere il firmware audio_test.
; ============================================================================

!to "c64_audio_test.prg", cbm

basout = $ffd2
getin  = $ffe4
ptr    = $fb

* = $0801
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"
        !byte 0
.basic_end
        !word 0
!if * != 2061 {
        !error "SYS address mismatch"
}

start
        sei
        lda #$37
        sta $01

        ; salva il vettore NMI originale
        lda $0318
        sta old_nmi
        lda $0319
        sta old_nmi+1

        ; nuovo vettore NMI audio
        lda #<audio_nmi
        sta $0318
        lda #>audio_nmi
        sta $0319
        cli

        lda #23
        sta $d018
        lda #1
        sta 646
        lda #0
        sta $d020
        lda #0
        sta $d021
        jsr $e544

        ldx #<msg_title
        ldy #>msg_title
        jsr print_z

        ; avvia audio
        lda #70
        sta $de00

.wait   jsr getin
        beq .wait
        cmp #32
        bne .wait

        ; stop audio
        lda #71
        sta $de00
        lda #0
        sta $d418

        sei
        lda old_nmi
        sta $0318
        lda old_nmi+1
        sta $0319
        cli
        rts

; ---------------------------------------------------------------------------
; NMI audio: legge $DF00, prende il nibble alto, scrive il volume SID
; ---------------------------------------------------------------------------
audio_nmi
        bit $dd0d
        pha
        lda $df00
        lsr
        lsr
        lsr
        lsr
        sta $d418
        pla
        rti

; ---------------------------------------------------------------------------
print_z
        stx ptr
        sty ptr+1
.loop   ldy #0
        lda (ptr),y
        beq .done
        jsr basout
        inc ptr
        bne .loop
        inc ptr+1
        jmp .loop
.done   rts

old_nmi !word 0

msg_title
        !pet "c64 <-> esp32 audio test", 13, 13
        !pet "tono 440 hz - 11 khz - 4 bit", 13
        !pet "volume sid da $d418", 13, 13
        !pet "premi spazio per fermare", 13, 0
