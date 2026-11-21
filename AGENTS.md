# AGENTS.md — C64 ESP32 Cart

> **Ripresa rapida:** all'avvio leggere [`PROJECT_STATE.md`](PROJECT_STATE.md)
> (riepilogo aggiornato al 2026-11-17: stato, hash, bootstrap EPROM, test da
> fare). Il diario esteso è in [`SESSION_NOTES.md`](SESSION_NOTES.md).

Contesto operativo per Pi. Per il diario completo della sessione vedi
[`SESSION_NOTES.md`](SESSION_NOTES.md).

> **Git:** primo commit di questa serie su `pi-work` (2026-11-21). NON
> committare MAI i sorgenti di SAM (`sam.c`, `sam.h`, `render.c`, `render.h`,
> `reciter.c`, `reciter.h`, `RenderTabs.h`, `ReciterTabs.h`, `SamTabs.h`,
> `debug.c`, `debug.h`): software commerciale (SoftVoice), uso personale, gia'
> in `.gitignore`.

## Stato del progetto

- Branch di sviluppo: `pi-work`
- `main`: stabile
- `main` si aggiorna solo con PR da `pi-work` (squash merge); dopo il
  merge `pi-work` va riallineato a `main` (`git reset --hard origin/main` +
  `git push --force-with-lease`)
- Firmware ESP32 principale attualmente flashato: `c64esp32Cart`
- Firmware `audio_test` e `comms_test` sono solo prove separate, non attivi

## Hardware

- MH ET LIVE ESP32MiniKit / ESP32-WROOM-32
- FQBN: `esp32:esp32:mhetesp32minikit`
- Porta USB: `/dev/ttyACM0`
- Il ponticello EXROM che teneva mappata la ROM esterna a `$8000-$9FFF`
  è stato rimosso
- La scheda precedente aveva GPIO4 in corto: sostituita e da non usare

## Comandi di build

### Firmware ESP32 principale

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit c64esp32Cart
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit c64esp32Cart
```

### Bootstrap EPROM 8K (consigliato per la 27C512)

```bash
acme bootstrap.asm
# -> c64esp8kBOOT.bin (8192 byte, da inserire nel file 64K della 27C512)
```

Il bootstrap:
- parte dalla EPROM a `$8000` (CBM80);
- copia un loader di ~340 byte a `$C000`;
- invia il comando ESP32 **112** per alzare EXROM (GPIO2/CLED -> transistor):
  la EPROM `$8000-$9FFF` viene rimossa e resta solo RAM;
- carica `espnewsauto.prg` dalla SD dell'ESP32 a `$0801`;
- salta all'entry point del PRG (indirizzo ricavato dal `SYS` della riga
  BASIC, comando ESP32 60).

Requisiti: firmware ESP32 con comando 112 e `espnewsauto.prg` nella root della SD.
Il setup ESP32 tiene EXROM basso per 1.5 s dopo l'init come fallback, poi lo alza;
il loader invia comunque il comando 112 appena pronto.

**Non chiamare `CLR` (`$a659`) ne' il relink BASIC (`$a533`) prima del salto
all'entry point**: il coldstart da cartuccia inizializza solo il KERNAL, quindi
`TXTTAB`/`VARTAB`/`ARYTAB`/`STREND` possono contenere spazzatura e il bootstrap
va in crash silenzioso (schermo fermo, menu mai avviato). Il PRG fa da solo
l'init BASIC che gli serve. Verificato su hardware reale il 2026-11-17.

#### Banco della 27C512 (importante)

Nella scheda `c64esp32crt1.1` le linee A13/A14/A15 della EPROM hanno un
pull-up a +5V (R9/R10/R11) e un jumper verso GND (J4=JA13, J5=JA14,
J6=JA15). Quindi:

| Jumper JA13/JA14/JA15 | A13 A14 A15 | Banco | Offset nel file 64K |
|---|---|---|---|
| tutti assenti | 1 1 1 | **7** | `0xE000` |
| tutti presenti | 0 0 0 | **0** | `0x0000` |

Senza jumper la EPROM non legge il banco 0: la C64 non trova `CBM80` e va al
BASIC normale (schermo blu con bordo celeste) senza avviare la cartuccia.
Per essere indipendenti dai jumper, replicare gli 8K in tutti i banchi:

```bash
acme bootstrap.asm
tools/make_eprom_image.sh all c64esp8kBOOT.bin   # W27C512_c64esp8kBOOT.bin
```

Stato jumper rilevante anche per la mappatura:
- **J9** (`/OE` della EPROM): deve stare su **ROM-L** (pin 1-2) per la cartuccia
  8K a `$8000-$9FFF`;  
- **J2** (`GAME`): deve essere **aperto** (GAME alto) per il modo ROMLO;
- `/CE` della EPROM (pin 20) e' cablato a GND.

Build di debug del bootstrap (mostra a schermo fino a dove arriva):

```bash
acme -DDBGBOOT=1 -o c64esp8kBOOT_dbg.bin bootstrap.asm
```

- bordo bianco / sfondo nero = EPROM mappata, coldstart eseguito;
- `2` = loader copiato a `$C000`; bordo ciano;
- `3` = ESP32 ha risposto al comando 112; bordo verde;
- `4` = nome file inviato; bordo giallo;
- `5` = PRG ricevuto; bordo viola;
- `6` = entry point ricevuto; bordo verde chiaro;
- poi 16 byte in hex di `$0801`, indirizzo fine pagina, entry point e
  checksum a 16 bit del PRG (atteso `5A47` per `espnewsauto.prg`), quindi
  attesa di un tasto prima del salto.

### C64 ROM cartuccia

```bash
acme -DCART=1 c64esp32cart.asm
# -> c64esp8kROM.bin
```

### C64 PRG diretto a $C000

```bash
acme -DCART=1 -DC400=1 c64esp32cart.asm
# -> c64esp32menu_c000_cart.prg
```

Caricamento:

```basic
LOAD"c64esp32menu_c000_cart.prg",8,1
SYS 49152
```

Il flag `-DC400` è storico: il codice ora parte da `$C000`.

### C64 PRG auto-avviante da $0801 (comodo con SD2IEC)

```bash
acme -DCART=1 -DPRGAUTO=1 c64esp32cart.asm
# -> espnewsauto.prg
```

Caricamento:

```basic
LOAD"espnewsauto.prg",8
RUN
```

Copia il payload a `$C000` e avvia il menu; non serve `,8,1` né `SYS`.

### Test comunicazione

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit comms_test
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit comms_test
cd comms_test && acme c64_comm_test.asm
```

### Test audio

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit audio_test
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit audio_test
cd audio_test && acme c64_audio_test.asm
```

## Sicurezza / Git

- Non committare mai `c64esp32Cart/secrets.h`
- Non committare mai `firmware_backup/`
- Il locale `secrets.h` contiene WiFi e API key; usare
  `c64esp32Cart/secrets.example.h` come template
- Le vecchie credenziali sono ancora nella storia Git: ruotare WiFi e API key
- Repository GitHub **pubblico** dal 2026-11-17 con Issues abilitate:
  <https://github.com/matt199394/c64esp32cart>. Le vecchie credenziali sono
  nella storia pubblica: vanno ruotate.
- Push di sviluppo:

```bash
git add <file>
git commit -m "..."
git push origin pi-work
```

## Vincoli tecnici importanti

- `$D418` è solo **4 bit** (volume SID); l'audio è flebile
- L'audio test usa onda quadra 0/15 a 11 kHz, nibble alto del campione
- I SID di sottofondo devono avere conflict mask `00` rispetto a bitmap,
  sorgente Koala, testo NEWS e codice
- La lista directory è limitata a **12032 byte** per non sovrascrivere i SID
  a `$4000`
- Il codice C64 è a `$C000` e ha poco spazio libero
- Il layout NEWS+Koala usa:
  - screen `$0400-$07E7`
  - charset `$0800-$0FFF`
  - bitmap `$2000-$3F3F`
  - sorgente Koala `$6000-$8710`
  - testo NEWS `$8800-$8FD0`
  - art PETSCII NEWS `$9000-$97CF`
  - template menu `$9800-$9FCF` (screen+colori)
  - staging SEQ/HELP `$A000-$BFFF` (BASIC ROM spenta nel menu con `$01=$36`)
  - codice `$C000-$CFFB`
- I file interni `directory.txt` e `koalist.txt` sono filtrati dalla lista

## Funzioni già presenti

- WiFi opzionale/non bloccante
- SD con lettura diretta in RAM (niente file temporaneo per la directory)
- directory con F1/F7, sottodirectory, `/..`, schermata con cornice;
  apertura file/sottocartelle corretta (lettura dal col 2), 22 righe per pagina
- PRG caricati dalla directory avviati con `$01=$37` + CLR/relink BASIC
- sottomenu NEWS BACKGROUND con frecce/ RETURN: PETSCII, KOALA, CLOCK/WEATHER
- KOALA come sfondo NEWS: slideshow dei Koala della directory corrente
- player SID: F1/F7 SID precedente/successivo, SPACE torna al menu
- slideshow Koala standalone: F1/F7 precedente/successivo, SPACE esce
- NEWS: Koala ogni ~60 s, PETSCII ogni ~30 s; meteo snapshot senza refresh
- meteo standalone aggiornato ogni ~5 s
- nomi file oltre 37 caratteri accorciati sulla SD
- Koala auto-display
- menu grafico generato dall'ESP32 (comando 64): cornice, titolo centrato,
  voci a sinistra, color wash sulla voce selezionata, frecce SU/GIU + RETURN;
  il color wash non cambia il colore dei bordi
- HELP in inglese (comando 63) con descrizione di tutte le funzioni,
  `Q = QUIT DIRECTORY` e avviso di caricare prima il SID da SD
- tutto il testo visibile del programma e' in inglese
- BASIC ROM disattivata nel menu per usare `$A000-$BFFF` come RAM
- template menu salvato in `$9800-$9FCF` e ricaricato se un SID/PRG lo sovrascrive
- richiesta `PLAY LOADED SID? Y/N` prima di Koala slideshow, SEQ slideshow e NEWS
- NEWS: scelta fonte con schermata dedicata (comando 65), frecce + RETURN;
  poi scelta sfondo (comando 67): PETSCII, KOALA, CLOCK/WEATHER;
  scelta sfondo PETSCII / Koala della directory corrente / clock-weather;
  con clock-weather la schermata meteo resta come snapshot
- schermata meteo/ora generata dall'ESP32 (comando 52), testi centrati e
  colorati; orario snapshot; `Clouds` corretto
- slideshow Koala con lista Koala preparata durante la scansione della
  directory (niente attesa di 10-20 s all'avvio) e schermo nero senza cursore
  tra le immagini
- slideshow PETSCII con lista letta da `manifest.txt`/`seqlist.txt`
  (niente scansione lenta delle 681 entry); SID in pausa durante l'NMI
- SID di sottofondo nello slideshow, in pausa durante l'NMI
- NEWS con Koala/PETSCII/clock-weather di sfondo e testo scorrevole
- grafica PETSCII casuale nello schermo NEWS quando non c'è un Koala caricato
  (charset ROM in alto, font 2x2 solo nelle righe dello scroller)
- nome della fonte sotto il titolo e 4 pattern (city, stelle, mattoni, labirinto)
  rigenerati ogni ~60 s durante lo scroller
- NEWS da più fonti selezionabili (ANSA, Rai News, Repubblica, Il Sole 24 Ore,
  Google News, NewsAPI, BBC World, Al Jazeera, SCMP, CGTN World) tramite
  feed RSS/JSON
- slideshow di immagini PETSCII SEQ lette da file `.d64` nella SD dell'ESP32
  (cartella `/petscii-art`) durante lo scroller delle NEWS
- slideshow SEQ standalone (`9.SEQ`) da `/petscii-seq`, con auto-avanzamento,
  F7 avanti, F1 indietro, SPACE per uscire
- script `tools/extract_seq.py` per estrarre i SEQ dai `.d64`
- caricamento SID e PRG con protocollo corretto
- Televideo via PNG RAI convertito in `.koa` con `tools/televideo_fetch.sh`
- script `tools/` per sidreloc e scansione SID safe

## Diagnostica (serial ESP32, 115200)

Il firmware logga ogni comando e ogni passo SAM:

```
CMD n                          ogni byte di comando ricevuto dal C64
PRG: <file> load $xxxx, entry $xxxx, N byte
SAM: fonte news N '<nome>'      comando 117
SAM: testo news al C64, N byte  comando 118
SAM: leggo le news in inglese/italiano
SAM: 'pezzo' -> N campioni
hb: N letture $DF00             ogni 2 s, solo se il C64 legge $DF00
tx: timeout dal C64             sendByte non ha ricevuto l'ack
SAM: comando N durante la frase, stop
```

Cattura da PC senza disturbare la scheda (non tocca DTR/RTS):

```bash
stty -F /dev/ttyACM0 115200 raw -echo clocal -crtscts
nohup timeout 3600 cat /dev/ttyACM0 > /tmp/esp.log &
grep -v '^hb:' /tmp/esp.log        # solo gli eventi
```

Attenzione: dopo `arduino-cli upload` la scheda puo' restare in **download
mode** (`boot:0x3 ... waiting for download`). Per un avvio pulito:

```bash
python3 - <<'EOF'
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.2)
s.dtr = False; s.rts = True; time.sleep(0.15); s.rts = False
EOF
```

Deve comparire `boot:0x13 (SPI_FAST_FLASH_BOOT)` e poi `done!`.

## Test in emulatore (VICE)

I test della parte C64 (senza ESP32) sono in `tools/emu/` con il relativo
README: ricezione del blocco con NMI spuri, stampa lunga con NMI audio a
11 kHz, cosa azzera RAMTAS, verifica del driver NMI su CIA2.

## Stato al 2026-11-21 12:10 (handover)

**Funziona su hardware:**
- bootstrap (`BOOTSD` e EPROM), menu SAM a 5 voci, schermata fonti NEWS;
- SAM TALK (scrive, pronuncia, RUN/STOP torna al menu);
- rientro dal player al menu (non piu' al BASIC): `sam_stop` mette `$01=$37`
  e salta a `$C000` (`main_init`);
- SAM NEWS: testo ricevuto e stampato a video, voce italiana e inglese;
- BBC letta e uscita al menu con il player 988 (fix NMI spuri).

**BUG APERTO RISOLTO (2026-11-21, fix firmware-side).** Scegliendo una fonte
inglese (BBC) a volte non partiva nulla e premendo un tasto si finiva in BASIC.
Causa reale (diversa da quella ipotizzata sotto): il loop scriveva `outByte(0xFF)`
all'INIZIO di ogni giro, riasserivando D7 alto dopo che l'ISR di IO1 l'aveva
abbassato; il byte di parametro del comando 117 (indice fonte) arrivava cosi'
a comando non ancora consumato, sovrascriveva `ch` (volatile) durante il
`Serial.printf("CMD n")` e il `switch(ch)` vedeva l'indice invece di 117 ->
`CMD 117` poi `CMD 7` e `case 117` non girava mai (fonte restava 0 = ANSA).

Fix in `c64esp32Cart/c64esp32Cart.ino` (nessuna modifica ai `.asm`):
- `byte cmd = ch;` catturato SUBITO, prima di ogni chiamata lenta; `switch(cmd)`
  (e i riferimenti `ch` -> `cmd` dentro i case 68/70-105);
- `outByte(0xFF)` ("pronto") ora si scrive SOLO nel ramo `else` (loop fermo),
  non piu' all'inizio del giro: durante l'elaborazione D7 resta basso e il
  parametro arriva solo quando `samReadByte`/`case 51` alzano D7;
- rimossi i due `dataFromC64 = false;` "a freddo" in `case 51` e `samReadByte`
  (erano workaround del vecchio loop e scartavano un byte gia' arrivato).

Verifica fatta: compila (87% flash / 21% RAM), flashato, boot pulito
(`boot:0x13` + `done!`). Da verificare su hardware: 117 -> `SAM: fonte news 6
'BBC World'` deve comparire nel log e le news inglesi devono partire.

Il log che mostrava il problema (12:07):

```
CMD 117      <- scelta fonte
CMD 7        <- ?! il byte del parametro (indice fonte) letto come COMANDO
CMD 119      <- sam_load: ok
PRG: /samplayer.prg load $0801, entry $080D, 986 byte
CMD 60
CMD 118 / CMD 116 / SAM: leggo le news in italiano    <- fonte 0 (ANSA)!
```

Quindi il **comando 117 non e' stato eseguito** (`SAM: fonte news` manca) e la
fonte e' rimasta 0. Causa individuata: **race sul comando con parametro**.
`samnews_go` scrive 117, poi aspetta `$DF00 >= 128` e scrive l'indice; ma il
"pronto" (`$DF00 = $FF`, scritto dal loop all'inizio di ogni giro) e' alto
**anche prima** che il comando sia consumato: se l'indice arriva mentre il
firmware stampa `CMD n` (D7 basso ma `ch` non ancora letto dallo switch), il
parametro sovrascrive `ch` e il comando si perde. Il caso 117 non gira, l'indice
7 viene poi letto come comando (`CMD 7`).

**Fix proposto (da implementare e provare):** usare `$FF` come unico "pronto
per un byte" (i campioni usano solo il nibble alto, quindi `$FF` compare solo
quando il firmware e' in attesa) e aspettare `$DF00 == $FF` prima di mandare un
parametro, al posto di `>= 128`:

- C64: `sam_wait_ready` (menu, `samnews_go`), `wait_ready` (player) e
  `wait_for_ready_to_receive` della build SAM devono confrontare con `$FF`,
  non con 128 (con timeout come adesso);
- firmware: nessun cambio (il loop scrive gia' `outByte(0xFF)` prima di
  leggere il comando; `samReadByte`/`case 51` alzano solo D7 lasciando il resto
  a `$FF`).
- se non basta: far mandare al firmware un ack esplicito dopo ogni comando
  (es. riscrivere `$FF` a fine `switch`) e attendere quello.

Poi verificare in ordine: 117 (fonte) -> 118/116 (news inglesi) -> uscita ->
rientro in SAM TALK -> 115 (riga scritta) -> uscita. Controllare nel log che
`SAM: fonte news 6 'BBC World'` compaia e che non ci siano `CMD n` inattesi.

**Artefatti attuali** (tutti rigenerabili con `acme`, nessuno committato):

| File | Byte | Dove va |
|---|---|---|
| `espsamauto.prg` | 4011 | SD ESP32 (root) |
| `espnewsauto.prg` | 4158 | SD ESP32 (root) |
| `samplayer.prg` | 988 | SD ESP32 (root) e, per prova, sd2iec |
| `sam_talk.prg` / `sam_news.prg` | 982 / 985 | sd2iec (PRG autonomi) |
| `BOOTSD` (= `boot_sd.prg`) | 743 | sd2iec |
| `c64esp8kBOOT.bin` | 8192 | EPROM 8K |
| `W27C512_c64esp8kBOOT.bin` | 65536 | 27C512 (`sha256 15460daa...`) |

Firmware: 87% flash, 21% RAM (`--build-property build.partitions=no_ota`).
Comandi firmware usati: 49-105 (menu), 106-111 (espSAM), 112 (EXROM alto),
113 (build SAM), 114 (stop voce), 115 (riga), 116 (news), 117 (fonte),
118 (testo news), 119 (player SAM). Libero: 120-124.

> **Non committare i sorgenti di SAM** (software commerciale): sono in
> `.gitignore` (vedi sopra). Tutto il resto si committa su `pi-work`.

**Fix uscita SAM NEWS -> a volte BASIC (2026-11-21, seconda sessione).**
Dopo il fix del 117 le news partivano, ma a volte l'uscita da SAM NEWS finiva
al BASIC (READY) invece del menu: nel log mancava `CMD 114` (SAM TALK invece
lo manda sempre). Causa: race sull'aggiornamento del vettore NMI in `sam_off`
(`sta $0318` + `sta $0319` NON atomico, e `sei` NON maschera l'NMI). Con la
voce ancora attiva a 11 kHz (news lunghe, l'utente preme il tasto per fermare),
un NMI audio tra i due `STA` faceva saltare a un indirizzo "a meta'" -> crash
verso BASIC. SAM TALK non lo mostrava perche' la frase era gia' finita quando
si usciva. Fix in `sam_player.inc`: `sam_stop` ora fa `jsr stop_delay` (~100 ms,
6 jiffy su `$A2`) tra il comando 114 e `sam_off`: l'ESP32 smette di mandare NMI
in ~1 ms, quindi il vettore viene aggiornato con la linea NMI ferma. I PRG
rigenerati: `samplayer.prg` 1001, `sam_talk.prg` 995, `sam_news.prg` 998.

**Fix news menu normale "no news available" dopo SAM (2026-11-21).**
Dopo aver usato SAM (TALK/NEWS) il buffer `samBuf` (44 KB) restava allocato:
il fetch TLS delle news del menu normale (comando 70-105/90-105) partiva con
44 KB in meno e `http.GET()` tornava `-1` -> "no news available" (mentre le
SAM news andavano perche' `samFetchNews` fa `samRelease()` prima). Fix in
`c64esp32Cart/c64esp32Cart.ino`: `samSpeak()` ora fa `samRelease()` a fine
frase, e il case 58/70-105 fa comunque `samRelease()` + log dell'heap prima
del fetch. Verificato: compila 87% flash / 21% RAM, flashato, boot pulito.

**Fix Koala slideshow + news sfondo Koala rotti (2026-11-21).**
Colpa di una mia modifica: avevo inserito `case CMD_SAM_PLAYER` (119) TRA
`case 61` (prossimo Koala) e `case 53` (sendFile). `case 61` non aveva il
`break` ma cadeva nel case successivo: prima nel 53, dopo la modifica nel 119
-> al comando 61 mandava `samplayer.prg` invece dell'immagine Koala (nel log:
`CMD 61` seguito da `PRG: /samplayer.prg load ...`), quindi lo slideshow Koala
e lo sfondo Koala delle news non partivano. Fix: `case 61` ora usa
`goto sendFile;` (come faceva gia' `case 54`), niente piu' fall-through.
Compila 87% flash / 21% RAM, flashato, boot pulito.

**Karaoke SAM NEWS + grafica/nome fonte (2026-11-21).**
- **Nome fonte**: il comando 118 ora manda, prima del testo, il nome della fonte
  ("NOME: "), cosi' il C64 lo mostra in testa. Titoli di SAM TALK/NEWS in
  reverse video e bordo azzurro (`scr_init`).
- **Karaoke**: il 118 ora manda DUE blocchi: il testo mostrato a $A000 e una
  tabella pezzi a $A500 (per ogni pezzo: offset nel testo mostrato + campioni).
  Il firmware calcola i pezzi una volta (`samComputePieces`, stessa divisione
  di `samSpeak`, sintetizzando senza riprodurre) e li riusa al 116. Il player
  stampa PRIMA il testo (riga 4, `scr_base=$04a0`, max 800 char -> niente
  scroll), poi avvia la voce e conta i campioni nell'NMI (`samp_cnt`): quando
  raggiunge i campioni del pezzo corrente lo evidenzia in giallo (color RAM
  `col_base=$d8a0`) e rimette il precedente azzurro (`hl_set`/`hl_revert`/
  `hl_advance`). Un tasto ferma tutto. Il testo SAM e' ora limitato a ~780
  caratteri (sta su schermo). Verificato in emulatore (test `tools/emu`:
  hl_set/hl_advance/color_fill, tabella finta, color RAM OK).
  PRG: `samplayer.prg` 1337, `sam_talk.prg` 1331, `sam_news.prg` 1334.

**Fix "nessun testo dall esp32" (2026-11-21).**
`recv_news` salvava `text_len = rxlen` DOPO la ricezione, ma `rxlen` viene
decrementato a zero dall'NMI (`nmi_recv` lo usa come contatore): quindi
`text_len` e `num_pieces` erano sempre 0 -> il player mostrava "nessun testo".
Fix: `nmi_recv` salva la lunghezza originale in `blk_len` quando completa
l'header, e `recv_news` legge `blk_len` (`num_pieces = blk_len >> 2` a 16 bit).
PRG: `samplayer.prg` 1360, `sam_talk.prg` 1354, `sam_news.prg` 1357.

**Rifiniture news (2026-11-21, richieste di Matteo).**
- **numeri -> parole** (`itNumber`/`enNumber`/`convertNumbers` nel firmware):
  le cifre vengono convertite in parole prima di passare a SAM (es. "9" ->
  "nove", "2028" -> "duemila ventotto"), perche' i fonemi ignorano le cifre.
  Vale per SAM NEWS (italiano e inglese); lo scroller normale resta con le cifre.
- **punto tra una notizia e l'altra**: il separatore tra i titoli RSS e' ora
  ". " (in `RssTitleSink` e `fetchNewsApiHeadlines`), quindi vale sia per SAM
  sia per le news del menu normale.
- **accenti**: le vocali accentate diventano `a' e' i' o' u'` (lettera +
  apostrofo) in `utf8ToScrollerAscii`.
- **fonte in maiuscolo** e **layout schermata SAM NEWS**: tolta la riga
  "l esp32 scarica e legge le news", spostata "premi un tasto per interrompere"
  al suo posto, riga vuota, poi "NOME FONTE: testo". `print_text` fa un PLOT
  esplicito a riga 4/colonna 0, cosi' il testo parte sempre dalla prima colonna.
  PRG: `samplayer.prg` 1343, `sam_talk.prg` 1337, `sam_news.prg` 1340.

**Rifiniture schermata SAM NEWS (2026-11-21, richieste di Matteo).**
- fonte in MAIUSCOLO + prima lettera dopo ogni punto in maiuscolo
  (`Ascii_to_screenCode_case` + `capitalizeSentences` nel firmware);
- titolo "SAM NEWS" maiuscolo in reverse, "Premi un tasto per interrompere"
  (P maiuscola) sulla SECONDA riga -> il testo guadagna una riga (parte dalla
  riga 3: `scr_base=$0478`, `col_base=$d878`, PLOT riga 3);
- cap del testo tagliato all'ultimo spazio (niente parole a meta').
  PRG: `samplayer.prg` 1340, `sam_talk.prg` 1334, `sam_news.prg` 1337.

**Altre cose aperte / note:**
- con la fonte inglese il testo letto era italiano: e' una conseguenza del
  bug 117 qui sopra (indice perso -> resta ANSA).
- se compare `READY` con grafica strana: e' il fallback del bootstrap
  (`PRG_ENTRY = 0` -> `$e37b`) oppure un salto a BASIC dopo un crash; nel log
  si vede se mancano `CMD 119`/`PRG: ...`.
- `name_samplayer`/`TXBUFFER` non servono piu' (comando 119): se in futuro si
  rimette un nome in RAM, ricordare che `read_from_screen` scrive fino a
  `TXBUFFER+37` e puo' sovrascrivere i byte che seguono.
- **karaoke** (richiesta di Matteo, non iniziato): il comando 118 dovrebbe
  mandare, dopo il testo, una tabella `(offset carattere, campioni)` per ogni
  pezzo; il C64 conta i campioni nell'NMI e stampa/evidenzia il pezzo in corso,
  cosi' il testo scorre mentre parla invece di apparire tutto subito.

## Prossimi passi

**Da fare (deciso con Matteo, 2026-11-21):**
1. Scrivere `BOOTSD` (`boot_sd.prg`) sulla EEPROM 64K mappata a `$8000`
   (27C512: replicare in tutti i banchi con `tools/make_eprom_image.sh all`).
2. Firmware ESP32: tenere **EXROM basso** all'avvio e, una volta letta la
   EPROM, alzare EXROM per escludere la EEPROM (oggi e' il comando 112 /
   `CLED` mandato dal loader).
3. Ripristinare il **pulsante di reset** del PCB: reset contemporaneo di
   ESP32 e C64 (l'ISR su GPIO15 era stato disattivato in via sperimentale).

Karaoke SAM NEWS e race del comando 117: FATTI (vedi "Stato al 2026-11-21").
Sotto, la lista storica:

1. **Webradio: progetto dedicato** (sketch separato, es. `webradio_test/`), non
   dentro `c64esp32Cart`: solo ciò che serve a decodificare e mandare PCM al
   C64, togliendo JSON/RSS, SAM, koala, menu, directory, meteo.
   Requisiti decisi: MP3/AAC → PCM mono 11025 Hz → 4 bit sul nibble alto di
   `$D418`, stesso handshake NMI di `audio_test`/SAM (un NMI per campione, ack
   il C64 legge `$DF00`), con ring buffer PCM e invio alla massima velocità
   possibile. Partire da un `.mp3` sulla SD e poi da uno stream HTTP.
   Nota misurata: con `PartitionScheme=no_ota` la flash dell'app è 2 MB
   (sketch attuale 54%); il limite vero è la RAM (no PSRAM) e il jitter del
   flusso a 11025 NMI/s, non la flash. Supermon pesa solo 2820 byte: non serve
   toglierlo.
2. **Build SAM del menu C64 (`espsamauto.prg`) — FUNZIONANTE su hardware**
   (provato: menu SAM a 5 voci, SAM TALK, SAM NEWS, news lette da SAM).
   Architettura finale (dopo vari tentativi falliti):
   - il player **non** sta piu' a `$A000`: e' un PRG normale a **`$0801`**
     (`samplayer.asm` -> `samplayer.prg`, ~885 byte) con due ingressi,
     `$080D` = SAM TALK e `$0810` = SAM NEWS (il menu somma 3 all'entry
     ricevuto con il comando 60). All'uscita fa `jmp $C003` = `menu_return`,
     uno stub di 3 byte in testa al payload del menu (`jmp restart`): stesso
     rientro delle altre voci, senza passare dal BASIC. `$C000` resta
     `main_init` (entry usata dal bootstrap). I PRG autonomi `sam_talk.prg` /
     `sam_news.prg` usano invece `$e37b` (BASIC warm start) + `EXIT_TO_BASIC=1`
     per rimettere `$01=$37`.
   - il menu (`-DSAM=1`) ha 5 voci: `SD CARD, HELP, BASIC, SAM TALK, SAM NEWS`
     (`menu_max=4`); `do_supermon`, `do_weather` e `load_weather_template`
     sono esclusi dalla build SAM per fare spazio (`sam_load` + `sam_go`).
     Dalla build SAM sono esclusi anche `do_koala`, `do_news`, `bg_menu`,
     `do_slideshow`, `do_seq_slideshow`, `slideshow_sid_*` e `ask_sid`
     (~300 byte, servono per SAM NEWS e per `menu_return`).
     Controllo permanente `CODE_END > $D000` = errore di assemblaggio
     (build SAM `$cefd`, build normale `$cffd`: margine quasi nullo).
   - il bootstrap **non** carica piu' il player: lo carica il menu per nome
     (51/53/60), quindi `samplayer.prg` deve stare nella root della SD.
   - bug risolti: `Output8BitAry` scriveva su `NULL+3` perche' il `malloc`
     del buffer SAM (66 KB) falliva dopo il download TLS (`max` contiguo
     sceso a ~65 KB) -> ora c'e' il controllo su `malloc` in `sam.c` e
     `SAM_BUF_SECONDS=2` (44 KB) + pezzi da `SAM_MAX_CHARS=16` (~1,6 s);
     `Serial.flush()+delay(100)` prima del download per non perdere i log.
   - firmware: comando 112 azzera `samVariant` (evita la variante SAM
     "ereditata" tra un avvio e l'altro).
   - comandi SAM (il firmware usa 49-105): **113** build SAM, **114** stop,
     **115** riga da pronunciare, **116** pronuncia le news, **117** fonte
     delle news (1 byte, indice in `NEWS_SOURCES`), **118** testo delle news
     al C64 a `$A000` (2 byte lunghezza + 2 di indirizzo + testo in screen
     code).
   **Fatto il 2026-11-21 (a-e):**
   a. ritorno dalle news SAM: ora torna al menu (`MENU_INIT = $c003`), non
      piu' al BASIC. Il `$01=$37` resta: `main_init` non si chiama piu' e
      `restart` rimette da solo `$01=$36`.
   b. menu di `bootsd`: `1) menu senza sam` / `2) menu con sam`, ognuno con
      la riga di dettaglio; tolta la riga "scrivi il testo da qui".
   c. SAM NEWS stampa a video il testo mentre SAM lo legge: il player riceve
      il testo (118) a `$A000` con lo stesso NMI di `receive_buffer`, poi lo
      stampa con `CHROUT` (screen code -> PETSCII) mentre l'NMI audio suona;
      un tasto interrompe. L'IRQ viene riportata a quella di default, cosi'
      il "colore" del menu non fa lampeggiare il testo.
   d. SAM NEWS chiede prima la fonte (schermata 65 come il menu normale,
      `menu_mode=3`) e la manda con il comando 117; il firmware tiene il
      testo in `samNewsText`, quindi 118 e 116 non rifanno il download.
   e. `espSAM/fetch_sam.sh` applica anche il check `malloc` in `sam.c`
      (idempotente) e accetta `SAM_BUF_SECONDS`/`SAM_MAX_CHARS` da ambiente:
      `SAM_BUF_SECONDS=2 SAM_MAX_CHARS=16 ./fetch_sam.sh` rigenera copie
      **identiche** a quelle di `c64esp32Cart/` (verificato con `diff`).
   **Fix del 2026-11-21 (dopo le prove su hardware), problemi visti:**
   - **comandi persi durante la frase**: se il C64 mandava un comando mentre
     l'ESP32 stava ancora parlando (es. 118 dopo SAM TALK), `samPlay` lo
     scartava e `sendByte` poteva restare in attesa per sempre -> niente testo
     delle news e "blocco" al rientro in SAM TALK/NEWS. Ora: qualunque comando
     diverso da 114 ferma la frase e viene riprocessato dal loop
     (`dataFromC64` resta alto), `sendByte` ha un timeout di 50 ms con flag
     `sendFailed`, e la build SAM aspetta il marker di ESP32 fermo prima di
     mandare 51/53 (`sam_wait_idle`: il loop scrive `$FF` quando e' in attesa
     di un comando; i campioni usano solo il nibble alto, quindi `$FF` non
     compare mai durante una frase).
   - **lingua**: `NewsSource` ha il campo `italian` (ANSA/Rai/Repubblica/
     Sole/Google/NewsAPI = it, BBC/Al Jazeera/SCMP/CGTN = en) e SAM NEWS usa
     `SAM_LANG_EN` per le fonti estere. Per SAM TALK si puo' scrivere
     `/en testo...` (o `/it ...`) sulla riga: sulla tastiera C64 il backslash
     non c'e'.
   - **cursore**: spento nella schermata di SAM TALK/NEWS e durante la stampa
     delle news.
   - `sam_load` controlla indirizzo (`$0801`) ed entry (`$080D`) del player e
     in caso di errore mostra "player sam non caricato" invece di saltare a un
     indirizzo sbagliato (blocco).
   - **corruzione di memoria dopo le news** (2026-11-21, visto sull'hardware):
     il ricevitore del player (`nmi_recv` in `sam_player.inc`) con
     `rxstate = 0` trattava l'NMI successivo come header di un nuovo blocco:
     un glitch sulla linea NMI faceva leggere una lunghezza a caso e scriveva
     fino a 64 KB da `$A000`, passando sopra i dati del menu a `$CAxx`
     (`name_samplayer` -> nome file vuoto/"PRG troppo corto", `menu_ready`,
     ...) e sulla sua area codice -> schermo con grafica strana e `READY`.
     Capitava di piu' col testo inglese (piu' lungo -> piu' NMI). Ora: con
     `rxstate = 0` l'NMI viene solo confermato (lettura di `$DF00`) e
     scartato, e i blocchi con lunghezza > `$0BFF` vengono rifiutati. Il
     ricevitore del menu e del bootstrap erano gia' sicuri (con `rxState = 0`
     non scrivono nulla). Verificato in emulatore con 8 NMI spuri dopo il
     blocco: `$A800` resta intatto, il testo si stampa.
   - **nome file corrotto** (`3espsamauto.prg`): un edge spurio su IO1 faceva
     rileggere il byte del comando 51 come primo carattere del nome. Ora il
     firmware azzera `dataFromC64` prima di dare il via alla ricezione
     (`case 51` e `samReadByte`) e ricava la variante dal nome del menu.
   - **nome del player**: `sam_load` non manda piu' il nome del file (era in
     RAM e in due prove su hardware e' arrivato vuoto: `CMD 51` senza nome ->
     "PRG troppo corto"). Ora manda il comando **119** e il firmware apre da
     solo `/samplayer.prg` (fallthrough nel codice del comando 53); il C64
     controlla indirizzo `$0801`, riga BASIC "2026" ed entry `$080D`.
   **Verificato in emulatore (VICE/x64sc, `SDL_VIDEODRIVER=dummy`):** menu di
   `bootsd` a video, avvio di `sam_talk.prg` con echo del testo, uscita con
   RUN/STOP (`READY.` per il PRG autonomo, `menu ok 01=36` con
   `MENU_INIT=$c003` e `$01=$36` come nel menu), stampa di 1180 byte di testo
   da `$A000` con scroll corretto. **Non verificato:** ricezione NMI del
   blocco (serve l'ESP32), handshake 117/118, audio, ritorno al menu vero.
   **Attenzione:** la build simulata `acme c64esp32cart.asm` (senza `-DCART`)
   non assembla (simboli `sid_*`/`show_a000_template` fuori da `!ifdef CART`):
   era gia' rotta prima di queste modifiche e non serve per l'hardware.
3. Eventuale conversione Televideo direttamente sull'ESP32
4. Rifinitura pausa musicale durante il caricamento immagini
5. Fix cosmetico lista directory (`_` mostrato come `←` in `asciiToPET`)

## File di riferimento

- `SESSION_NOTES.md`: diario completo della sessione
- `c64esp32cart.asm`: sorgente C64
- `c64esp32Cart/c64esp32Cart.ino`: firmware principale
- `comms_test/`: test comunicazione
- `audio_test/`: test audio 4 bit
- `tools/`: script di supporto
