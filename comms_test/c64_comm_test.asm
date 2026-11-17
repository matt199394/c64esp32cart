; ============================================================================
;  C64 <-> ESP32 - test minimale di comunicazione
; ----------------------------------------------------------------------------
;  Build come PRG BASIC (autostart con RUN):
;       acme c64_comm_test.asm              -> c64_comm_test.prg
;
;  Build caricabile direttamente a $C400 (SYS 50176):
;       acme -DC400=1 c64_comm_test.asm     -> c64_comm_test_c400.prg
;
;  Il test invia 0..255 + una serie di pattern e verifica che l'ESP32
;  risponda con lo stesso byte (echo). Il risultato appare sullo schermo.
; ============================================================================

!ifdef C400 {
    !to "c64_comm_test_c400.prg", cbm
    * = $c400
} else {
    !to "c64_comm_test.prg", cbm
    * = $0801
    !word .basic_end
    !word 2026
    !byte $9e
    !text "2061"
    !byte 0
.basic_end
    !word 0
!if * != 2061 {
    !error "l'indirizzo SYS non coincide con il codice"
}
}

basout = $ffd2
getin  = $ffe4
ptr    = $fd                        ; puntatore temporaneo per print_z

; ---------------------------------------------------------------------------
; Avvio
; ---------------------------------------------------------------------------
!zone main_test
start
        sei
        lda #$37                          ; BASIC, Kernal e I/O
        sta $01

        ; IRQ standard del Kernal: se la cartuccia aveva installato
        ; un IRQ che punta a codice sovrascritto, non deve essere usato.
        lda #$31
        sta $0314
        lda #$ea
        sta $0315

        ; niente IRQ raster
        lda #0
        sta $d01a
        sta $d019

        ; NMI del test
        lda #<nmi_handler
        sta $0318
        lda #>nmi_handler
        sta $0319
        cli

        lda #23
        sta $d018                          ; minuscolo
        lda #1
        sta 646

.main
        jsr $e544
        ldx #<msg_title
        ldy #>msg_title
        jsr print_z

        ; aspetta che l'ESP32 sia pronta (D7 alto)
        jsr wait_ready
        bcc .esp_ok
        ldx #<msg_noesp
        ldy #>msg_noesp
        jsr print_z
        jmp .wait_key

.esp_ok
        jsr run_tests
        jsr print_results

.wait_key
        ldx #<msg_key
        ldy #>msg_key
        jsr print_z
.wait   jsr getin
        beq .wait
        cmp #'q'
        beq .exit
        jmp .main

.exit
        rts

; ---------------------------------------------------------------------------
; Test: 0..255, poi pattern 55/AA/00/FF
; ---------------------------------------------------------------------------
!zone tests
run_tests
        lda #0
        sta tx_count
        sta tx_count+1
        sta err_count
        sta err_count+1
        sta last_tx
        sta last_rx
        sta err_type
        sta err_mask

        ; Ping rapido: se il C64 non riceve nemmeno un echo, evita di
        ; aspettare centinaia di timeout per l'intero test.
        lda #$a5
        jsr send_check

        ; Se l'echo arriva ma il byte e' sbagliato (tipo 3) continua con
        ; il test completo: serve a misurare quali bit sono errati.
        lda err_type
        cmp #3
        beq .continue
        lda err_count
        ora err_count+1
        bne .done

.continue
        ; Statistiche azzerate: il ping non fa parte del test completo.
        lda #0
        sta tx_count
        sta tx_count+1
        sta err_count
        sta err_count+1
        sta last_tx
        sta last_rx
        sta err_type
        sta err_mask

        ldx #0
.loop_all
        txa
        jsr send_check
        inx
        bne .loop_all

        ldx #32
.loop_55
        lda #$55
        jsr send_check
        dex
        bne .loop_55

        ldx #32
.loop_aa
        lda #$aa
        jsr send_check
        dex
        bne .loop_aa

        ldx #8
.loop_00
        lda #0
        jsr send_check
        dex
        bne .loop_00

        ldx #8
.loop_ff
        lda #$ff
        jsr send_check
        dex
        bne .loop_ff

.done
        rts

; ---------------------------------------------------------------------------
; Invia A e confronta l'echo. Preserva X/Y.
; ---------------------------------------------------------------------------
!zone sendcheck
send_check
        sta last_tx

        inc tx_count
        bne .tx_hi
        inc tx_count+1
.tx_hi
        lda #0
        sta rx_flag

        jsr wait_ready
        bcs .ready_timeout

        lda last_tx
        sta $de00

        jsr wait_echo
        bcs .echo_timeout

        lda rx_data
        sta last_rx
        cmp last_tx
        bne .mismatch
        rts

.ready_timeout
        lda #1
        sta err_type
        jmp .error
.echo_timeout
        lda #2
        sta err_type
        jmp .error
.mismatch
        lda #3
        sta err_type
        lda last_tx
        eor rx_data
        ora err_mask
        sta err_mask
        jmp .error
.error
        inc err_count
        bne .err_hi
        inc err_count+1
.err_hi
        rts

; ---------------------------------------------------------------------------
; Aspetta D7=1 su $DF00 (ESP32 pronta). C=1 se timeout.
; ---------------------------------------------------------------------------
!zone waitready
wait_ready
        lda $a2
        sta t0
.loop
        lda $df00
        bmi .ok
        lda $a2
        sec
        sbc t0
        cmp #30                           ; circa 0,5 secondi
        bcc .loop
        sec
        rts
.ok
        clc
        rts

; ---------------------------------------------------------------------------
; Aspetta l'echo via NMI. C=1 se timeout.
; ---------------------------------------------------------------------------
!zone waitecho
wait_echo
        lda $a2
        sta t0
.loop
        lda rx_flag
        bne .ok
        lda $a2
        sec
        sbc t0
        cmp #5                            ; circa 80 ms
        bcc .loop
        sec
        rts
.ok
        lda #0
        sta rx_flag
        clc
        rts

; ---------------------------------------------------------------------------
; NMI: legge il byte da $DF00 (questa lettura fa da acknowledge all'ESP32)
; ---------------------------------------------------------------------------
nmi_handler
        bit $dd0d
        pha
        lda $df00
        sta rx_data
        lda #1
        sta rx_flag
        pla
        rti

; ---------------------------------------------------------------------------
; Stampa risultati
; ---------------------------------------------------------------------------
!zone results
print_results
        ldx #<msg_done
        ldy #>msg_done
        jsr print_z

        ldx #<msg_tx
        ldy #>msg_tx
        jsr print_z
        lda tx_count
        sta num
        lda tx_count+1
        sta num+1
        jsr print_dec

        ldx #<msg_err
        ldy #>msg_err
        jsr print_z
        lda err_count
        sta num
        lda err_count+1
        sta num+1
        jsr print_dec

        ldx #<msg_last
        ldy #>msg_last
        jsr print_z
        lda last_tx
        jsr print_hex

        ldx #<msg_rx
        ldy #>msg_rx
        jsr print_z
        lda last_rx
        jsr print_hex

        ldx #<msg_type
        ldy #>msg_type
        jsr print_z
        lda err_type
        sta num
        lda #0
        sta num+1
        jsr print_dec

        ldx #<msg_mask
        ldy #>msg_mask
        jsr print_z
        lda err_mask
        jsr print_hex

        lda err_count
        ora err_count+1
        bne .bad
        ldx #<msg_ok
        ldy #>msg_ok
        jsr print_z
        rts
.bad
        ldx #<msg_bad
        ldy #>msg_bad
        jsr print_z
        rts

; ---------------------------------------------------------------------------
; Stampa stringa PETSCII terminata da 0. X/Y = indirizzo.
; ---------------------------------------------------------------------------
!zone printz
print_z
        stx ptr
        sty ptr+1
.loop
        ldy #0
        lda (ptr),y
        beq .done
        jsr basout
        inc ptr
        bne .loop
        inc ptr+1
        jmp .loop
.done
        rts

; ---------------------------------------------------------------------------
; A = byte -> due cifre esadecimali
; ---------------------------------------------------------------------------
!zone printhex
print_hex
        pha
        lsr
        lsr
        lsr
        lsr
        jsr print_nibble
        pla
        and #$0f
print_nibble
        cmp #10
        bcc .digit
        adc #6
.digit
        adc #48
        jsr basout
        rts

; ---------------------------------------------------------------------------
; num (16 bit) -> decimale
; ---------------------------------------------------------------------------
!zone printdec
print_dec
        lda num
        ora num+1
        bne .nonzero
        lda #'0'
        jsr basout
        rts

.nonzero
        ldx #0
.divide
        jsr div10
        pha
        inx
        lda num
        ora num+1
        bne .divide
.print
        pla
        clc
        adc #'0'
        jsr basout
        dex
        bne .print
        rts

; num = num / 10, A = resto (Y modificato)
div10
        lda #0
        ldy #16
.bit
        asl num
        rol num+1
        rol
        cmp #10
        bcc .skip
        sbc #10
        inc num
.skip
        dey
        bne .bit
        rts

; ---------------------------------------------------------------------------
; Variabili
; ---------------------------------------------------------------------------
num       !word 0
t0        !byte 0
rx_flag   !byte 0
rx_data   !byte 0
last_tx   !byte 0
last_rx   !byte 0
err_type  !byte 0
err_mask  !byte 0
tx_count  !word 0
err_count !word 0

; ---------------------------------------------------------------------------
; Testi
; ---------------------------------------------------------------------------
msg_title !pet "c64 <-> esp32 comm test", 13, 13, 0
msg_noesp !pet "esp32 non pronto (timeout)", 13, 0
msg_done  !pet "test completato", 13, 0
msg_tx    !pet "tx: ", 0
msg_err   !pet "  errori: ", 0
msg_last  !pet 13, "ultimo tx: $", 0
msg_rx    !pet "  rx: $", 0
msg_type  !pet "  tipo: ", 0
msg_mask  !pet 13, "bit errati: $", 0
msg_ok    !pet 13, "comunicazione ok", 13, 0
msg_bad   !pet 13, "errori di comunicazione!", 13, 0
msg_key   !pet 13, "premi un tasto: ripeti, q = esci", 13, 0
