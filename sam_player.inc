; ============================================================================
;  sam_player.inc - player SAM del menu C64 (PRG normale a $0801)
; ----------------------------------------------------------------------------
;  Incluso da samplayer.asm (menu), sam_talk.asm e sam_news.asm (autonomi).
;
;  Due entry point fissi, usati come voci di menu:
;       $080D = SAM TALK  (scrivi una riga, RETURN la pronuncia)
;       $0810 = SAM NEWS  (l'ESP32 legge le news correnti)
;
;  L'ESP32 genera i campioni e manda un NMI per campione; qui si legge
;  $DF00 e si scrive il nibble alto in $D418. Comandi ESP32:
;       114 = stop, 115 = riga da pronunciare, 116 = news,
;       117 = fonte delle news (1 byte), 118 = testo delle news a $A000
;
;  Uscita: ripristina il vettore NMI e salta a MENU_INIT
;  (samplayer.asm: $C003 = "jmp restart" del menu; PRG autonomi: BASIC).
;  NOTA: $A000-$BFFF e' anche l'area di staging del menu (HELP, SID, PRG):
;  se il menu la sovrascrive, SAM TALK/NEWS vanno ricaricati (riavvio).
; ============================================================================

!addr basout = $ffd2
!addr getin  = $ffe4

; indirizzo a cui tornare all'uscita (definito dal file che include:
; $c003 = menu per samplayer.prg, $e37b = BASIC warm start per i PRG autonomi)
!ifndef MENU_INIT {
        !error "definire MENU_INIT prima di includere sam_player.inc"
}
!addr ptr1   = $fd
!addr rxdest = $fb                  ; destinazione del blocco ricevuto (indiretta)
!addr ptr2   = $f9                  ; puntatore generico (karaoke)

; indirizzi fissi dei due blocchi e dello schermo per il karaoke
txt_src  = $a000                     ; blocco 1: testo mostrato
tbl_src  = $a500                     ; blocco 2: tabella pezzi (4 byte ognuno)
scr_base = $0478                     ; il testo parte alla riga 3 (colonna 0)
col_base = $d878                     ; color RAM della riga 3

        jmp sam_talk
        jmp sam_news

; ---------------------------------------------------------------------------
;  NMI audio: $DF00 -> nibble alto -> $D418
; ---------------------------------------------------------------------------
sam_nmi
        bit $dd0d
        pha
        lda $df00
        lsr
        lsr
        lsr
        lsr
        sta $d418
        inc samp_cnt                        ; contatore campioni (karaoke)
        bne .nz
        inc samp_cnt+1
.nz     pla
        rti

; ---------------------------------------------------------------------------
;  NMI di ricezione: blocco dall'ESP32 (2 byte lunghezza + 2 di indirizzo).
;  Stesso protocollo del menu (receive_buffer), un NMI per byte.
!zone recv
;  rxstate: 0 fermo, 1 header, 2 dati.
; ---------------------------------------------------------------------------
nmi_recv
        bit $dd0d
        pha
        tya
        pha
        lda rxstate
        beq .idle                  ; fermo: NMI spurio, NON e' un header
        cmp #2
        beq .data
        ldy rxidx
        lda $df00
        cpy #2
        bcs .addr
        sta rxlen,y                 ; lunghezza
        jmp .next
.addr   sta rxdest-2,y              ; indirizzo ($fb/$fc)
.next   iny
        sty rxidx
        cpy #4
        bcc .out
        lda #0
        sta rxidx
        lda rxlen                           ; salva la lunghezza originale:
        sta blk_len                         ; rxlen verra' decrementato a zero
        lda rxlen+1
        sta blk_len+1
        lda rxlen
        ora rxlen+1
        beq .stop                   ; blocco vuoto
        lda rxlen+1
        cmp #$0c                    ; blocchi assurdi (> $0BFF): rifiuta
        bcs .stop
        lda #2
        sta rxstate
        jmp .out
.data   ldy rxidx
        lda $df00
        sta (rxdest),y
        iny
        sty rxidx
        bne .dec
        inc rxdest+1
.dec    lda rxlen
        bne .nohi
        dec rxlen+1
.nohi   dec rxlen
        lda rxlen
        ora rxlen+1
        bne .out
.stop   lda #0
        sta rxstate
.out    pla
        tay
        pla
        rti
.idle   lda $df00                  ; byte arrivato a sproposito: l'ho letto
        jmp .out                   ; (ack, cosi' l'ESP32 non aspetta) e scartato

; ---------------------------------------------------------------------------
;  Voci SID come sorgente di continua: il volume ($D418) scala il mixer,
;  senza continua il suono e' debolissimo (verificato su hardware).
!zone samon
;  Salva il vecchio vettore NMI e installa sam_nmi.
; ---------------------------------------------------------------------------
sam_on
        lda $0318
        sta old_nmi
        lda $0319
        sta old_nmi+1
        lda #$31                            ; IRQ di default del KERNAL: via
        sta $0314                           ; il "colore" del menu, cosi' il
        lda #$ea                            ; testo delle news non lampeggia
        sta $0315
        jsr sam_nmi_on
        ldx #0
.so_v   lda #0
        sta $d402,x                        ; PW low/high = 0
        sta $d403,x
        sta $d405,x                        ; attack/decay = 0
        lda #$f0
        sta $d406,x                        ; sustain 15 / release 0
        lda #$41                           ; pulse + gate
        sta $d404,x
        txa
        clc
        adc #7
        tax
        cpx #21
        bne .so_v
        lda #8
        sta $d418
        rts

; installa il vettore NMI audio (usato anche dopo la ricezione del testo)
sam_nmi_on
        sei
        lda #<sam_nmi
        sta $0318
        lda #>sam_nmi
        sta $0319
        cli
        rts

; ripristina SID e vettore NMI del menu
sam_off
        lda #0
        sta $d418
        sta $d404
        sta $d40b
        sta $d412
        sei
        lda old_nmi
        sta $0318
        lda old_nmi+1
        sta $0319
        cli
        rts

!zone scr
; PETSCII -> screen code (il firmware la converte con screenCode_to_Ascii)
pet2scr
        cmp #$41
        bcc .p_done
        cmp #$5b
        bcs .p_low
        sec
        sbc #$40
        rts
.p_low  cmp #$c1
        bcc .p_done
        cmp #$db
        bcs .p_done
        sec
        sbc #$c0
.p_done rts

; screen code -> PETSCII (inverso di pet2scr), per stampare con CHROUT
scr2pet
        cmp #$01
        bcc .s_done
        cmp #$1b
        bcs .s_hi
        adc #$40                            ; $01-$1a -> $41-$5a (a..z)
        rts
.s_hi   cmp #$41
        bcc .s_done
        cmp #$5b
        bcs .s_done
        adc #$80                            ; $41-$5a -> $c1-$da (A..Z)
.s_done rts

!zone screen
; schermo pulito + messaggio X/Y + cursore acceso
scr_init
        stx msg_lo
        sty msg_hi
        jsr $e544
        lda #14
        sta $d020                           ; bordo azzurro (grafica meno spenta)
        lda #0
        sta $d021
        lda #1
        sta $cc                            ; cursore spento (non deve comparire)
        lda #14
        sta 646
        ldx msg_lo
        ldy msg_hi
; stampa stringa PETSCII terminata da 0 (X=lo, Y=hi)
print_z
        stx ptr1
        sty ptr1+1
.pz_l   ldy #0
        lda (ptr1),y
        beq .pz_e
        jsr basout
        inc ptr1
        bne .pz_l
        inc ptr1+1
        jmp .pz_l
.pz_e   rts

!zone wait
; attende che l'ESP32 sia pronto ($DF00 >= 128), massimo ~1 s: senza
; timeout un ESP32 che non risponde lasciava il C64 bloccato per sempre.
wait_ready
        lda $a2
        sta wr_t0
.w      lda $df00
        cmp #128
        bcs .ok
        lda $a2
        sec
        sbc wr_t0
        cmp #60                             ; ~1 secondo
        bcc .w
.ok     rts

!zone recvwait
; attende la fine della ricezione (rxstate = 0), timeout ~34 s
; (il download delle news via TLS puo' richiedere parecchi secondi)
recv_wait
        lda $a2
        sta rw_t0
        lda #0
        sta rw_wrap
.loop   lda rxstate
        beq .ok
        lda $a2
        cmp rw_t0
        bcs .nowrap
        inc rw_wrap
        lda rw_wrap
        cmp #8
        bcs .timeout
.nowrap lda $a2
        sta rw_t0
        jmp .loop
.timeout
        lda #0
        sta rxstate
.ok     rts

!zone recvnews
; chiede il testo delle news all'ESP32 (comando 118) e lo riceve a $A000
recv_news
        sei
        lda #<nmi_recv
        sta $0318
        lda #>nmi_recv
        sta $0319
        cli
        ; 118 una volta: l'ESP32 risponde con DUE blocchi (testo + tabella pezzi)
        lda #1
        sta rxstate
        lda #0
        sta rxidx
        sta rxlen
        sta rxlen+1
        jsr wait_ready
        lda #118                            ; comando: testo + tabella pezzi
        sta $de00
        jsr recv_wait                       ; blocco 1: testo a $A000
        lda blk_len                         ; lunghezza originale (rxlen e' 0)
        sta text_len
        lda blk_len+1
        sta text_len+1
        lda #1                              ; secondo blocco: tabella a $A500
        sta rxstate
        lda #0
        sta rxidx
        sta rxlen
        sta rxlen+1
        jsr recv_wait                       ; blocco 2: tabella pezzi
        ; num_pieces = blk_len / 4 (shift a 16 bit)
        lda blk_len+1
        lsr
        ror blk_len
        lsr
        ror blk_len
        lda blk_len
        sta num_pieces
        jmp sam_nmi_on                      ; torna al NMI audio

!zone sendline
; invia 115 + la riga scritta (screen code) + terminatore 128
send_line
        lda #115
        sta $de00
        ldx #0
.sl_l   jsr wait_ready
        lda sam_text,x
        jsr pet2scr
        sta $de00
        inx
        cpx sam_len
        bcc .sl_l
        jsr wait_ready
        lda #128
        sta $de00
        rts

; ---------------------------------------------------------------------------
;  SAM TALK
; ---------------------------------------------------------------------------
!zone talk
sam_talk
        jsr sam_on
        ldx #<msg_talk
        ldy #>msg_talk
        jsr scr_init
        lda #0
        sta sam_len
.t_read jsr getin
        beq .t_read
        cmp #13                            ; RETURN: pronuncia
        beq .t_speak
        cmp #3                             ; RUN/STOP: torna al menu
        beq .t_back
        cmp #20                            ; DEL
        beq .t_del
        cmp #32
        bcc .t_read
        cmp #91
        bcs .t_read
        ldx sam_len
        cpx #37
        bcs .t_read
        sta sam_text,x
        inx
        stx sam_len
        jsr basout
        jmp .t_read
.t_del  ldx sam_len
        beq .t_read
        dex
        stx sam_len
        lda #20
        jsr basout
        lda #32
        jsr basout
        lda #20
        jsr basout
        jmp .t_read
.t_speak
        lda sam_len
        beq .t_read
        jsr send_line
        jmp .t_read
.t_back jmp sam_stop

; ---------------------------------------------------------------------------
;  SAM NEWS: l'ESP32 scarica le news, manda il testo (stampato qui a video
;  mentre parla) e poi lo pronuncia. Un tasto (o RUN/STOP) interrompe.
!zone news
; ---------------------------------------------------------------------------
sam_news
        jsr sam_on
        ldx #<msg_news
        ldy #>msg_news
        jsr scr_init
        jsr recv_news                       ; testo a $A000 + tabella a $A500
        lda num_pieces
        bne .testo                          ; se l'ESP32 non ha mandato nulla
        ldx #<msg_notext                    ; lo si dice a video (la voce parte
        ldy #>msg_notext                    ; lo stesso)
        jsr print_z
        lda #116
        sta $de00
        jmp .wait                           ; niente karaoke: aspetta solo il tasto
.testo  jsr print_text                      ; PRIMA stampa il testo...
        lda #116
        sta $de00                           ; ...poi avvia la voce
        lda #0
        sta samp_cnt
        sta samp_cnt+1
        sta cur_piece
        jsr hl_set                          ; evidenzia il pezzo 0
        jsr load_cur
.wait   jsr getin                           ; karaoke + attesa tasto
        bne .stop
        lda num_pieces
        beq .wait
        lda cur_piece
        cmp num_pieces
        bcs .wait                           ; finito: aspetta solo il tasto
        lda samp_cnt+1
        cmp piece_hi
        bcc .wait
        bne .adv
        lda samp_cnt
        cmp piece_lo
        bcc .wait
.adv    jsr hl_advance
        jmp .wait
.stop   jmp sam_stop

; ---------------------------------------------------------------------------
;  Karaoke: il testo (stampato in riga 4) viene evidenziato in giallo man mano
;  che la voce lo pronuncia. Il testo e' <=800 caratteri e non scorre.
; ---------------------------------------------------------------------------
!zone karaoke
; stampa il testo (screen code) ricevuto a $A000 con CHROUT
print_text
        lda #1
        sta $cc                            ; cursore spento
        ldx #3                              ; riga 3, colonna 0: il testo parte
        ldy #0                              ; SEMPRE dalla prima colonna
        sec
        jsr $fff0                           ; PLOT (set cursore)
        lda #<txt_src
        sta ptr1
        lda #>txt_src
        sta ptr1+1
        lda text_len
        sta text_cnt
        lda text_len+1
        sta text_cnt+1
.loop   lda text_cnt
        ora text_cnt+1
        beq .pdone
        ldy #0
        lda (ptr1),y
        jsr scr2pet
        jsr basout
        inc ptr1
        bne .nohi
        inc ptr1+1
.nohi   lda text_cnt
        bne .declo
        dec text_cnt+1
.declo  dec text_cnt
        jmp .loop
.pdone  rts

; ptr1 = tbl_src + A*4 (ingresso di tabella del pezzo A)
tbl_entry
        asl
        asl
        clc
        adc #<tbl_src
        sta ptr1
        lda #>tbl_src
        adc #0
        sta ptr1+1
        rts

; carica piece_lo/piece_hi (campioni) del pezzo cur_piece
load_cur
        lda cur_piece
        jsr tbl_entry
        ldy #2
        lda (ptr1),y
        sta piece_lo
        iny
        lda (ptr1),y
        sta piece_hi
        rts

; riempie hl_len byte di color RAM (a hl_lo/hl_hi) con il colore in A
color_fill
        tax                             ; salva il colore (A verra' sovrascritto)
        lda hl_lo
        sta ptr2
        lda hl_hi
        sta ptr2+1
        ldy #0
.cf     cpy hl_len
        beq .cdone
        txa                             ; ripristina il colore
        sta (ptr2),y
        iny
        bne .cf
.cdone  rts

; calcola il range del pezzo cur_piece e lo evidenzia in giallo
hl_set
        lda cur_piece                       ; dispOff del pezzo -> hl_off
        jsr tbl_entry
        ldy #0
        lda (ptr1),y
        sta hl_off
        iny
        lda (ptr1),y
        sta hl_off+1
        lda cur_piece                       ; dispOff del successivo (o text_len)
        clc
        adc #1
        cmp num_pieces
        bcc .nxt
        lda text_len
        sta hl_off2
        lda text_len+1
        sta hl_off2+1
        jmp .len
.nxt    jsr tbl_entry
        ldy #0
        lda (ptr1),y
        sta hl_off2
        iny
        lda (ptr1),y
        sta hl_off2+1
.len    lda hl_off2                         ; hl_len = hl_off2 - hl_off
        sec
        sbc hl_off
        sta hl_len
        lda hl_off                          ; color RAM = col_base + hl_off
        clc
        adc #<col_base
        sta hl_lo
        lda hl_off+1
        adc #>col_base
        sta hl_hi
        lda #7                              ; giallo
        jmp color_fill

; toglie l'evidenza al pezzo precedente (lo rimette azzurro, come il resto)
hl_revert
        lda #14                             ; azzurro (colore del testo)
        jmp color_fill

; passa al pezzo successivo: toglie il vecchio, evidenzia il nuovo
hl_advance
        lda hl_len
        beq .skip
        jsr hl_revert
.skip   inc cur_piece
        lda cur_piece
        cmp num_pieces
        bcs .adone
        jsr hl_set
        jsr load_cur
.adone  lda #0
        sta hl_len
        sta samp_cnt
        sta samp_cnt+1
        rts

; ---------------------------------------------------------------------------
;  Uscita comune: ferma la voce, ripristina SID/NMI e torna a MENU_INIT
; ---------------------------------------------------------------------------
!zone stop
; ---------------------------------------------------------------------------
sam_stop
        lda #114
        sta $de00
        jsr stop_delay                     ; l'ESP32 smette di mandare NMI in
        jsr sam_off                       ; ~1 ms: senza questa pausa un NMI audio
        lda #$37                           ; tra i due STA $0318/$0319 di sam_off
        sta $01                           ; saltava a un indirizzo a meta' (a volte
        jmp MENU_INIT                     ; si finiva in BASIC).

; attende ~100 ms (6 jiffy, clock IRQ) per far fermare l'ESP32 dopo il 114
stop_delay
        lda $a2
        clc
        adc #6
.dw     cmp $a2
        bne .dw
        rts

; ---------------------------------------------------------------------------
;  Dati
; ---------------------------------------------------------------------------
!zone data
; ---------------------------------------------------------------------------
old_nmi    !word 0
msg_lo     !byte 0
msg_hi     !byte 0
sam_text   !fill 40
sam_len    !byte 0

rxstate    !byte 0                          ; 0 fermo, 1 header, 2 dati
rxidx      !byte 0                          ; indice del byte in corso
rxlen      !word 0                          ; contatore (viene decrementato a 0)
blk_len    !word 0                          ; lunghezza originale del blocco
rw_t0      !byte 0
rw_wrap    !byte 0
wr_t0      !byte 0

text_len   !word 0                          ; lunghezza del testo mostrato
text_cnt   !word 0                          ; contatore di stampa
num_pieces !byte 0                          ; numero di pezzi (karaoke)
cur_piece  !byte 0                          ; pezzo corrente
piece_lo   !byte 0                          ; campioni del pezzo corrente (lo)
piece_hi   !byte 0                          ; campioni del pezzo corrente (hi)
samp_cnt   !word 0                          ; campioni contati dall'NMI
hl_lo      !byte 0                          ; color RAM da evidenziare (lo)
hl_hi      !byte 0                          ; color RAM da evidenziare (hi)
hl_len     !byte 0                          ; lunghezza dell'evidenza (char)
hl_off     !word 0                          ; offset del pezzo (tmp)
hl_off2    !word 0                          ; offset del pezzo successivo (tmp)

msg_talk
        !byte 18                            ; RVS ON
        !pet "    sam talk    "
        !byte 146                           ; RVS OFF
        !pet 13, 13
        !pet "scrivi una frase e premi return", 13
        !pet "/en testo... per l inglese", 13
        !pet "run/stop per tornare al menu", 13, 13
        !pet "  ", 0
msg_news
        !byte 18                            ; RVS ON
        !pet "   SAM NEWS   "
        !byte 146                           ; RVS OFF
        !pet 13
        !pet "Premi un tasto per interrompere", 13, 13
        !byte 0
msg_notext
        !pet 13, "nessun testo dall esp32", 13, 0
