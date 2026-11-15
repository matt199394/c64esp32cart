; ============================================================================
;  C64 ESP32 Cart - menu 8k (revisione)       originale: M. Angelini
; ----------------------------------------------------------------------------
;  Cartuccia (ROM 8k, da mettere in un file 64k per la 27C512):
;       acme -DCART=1 c64esp32cart.asm            -> c64esp8kROM.bin
;
;  PRG senza cartuccia (VICE o C64 reale). L'ESP32 e' SIMULATO con dati finti
;  (directory, meteo/ora, un PRG BASIC, un SID di prova):
;       acme c64esp32cart.asm                     -> c64esp32menu.prg
;       x64sc c64esp32menu.prg
;
;  PRG caricato direttamente a $c000 (avvio manuale con SYS 49152).
;  Il flag -DC400 e' storico: il codice ora parte da $c000 per avere spazio.
;  Con -DCART=1 usa l'ESP32 reale della cartuccia (NMI, $df00, ecc.):
;       acme -DCART=1 -DC400=1 c64esp32cart.asm   -> c64esp32menu_c000_cart.prg
;       LOAD"c64esp32menu_c000_cart.prg",8,1   poi   SYS 49152
;  Senza -DCART resta la versione simulata:
;       acme -DC400=1 c64esp32cart.asm            -> c64esp32menu_c000.prg
;       LOAD"c64esp32menu_c000.prg",8,1   poi   SYS 49152
;
;  Solo cartuccia, opzionale: -DFAST_BOOT=1 toglie i test RAM del boot
;  (provare su hardware reale: se non parte, ricompilare senza).
; ============================================================================

!ifdef CART {
    !ifdef C400 {
        !to "c64esp32menu_c000_cart.prg", cbm
    } else {
        !ifdef PRGAUTO {
            !to "espnewsauto.prg", cbm
        } else {
            !to "c64esp8kROM.bin", plain
        }
    }
} else {
    !ifdef C400 {
        !to "c64esp32menu_c000.prg", cbm
    } else {
        !to "c64esp32menu.prg", cbm
    }
}

; ---------------------------------------------------------------------------
;  Kernal, zero page, costanti
; ---------------------------------------------------------------------------
!addr basout = $ffd2
!addr getin  = $ffe4
PRG_ENTRY = $0339

!addr ptr1 = $fd            ; puntatore dati (destinazione ricezione / lettura)
!addr ptr2 = $fb            ; dimensione in ricezione / puntatore riga schermo

!addr cia1 = $dc00
screenRam       = $0400
cassette_buffer = $033c

CODE_BASE = $c000           ; il codice gira in RAM da qui (max fino a $cfff)

; Koala: bitmap 8000 byte, poi matrice video 1000, colori 1000, sfondo 1
PICTURE    = $6000
VIDEO      = PICTURE + $1f40
COLOR      = PICTURE + $2328        ; (nel sorgente originale era $2327: errore)
BACKGROUND = PICTURE + $2710

SCROLL_AT_LINE = 23
SCREEN = screenRam + SCROLL_AT_LINE * 40
SCREEN_COLOR = $d800 + SCROLL_AT_LINE * 40
SPEED  = 1

; Blocco unico NEWS: testo a $8800, grafica PETSCII a $9000/$93E8
ART_SOURCE = $9000
ART_COLOR  = ART_SOURCE + $3E8

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
;  Avvio: cartuccia (autostart CBM80) oppure PRG con SYS
; ---------------------------------------------------------------------------
!ifdef C400 {
        * = CODE_BASE                       ; PRG gia' a $c000: SYS 49152
} else {
    !ifdef PRGAUTO {
        * = $0801                           ; PRG normale: LOAD + RUN
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"                        ; SYS 2061
        !byte 0
.basic_end
        !word 0
!if * != 2061 {
        !error "l'indirizzo SYS non coincide con il codice"
}
    } else {
    !ifdef CART {
        * = $8000
        !word coldstart
        !word warmstart
        !byte $c3,$c2,$cd,$38,$30           ; "CBM80"

coldstart
        sei
        stx $d016
        jsr $fda3                           ; IOINIT
!ifndef FAST_BOOT {
        jsr $fd50                           ; RAMTAS (con test RAM, lento)
}
        jsr $fd15                           ; RESTOR
        jsr $ff5b                           ; CINT
        cli
    } else {
        * = $0801
        !word .basic_end
        !word 2026
        !byte $9e
        !text "2061"                        ; SYS 2061
        !byte 0
.basic_end
        !word 0
!if * != 2061 {
        !error "l'indirizzo SYS non coincide con il codice"
}
    }
    }
}

!ifndef C400 {
; copia il blocco da ROM/PRG a CODE_BASE (lunghezza calcolata, non piu' fissa)
warmstart
        lda #<payload_rom
        sta ptr1
        lda #>payload_rom
        sta ptr1+1
        lda #<CODE_BASE
        sta ptr2
        lda #>CODE_BASE
        sta ptr2+1
        ldy #0
        ldx #>CODE_LEN
        beq .tail
.pages  lda (ptr1),y
        sta (ptr2),y
        iny
        bne .pages
        inc ptr1+1
        inc ptr2+1
        dex
        bne .pages
.tail   ldx #<CODE_LEN
        beq .go
.rest   lda (ptr1),y
        sta (ptr2),y
        iny
        dex
        bne .rest
.go     jmp CODE_BASE
}

payload_rom = *
!pseudopc CODE_BASE {

; ---------------------------------------------------------------------------
;  Inizializzazione
; ---------------------------------------------------------------------------
!zone main_init
main_init
        sei
        cld
        ldx #$ff
        txs
!ifdef CART {
        jsr $ff84                           ; IOINIT
        ldx #$17                            ; azzera i registri SID
        lda #$00
.sid    sta $d400,x
        dex
        bpl .sid
!ifndef FAST_BOOT {
        jsr $ff87                           ; RAMTAS con test RAM (nel sorgente
}                                           ; originale e' attiva, nonostante il commento)
        lda #$00                            ; RAMTAS senza test RAM
        tay
.ram    sta $0002,y
        sta $0200,y
        sta $0300,y
        iny
        bne .ram
        ldx #$00
        ldy #$a0
        jsr $fd8c
        jsr $ff8a                           ; RESTOR
        jsr $ff81                           ; CINT
        cli
        jsr $e453                           ; init vettori BASIC
        jsr $e3bf                           ; init RAM BASIC
        ldx #$fb
        txs
        sei
        lda #<nmi                           ; vettore NMI: ricezione dall'ESP32
        sta $0318
        lda #>nmi
        sta $0319
        cli
} else {
        ldx #$fb
        txs
        cli
}

; ---------------------------------------------------------------------------
;  Menu principale
; ---------------------------------------------------------------------------
!zone restart
restart
        jsr set_default_irq
        lda #$37
        sta $01                             ; BASIC/Kernal/I-O sempre attivi nel menu
        lda #1
        sta $cc                             ; cursore KERNAL spento nel menu
        lda #0
        sta cassette_buffer                 ; 0 = ultimo file ricevuto non e' un SID
        lda #11
        sta $d021
        lda #15
        sta $d020
        lda #1
        sta 646                             ; colore carattere
        lda #0
        ldx #24
.clr    sta $d400,x                         ; azzera SID (ora anche $d400)
        dex
        bpl .clr
        jsr $e544                           ; clear screen
        lda #23
        sta $d018                           ; minuscolo
        ldx #<menu_text
        ldy #>menu_text
        jsr print_z

main_loop
        jsr getin
        beq main_loop
        cmp #'1'
        bne +
        jmp directory
+       cmp #'2'
        bne +
        jmp do_supermon
+       cmp #'3'
        bne +
        jmp do_message
+       cmp #'4'
        bne +
        jmp do_weather
+       cmp #'5'
        bne +
        jmp run_prg
+       cmp #'6'
        bne +
        jmp do_koala
+       cmp #'7'
        bne +
        jmp do_news
+       cmp #'8'
        bne +
        jmp do_slideshow
+       cmp #'9'
        bne main_loop
        jmp do_seq_slideshow

; ---------------------------------------------------------------------------
;  Voci di menu semplici
; ---------------------------------------------------------------------------
!zone do_supermon
do_supermon
        lda #50
        sta OPTION
!ifdef CART {
        jsr receive_buffer
        jsr wait_rx
        sei
        lda #$36                            ; BASIC ROM spenta: Supermon+ sta in RAM $a000
        sta $01
        cli
        jmp $a000
} else {
        ldx #<msg_nocart
        ldy #>msg_nocart
        jsr show_msg
        jmp restart
}

!zone do_message
do_message
        ldx #0                              ; rimette il testo di prova (TXBUFFER
.cp     lda test_msg,x                      ; puo' contenere l'ultimo nome file)
        sta TXBUFFER,x
        inx
        cpx #test_msg_end - test_msg
        bne .cp
        lda #51
        sta OPTION
        jsr sound
        jsr delay
        jsr send_buffer
!ifndef CART {
        ldx #<msg_sent
        ldy #>msg_sent
        jsr show_msg
}
        jmp restart

!zone do_weather
do_weather
        lda #52
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        jsr $e544
        jsr print_text
        jsr sound
        jsr setTOD
.wait   jsr getin
        cmp #32
        bne .wait
        jsr set_default_irq
        jmp restart

!zone do_koala
do_koala
        lda koala_ok
        bne .show
        ldx #<msg_nokoala
        ldy #>msg_nokoala
        jsr show_msg
        jmp restart
.show   jsr koala
        jmp restart

!zone do_news
do_news
!ifdef CART {
        jsr $e544
        ldx #<news_menu
        ldy #>news_menu
        jsr print_z
.pick   jsr getin
        beq .pick
        cmp #'0'
        bne .notzero
        lda #9
        bne .go
.notzero sec
        sbc #'1'
        cmp #10
        bcs .pick
.go     sta news_source
        jsr news
        jmp restart
} else {
        ldx #<msg_nocart
        ldy #>msg_nocart
        jsr show_msg
}
        jmp restart

; ---------------------------------------------------------------------------
;  Slideshow dei Koala nella directory corrente
; ---------------------------------------------------------------------------
!zone do_slideshow
do_slideshow
!ifdef CART {
        lda #62
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long                    ; la lista/cache puo' richiedere tempo
        lda #0
        sta slideshow_sid_started
.next   lda #61
        sta OPTION
        lda #1
        sta sid_paused                      ; ferma il play durante NMI
        jsr receive_buffer
        jsr wait_rx_long
        lda SIZE+3
        cmp #>PICTURE
        bne .end
        lda SIZE+2
        bne .end
        jsr koala_slide
        bcs .end
        jmp .next
.end    jsr slideshow_sid_stop
        jmp restart
} else {
        ldx #<msg_nocart
        ldy #>msg_nocart
        jsr show_msg
        jmp restart
}

; ---------------------------------------------------------------------------
;  Slideshow SEQ standalone: auto ogni ~5 s, F7 avanti, F1 indietro, SPACE esci
; ---------------------------------------------------------------------------
!zone do_seq
do_seq_slideshow
!ifdef CART {
        jsr clear_screen_text
        lda #89
        jsr load_art_cmd
.loop   lda $a2
        sta seq_t0
.wait   jsr getin
        beq .timer
        cmp #32
        beq .done
        cmp #136
        beq .next
        cmp #133
        beq .prev
        jmp .wait
.timer  lda $a2
        sec
        sbc seq_t0
        cmp #250
        bcc .wait
.next   lda #87
        jsr load_art_cmd
        jmp .loop
.prev   lda #88
        jsr load_art_cmd
        jmp .loop
.done   jmp restart
} else {
        ldx #<msg_nocart
        ldy #>msg_nocart
        jsr show_msg
        jmp restart
}

; ---------------------------------------------------------------------------
;  SID di sottofondo per lo slideshow
; ---------------------------------------------------------------------------
!zone slideshow_sid
slideshow_sid_start
        lda #0
        sta sid_bg_active
        lda sid_loaded
        beq .nosid
        lda sid_conflict
        and #$2D                            ; screen+bitmap+sorgente Koala+codice
        bne .nosid                           ; SID non compatibile con lo slideshow
        jsr sid_set_calls
        sei
        jsr sid_init_once
        lda #<irq_sid_slide
        sta $0314
        lda #>irq_sid_slide
        sta $0315
        lda #250
        sta $d012
        lda $d01a
        ora #$01
        sta $d01a
        asl $d019
        lda #1
        sta sid_bg_active
        cli
        rts
.nosid  rts

irq_sid_slide
        lda $d019
        and #$01
        beq .skip
        asl $d019
        lda sid_bg_active
        beq .skip
        lda sid_paused
        bne .skip
        jsr sid_play_once
.skip   jmp $ea31

slideshow_sid_stop
        sei
        lda #0
        sta sid_bg_active
        lda $d01a
        and #$fe
        sta $d01a
        asl $d019
        lda #$31
        sta $0314
        lda #$ea
        sta $0315
        ldx #24
        lda #0
.sil    sta $d400,x
        dex
        bpl .sil
        lda #$37
        sta $01
        cli
        rts

; ---------------------------------------------------------------------------
;  Directory SD
;  Dati ricevuti: testo a SIZE+2/3, lunghezza SIZE/SIZE+1.
;  Nessun valore resta nei registri X/Y attraverso GETIN (il Kernal li modifica).
; ---------------------------------------------------------------------------
!zone directory
directory
        lda #49                             ; comando: leggi directory dalla SD
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
.show   lda SIZE+2
        sta dir_start
        lda SIZE+3
        sta dir_start+1
        lda SIZE
        sta dir_size
        lda SIZE+1
        sta dir_size+1
.first  jsr $e544                           ; (F1 ora pulisce lo schermo)
        lda dir_start
        sta ptr1
        lda dir_start+1
        sta ptr1+1
        lda dir_size
        sta dir_left
        lda dir_size+1
        sta dir_left+1
.page   lda ptr1
        sta page_start
        lda ptr1+1
        sta page_start+1
        lda #0
        sta line_count
.print  lda dir_left
        ora dir_left+1
        beq .menu                           ; fine dati
        ldy #0
        lda (ptr1),y
        sta dir_ch
        cmp #10                             ; ignora LF di eventuali CRLF
        beq .skip
        jsr basout
.skip   +inc16 ptr1
        +dec16 dir_left
        lda dir_ch
        cmp #13
        bne .print
        inc line_count
        lda line_count
        cmp #24
        bcc .print                          ; pagina piena: si ferma qui

.menu   jsr flash_line_0
        jsr pointer_line_0
        jsr set_irq_color
.key    jsr getin
        beq .key
        cmp #133                            ; F1: pagina precedente
        bne .f7
        jsr set_default_irq
        jsr reset_default_color
        jsr page_up
        jsr $e544
        jmp .page
.f7     cmp #136                            ; F7: pagina successiva
        bne .down
        lda dir_left
        ora dir_left+1
        beq .key                            ; non c'e' altro
        jsr set_default_irq
        jsr reset_default_color
        jsr $e544
        jmp .page
.down   cmp #17                             ; cursore giu'
        bne .up
        inc HOME_LINE
        lda HOME_LINE
        cmp #25
        bcc .okd
        jsr pointer_line_24
        jsr flash_line_24
        jmp .key
.okd    jsr set_default_irq
        jsr reset_default_color
        jsr inc_flash_line
        jsr inc_pointer_line
        jsr set_irq_color
        jmp .key
.up     cmp #145                            ; cursore su
        bne .quit
        dec HOME_LINE
        lda HOME_LINE
        bpl .oku
        jsr pointer_line_0
        jsr flash_line_0
        jmp .key
.oku    jsr set_default_irq
        jsr reset_default_color
        jsr dec_flash_line
        jsr dec_pointer_line
        jsr set_irq_color
        jmp .key
.quit   cmp #81                             ; 'q'
        bne .ret
        jsr set_default_irq
        jsr reset_default_color
        jmp restart
.ret    cmp #13                             ; invio: carica il file
        beq .load
        jmp .key
.load   jsr set_default_irq
        jsr reset_default_color
        jsr read_from_screen
        bne .go
        jsr set_irq_color                   ; riga vuota: ignora
        jmp .key
.go     lda #51                             ; invia il nome file / directory
        sta OPTION
        jsr send_buffer
        jsr delay
        lda TXBUFFER
        cmp #47                             ; '/' = voce di directory
        beq .newdir
        lda #53                             ; ricevi il file
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        lda cassette_buffer
        beq .prg
        lda #0
        sta koala_ok
        jsr play_SID                        ; e' un SID
        lda cassette_buffer+1
        sta sid_init_lo
        lda cassette_buffer+2
        sta sid_init_hi
        lda cassette_buffer+3
        sta sid_play_lo
        lda cassette_buffer+4
        sta sid_play_hi
        lda cassette_buffer+5
        sta sid_conflict
        lda #1
        sta sid_loaded
        jmp restart
.prg    lda #0
        sta koala_ok
        lda SIZE+3                          ; file caricato a $6000 = immagine Koala
        cmp #>PICTURE
        bne .runprg
        lda SIZE+2
        bne .runprg
        lda #1
        sta koala_ok
        jsr koala                           ; mostra subito l'immagine caricata
        jmp restart

.runprg lda #0
        sta sid_loaded
        lda #60                             ; chiedi all'ESP32 l'entry point
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        lda PRG_ENTRY
        ora PRG_ENTRY+1
        beq .done                           ; entry = 0: torna al menu
        jmp (PRG_ENTRY)
.done   jmp restart

.newdir lda #49                             ; directory cambiata: richiedine il testo
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        jmp .show

; ---------------------------------------------------------------------------
;  Pagina precedente della directory: cerca il confine dei 24 CR precedenti
; ---------------------------------------------------------------------------
!zone dirpage
page_up
        lda dir_start
        sta ptr2
        sta page_scan
        lda dir_start+1
        sta ptr2+1
        sta page_scan+1
        lda #0
        sta line_count

.loop   lda ptr2
        cmp page_start
        lda ptr2+1
        sbc page_start+1
        bcs .found

        ldy #0
        lda (ptr2),y
        +inc16 ptr2
        cmp #13
        bne .loop

        inc line_count
        lda line_count
        cmp #24
        bcc .loop

        ; confine di pagina: se siamo arrivati all'inizio della pagina
        ; corrente, page_scan e' gia' la pagina precedente.
        lda ptr2
        cmp page_start
        lda ptr2+1
        sbc page_start+1
        bcs .found

        lda ptr2
        sta page_scan
        lda ptr2+1
        sta page_scan+1
        lda #0
        sta line_count
        jmp .loop

.found  lda page_scan
        sta ptr1
        lda page_scan+1
        sta ptr1+1

        ; dir_left = dir_size - (ptr1 - dir_start)
        sec
        lda ptr1
        sbc dir_start
        sta cnt
        lda ptr1+1
        sbc dir_start+1
        sta cnt+1

        lda dir_size
        sec
        sbc cnt
        sta dir_left
        lda dir_size+1
        sbc cnt+1
        sta dir_left+1
        rts

; ---------------------------------------------------------------------------
;  Evidenziazione riga (colore + puntatore alla riga di schermo)
; ---------------------------------------------------------------------------
!zone lines
set_default_irq
        sei
        lda #$31
        sta $0314
        lda #$ea
        sta $0315
        cli
        rts

set_irq_color
        sei
        lda #<irq_color
        sta $0314
        lda #>irq_color
        sta $0315
        cli
        rts

irq_color
        jsr colwash
        jmp $ea31

flash_line_0
        lda #$d8
        sta cw_sta+2
        sta cr_sta+2
        lda #0
        sta cw_sta+1
        sta cr_sta+1
        rts

flash_line_24
        lda #$db
        sta cw_sta+2
        sta cr_sta+2
        lda #$c0
        sta cw_sta+1
        sta cr_sta+1
        rts

inc_flash_line
        clc
        lda cw_sta+1
        adc #40
        sta cw_sta+1
        sta cr_sta+1
        lda cw_sta+2
        adc #0
        sta cw_sta+2
        sta cr_sta+2
        rts

dec_flash_line
        sec
        lda cw_sta+1
        sbc #40
        sta cw_sta+1
        sta cr_sta+1
        lda cw_sta+2
        sbc #0
        sta cw_sta+2
        sta cr_sta+2
        rts

pointer_line_0
        lda #$04
        sta ptr2+1
        lda #0
        sta ptr2
        sta HOME_LINE
        rts

pointer_line_24
        lda #$07
        sta ptr2+1
        lda #$c0
        sta ptr2
        lda #24
        sta HOME_LINE
        rts

inc_pointer_line
        clc
        lda ptr2
        adc #40
        sta ptr2
        lda ptr2+1
        adc #0
        sta ptr2+1
        rts

dec_pointer_line
        sec
        lda ptr2
        sbc #40
        sta ptr2
        lda ptr2+1
        sbc #0
        sta ptr2+1
        rts

!zone colwash
colwash
        lda colourdelay
        cmp #4
        beq .go
        inc colourdelay
        rts
.go     lda #0
        sta colourdelay
        ldx tmp
        lda colour,x
        ldy #39
cw_sta  sta $d800,y                         ; operando modificato da flash_line_*
        dey
        bpl cw_sta
        inx
        cpx #13
        bne .keep
        ldx #0
.keep   stx tmp
        rts

reset_default_color
        lda CHAR_COLOR
        ldy #39
cr_sta  sta $d800,y                         ; operando modificato da flash_line_*
        dey
        bpl cr_sta
        rts

; copia in TXBUFFER il nome file sotto il cursore (codici schermo).
; Esce al primo doppio spazio, massimo 40 caratteri (prima non c'era limite).
; Ritorna A = lunghezza, Z=1 se riga vuota.
!zone read_from_screen
read_from_screen
        ldy #0
.find   cpy #40
        bcs .copy
        lda (ptr2),y
        cmp #32
        bne .next
        cpy #39
        bcs .copy
        iny
        lda (ptr2),y
        dey
        cmp #32
        beq .copy
.next   iny
        bne .find
.copy   sty name_len
        ldx #0
.cl     cpx name_len
        beq .end
        txa
        tay
        lda (ptr2),y
        sta TXBUFFER,x
        inx
        bne .cl
.end    lda #128
        sta TXBUFFER,x
        lda name_len
        rts

; ---------------------------------------------------------------------------
;  Stampa testo
; ---------------------------------------------------------------------------
!zone text
; stampa la stringa terminata da 0 all'indirizzo X(lo)/Y(hi)
print_z
        stx ptr1
        sty ptr1+1
.l      ldy #0
        lda (ptr1),y
        beq .e
        jsr basout
        +inc16 ptr1
        jmp .l
.e      rts

; pulisce lo schermo, stampa il messaggio X/Y e aspetta un tasto
show_msg
        stx msg_lo
        sty msg_hi
        jsr $e544
        ldx msg_lo
        ldy msg_hi
        jsr print_z
.w      jsr getin
        beq .w
        rts

; stampa il blocco ricevuto (start SIZE+2/3, lunghezza SIZE/SIZE+1)
print_text
        lda SIZE+2
        sta ptr1
        lda SIZE+3
        sta ptr1+1
        lda SIZE
        sta cnt
        lda SIZE+1
        sta cnt+1
.pl     lda cnt
        ora cnt+1
        beq .pe
        ldy #0
        lda (ptr1),y
        jsr basout
        +inc16 ptr1
        +dec16 cnt
        jmp .pl
.pe     rts

; ---------------------------------------------------------------------------
;  Attese
; ---------------------------------------------------------------------------
!zone wait
; attende la fine della ricezione (rxState = 0), massimo circa 5 secondi
wait_rx
        lda $a2
        sta wait_t0
.w      lda rxState
        beq .ok
        lda $a2
        sec
        sbc wait_t0
        cmp #250
        bcc .w
        lda #0
        sta rxState                         ; timeout: abbandona
.ok     rts

; ---------------------------------------------------------------------------
;  Attesa lunga per comandi che usano Internet (NEWS/API)
;  Circa 8 giri del contatore jiffy = ~34 secondi.
; ---------------------------------------------------------------------------
!zone waitlong
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

; pausa fissa di 20 jiffy (circa 1/3 s), indipendente dai cicli CPU
!zone delay
delay
        lda $a2
        clc
        adc #20
.dw     cmp $a2
        bne .dw
        rts

; ---------------------------------------------------------------------------
;  Suono
; ---------------------------------------------------------------------------
!zone sound
sound
        lda #0
        sta $d400
        sta $d407
        lda #143
        sta $d418
        lda PITCH
        sta $d401
        sta $d408
        lda #15
        sta $d405
        sta $d40c
        lda #249
        sta $d406
        sta $d40d
        lda #17
        sta $d404
        lda #16
        sta $d404
        rts

; ---------------------------------------------------------------------------
;  BASIC: riaggancia le righe e torna al prompt (poi RUN)
; ---------------------------------------------------------------------------
!zone run_prg
run_prg
        lda #0
        sta $cc                             ; cursore normale in BASIC
        jsr $a659                           ; CLR
        jsr $a533                           ; rilega le righe
        jsr $e37b                           ; warm start BASIC (non ritorna)
        jmp $a7ae

; ---------------------------------------------------------------------------
;  Ora (TOD del CIA1) dalla prima riga di testo "xxxxx: HH:MM:SS"
; ---------------------------------------------------------------------------
!zone tod
scr2bcd                                     ; X = offset sullo schermo -> A = BCD
        lda screenRam,x
        asl
        asl
        asl
        asl
        sta bcd_tmp
        lda screenRam+1,x
        and #$0f
        ora bcd_tmp
        rts

bcd_ok                                      ; C=1 se non e' BCD valido
        tay
        and #$0f
        cmp #$0a
        bcs .bad
        tya
        lsr
        lsr
        lsr
        lsr
        cmp #$0a
        rts
.bad    sec
        rts

setTOD
        ldx #6
        jsr scr2bcd
        sta second+2
        jsr bcd_ok
        bcs .skip
        ldx #9
        jsr scr2bcd
        sta second+1
        jsr bcd_ok
        bcs .skip
        ldx #12
        jsr scr2bcd
        sta second
        jsr bcd_ok
        bcs .skip

        lda second+2                        ; 24h -> formato CIA (12h + bit PM)
        beq .midnight                       ; 00 -> 12 AM
        cmp #$12
        beq .noon                           ; 12 -> 12 PM
        bcc .hset                           ; 01..11 AM
        sed
        sec
        sbc #$12                            ; 13..23 -> 01..11 PM
        cld
        ora #$80
        bne .hset
.midnight
        lda #$12
        bne .hset
.noon   lda #$92
.hset   sta second+2

        lda cia1+15                         ; CRB bit7=0: le scritture regolano l'ora
        and #$7f
        sta cia1+15
        lda cia1+14                         ; CRA bit7=1: rete a 50 Hz
        ora #$80
        sta cia1+14
        lda second+2
        sta cia1+11                         ; ore (ferma il clock)
        lda second+1
        sta cia1+10
        lda second
        sta cia1+9
        lda #0
        sta cia1+8                          ; decimi (riavvia il clock)

        lda #15
        sta toggle
        sei
        lda #<update
        sta $0314
        lda #>update
        sta $0315
        cli
.skip   rts

; IRQ: ogni 15 interrupt rilegge il TOD e aggiorna le cifre sullo schermo.
; (nell'originale c'era un CLI dentro l'IRQ: qui non serve)
update
        dec toggle
        beq .go
        jmp $ea31
.go     lda #15
        sta toggle
        lda cia1+11                         ; ore per prime: blocca la lettura
        sta second+2
        lda cia1+10
        sta second+1
        lda cia1+9
        sta second
        lda cia1+8                          ; decimi: sblocca
        cmp #5
        bcs .noblink
        lda #':'
        bne .colon
.noblink
        lda #32
.colon  sta screenRam+8
        sta screenRam+11
        lda second+2                        ; 12h -> 24h
        bmi .pm
        cmp #$12
        bne .store
        lda #0                              ; 12 AM -> 00
        beq .store
.pm     and #$1f
        cmp #$12
        beq .store                          ; 12 PM -> 12
        sed
        clc
        adc #$12
        cld
.store  sta second+2
        ldx #6
        lda second+2
        jsr put_bcd
        ldx #9
        lda second+1
        jsr put_bcd
        ldx #12
        lda second
        jsr put_bcd
        jmp $ea31

put_bcd                                     ; A = BCD, X = offset sullo schermo
        pha
        lsr
        lsr
        lsr
        lsr
        ora #$30
        sta screenRam,x
        pla
        and #$0f
        ora #$30
        sta screenRam+1,x
        rts

; ---------------------------------------------------------------------------
;  Immagine Koala (caricata dalla SD a $6000)
; ---------------------------------------------------------------------------
!zone koala
koala_prepare
        lda #<PICTURE
        sta ptr1
        lda #>PICTURE
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$20
        sta ptr2+1                          ; bitmap a $2000
        ldy #0
        ldx #$1f                            ; 31 pagine
.pg     lda (ptr1),y
        sta (ptr2),y
        iny
        bne .pg
        inc ptr1+1
        inc ptr2+1
        dex
        bne .pg
        ldx #$40                            ; + $40 byte = 8000 (era $3f: ne mancava uno)
.rest   lda (ptr1),y
        sta (ptr2),y
        iny
        dex
        bne .rest

        ; copia video matrix e color RAM leggendo la RAM sotto la ROM
        lda #$36
        sta $01                 ; BASIC ROM off, Kernal e I/O on
        lda #$00
        sta $d020
        lda BACKGROUND
        sta $d021
        ldx #$00
.l      lda VIDEO,x
        sta $0400,x
        lda VIDEO+$100,x
        sta $0500,x
        lda VIDEO+$200,x
        sta $0600,x
        lda VIDEO+$2e8,x
        sta $06e8,x
        lda COLOR,x
        sta $d800,x
        lda COLOR+$100,x
        sta $d900,x
        lda COLOR+$200,x
        sta $da00,x
        lda COLOR+$2e8,x
        sta $dae8,x
        inx
        bne .l
        lda #$37
        sta $01                 ; ripristina la configurazione normale
        rts

; mostra il bitmap multicolor e aspetta SPACE
koala
        jsr koala_prepare

        ; configura il VIC II per il bitmap multicolor
        lda #1
        sta $cc                             ; cursore spento
        lda $dd00
        and #$fc
        ora #$03                            ; VIC bank 0
        sta $dd00
        lda #$3b
        sta $d011                           ; bitmap mode, DEN, RSEL
        lda #$18
        sta $d018                           ; screen $0400, bitmap $2000
        sta $d016                           ; multicolor, 40 colonne

.hold   jsr getin
        cmp #32
        bne .hold

        ; ripristina la modalita' testo del menu
        lda #$1b
        sta $d011
        lda #$17
        sta $d018
        lda #$c8
        sta $d016
        lda #0
        sta $cc
        rts

; ---------------------------------------------------------------------------
;  Slideshow Koala: mostra l'immagine per ~4 secondi o fino a SPACE
; ---------------------------------------------------------------------------
!zone koalaslide
koala_slide
        jsr koala_prepare

        lda #1
        sta $cc
        lda $dd00
        and #$fc
        ora #$03                            ; VIC bank 0
        sta $dd00
        lda #$3b
        sta $d011
        lda #$18
        sta $d018
        sta $d016

        lda slideshow_sid_started
        bne .sid_ready
        inc slideshow_sid_started
        jsr slideshow_sid_start
.sid_ready
        lda #0
        sta sid_paused                      ; riprendi il SID a immagine pronta
        lda $a2
        sta wait_t0
.slide  jsr getin
        cmp #32
        beq .exit
        lda $a2
        sec
        sbc wait_t0
        cmp #250                            ; circa 4 secondi
        bcc .slide

        jsr slide_text_mode
        clc
        rts

.exit   jsr slide_text_mode
        sec
        rts

slide_text_mode
        lda #$1b
        sta $d011
        lda #$17
        sta $d018
        lda #$c8
        sta $d016
        lda #0
        sta $cc
        rts

; ---------------------------------------------------------------------------
;  SID: init/play dai dati del file (header a cassette_buffer: flag, init, play)
; ---------------------------------------------------------------------------
!zone sid
play_SID
        lda cassette_buffer+1
        sta SID_init+1
        lda cassette_buffer+2
        sta SID_init+2
        lda cassette_buffer+3
        sta SID_play+1
        lda cassette_buffer+4
        sta SID_play+2
        ora cassette_buffer+3
        bne .hasplay
        lda #<no_play                       ; play = 0: ci pensa l'init (IRQ proprio)
        sta SID_play+1
        lda #>no_play
        sta SID_play+2
.hasplay
        lda #56                             ; testo informativo
        sta OPTION
        jsr receive_buffer
        jsr wait_rx
        jsr $e544
        jsr print_text
        lda #55                             ; codice del SID
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long

        sei
        lda #$36                            ; BASIC off, Kernal e I/O on
        sta $01
        ldx #<IRQ_SIDplay
        ldy #>IRQ_SIDplay
        stx $0314
        sty $0315
        lda #53
        sta $d012
        lda #$7f
        sta $dc0d                           ; niente IRQ da CIA: solo raster
        lda #$1b
        sta $d011
        lda #$01
        sta $d019                           ; azzera eventuale IRQ raster pendente
        sta $d01a
        lda #$35                            ; tutta RAM + I/O durante init
        sta $01
        lda #0
        tax
        tay
SID_init
        jsr $1000
        lda #$36
        sta $01
        cli
.loop   jsr getin
        cmp #32
        bne .loop
        jsr stop_SID
        rts

no_play rts

IRQ_SIDplay
        asl $d019
        inc $d020
        lda #$35
        sta $01
SID_play
        jsr $1003
        lda #$36
        sta $01
        dec $d020
        jmp $ea31

; routine per il SID in background (usate da NEWS)
sid_set_calls
        lda sid_init_lo
        sta SID_init_call+1
        lda sid_init_hi
        sta SID_init_call+2
        lda sid_play_lo
        sta SID_play_call+1
        lda sid_play_hi
        sta SID_play_call+2
        rts

sid_init_once
        lda #$35
        sta $01
SID_init_call
        jsr $1000
        lda #$36
        sta $01
        rts

sid_play_once
        lda #$35
        sta $01
SID_play_call
        jsr $1003
        lda #$36
        sta $01
        rts

stop_SID
        sei
        lda #$ff
        sta $d019
        lda #$00
        sta $d01a
        lda #$31
        sta $0314
        lda #$ea
        sta $0315
        lda #0
        ldy #25
.l      sta $d400-1,y
        dey
        bne .l
        jsr $ff84                           ; IOINIT (rimette il CIA a posto)
        lda #$37
        sta $01                             ; (prima restava $36: BASIC irraggiungibile)
        cli
        rts

; ---------------------------------------------------------------------------
;  Menu: testi
; ---------------------------------------------------------------------------
menu_text
        !pet " --------------------", 13
        !pet "    *** C64 ESP32 Cart ***", 13
        !pet "    *** by M.Angelini ***", 13
!ifndef CART {
        !pet "   (PRG: ESP32 simulato)", 13
}
        !pet " --------------------", 13, 13
        !pet "1.SD", 13
        !pet "2.Super", 13
        !pet "3.Msg", 13
        !pet "4.Ora", 13
        !pet "5.BASIC", 13
        !pet "6.Koala", 13
        !pet "7.NEWS", 13
        !pet "8.Koala", 13
        !pet "9.SEQ", 13, 0

!ifndef CART {
msg_nocart
        !pet "Serve la cartuccia.", 13, 13
        !pet "Premi un tasto.", 0
msg_sent
        !pet "Messaggio inviato.", 13, 13
        !pet "Premi un tasto.", 0
}
msg_nokoala
        !pet "Nessun Koala.", 13
        !pet "Carica .koa da SD.", 13, 13
        !pet "Premi tasto.", 0

test_msg
        !pet "prova invio testo", 128
test_msg_end

; ---------------------------------------------------------------------------
;  Colori e variabili
; ---------------------------------------------------------------------------
colour      !byte 6,2,4,5,3,7,1,7,3,5,4,2,6,0
tmp         !byte 0
colourdelay !byte 0
CHAR_COLOR  !byte 1
PITCH       !byte 125

OPTION      !byte 0                         ; comando per l'ESP32
SIZE        !byte 0,0,0,0                   ; lunghezza (2) e indirizzo (2) dell'ultimo blocco
rxState     !byte 0                         ; 0 = fermo, 1 = header, 2 = dati
RXFULL      !byte 0
RYINDEX     !byte 0                         ; (erano $02,$03,$04: $03/$04 sono vettori BASIC)
wait_t0     !byte 0
wait_wrap   !byte 0

HOME_LINE   !byte 0
line_count  !byte 0
dir_ch      !byte 0
dir_left    !word 0
dir_start   !word 0
dir_size    !word 0
page_start  !word 0
page_scan   !word 0
cnt         !word 0
name_len    !byte 0
koala_ok    !byte 0
news_source !byte 0
msg_lo      !byte 0
msg_hi      !byte 0

second      !byte 0,0,0                     ; secondi, minuti, ore
toggle      !byte 0
bcd_tmp     !byte 0

TXBUFFER    !pet "prova invio testo", 128
            !fill 30, 0

; ---------------------------------------------------------------------------
;  SOLO CARTUCCIA: ESP32 reale (NMI, handshake, scroller news)
; ---------------------------------------------------------------------------
!ifdef CART {

!zone nmi
; Con l'ESP32 che mette un byte nel 74LS244 la linea NMI va bassa.
; Stati: 0 fermo (NMI ignorati: es. tasto RESTORE), 1 header (4 byte: lunghezza
; e indirizzo), 2 dati.  Prima la distinzione dipendeva da ptr2, che il menu
; riusava come puntatore di schermo: un NMI spurio scriveva in RAM.
nmi
        bit $dd0d
        +pushreg
        lda rxState
        cmp #2
        bne .notdata
        ldy RYINDEX
        ldx ptr2+1
        beq .rem
        lda $df00                           ; pagine intere
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
.rem    ldx ptr2                            ; byte rimanenti
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
        bne .complete                       ; fermo: NMI spurio
        ldy RYINDEX
        lda $df00
        sta ptr2,y                          ; byte 0,1 -> ptr2 (lunghezza); 2,3 -> ptr1 (indirizzo)
        sta SIZE,y
        iny
        sty RYINDEX
        cpy #4
        bcc .exit
        lda ptr2
        ora ptr2+1
        beq .complete                       ; blocco vuoto
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

!zone esp
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

!zone news
news_menu
        !pet 13, "Fonte news:", 13, 13
        !pet "1.ANSA", 13
        !pet "2.RAI", 13
        !pet "3.REPUBBLICA", 13
        !pet "4.SOLE24", 13
        !pet "5.GOOGLE", 13
        !pet "6.NEWSAPI", 13
        !pet "7.BBC", 13
        !pet "8.ALJAZEERA", 13
        !pet "9.SCMP", 13
        !pet "0.CGTN", 13, 13
        !pet "1-9,0.", 13, 0

news
        jsr clear_screen_text
        lda #57
        sta OPTION
        jsr receive_buffer
        jsr wait_rx
        lda news_source
        clc
        adc #70
        sta OPTION
        lda koala_ok
        bne .send
        lda OPTION
        clc
        adc #20                             ; 90..99: NEWS + grafica PETSCII
        sta OPTION
.send   jsr receive_buffer
        jsr wait_rx_long
        lda #0
        sta RXFULL
        lda koala_ok
        bne .noart
        jsr copy_art                        ; grafica accodata al testo
.noart  lda koala_ok
        sta split_mode
        lda #0
        sta sid_bg_active
        lda sid_loaded
        beq .nosid
        lda split_mode
        beq .textmask
        lda #$3F                            ; screen+charset+bitmap+source+news+code
        jmp .check
.textmask lda #$33                      ; screen+charset+news+code
.check  and sid_conflict
        bne .nosid                          ; conflitto: non avviare il SID
        jsr sid_set_calls
        sei
        jsr sid_init_once
        lda #1
        sta sid_bg_active
.nosid  jsr sound

        ; carica subito il primo art / immagine SEQ prima di far partire lo scroller
        lda art_interval
        sta art_cnt
        lda art_interval+1
        sta art_cnt+1
        jsr art_tick

        jsr set_scroller_irq
        sei
        lda #$00
        sta $d019
        sta $d01a
        ldx #24
.sil    sta $d400,x
        dex
        bpl .sil
        lda #$31
        sta $0314
        lda #$ea
        sta $0315
        lda #$1b
        sta $d011                           ; torna in modalita' testo
        lda #$17
        sta $d018
        lda #$c8
        sta $d016
        lda #0
        sta split_mode
        sta sid_bg_active
        jsr $ff84                           ; IOINIT
        lda #$37
        sta $01                             ; ripristina BASIC/Kernal/I-O
        cli
        rts

!zone scroller
; Copia i 1000 byte di screen e i 1000 byte di colori accodati al testo NEWS
; (ricevuti a $9000/$93E8) nelle aree VIC $0400/$D800.
copy_art
        lda #<ART_SOURCE
        sta ptr1
        lda #>ART_SOURCE
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$04
        sta ptr2+1
        jsr copy920

        lda #<ART_COLOR
        sta ptr1
        lda #>ART_COLOR
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$d8
        sta ptr2+1
        jmp copy920

copy920
        ldy #0
        ldx #3
.pages  lda (ptr1),y
        sta (ptr2),y
        iny
        bne .pages
        inc ptr1+1
        inc ptr2+1
        dex
        bne .pages
        ldx #232
.tail   lda (ptr1),y
        sta (ptr2),y
        iny
        dex
        bne .tail
        rts

; Mostra un'immagine SEQ/C/G: stream PETSCII a $A000, lunghezza in SIZE.
; Usa indirizzamento auto-modificante per non dipendere dai puntatori ZP
; (il KERNAL CHROUT puo' usarli).
show_seq
        lda $01
        pha
        lda #$36                            ; BASIC ROM off: $A000 e' RAM
        sta $01
        lda #$93
        jsr $ffd2                           ; clear screen + home
        lda #%00010100
        sta $d018                           ; charset ROM
        lda #$1b
        sta $d011
        lda SIZE
        sta seq_cnt
        lda SIZE+1
        sta seq_cnt+1
        lda #$00
        sta .ld+1
        lda #$a0
        sta .ld+2
        ldy #0
.ss_loop
        lda seq_cnt
        ora seq_cnt+1
        beq .ss_done
.ld     lda $a000,y
        jsr $ffd2
        iny
        bne .ss_noinc
        inc .ld+2
.ss_noinc
        dec seq_cnt
        lda seq_cnt
        cmp #$ff
        bne .ss_loop
        dec seq_cnt+1
        jmp .ss_loop
.ss_done pla
        sta $01
        rts

; A = comando (86 news, 87 next SEQ, 88 prev SEQ, 89 first SEQ).
; Riceve un blocco e lo mostra: SEQ a $A000 oppure art generata a $9000.
load_art_cmd
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        lda SIZE+3
        cmp #$a0
        bcs .la_seq
        jsr copy_art
        rts
.la_seq jsr show_seq
        rts

; Chiede un nuovo art all'ESP32. Se arriva a $A000 e' un'immagine SEQ,
; se arriva a $9000 e' un pattern generato. Il timer passa a 15 s per le
; immagini e torna a 60 s per i pattern generati.
art_tick
        lda split_mode
        beq .at_run
        jmp .atend
.at_run inc art_cnt
        bne .at_cmp
        inc art_cnt+1
.at_cmp    lda art_cnt+1
        cmp art_interval+1
        bcc .at_short
        bne .at_fire
        lda art_cnt
        cmp art_interval
        bcc .at_short
        jmp .at_fire
.at_short jmp .atend
.at_fire   lda rxState
        beq .at_go
        jmp .atend
.at_go  lda #0
        sta art_cnt
        sta art_cnt+1

        lda ptr1
        sta save_ptr
        lda ptr1+1
        sta save_ptr+1
        lda sid_bg_active
        sta save_sid
        lda #0
        sta sid_bg_active

        lda #86
        jsr load_art_cmd

        lda SIZE+3
        cmp #$a0
        bcs .seq
        jsr copy_art
        lda #<3000
        sta art_interval
        lda #>3000
        sta art_interval+1
        jmp .at_after
.seq    jsr show_seq
        lda #<750
        sta art_interval
        lda #>750
        sta art_interval+1
.at_after  jsr clear_bottom
        lda #%00010100
        sta $d018
        lda #$1b
        sta $d011
        lda #0
        sta RXFULL

        lda save_ptr
        sta ptr1
        lda save_ptr+1
        sta ptr1+1
        lda save_sid
        sta sid_bg_active
.atend   rts

set_scroller_irq
        lda split_mode
        bne .koala
        jsr clear_bottom
        jmp .common

.koala  jsr koala_prepare
        jsr clear_bottom
        lda $dd00
        and #$fc
        ora #$03                            ; VIC bank 0
        sta $dd00

.common lda #0
        sta ptr1
        lda #$88
        sta ptr1+1                          ; testo a $8800
        lda #%00010100
        sta $d018                           ; top: charset ROM a $1000
        lda split_mode
        beq .irq
        lda #$3b
        sta $d011                           ; top in bitmap mode
        lda #$18
        sta $d018                           ; screen $0400, bitmap $2000
        sta $d016

.irq    sei
        lda #$7f
        sta $dc0d
        sta $dd0d
        lda $d01a
        ora #$01
        sta $d01a
        lda $d011
        and #$7f
        sta $d011
        lda #<irq1
        sta $0314
        lda #>irq1
        sta $0315
        lda #233
        sta $d012
        lda $dc0d
        lda $dd0d
        asl $d019
        cli
.main   lda #0
        sta sync
.back   cmp sync
        beq .back
        jsr scroll1
        jsr getin
        cmp #32
        beq .exit
        jsr art_tick
        jmp .main
.exit   rts

clear_screen_text
        lda #0
        sta $d020
        sta $d021
        ldx #0
        lda #32
.clr    sta $0400,x
        sta $0500,x
        sta $0600,x
        sta $06e8,x
        lda #1
        sta $d800,x
        sta $d900,x
        sta $da00,x
        sta $dae8,x
        lda #32
        inx
        bne .clr
        lda #$1b
        sta $d011
        lda #%00010100
        sta $d018
        lda #$c8
        sta $d016
        rts

clear_bottom
        ldx #0
        lda #32
.loop   sta SCREEN,x
        sta SCREEN+40,x
        lda #1
        sta SCREEN_COLOR,x
        sta SCREEN_COLOR+40,x
        lda #32
        inx
        cpx #40
        bne .loop
        rts

irq1
        asl $d019
        lda #<irq2
        sta $0314
        lda #>irq2
        sta $0315
        lda #250
        sta $d012
        lda #$1b
        sta $d011                           ; bottom in text mode
        lda #%00010010
        sta $d018                           ; bottom: font 2x2 a $0800
        lda scroll_x
        sta $d016
        jmp $ea81

irq2
        asl $d019
        lda sid_bg_active
        beq .nosidplay
        jsr sid_play_once
.nosidplay
        lda #<irq1
        sta $0314
        lda #>irq1
        sta $0315
        lda #233
        sta $d012
        lda split_mode
        beq .nobitmap
        lda #$3b
        sta $d011                           ; next frame top bitmap
        lda #$18
        sta $d018
        lda #$18
        sta $d016
        jmp .done
.nobitmap lda #%00010100
        sta $d018                           ; top: charset ROM a $1000
        lda #%00001000
        sta $d016
.done   inc sync
        jmp $ea31

scroll1
        dec speed
        bne .end
        lda #SPEED
        sta speed
        dec scroll_x
        lda scroll_x
        and #07
        sta scroll_x
        cmp #07
        bne .end
        ldx #0
.l      lda SCREEN+1,x
        sta SCREEN,x
        lda SCREEN+40+1,x
        sta SCREEN+40,x
        inx
        cpx #39
        bne .l
        ldy #0                              ; era "ldy 0": leggeva $0000 (=$2f)
        lda (ptr1),y
        cmp #$ff
        bne .put
        lda #0
        sta half_char
        sta ptr1
        lda #$88
        sta ptr1+1
        lda (ptr1),y
.put    ora half_char
        sta SCREEN+39
        ora #$80
        sta SCREEN+40+39
        lda half_char
        eor #$40
        sta half_char
        bne .end
        inc ptr1
        bne .end
        inc ptr1+1
.end    rts

sync        !byte 1
split_mode  !byte 0
sid_loaded  !byte 0
sid_bg_active !byte 0
slideshow_sid_started !byte 0
sid_paused  !byte 0
sid_conflict  !byte 0
sid_init_lo !byte 0
sid_init_hi !byte 0
sid_play_lo !byte 0
sid_play_hi !byte 0
scroll_x    !byte 7
speed       !byte SPEED
half_char   !byte 0
art_cnt     !word 0
art_interval !word 3000
seq_cnt     !word 0
seq_t0      !byte 0
save_ptr    !word 0
save_sid    !byte 0

} else {

; ---------------------------------------------------------------------------
;  SOLO PRG: ESP32 simulato
;  receive_buffer riempie SIZE (lunghezza, indirizzo) come farebbe l'NMI e,
;  se serve, copia i dati incorporati nella destinazione richiesta.
; ---------------------------------------------------------------------------
!zone mock
!macro mock_item .o, .s, .l, .d {
        !byte .o
        !word .s, .l, .d
}

send_buffer
        rts                                 ; il messaggio va nel vuoto

receive_buffer
        lda #0
        sta rxState
        sta RYINDEX
        ldx OPTION
        cpx #53
        bne .look
        ldx #254                            ; file = PRG di prova
        ldy name_len
        beq .look
        lda TXBUFFER-1,y
        cmp #$04                            ; codice schermo di 'd' (".sid")
        bne .look
        ldx #253                            ; file = SID di prova
.look   stx mock_opt
        ldx #0
.find   lda mock_table,x
        cmp #255
        beq .found
        cmp mock_opt
        beq .found
        txa
        clc
        adc #7
        tax
        bne .find
.found  lda mock_table+1,x
        sta ptr1
        lda mock_table+2,x
        sta ptr1+1
        lda mock_table+3,x
        sta SIZE
        sta cnt
        lda mock_table+4,x
        sta SIZE+1
        sta cnt+1
        lda mock_table+5,x
        sta SIZE+2
        sta ptr2
        lda mock_table+6,x
        sta SIZE+3
        sta ptr2+1
        ora SIZE+2
        bne .copy
        lda ptr1                            ; destinazione 0 = resta dov'e'
        sta SIZE+2
        lda ptr1+1
        sta SIZE+3
        jmp .done
.copy   ldy #0
.cl     lda cnt
        ora cnt+1
        beq .done
        lda (ptr1),y
        sta (ptr2),y
        +inc16 ptr1
        +inc16 ptr2
        +dec16 cnt
        jmp .cl
.done   lda #1
        sta RXFULL
        rts

mock_opt    !byte 0

; opzione, sorgente, lunghezza, destinazione (0 = sul posto); 255 = qualunque altra
mock_table
        +mock_item 49,  m_dir,     m_dir_end - m_dir,         0
        +mock_item 52,  m_weather, m_weather_end - m_weather, 0
        +mock_item 56,  m_sidinfo, m_sidinfo_end - m_sidinfo, 0
        +mock_item 55,  m_sidcode, m_sidcode_end - m_sidcode, $1000
        +mock_item 253, m_sidhdr,  5,                         cassette_buffer
        +mock_item 254, m_prg,     m_prg_end - m_prg,         $0801
        +mock_item 255, m_na,      m_na_end - m_na,           0

m_dir
        !pet "demo.prg", 13
        !pet "hello.prg", 13
        !pet "tune.sid", 13
        !pet "game.prg", 13
m_dir_end

m_weather
        !pet "Time: 12:34:56", 13
        !pet "Roma  18C  nuvoloso", 13, 13
        !pet "(dati simulati)", 13, 13
        !pet "Spazio = menu", 13
m_weather_end

m_sidinfo
        !pet "Mock SID tune (simulato)", 13, 13
        !pet "Sirena su voce 1", 13, 13
        !pet "Spazio = stop", 13
m_sidinfo_end

m_na
        !pet "Dato non disponibile (simulato)", 13
m_na_end

m_sidhdr
        !byte 1, $00, $10, $03, $10         ; flag SID, init $1000, play $1003

; SID di prova, da copiare a $1000: sirena che sale sulla voce 1
m_sidcode
!pseudopc $1000 {
        jmp .init                           ; $1000
        jmp .play                           ; $1003
.init   lda #$0f
        sta $d418
        lda #$00
        sta $d405
        lda #$f0
        sta $d406
        lda #$10
        sta $d401
        lda #$11
        sta $d404
        rts
.play   inc $d401
        rts
}
m_sidcode_end

; programma BASIC di prova, da copiare a $0801:  10 PRINT"HELLO FROM MOCK PRG"
m_prg
        !word $0801 + (.l1_end - m_prg)
        !word 10
        !byte $99, '"'
        !pet "hello from mock prg"
        !byte '"', 0
.l1_end !word 0
m_prg_end

}

CODE_END = *
}
payload_end = *
CODE_LEN = payload_end - payload_rom

!ifdef CART {
    !ifndef C400 {
        !ifndef PRGAUTO {
            * = $9fff                       ; riempie fino a 8k
            !byte 0
        }
    }
}
