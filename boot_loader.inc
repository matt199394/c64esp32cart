; ===========================================================================
;  boot_loader.inc - loader del bootstrap (assemblato a $C000)
;  Usato da bootstrap.asm (EPROM 8K) e da bootstrap_sd.asm (prova da SD2IEC).
;  Va incluso dentro:  !pseudopc $c000 { !src "boot_loader.inc" }
; ===========================================================================


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

        ; 0) menu di scelta: 1 = menu normale, 2 = versione SAM
        jsr menu_boot

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

        ; 1b) variante scelta: comando 113 = menu in versione SAM
        lda variant
        beq .noflag
        lda #113
        sta $de00
        jsr short_delay                     ; l'ESP32 deve leggere 113 prima
.noflag                                    ; del comando 51 che arriva subito dopo

        ; 2) nome del PRG da caricare (screen code -> ASCII nell'ESP32)
        lda #<name_normal
        sta ptr2
        lda #>name_normal
        sta ptr2+1
        lda variant
        beq .nameok
        lda #<name_sam
        sta ptr2
        lda #>name_sam
        sta ptr2+1
.nameok lda #51
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
.mbwaitkey
        jsr $ffe4
        beq .mbwaitkey
}

        ; 5) il player SAM NON si carica qui: e' un PRG a $0801 che il menu
        ;    carica all'occorrenza (samplayer.prg). Vedi c64esp32cart.asm.

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

; pausa breve (~80 ms): serve tra due comandi scritti di seguito ($de00),
; altrimenti il secondo puo' sovrascrivere il primo prima che l'ESP32 lo legga
short_delay
        ldx #$40
.sd1    ldy #$00
.sd2    dey
        bne .sd2
        dex
        bne .sd1
        rts

; invia la stringa puntata da ptr2 (screen code, terminatore 128)
send_buffer
        lda OPTION
        sta $de00
        ldy #0
.l      jsr wait_for_ready_to_receive
        lda (ptr2),y
        sta $de00
        cmp #128
        beq .e
        iny
        cpy #48
        bcc .l
.e      rts

; timeout lungo (~34 s): copre anche il setup WiFi/SD dell'ESP32
wait_rx_long
        lda $a2
        sta wait_t0
        lda #0
        sta wait_wrap
.mploop   lda rxState
        beq .ok
        lda $a2
        cmp wait_t0
        bcs .nowrap
        inc wait_wrap
        lda wait_wrap
        cmp #8
        bcs .mbtimeout
.nowrap lda $a2
        sta wait_t0
        jmp .mploop
.mbtimeout
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

variant     !byte 0
menu_t0     !byte 0

; nomi dei PRG in screen code (1..26 = a..z, 46 = '.'), terminatore 128
name_normal !byte 5,19,16,14,5,23,19,1,21,20,15,46,16,18,7,128   ; espnewsauto.prg
name_sam    !byte 5,19,16,19,1,13,1,21,20,15,46,16,18,7,128      ; espsamauto.prg

; ---------------------------------------------------------------------------
;  Menu di avvio: 1 = menu normale, 2 = SAM
;  (gira a $C000 con la EPROM ancora mappata: non usa $8000-$9FFF)
;  La schermata (cornice, titolo giallo, sottotitolo grigio) e' un template
;  40x25 generato da tools/make_boot_menu.py (boot_menu.inc, 1024+1024 byte)
;  e incluso nella EPROM: qui si copia in $0400 e nella color RAM $D800.
; ---------------------------------------------------------------------------
menu_boot
        lda #$00
        sta $d021                           ; sfondo nero
        lda #$0e
        sta $d020                           ; bordo azzurro
        lda #1
        sta $cc                             ; cursore non lampeggiante
        jsr copy_menu_screen
        jsr copy_menu_colors

        lda #0
        sta variant
.wait   jsr $ffe4                           ; GETIN: aspetta 1 o 2 (niente timeout)
        beq .wait
        cmp #$31                            ; '1'
        beq .show
        cmp #$32                            ; '2'
        bne .wait
        lda #1
        sta variant
.show   lda #1                              ; colore del testo (CHROUT scrive
        sta $0286                           ; anche la color RAM): bianco
        ldx #15                             ; PLOT: riga 15, colonna 6
        ldy #6
        jsr $e50c
        lda variant                         ; conferma a video di cosa si carica
        beq .norm
        ldx #<msg_load_sam
        ldy #>msg_load_sam
        jmp mprint
.norm   ldx #<msg_load_norm
        ldy #>msg_load_norm
        jmp mprint

; copia X pagine da (ptr1) a (ptr2)
copy_page
        ldy #0
.pg     lda (ptr1),y
        sta (ptr2),y
        iny
        bne .pg
        inc ptr1+1
        inc ptr2+1
        dex
        bne copy_page
        rts

; template del menu: 1024 byte (4 pagine) dall'EPROM alla screen RAM
copy_menu_screen
        lda #<boot_menu_screen
        sta ptr1
        lda #>boot_menu_screen
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$04
        sta ptr2+1
        ldx #4
        jmp copy_page

; colori del menu: 1024 byte dall'EPROM alla color RAM $D800
copy_menu_colors
        lda #<boot_menu_colors
        sta ptr1
        lda #>boot_menu_colors
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$d8
        sta ptr2+1
        ldx #4
        jmp copy_page

; stampa la stringa puntata da X/Y (PETSCII, terminatore 0)
mprint  stx ptr1
        sty ptr1+1
.loop   ldy #0
        lda (ptr1),y
        beq .done
        jsr $ffd2
        inc ptr1
        bne .loop
        inc ptr1+1
        jmp .loop
.done   rts

msg_load_norm !pet "loading espnewsauto.prg", 13, 0
msg_load_sam  !pet "loading espsamauto.prg", 13, 0

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
