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
MENU_TEMPLATE = $9800       ; screen+colori del menu (2000 byte), area libera

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
        lda #$36                            ; BASIC ROM spenta: $A000-$BFFF e' RAM
        sta $01                             ; il menu puo' usare la RAM sotto la ROM BASIC
        lda $dd00
        and #$fc
        ora #$03                            ; VIC bank 0
        sta $dd00
        lda #1
        sta $cc                             ; cursore KERNAL spento nel menu
        lda #0
        sta cassette_buffer                 ; 0 = ultimo file ricevuto non e' un SID
        lda #0
        sta $d021                           ; sfondo nero
        lda #14
        sta $d020                           ; bordo azzurro
        lda #0
        sta menu_mode                       ; 0 = main menu
        lda #8
        sta menu_max
        lda #0
        sta news_bg
        lda #1
        sta 646                             ; colore carattere bianco
        lda #0
        ldx #24
.clr    sta $d400,x                         ; azzera SID (ora anche $d400)
        dex
        bpl .clr
        lda #$1b
        sta $d011                           ; text mode, 25 rows
        lda #23
        sta $d018                           ; minuscolo
        lda #$c8
        sta $d016                           ; 40 colonne, no scroll
!ifdef CART {
        jsr menu_prepare                    ; screen+colori ricevuti dall'ESP32
} else {
        ldx #<menu_text
        ldy #>menu_text
        jsr print_z
}
        jsr menu_nav_init

main_loop
        jsr getin
        beq main_loop
        cmp #17                             ; cursore giu'
        bne +
        jmp menu_down
+       cmp #145                            ; cursore su
        bne +
        jmp menu_up
+       cmp #13                             ; invio
        bne main_loop
        jmp menu_enter

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
        jsr $e544                           ; pulisce le scritte del menu
        jmp $a000
} else {
        ldx #<msg_nocart
        ldy #>msg_nocart
        jsr show_msg
        jmp restart
}

!zone do_help
do_help
!ifdef CART {
        lda #63                             ; HELP: stream PETSCII a $A000
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        lda SIZE+3
        cmp #$a0
        beq .show
        jmp restart                        ; HELP not available
.show   jsr show_seq                        ; print the stream with CHROUT
.wait   jsr getin
        beq .wait
        jmp restart
} else {
        ldx #<msg_nocart
        ldy #>msg_nocart
        jsr show_msg
        jmp restart
}

; ---------------------------------------------------------------------------
;  Grafic menu: template received from ESP32, arrow-key navigation
; ---------------------------------------------------------------------------
!ifdef CART {
!zone menu_template
menu_prepare
        lda menu_ready
        bne .show
        inc menu_ready
        lda #64                             ; menu screen+colors -> $9800
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long

.show   lda #<MENU_TEMPLATE                ; screen template -> $0400
        sta ptr1
        lda #>MENU_TEMPLATE
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$04
        sta ptr2+1
        jsr copy920

        lda #<(MENU_TEMPLATE+1000)          ; colors template -> $D800
        sta ptr1
        lda #>(MENU_TEMPLATE+1000)
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$d8
        sta ptr2+1
        jmp copy920
}

!ifdef CART {
!zone a000template
show_a000_template
        lda #<$a000
        sta ptr1
        lda #>$a000
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$04
        sta ptr2+1
        jsr copy920

        lda #<($a000+1000)
        sta ptr1
        lda #>($a000+1000)
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$d8
        sta ptr2+1
        jmp copy920

load_weather_template
        lda #52
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        lda #$14                            ; charset ROM maiuscolo ($1000)
        sta $d018
        jmp show_a000_template
}

!zone dir_show
dir_show
!ifdef CART {
        lda ptr1
        pha
        lda ptr1+1
        pha
        jsr show_a000_template
        pla
        sta ptr1+1
        pla
        sta ptr1
        ldx #1                              ; PLOT: X=riga, Y=colonna (0-based)
        ldy #2
        clc
        jmp $fff0                           ; riga 1, colonna 2
} else {
        jmp $e544
}

!zone menunav
menu_nav_init
        jsr pointer_line_0
        jsr flash_line_0
        ldx #6                             ; first menu entry: row 6
.l      jsr inc_pointer_line
        jsr inc_flash_line
        dex
        bne .l
        jsr set_irq_color                   ; same colour wash as the directory
        rts

menu_up
        lda HOME_LINE
        beq .doneup
        jsr set_default_irq
        jsr reset_default_color
        dec HOME_LINE
        jsr dec_flash_line
        jsr dec_pointer_line
        jsr set_irq_color
.doneup jmp main_loop

menu_down
        lda HOME_LINE
        cmp menu_max
        bcs .donedn
        jsr set_default_irq
        jsr reset_default_color
        inc HOME_LINE
        jsr inc_flash_line
        jsr inc_pointer_line
        jsr set_irq_color
.donedn jmp main_loop

menu_enter
        jsr set_default_irq
        lda menu_mode
        beq .main
        cmp #1
        beq .news
        ; mode 2: scelta dello sfondo NEWS
        lda HOME_LINE
        sta news_bg
        jsr ask_sid
        jsr news
        jmp restart
.news   lda HOME_LINE
        sta news_source
        jsr bg_menu
        jmp main_loop
.main   lda HOME_LINE
        asl
        tax
        lda menu_actions,x
        sta ptr1
        lda menu_actions+1,x
        sta ptr1+1
        jmp (ptr1)

menu_actions
        !word directory, do_supermon, do_help, do_weather, run_prg
        !word do_koala, do_news, do_slideshow, do_seq_slideshow

!zone do_weather
do_weather
        jsr load_weather_template
.loop   lda #15
        sta weather_wait
.slice  jsr delay
        jsr getin
        cmp #32
        beq .done
        dec weather_wait
        bne .slice
        jsr load_weather_template
        jmp .loop
.done   jmp restart

!zone do_koala
do_koala
        lda koala_ok
        bne .show
        ldx #<msg_nokoala
        ldy #>msg_nokoala
        jsr show_msg
        jmp restart
.show   jsr koala_once
        jmp restart

!zone ask_sid
ask_sid
        lda sid_loaded
        beq .nosid
        ldx #<msg_sid
        ldy #>msg_sid
        jsr show_msg
        ora #$20                            ; accetta Y/y e N/n
        cmp #'n'
        beq .nosid
        lda #1
        sta sid_choice
        rts
.nosid  lda #0
        sta sid_choice
        rts

!zone do_news
do_news
!ifdef CART {
        lda #65                             ; news source screen from ESP32
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        jsr show_a000_template
        lda #9
        sta menu_max
        lda #1
        sta menu_mode
        jsr menu_nav_init
        jmp main_loop
} else {
        ldx #<msg_nocart
        ldy #>msg_nocart
        jsr show_msg
        jmp restart
}

!zone news_bg_menu
bg_menu
        lda #67                             ; schermata scelta sfondo NEWS
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        jsr show_a000_template
        lda #2
        sta menu_max
        sta menu_mode
        jsr menu_nav_init
        rts

; ---------------------------------------------------------------------------
;  Slideshow dei Koala nella directory corrente
; ---------------------------------------------------------------------------
!zone do_slideshow
do_slideshow
!ifdef CART {
        jsr ask_sid
        jsr clear_screen_text
        lda #$2d                            ; mask for the Koala slideshow
        sta sid_mask
        lda #62
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long                    ; la lista/cache puo' richiedere tempo
        lda #0
        sta slideshow_sid_started
.next   lda #61
        jmp .load
.prev   lda #54
.load   sta OPTION
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
        cmp #32
        beq .end
        cmp #133
        beq .prev
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
        jsr ask_sid
        jsr clear_screen_text
        lda #$3d                            ; SID must not touch SEQ staging
        sta sid_mask
        jsr slideshow_sid_start
        lda #1
        sta sid_paused
        lda #89
        jsr load_art_cmd
        lda #0
        sta sid_paused
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
.next   lda #1
        sta sid_paused
        lda #87
        jsr load_art_cmd
        lda #0
        sta sid_paused
        jmp .loop
.prev   lda #1
        sta sid_paused
        lda #88
        jsr load_art_cmd
        lda #0
        sta sid_paused
        jmp .loop
.done   jsr slideshow_sid_stop
        jmp restart
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
        lda sid_choice
        beq .nosid
        lda sid_conflict
        and sid_mask                        ; aree da non sovrascrivere
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
.first
!ifdef CART {
        lda #66                             ; border screen -> $A000
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
}
        jsr dir_show
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
        cmp #22
        bcs .nospace
        lda #29                             ; il CR torna al margine: due cursor-right
        jsr basout
        jsr basout
.nospace
        lda line_count
        cmp #22
        bcc .print                          ; pagina piena: si ferma qui

.menu   jsr flash_line_0
        jsr pointer_line_0
        jsr inc_flash_line
        jsr inc_pointer_line
        jsr set_irq_color
.key    jsr getin
        beq .key
        cmp #133                            ; F1: pagina precedente
        bne .f7
        jsr set_default_irq
        jsr page_up
        jsr dir_show
        jmp .page
.f7     cmp #136                            ; F7: pagina successiva
        bne .down
        lda dir_left
        ora dir_left+1
        beq .key                            ; non c'e' altro
        jsr set_default_irq
        jsr dir_show
        jmp .page
.down   cmp #17                             ; cursore giu'
        bne .up
        inc HOME_LINE
        lda HOME_LINE
        cmp #22
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
        jsr inc_pointer_line
        jsr inc_flash_line
        jmp .key
.oku    jsr set_default_irq
        jsr reset_default_color
        jsr dec_flash_line
        jsr dec_pointer_line
        jsr set_irq_color
        jmp .key
.quit   cmp #81                             ; 'q'
        bne .ret
        jmp restart
.ret    cmp #13                             ; invio: carica il file
        beq .load
        jmp .key
.load   jsr set_default_irq
        jsr read_from_screen
        bne .go
        jsr set_irq_color                   ; riga vuota: ignora
        jmp .key
.go     lda #51                             ; invia il nome file / directory
        sta OPTION
        jsr send_buffer
        lda TXBUFFER
        cmp #47                             ; '/' = voce di directory
        bne .notdir
        jmp .newdir
.notdir lda #53                             ; ricevi il file
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
        and #$40                            ; SID sopra il menu template?
        beq .sidok
        lda #0
        sta menu_ready
.sidok  lda #1
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
        jsr koala_once                      ; mostra subito l'immagine caricata
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
        lda #$37                            ; BASIC/Kernal/I-O per il PRG caricato
        sta $01
        jsr $a659                           ; CLR
        jsr $a533                           ; rilega le righe BASIC
        lda #0
        sta $cc
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
        lda page_start
        sta ptr1
        lda page_start+1
        sta ptr1+1
        ldx #0
.back   lda ptr1
        cmp dir_start
        bne .notstart
        lda ptr1+1
        cmp dir_start+1
        beq .atstart
.notstart
        +dec16 ptr1
        ldy #0
        lda (ptr1),y
        cmp #13
        bne .back
        inx
        cpx #22
        bne .back
        +inc16 ptr1
        jmp .found
.atstart
        lda dir_start
        sta ptr1
        lda dir_start+1
        sta ptr1+1
.found
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
        lda #$70
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
        lda #$70
        sta ptr2
        lda #22
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
        ldy #38                             ; non toccare i bordi (col 0 e 39)
cw_sta  sta $d800,y                         ; operando modificato da flash_line_*
        dey
        cpy #1
        bcs cw_sta
        inx
        cpx #13
        bne .keep
        ldx #0
.keep   stx tmp
        rts

reset_default_color
        lda CHAR_COLOR
        ldy #38                             ; non toccare i bordi (col 0 e 39)
cr_sta  sta $d800,y                         ; operando modificato da flash_line_*
        dey
        cpy #1
        bcs cr_sta
        rts

; copia in TXBUFFER il nome file sotto il cursore (codici schermo).
; Esce al primo doppio spazio, massimo 40 caratteri (prima non c'era limite).
; Ritorna A = lunghezza, Z=1 se riga vuota.
!zone read_from_screen
read_from_screen
        ldy #2                              ; dentro il bordo (2 colonne)
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
        ldy #2
.cl     cpy name_len
        beq .end
        lda (ptr2),y
        sta TXBUFFER,x
        inx
        iny
        bne .cl
.end    lda #128
        sta TXBUFFER,x
        lda name_len
        sec
        sbc #2
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
;  BASIC: riaggancia le righe e torna al prompt (poi RUN)
; ---------------------------------------------------------------------------
!zone run_prg
run_prg
        lda #$37                            ; BASIC/Kernal/I-O per il ritorno a BASIC
        sta $01
        lda #0
        sta $cc                             ; cursore normale in BASIC
        jsr $a659                           ; CLR
        jsr $a533                           ; rilega le righe
        jsr $e37b                           ; warm start BASIC (non ritorna)
        jmp $a7ae

; ---------------------------------------------------------------------------
;  Ora (TOD del CIA1) dalla prima riga di testo "xxxxx: HH:MM:SS"
; ---------------------------------------------------------------------------
!zone tod; ---------------------------------------------------------------------------
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

; mostra l'immagine Koala caricata riusando lo slideshow, senza SID
koala_once
        lda #1
        sta slideshow_sid_started
        jsr koala_slide
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
        cmp #136
        beq .exit
        cmp #133
        beq .exit
        lda $a2
        sec
        sbc wait_t0
        cmp #250                            ; circa 4 secondi
        bcc .slide
        lda #0
        sta sid_cmd
        jmp .done
.exit   sta sid_cmd
.done   jsr slide_text_mode
        lda sid_cmd
        rts

slide_text_mode
!ifdef CART {
        jsr clear_screen_text               ; blank/black screen between slides
} else {
        lda #$1b
        sta $d011
        lda #$17
        sta $d018
        lda #$c8
        sta $d016
}
        lda #1                              ; cursore spento anche nello schermo nero
        sta $cc
        rts

; ---------------------------------------------------------------------------
;  SID: init/play dai dati del file (header a cassette_buffer: flag, init, play)
; ---------------------------------------------------------------------------
!zone sid
play_SID
.reload
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
        lda #56                             ; schermata SID player
        sta OPTION
        jsr receive_buffer
        jsr wait_rx
        lda #$36                            ; $A000 = RAM per leggere il template
        sta $01
        jsr show_a000_template
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
        beq .stop
        cmp #136                            ; F7: SID successivo
        beq .next
        cmp #133                            ; F1: SID precedente
        beq .prev
        jmp .loop
.next   lda #68
        jmp .load
.prev   lda #69
.load   sta sid_cmd
        jsr stop_SID
        lda #$1b                            ; il SID puo' aver cambiato il VIC
        sta $d011
        lda #$17
        sta $d018
        lda #$c8
        sta $d016
        lda $dd00
        and #$fc
        ora #$03                            ; VIC bank 0
        sta $dd00
        lda sid_cmd
        sta OPTION
        jsr receive_buffer
        jsr wait_rx
        lda SIZE
        cmp #6
        bne .stopped
        jmp .reload
.stop   jsr stop_SID
.stopped rts

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
!ifndef CART {
menu_text
        !byte 13
        !pet "      C64 ESP32 CARTRIDGE", 13
        !pet "        (SIMULATED ESP32)", 13, 13, 13, 13
        !pet "         SD CARD", 13
        !pet "         SUPERMON", 13
        !pet "           HELP", 13
        !pet "       CLOCK / WEATHER", 13
        !pet "           BASIC", 13
        !pet "           KOALA", 13
        !pet "           NEWS", 13
        !pet "      KOALA SLIDESHOW", 13
        !pet "     PETSCII SLIDESHOW", 13, 0

msg_nocart
        !pet "Cartridge required.", 13, 13
        !pet "Press a key.", 0
}
msg_nokoala
        !pet "No Koala image.", 0
msg_sid
        !pet "PLAY LOADED SID? Y/N", 0

; ---------------------------------------------------------------------------
;  Colori e variabili
; ---------------------------------------------------------------------------
colour      !byte 6,2,4,5,3,7,1,7,3,5,4,2,6,1
tmp         !byte 0
colourdelay !byte 0
CHAR_COLOR  !byte 1

OPTION      !byte 0                         ; comando per l'ESP32
SIZE        !byte 0,0,0,0                   ; lunghezza (2) e indirizzo (2) dell'ultimo blocco
rxState     !byte 0                         ; 0 = fermo, 1 = header, 2 = dati
RXFULL      !byte 0
RYINDEX     !byte 0                         ; (erano $02,$03,$04: $03/$04 sono vettori BASIC)
wait_t0     !byte 0
wait_wrap   !byte 0
weather_wait !byte 0
sid_cmd     !byte 0

HOME_LINE   !byte 0
menu_mode   !byte 0
menu_max    !byte 8
news_bg     !byte 0
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
menu_ready  !byte 0
news_source !byte 0
msg_lo      !byte 0
msg_hi      !byte 0

TXBUFFER    !pet "test message", 128
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
news
        lda news_bg
        cmp #1
        bne .nokoala
        lda #62                             ; reset lista Koala della directory
        sta OPTION
        jsr receive_buffer
        jsr wait_rx
        lda #61                             ; primo Koala
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        lda SIZE+3
        cmp #>PICTURE
        bne .nokoala
        lda #<3000
        sta art_interval
        lda #>3000
        sta art_interval+1
        lda #0
        sta art_cnt
        sta art_cnt+1
        jmp .koalaok
.nokoala lda #0
        sta news_bg                         ; nessun Koala: torna a PETSCII
.koalaok
        lda news_bg
        cmp #2
        beq .keepbg                         ; clock/weather: template gia' a video
        jsr clear_screen_text
.keepbg lda #57
        sta OPTION
        jsr receive_buffer
        jsr wait_rx
        lda news_source
        clc
        adc #70
        sta OPTION
        lda news_bg
        bne .send
        lda OPTION
        clc
        adc #20                             ; 90..99: NEWS + PETSCII art
        sta OPTION
.send   jsr receive_buffer
        jsr wait_rx_long
        lda #0
        sta RXFULL
        lda news_bg
        bne .noart
        jsr copy_art                        ; grafica accodata al testo
.noart  lda #0
        sta split_mode
        lda news_bg
        cmp #1
        bne .splitdone
        lda #1
        sta split_mode
.splitdone
        lda #0
        sta sid_bg_active
        lda sid_loaded
        beq .nosid
        lda sid_choice
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
.nosid
        lda #<3000
        sta art_interval
        lda #>3000
        sta art_interval+1
        lda news_bg
        cmp #1
        beq .noinitial
        ; carica subito il primo art / immagine SEQ prima di far partire lo scroller
        lda art_interval
        sta art_cnt
        lda art_interval+1
        sta art_cnt+1
        jsr art_tick
.noinitial
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

copy_weather_top
        lda #<$a000
        sta ptr1
        lda #>$a000
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$04
        sta ptr2+1
        jsr copy_top
        lda #<($a000+1000)
        sta ptr1
        lda #>($a000+1000)
        sta ptr1+1
        lda #$00
        sta ptr2
        lda #$d8
        sta ptr2+1
        jmp copy_top

copy_top
        ldy #0
        ldx #3
.wpages lda (ptr1),y
        sta (ptr2),y
        iny
        bne .wpages
        inc ptr1+1
        inc ptr2+1
        dex
        bne .wpages
        ldx #152
.wtail  lda (ptr1),y
        sta (ptr2),y
        iny
        dex
        bne .wtail
        rts

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
        lda news_bg
        cmp #2
        bne .notweather
        lda art_interval
        ora art_interval+1
        bne .notweather
        jmp .atend                          ; meteo NEWS: snapshot, niente refresh
.notweather
        lda split_mode
        beq .at_run
        lda news_bg
        cmp #1
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

        lda news_bg
        cmp #1
        beq .koala_pic
        cmp #2
        beq .weather

        lda #86
        jsr load_art_cmd

        lda SIZE+3
        cmp #$a0
        bcs .seq
        jsr copy_art
        lda #<1500
        sta art_interval
        lda #>1500
        sta art_interval+1
        jmp .at_after
.seq    jsr show_seq
        lda #<1500
        sta art_interval
        lda #>1500
        sta art_interval+1
        jmp .at_after
.koala_pic
        lda #61                             ; prossimo Koala
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        lda SIZE+3
        cmp #>PICTURE
        bne .koala_done
        jsr koala_prepare
.koala_done
        jmp .at_after
.weather
        lda #52
        sta OPTION
        jsr receive_buffer
        jsr wait_rx_long
        jsr copy_weather_top
        lda #0                              ; snapshot: niente refresh in NEWS
        sta art_interval
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
sid_choice  !byte 0
sid_mask    !byte $2d
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
