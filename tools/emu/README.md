# tools/emu — test del player SAM in emulatore (VICE)

Servono a provare la parte C64 **senza** l'ESP32: `$DF00` viene sostituito da una
locazione RAM (`fakeio = $02FA`) e gli NMI sono generati da CIA2 (timer A).

Tutti i comandi si lanciano da questa cartella, con il player in `sam_player.inc`
della cartella principale.

```bash
cd tools/emu

# 1) copia del player con $DF00 -> fakeio (file generato, non committare)
sed 's/\$df00/fakeio/g' ../../sam_player.inc > sam_player_test.inc

# 2) ricezione di un blocco + 8 NMI spuri (non devono corrompere la RAM) + stampa
acme recv_test.asm
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 10000000 -exitscreenshot /tmp/recv.png -autostart test9.prg

# 3) stampa di 1180 byte con NMI audio a ~11 kHz mentre lo schermo scorre
acme print_long_test.asm        # -> test8.prg (testo gia' a $A000)
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 60000000 -exitscreenshot /tmp/print.png -autostart test8.prg

# 4) cosa azzera RAMTAS ($FF87) chiamata da main_init: esito a video (11101)
acme ramtas_zones.asm
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 8000000 -exitscreenshot /tmp/ram.png -autostart ramtest5.prg

# 5) verifica del "driver" NMI (CIA2 timer A one-shot): stampa numero di NMI
acme nmi_driver_test.asm
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 6000000 -exitscreenshot /tmp/drv.png -autostart drv.prg
```

Note:

- `-keybuf "\x03"` inietta RUN/STOP (usato per provare le uscite del player).
- `recv_test.asm` e `print_long_test.asm` includono `sam_player_test.inc` e
  mettono il player a `$1000` (il `MENU_INIT` punta a uno stub "finto menu").
- l'esito dei test si legge nello screenshot; in caso di errore il programma
  mette il bordo rosso o stampa i valori di stato.

# 6) karaoke: hl_setup + render_window + hl_advance su tabella finta (bordo)
sed 's/\$df00/fakeio/g' ../../sam_player.inc > sam_player_test.inc
acme karaoke_test.asm      # -> kfinal.prg (verde = OK, colore = fail)
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 8000000 -exitscreenshot /tmp/k.png -autostart kfinal.prg

# 8) scroll del testo completo: finestra 22 righe, il pezzo 53 fa scorrere
#    (scroll_line 0 -> 1, evidenza a $DBA0). Esito sul bordo (verde = OK).
acme scroll_test.asm       # -> scroll.prg
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 20000000 -exitscreenshot /tmp/scroll.png -autostart scroll.prg

# 9) tbl_entry oltre il pezzo 63 (regressione overflow A*4 a 8 bit)
acme tbl_test.asm          # -> tbltest.prg (verde = OK per cur_piece 100 e 255)
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 8000000 -exitscreenshot /tmp/tbl.png -autostart tbltest.prg

# 7) color_fill isolato (minimo, senza player)
acme color_fill_test.asm   # -> cftest.prg (verde = OK)
SDL_VIDEODRIVER=dummy timeout 60 x64sc -console -warp -sounddev dummy \
    -limitcycles 8000000 -exitscreenshot /tmp/cf.png -autostart cftest.prg

Attenzione: nei test NON usare `lda #x; sta addr` sulla stessa riga (il `;` in
ACME inizia un commento) e non usare `acme -o file.asm` (rompe il load address
del PRG). Costruire sempre con `acme file.asm` (il `!to` interno decide).
