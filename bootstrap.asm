; ============================================================================
;  C64 ESP32 Cart - bootstrap EPROM 8k
; ----------------------------------------------------------------------------
;  Scopo:
;    - la C64 parte dalla EPROM a $8000 (CBM80);
;    - copia un piccolo loader a $C000;
;    - il loader chiede all'ESP32 di alzare EXROM (GPIO2 -> transistor);
;    - carica "espnewsauto.prg" dalla SD dell'ESP32 a $0801;
;    - salta all'entry point del PRG (stub BASIC SYS ...).
;
;  Build:
;       acme bootstrap.asm          -> c64esp8kBOOT.bin
;
;       acme -DDBGBOOT=1 -o c64esp8kBOOT_dbg.bin bootstrap.asm
;                                   -> build di debug: bordo bianco = EPROM
;                                      partita, poi caratteri/colori
;                                      '2'..'6' indicano fino a dove arriva
;                                      (2=loader a $C000, 3=112, 4=nome,
;                                      5=PRG ricevuto, 6=entry point)
;
;  Il file prodotto e' 8192 byte, da inserire in una 27C512.
;  Firmware ESP32 richiesto: comando 112 = EXROM alto.
;
;  Banco della 27C512 (scheda c64esp32crt1.1):
;    A13/A14/A15 hanno pull-up a +5V (R9/R10/R11) e jumper a GND
;    (J4=JA13, J5=JA14, J6=JA15).
;    Senza jumper -> A13=A14=A15=1 -> banco 7 (offset 0xE000).
;    Con tutti e tre i jumper -> banco 0 (offset 0x0000).
;    Per non dipendere dai jumper: replicare gli 8K in tutti i banchi con
;    "tools/make_eprom_image.sh all <bin>".
; ============================================================================

!to "c64esp8kBOOT.bin", plain

!addr basout = $ffd2
PRG_ENTRY = $0339

!addr ptr1 = $fd            ; puntatore dati / destinazione
!addr ptr2 = $fb            ; lunghezza / puntatore riga

!macro pushreg {
        php
        pha
        txa
        pha
        tya
        pha
}
!macro popreg {
        pla
        tay
        pla
        tax
        pla
        plp
}
!macro inc16 .a {
        inc .a
        bne +
        inc .a + 1
+
}
!macro dec16 .a {
        lda .a
        bne +
        dec .a + 1
+       dec .a
}

; ---------------------------------------------------------------------------
;  Vettori CBM80 a $8000
; ---------------------------------------------------------------------------
        * = $8000
        !word coldstart
        !word warmstart
        !byte $c3,$c2,$cd,$38,$30           ; "CBM80"

coldstart
        sei
        stx $d016
        jsr $fda3                           ; IOINIT
        jsr $fd50                           ; RAMTAS
        jsr $fd15                           ; RESTOR
        jsr $ff5b                           ; CINT
        cli

!ifdef DBGBOOT {
        ; DEBUG: segnale visivo che la EPROM e' mappata e il coldstart gira
        lda #$01
        sta $d020                           ; bordo bianco
        lda #$00
        sta $d021                           ; sfondo nero
}

; ---------------------------------------------------------------------------
;  Copia il loader da EPROM a $C000 e lo avvia
; ---------------------------------------------------------------------------
warmstart
        lda #<payload_rom
        sta ptr1
        lda #>payload_rom
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$c0
        sta ptr2+1

        ldy #0
        ldx #>BOOT_LEN
        beq .tail
.pages  lda (ptr1),y
        sta (ptr2),y
        iny
        bne .pages
        inc ptr1+1
        inc ptr2+1
        dex
        bne .pages

.tail   ldx #<BOOT_LEN
        beq .go
.rest   lda (ptr1),y
        sta (ptr2),y
        iny
        dex
        bne .rest
.go     jmp $c000

; ---------------------------------------------------------------------------
;  Loader: viene eseguito da $C000
; ---------------------------------------------------------------------------
payload_rom = *
!pseudopc $c000 {

start
        sei
        ldx #$ff
        txs
        lda #<nmi
        sta $0318
        lda #>nmi
        sta $0319
        cli

!ifdef DBGBOOT {
        ; DEBUG: loader copiato a $C000 e in esecuzione
        ldx #$03                            ; bordo ciano
        lda #$32                            ; '2'
        jsr dbg
}

        ; 1) EXROM alto: la EPROM a $8000 viene rimossa, resta solo RAM
        lda #112
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long

!ifdef DBGBOOT {
        ldx #$05                            ; bordo verde
        lda #$33                            ; '3': ESP32 ha risposto a 112
        jsr dbg
}

        ; 2) nome del PRG da caricare (screen code -> ASCII nell'ESP32)
        lda #51
        sta OPTION
        jsr send_buffer

!ifdef DBGBOOT {
        ldx #$07                            ; bordo giallo
        lda #$34                            ; '4'
        jsr dbg
}

        ; 3) carica il PRG nella RAM C64
        lda #53
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long

!ifdef DBGBOOT {
        ldx #$04                            ; bordo viola
        lda #$35                            ; '5': PRG ricevuto
        jsr dbg
        lda ptr1                            ; fine dati ricevuti (indirizzo
        sta endptr                          ; successivo all'ultimo byte)
        lda ptr1+1
        sta endptr+1
        jsr cksum16                         ; checksum dei 4154 byte a $0801
}

        ; 4) chiedi l'entry point
        lda #60
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long

!ifdef DBGBOOT {
        ldx #$0d                            ; bordo verde chiaro
        lda #$36                            ; '6'
        jsr dbg

        ; --- dump di debug ---
        jsr dump16                          ; 16 byte ricevuti a $0801
        lda endptr+1                        ; fine dati PRG (hi lo)
        jsr hexout
        lda endptr
        jsr hexout
        lda PRG_ENTRY+1                     ; entry point (hi lo)
        jsr hexout
        lda PRG_ENTRY
        jsr hexout
        lda cksum+1                         ; checksum 16 bit del PRG (hi lo)
        jsr hexout
        lda cksum
        jsr hexout

        ; attesa tasto: il dump resta a schermo per la foto
.waitkey
        jsr $ffe4
        beq .waitkey
}

        lda PRG_ENTRY
        ora PRG_ENTRY+1
        beq .basic

        lda #$37
        sta $01
        lda #0
        sta $cc
        ; NB: NON chiamare CLR ($a659) ne' il relink BASIC ($a533) qui.
        ; Il coldstart da cartuccia inizializza solo il KERNAL (IOINIT/
        ; RAMTAS/RESTOR/CINT): TXTTAB/VARTAB/ARYTAB/STREND possono contenere
        ; spazzatura e CLR/LINKPRG vanno in crash (schermo fermo, il PRG
        ; non parte). Il PRG fa da solo l'init BASIC che gli serve.
        jmp (PRG_ENTRY)

.basic  lda #$37
        sta $01
        jmp $e37b                           ; BASIC warm start

; ---------------------------------------------------------------------------
;  NMI: ricezione dati dall'ESP32 (stesso protocollo del menu)
; ---------------------------------------------------------------------------
nmi
        bit $dd0d
        +pushreg
        lda rxState
        cmp #2
        bne .notdata
        ldy RYINDEX
        ldx ptr2+1
        beq .rem
        lda $df00
        sta (ptr1),y
        iny
        bne .savey
        inc ptr1+1
        dex
        stx ptr2+1
        bne .savey
        lda ptr2
        bne .savey
        jmp .complete
.savey  sty RYINDEX
        jmp .exit
.rem    ldx ptr2
        beq .complete
        lda $df00
        sta (ptr1),y
        iny
        sty RYINDEX
        dex
        stx ptr2
        bne .exit
        beq .complete
.notdata
        cmp #1
        bne .complete
        ldy RYINDEX
        lda $df00
        sta ptr2,y
        iny
        sty RYINDEX
        cpy #4
        bcc .exit
        lda ptr2
        ora ptr2+1
        beq .complete
        lda #2
        sta rxState
        lda #0
        sta RYINDEX
        beq .exit
.complete
        lda #0
        sta rxState
        sta RYINDEX
        lda #1
        sta RXFULL
.exit   +popreg
        rti

; ---------------------------------------------------------------------------
;  Handshake e ricezione
; ---------------------------------------------------------------------------
wait_for_ready_to_receive
        lda $df00
        cmp #128
        bcc wait_for_ready_to_receive
        rts

receive_buffer
        lda #0
        sta RYINDEX
        sta RXFULL
        lda #1
        sta rxState
        jsr wait_for_ready_to_receive
        lda OPTION
        sta $de00
        rts

send_buffer
        lda OPTION
        sta $de00
        ldx #0
.l      jsr wait_for_ready_to_receive
        lda TXBUFFER,x
        sta $de00
        cmp #128
        beq .e
        inx
        cpx #48
        bcc .l
.e      rts

; timeout lungo (~34 s): copre anche il setup WiFi/SD dell'ESP32
wait_rx_long
        lda $a2
        sta wait_t0
        lda #0
        sta wait_wrap
.loop   lda rxState
        beq .ok
        lda $a2
        cmp wait_t0
        bcs .nowrap
        inc wait_wrap
        lda wait_wrap
        cmp #8
        bcs .timeout
.nowrap lda $a2
        sta wait_t0
        jmp .loop
.timeout
        lda #0
        sta rxState
.ok     rts

; ---------------------------------------------------------------------------
;  Variabili e nome file
; ---------------------------------------------------------------------------
OPTION      !byte 0
rxState     !byte 0
RXFULL      !byte 0
RYINDEX     !byte 0
wait_t0     !byte 0
wait_wrap   !byte 0

; "espnewsauto.prg" in screen code (1..26 = a..z), terminatore 128
TXBUFFER
        !byte 5,19,16,14,5,23,19,1,21,20,15,46,16,18,7,128

!ifdef DBGBOOT {
; X = colore bordo, A = carattere PETSCII da stampare a schermo
dbg     pha
        stx $d020
        pla
        jmp $ffd2

; stampa in hex i 16 byte a $0801 (inizio del PRG ricevuto)
; NB: Y non sopravvive a CHROUT, l'indice sta in memoria
dump16  lda #0
        sta dumpidx
.next   ldy dumpidx
        lda $0801,y
        jsr hexout
        lda #$20                            ; spazio
        jsr $ffd2
        inc dumpidx
        lda dumpidx
        cmp #16
        bne .next
        rts
dumpidx !byte 0
endptr  !byte 0,0

; somma a 16 bit dei 4154 byte ricevuti a $0801 (4154 = $103A)
cksum16 lda #$01
        sta $fb
        lda #$08
        sta $fc
        lda #$3a
        sta cscnt
        lda #$10
        sta cscnt+1
        lda #0
        sta cksum
        sta cksum+1
        ldy #0
.cloop  lda ($fb),y
        clc
        adc cksum
        sta cksum
        bcc +
        inc cksum+1
+       lda cscnt
        bne +
        dec cscnt+1
+       dec cscnt
        lda cscnt
        ora cscnt+1
        beq .cdone
        iny
        bne .cloop
        inc $fc
        jmp .cloop
.cdone  rts
cscnt   !byte 0,0
cksum   !byte 0,0

; A = byte -> due cifre esadecimali PETSCII
hexout  pha
        lsr
        lsr
        lsr
        lsr
        jsr hexdig
        pla
        and #$0f
hexdig  ora #$30
        cmp #$3a
        bcc +
        clc
        adc #$07                            ; 'A'..'F'
+       jmp $ffd2
}
}

payload_end = *
BOOT_LEN = payload_end - payload_rom

; riempie fino a 8k
        * = $9fff
        !byte 0
