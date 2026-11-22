# C64 ESP32 Cart — note sessione `pi-work`

Ultimo aggiornamento: 2026-11-17 (sessione serale, vedi sezione 15)
Branch di sviluppo: `pi-work`  
`main`: versione stabile; contiene il merge delle PR #1 e #2
(commit `4cedc61`). Repository **pubblico** su GitHub con Issues abilitate.

Questo file serve a recuperare il contesto della sessione di lavoro.

---

## 1. Hardware

- Scheda: **MH ET LIVE ESP32MiniKit / ESP32-WROOM-32**
- FQBN: `esp32:esp32:mhetesp32minikit`
- Flash: 4 MB, DIO, 80 MHz
- Porta USB tipica: `/dev/ttyACM0`

### Pin usati

```text
D0..D7 C64 : 4, 33, 14, 27, 13, 22, 17, 26
NMI        : 32
C64 RST    : 21
IO1        : 35
IO2        : 36
SDATA      : 34
SCLK       : 25
PLOAD      : 16
SD CS      : 5
SD MOSI    : 23
SD MISO    : 19
SD SCK     : 18
```

Il ponticello che teneva basso **EXROM** è stato rimosso: teneva mappata la ROM esterna a `$8000-$9FFF` e corrompeva la lettura della RAM (Koala/colori).

La scheda precedente aveva **GPIO4 in corto** verso GND: sostituita. La scheda difettosa non va usata.

---

## 2. Firmware ESP32 principale

File:

```text
c64esp32Cart/c64esp32Cart.ino
c64esp32Cart/secrets.h            (locale, ignorato da git)
c64esp32Cart/secrets.example.h    (template committabile)
```

`secrets.h` contiene SSID, password WiFi, OpenWeather API key e NewsAPI key.
**Non committarlo.**

Compilazione/flash:

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit c64esp32Cart
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit c64esp32Cart
```

### Funzioni implementate

- WiFi non bloccante: se non configurato o assente, SD e menu funzionano lo stesso
- SD SPI esplicita sui pin della MiniKit
- directory SD costruita in RAM e inviata direttamente al C64
- niente più `/directory.txt`
- cache opzionale `koalist.txt` per lo slideshow
- filtro dei file interni `directory.txt` / `koalist.txt`
- limite lista directory a **12032 byte** per non sovrascrivere i SID a `$4000`
- meteo/ora con timeout lungo
- NEWS:
  - UTF-8 convertito in ASCII compatibile col charset 2x2
  - endpoint NewsAPI `everything?q=italia&language=it`
  - testo a `$8800`, charset a `$0800`
- Koala:
  - riconoscimento `.kla` / `.koa`
  - header 2 byte `$00 $60` saltato
  - destinazione forzata a `$6000`
  - visualizzazione automatica dopo il caricamento
- SID:
  - lettura corretta del data offset PSID
  - load address embedded nei primi 2 byte dati
  - invio esplicito di lunghezza + destinazione
  - conflict mask per evitare sovrapposizioni con bitmap/Koala/NEWS/codice
- PRG:
  - invio esplicito di lunghezza + load address
  - comando 60 per l'entry point
  - parsing dello stub BASIC `SYS xxxx`
- Slideshow Koala:
  - comandi 61/62
  - lista Koala in RAM preparata durante la scansione della directory
  - fallback: se la directory non e' ancora pronta, `buildKoalaList` riscansiona la SD
  - SID di sottofondo con pausa durante il trasferimento NMI
- Audio test separato (vedi sotto)

### Comandi protocollo utili

```text
49 = directory
50 = Supermon+
51 = ricevi nome file / directory
52 = schermata meteo/ora (screen+colori a $A000)
53 = file PRG/SID/Koala
55 = dati SID
56 = info SID
57 = charset NEWS
58 = NEWS
60 = entry point PRG
61 = prossimo Koala slideshow
62 = reset lista slideshow
63 = pagina HELP (stream PETSCII a $A000)
64 = template grafico del menu (screen+colori a $9800)
65 = schermata scelta fonte NEWS (a $A000)
66 = schermata bordo directory SD (a $A000)
67 = schermata scelta sfondo NEWS (a $A000)
70 = avvia audio test
71 = ferma audio test
```

---

## 3. Programma C64 — `c64esp32cart.asm`

Build cartuccia ROM:

```bash
acme -DCART=1 c64esp32cart.asm
# -> c64esp8kROM.bin (menu completo incorporato, build storica)
```

### Bootstrap EPROM 8K (consigliato)

```bash
acme bootstrap.asm
# -> c64esp8kBOOT.bin (8192 byte)
```

Il bootstrap copia un loader minimale a `$C000`, invia il comando ESP32
**112** per alzare EXROM (GPIO2/CLED tramite transistor), carica
`espnewsauto.prg` dalla root della SD a `$0801` e salta all'entry point.
In questo modo la EPROM non deve piu' contenere tutto il menu e si puo'
aggiornare il PRG sulla SD senza riprogrammare la 27C512.

Build PRG diretto a `$C000` con funzioni cartuccia:

```bash
acme -DCART=1 -DC400=1 c64esp32cart.asm
# -> c64esp32menu_c000_cart.prg
```

Il flag `-DC400` è storico: il codice ora parte da **`$C000`**.

Caricamento:

```basic
LOAD"c64esp32menu_c000_cart.prg",8,1
SYS 49152
```

Marker menu corrente:

```text
C64 ESP32 CARTRIDGE / by M.Angelini
```

SHA256 ultimo PRG (`c64esp32menu_c000_cart.prg`):

```text
3d17e8a7bd14599eca27f6176c48fbaaffbf4a548c7d8b70591159951a58ffb2
```

### Mappa memoria C64 usata

```text
$0400-$07E7  screen RAM
$0800-$0FFF  charset NEWS 2x2
$1000-$3EFF  lista directory (max 12032 byte)
$2000-$3F3F  bitmap Koala (VIC bank 0, base $2000)
$6000-$8710  sorgente Koala
$8800-$8FD0  testo NEWS
$9000-$97CF  art PETSCII generata per le NEWS
$9800-$9FCF  template menu (1000 screen + 1000 colori)
$A000-$BFFF  staging SEQ e HELP (BASIC ROM disattivata con $01=$36)
$C000-$CFFB  codice (build C000k)
$D800-$DBE7  color RAM
```

### Modifiche C64 principali

- codice spostato da `$C400` a `$C000` per spazio
- routine Koala corretta:
  - bitmap + video matrix + color RAM
  - `$D011/$D018/$D016` corretti
  - auto-display dopo load
- cursore disabilitato nel menu, riabilitato in BASIC
- menu iniziale grafico generato dall'ESP32 (comando 64):
  - screen+colori inviati direttamente a `$9800-$9FCF`
  - titolo centrato in alto, voci allineate a sinistra, cornice sul bordo
  - navigazione con frecce SU/GIU + RETURN
  - la voce selezionata ha lo stesso effetto "flush"/color wash della directory
  - il color wash non tocca le colonne 0 e 39: i bordi restano del colore originale
- prima di Koala slideshow, SEQ slideshow e NEWS viene chiesto
  `PLAY LOADED SID? Y/N` (solo se un SID e' caricato; il SID va caricato prima da SD)
- NEWS:
  - scelta fonte con schermata dedicata (comando 65), frecce + RETURN
  - scelta sfondo con schermata dedicata (comando 67), frecce + RETURN:
    PETSCII, KOALA, CLOCK/WEATHER
  - KOALA come sfondo NEWS usa lo slideshow dei Koala della directory corrente;
    se non ce ne sono, ripiega su PETSCII
  - NEWS: Koala ogni ~60 s, PETSCII ogni ~30 s; meteo e' uno snapshot senza
    refresh periodico
  - scelta sfondo: PETSCII, Koala della directory corrente o clock/weather
  - con sfondo clock/weather la schermata meteo resta al suo posto come
    snapshot: non parte lo slideshow PETSCII e non c'e' refresh periodico
- directory:
  - schermata con cornice (comando 66), testo indentato dentro il bordo
  - F1/F7, frecce e RETURN funzionano come prima
  - `read_from_screen` legge dal col 2: apertura file e sottocartelle corretta
  - PLOT con X=riga, Y=colonna: X=1/Y=2 porta il testo a riga 1, colonna 2
  - 22 righe per pagina, cosi' l'ultima non tocca il bordo inferiore
  - dopo ogni CR due cursor-right: `/..` e i nomi dei file restano a colonna 2
    e la lettura dal col 2 funziona
  - F1/F7 salvano `ptr1` sullo stack prima di copiare il template: la pagina
    successiva/precedente riparte dal puntatore dati corretto
  - la scansione della directory si ferma quando la lista e' piena: evita
    timeout/hang su root con moltissimi file
  - il color wash non tocca i bordi
- meteo/ora: schermata 40x25 generata dall'ESP32 (comando 52), colori e testi
  centrati; dal menu si aggiorna ogni ~5 s, come sfondo NEWS resta uno snapshot
- `Clouds` corretto: usa `obj["clouds"]["all"]` (prima restava sempre 0)
- nel menu `$01=$36`: BASIC ROM disattivata, `$A000-$BFFF` disponibile come RAM
- voce HELP: l'ESP32 invia un testo PETSCII a `$A000` (comando 63), il C64 lo
  stampa con `show_seq` e torna al menu
- Supermon: prima del salto a `$A000` il C64 pulisce lo schermo, cosi' le
  scritte del menu non si sovrappongono al monitor
- il vecchio comando `3.Msg` e' stato rimosso
- se un SID o un PRG puo' sovrascrivere `$9800`, il template menu viene
  ricaricato al successivo ritorno al menu
- directory:
  - F7 pagina successiva
  - F1 pagina precedente
  - `/..` per tornare al parent
  - paginazione locale senza riscaricare i dati
- NEWS:
  - split-screen Koala + due righe di testo
  - charset `$0800`, testo `$8800`
  - niente CINT in modalità NEWS, per non toccare `$0800`
- slideshow:
  - menu `8.Koala slideshow`
  - 4 secondi per immagine
  - SPACE per uscire
  - SID di sottofondo se compatibile
  - SID in pausa durante il trasferimento NMI dell'immagine
- SID:
  - `$01 = $35` durante init/play
  - `sei` attorno all'init
  - IRQ raster a linea 250
  - `$01 = $37` ripristinato alla fine
- PRG:
  - lunghezza = filesize - 2
  - destinazione = load address
  - entry point via comando 60
  - prima del `jmp (PRG_ENTRY)` viene ripristinato `$01=$37` (BASIC/Kernal/I-O)
    e vengono chiamati CLR/relink BASIC, altrimenti i PRG non partono

---

## 4. Test comunicazione C64/ESP32

Cartella:

```text
comms_test/
```

Firmware:

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit comms_test
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit comms_test
```

C64:

```basic
LOAD"c64_comm_test.prg",8
RUN
```

oppure versione `$C400`:

```basic
LOAD"c64_comm_test_c400.prg",8,1
SYS 50176
```

Il test ha dato **0 errori** dopo la sostituzione della scheda con GPIO4 guasto.

---

## 5. Audio test 4 bit / 11 kHz

Cartella:

```text
audio_test/
```

Firmware:

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit audio_test
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit audio_test
```

C64:

```basic
LOAD"c64_audio_test.prg",8
RUN
```

Stato:

- il trasferimento funziona
- il tono si sente ma è **molto flebile**
- il volume SID `$D418` è solo 4 bit
- il test usa onda quadra 0/15 per massimizzare l'energia
- l'audio test è stato lasciato come esperimento; il firmware principale è stato ripristinato

Prossimo passo eventuale: webradio con decoder MP3/AAC sull'ESP32, PCM mono 11 kHz, nibble alto verso `$D418`. Il limite di volume resta il DAC a 4 bit del SID.

---

## 6. Tool e materiale su SD

### Script in `tools/`

```text
relocate_sids.sh       riloca SID con sidreloc
scan_sids_ranges.py    scansione veloce HVSC
relocate_safe_sids.py  rilocazione finale solo SID safe
televideo_fetch.sh     scarica pagina Televideo e la converte in .koa
```

### Directory esterne al repo (sulla macchina)

```text
/home/matteo/c64/all_gif_koa_split/   625 Koala in 13 cartelle da 50
/home/matteo/c64/C64Music_safe_reloc/ 5416 SID safe (46 MB)
/home/matteo/c64/televideo_koa/       pagine Televideo .koa
/home/matteo/c64/sid_reloc/           26 SID rilocati
/home/matteo/c64/sid_reloc_safe/      SID safe per Koala+NEWS
```

### SID safe

Sono stati considerati safe quelli con range finale dentro:

```text
$4000-$5FFF  oppure  $9000-$BFFF
```

I SID che si sovrappongono a bitmap, sorgente Koala, testo NEWS o codice vengono esclusi dalla musica di sottofondo.

---

## 7. Televideo

Endpoint usato:

```text
https://www.rainews.it/tlvd/images/nazionale/16_9_page-100.png
```

La pagina è un PNG 644x400. Lo script:

1. ritaglia 640x400
2. riduce a 320x200
3. quantizza a 8 colori, fallback 4
4. converte con `png2prg -m koala`

Esempio:

```bash
tools/televideo_fetch.sh 100
```

Output in:

```text
/home/matteo/c64/televideo_koa/tlvd_100.koa
```

Non è OCR: è la pagina Televideo convertita in immagine Koala.

---

## 8. Git e sicurezza

Il repository GitHub è **pubblico** dal 2026-11-17:
<https://github.com/matt199394/c64esp32cart> (vedi sezione 15.8).
Le credenziali elencate piu' sotto sono quindi da considerarsi esposte e
**vanno ruotate**.

Branch di lavoro:

```text
pi-work
```

`main` resta stabile; si aggiorna solo con PR da `pi-work` (squash merge).

### File ignorati

```text
c64esp32Cart/secrets.h
firmware_backup/
```

`firmware_backup/` contiene un dump completo della flash, inclusi NVS,
credenziali WiFi e calibrazione: non va pushato.

### Importante

Le vecchie credenziali sono ancora nella **storia Git** dei commit precedenti.
Il repository è stato reso privato, ma è comunque consigliato:

- cambiare la password WiFi
- rigenerare le API key OpenWeather e NewsAPI
- aggiornare `c64esp32Cart/secrets.h`

Per il push:

```bash
git add .gitignore README.md SESSION_NOTES.md
git add c64esp32Cart/c64esp32Cart.ino
git add c64esp32Cart/secrets.example.h
git add c64esp32cart.asm audio_test comms_test tools
git commit -m "Add session notes and development changes"
git push origin pi-work
```

Non aggiungere `c64esp32Cart/secrets.h` né `firmware_backup/`.

---

## 9. Prossimi passi possibili

1. Webradio:
   - decoder MP3/AAC sull'ESP32
   - downmix mono e resampling 11 kHz
   - nibble alto verso `$D418`
   - valutare un piccolo amplificatore o DAC esterno per il volume
2. Televideo live:
   - download PNG e conversione a bordo ESP32
3. Rifinitura slideshow:
   - eliminare la pausa musicale durante il caricamento NMI
   - cache/filename list già presente
4. PR finale `pi-work → main`

---

## 10. Fonti NEWS selezionabili

Il comando NEWS non usa più una sola fonte fissa. Premendo `7` compare un
sottomenu con:

1. ANSA (ultima ora)
2. Rai News
3. Repubblica
4. Il Sole 24 Ore
5. Google News Italia
6. NewsAPI

Il C64 invia il comando `70+indice` all'ESP32. Per i feed RSS l'ESP32 legge la
risposta in streaming con `HTTPClient::writeToStream()`, che decodifica anche
il `Transfer-Encoding: chunked`; un piccolo parser estrae i `<title>` dentro
gli `<item>` (inclusi i titoli in CDATA) senza tenere in RAM l'intero XML.
NewsAPI resta disponibile come sesta voce e usa il percorso JSON già esistente.

Il testo viene comunque limitato a 2000 caratteri e convertito con
`utf8ToScrollerAscii()` prima di essere inviato a `$8800`.

Per aggiungere o cambiare una fonte basta modificare `NEWS_SOURCES[]` in
`c64esp32Cart/c64esp32Cart.ino` e la relativa voce in `news_menu` nel sorgente
C64.

### Nota pratica: prove da SD2IEC senza EPROM

Con la SD2IEC si può evitare la programmazione della 27C512 caricando il
menu come PRG. Due varianti:

```basic
REM PRG a $C000 (richiede ,8,1)
LOAD"espnews.prg",8,1
SYS 49152
```

```basic
REM PRG auto-avviante a $0801
LOAD"espnewsauto.prg",8
RUN
```

La seconda si compila con:

```bash
acme -DCART=1 -DPRGAUTO=1 c64esp32cart.asm
```

`espnewsauto.prg` contiene lo stub BASIC a `$0801`, copia il payload a `$C000`
e salta al menu. La partenza è stata verificata in VICE. Attenzione: il vecchio
`esp32.prg` presente sulla SD non contiene il sottomenu delle fonti NEWS.

---

## 11. Grafica PETSCII durante le NEWS

Quando non c'è un'immagine Koala caricata, l'ESP32 genera una schermata
PETSCII casuale (23 righe x 40 colonne) e la invia al C64 prima che parta lo
scroller. La zona alta usa il charset ROM del C64 a `$1000`; le due righe
dello scroller continuano a usare il font 2x2 a `$0800`.

- comandi C64: `70..75` = NEWS senza grafica, `80..85` = NEWS + grafica
- l'art viene inviata a `$0400` (screen) e `$D800` (colori)
- il testo NEWS resta a `$8800`
- l'art non viene usata se `koala_ok` è attivo, per non sovrascrivere la
  screen matrix del Koala
- dopo `wait_rx_long` il C64 chiama `wait_quiet`, che aspetta 10 jiffy di
  linea inattiva: così tutti i blocchi (testo + grafica) sono completi prima
  di far partire lo scroller, evitando che l'NMI sovrascriva `ptr1/ptr2`
- il raster split è stato adattato: top con `d018=$14`, righe 23-24 con
  `d018=$12`

La resa è stata verificata in VICE iniettando l'art generata e avviando lo
scroller.

### Aggiornamento grafica NEWS

- Sotto `C64 NEWS` viene mostrato il nome della fonte corrente
  (`NEWS_SOURCES[newsSourceIndex].name`).
- Ogni richiesta NEWS sceglie casualmente uno di 4 pattern PETSCII:
  city/skyline (quello originale), cielo stellato, muro di mattoni, labirinto.
  Il pattern non si ripete immediatamente.
- Durante lo scroller il C64, ogni ~3000 frame (circa 60 s PAL), invia il
  comando `86` all'ESP32. L'ESP32 genera un nuovo pattern e lo invia in un
  blocco unico da 1840 byte a `$9000`/$9398; il C64 lo copia in
  `$0400`/`$D800` con `copy_art`.
- Il testo+grafica iniziali viaggiano in un unico blocco da 3888 byte a
  `$8800`: 2048 byte di testo (padding compreso), poi 920+920 byte di grafica.
  In questo modo il C64 aspetta una sola ricezione prima di avviare lo
  scroller.
- Durante l'aggiornamento lo scroller resta fermo e il SID viene messo in
  pausa; `ptr1` viene salvato/ripristinato perché l'NMI lo usa per la
  destinazione.
- La zona di staging NEWS è quindi `$8800-$972F`; il controllo di conflitto
  SID è stato esteso a questo intervallo.

---

## 12. Fonti internazionali e slideshow SEQ

### Fonti NEWS

Aggiunte 4 fonti in inglese (totale 10, tasti `1-9,0`):

- BBC World
- Al Jazeera
- SCMP (South China Morning Post)
- CGTN World

I feed CNN RSS non sono più raggiungibili; al loro posto sono state usate
SCMP/CGTN per la Cina e BBC/Al Jazeera per l'estero.

Comandi C64 aggiornati:

- `70..79` = NEWS senza grafica (Koala attivo)
- `90..99` = NEWS + grafica PETSCII generata
- `86` = richiesta nuovo art durante lo scroller

### Slideshow PETSCII da .d64

L'ESP32 legge i file `.d64` dalla propria SD, cerca i file SEQ nel directory
CBM DOS e ne estrae il contenuto. Il C64 riceve lo stream SEQ a `$A000` e lo
mostra con la routine `show_seq`, che usa il KERNAL `CHROUT` (quindi interpreta
i codici PETSCII: clear, colori, cursor, ecc.).

- cartella SD: `/petscii-art` (fallback: radice della SD)
- solo file SEQ; quelli oltre 8000 byte (animazioni lunghe) vengono saltati
- il C64 legge `$A000` con BASIC bancata fuori (`$01=$36`), poi ripristina
- il primo art viene richiesto subito prima di avviare lo scroller
- ogni 750 frame (~15 s PAL) arriva una nuova immagine SEQ; se non ci sono
  immagini valide, l'ESP32 invia un pattern generato e il timer torna a
  3000 frame (~60 s)
- durante il caricamento lo scroller resta fermo e il SID è in pausa
- le due righe inferiori vengono ripulite per lasciare spazio allo scroller

Layout NEWS aggiornato:

- testo + art generata: `$8800-$97CF`
- stream SEQ: `$A000-$BFFF`
- codice: `$C000-$CFDF`

---

## 13. Slideshow SEQ standalone

Nuova voce di menu: `9.SEQ`.

- Avanzamento automatico ogni ~5 s (250 jiffy PAL).
- `F7` = immagine successiva, `F1` = precedente, `SPACE` = torna al menu.
- Le immagini vengono lette da `/petscii-seq` sulla SD dell'ESP32.
- Comandi C64 -> ESP32: `89` prima immagine, `87` successiva, `88` precedente.
- L'ESP32 legge la lista da `/petscii-seq/manifest.txt` (creato da
  `extract_seq.py`) oppure da `/petscii-seq/seqlist.txt`; solo come ultimo
  fallback scandisce la directory e crea la cache. In questo modo l'avvio
  dello slideshow non richiede piu' 30 secondi.
- L'ESP32 ordina i file `.seq` per nome e salta quelli oltre 8000 byte.
- Se non ci sono `.seq`, l'ESP32 invia un pattern generato a `$9000`.

Estrazione dei SEQ dai `.d64`:

```bash
python3 tools/extract_seq.py /home/matteo/c64/petscii-art ./petscii-seq
```

Lo script genera `0001.seq`, `0002.seq`, ... e `manifest.txt`. In alternativa
si possono estrarre con `c1541`, ma lo script gestisce anche i nomi con spazi
iniziali e le catene settori corrotte.

La SD dell'ESP32 è stata preparata con:

```text
/petscii-art   (file .d64 originali, usati dalle NEWS)
/petscii-seq   (681 file .seq, usati dal menu 9)
espnews.prg
espnewsauto.prg
```

Attenzione: il codice C64 arriva a `$CFFB`, quindi restano 5 byte liberi
nella pagina `$C000-$CFFF`.

---

## 14. Menu grafico, HELP e avvio slideshow

- Il menu gira con `$01=$36`: BASIC ROM spenta, KERNAL e I/O attivi. In questo
  modo `$A000-$BFFF` e' RAM utilizzabile (serve gia' per lo staging SEQ).
- All'avvio il C64 chiede all'ESP32 il comando 64: il firmware genera una
  schermata 40x25 con cornice e titolo centrato in alto, poi invia 1000 byte
  di screen RAM + 1000 byte di color RAM direttamente a `$9800`.
- Il C64 mostra il template da `$9800`; ai successivi ritorni al menu lo
  ricopia senza nuove comunicazioni.
- Le voci del menu sono allineate a sinistra; la voce selezionata usa lo
  stesso color wash della directory. Frecce SU/GIU per muoversi, RETURN per
  attivare. Il color wash salta le colonne 0 e 39 per non cambiare il bordo.
- La scelta della fonte NEWS usa una schermata generata dall'ESP32 (comando
  65) e la stessa navigazione a frecce/color wash.
- Prima di Koala slideshow, SEQ slideshow e NEWS viene chiesto se suonare il
  SID in sottofondo (`PLAY LOADED SID? Y/N`), solo se un SID e' caricato.
  Nel caso SEQ il SID viene messo in pausa durante il trasferimento NMI.
- La schermata NEWS chiede anche lo sfondo con una schermata dedicata
  (comando 67) e frecce/RETURN: PETSCII, Koala della directory corrente
  oppure clock/weather.
- La directory SD usa una schermata con cornice (comando 66); il testo parte
  dalla colonna 2 e viene limitato a 22 righe per non toccare il bordo.
- La schermata meteo/ora (comando 52) e' generata dall'ESP32 con colori e
  testi centrati. Aprendola dal menu si aggiorna ogni ~5 s; come sfondo NEWS
  e' uno snapshot e non viene aggiornata periodicamente.
- La pagina HELP (comando 63) e' in inglese e descrive SD/PRG/SID/Koala, NEWS,
  meteo OpenWeather, BASIC, slideshow Koala/PETSCII e musica SID di sfondo.
  Include `Q = QUIT DIRECTORY` e l'avviso di caricare prima il SID da SD.
- Il player SID (comando 56) usa una schermata 40x25 a $A000 con nome/autore/
  dati e i comandi F1/F7/SPACE. Nel player F1/F7 caricano il SID precedente/
  successivo della directory (comandi 68/69), SPACE ferma e torna al menu.
  Prima di mostrare la schermata successiva vengono ripristinati i registri VIC
  che il SID precedente puo' aver cambiato e si riporta `$01=$36` per leggere
  il template da `$A000` in RAM (con `$01=$37` si leggerebbe la ROM BASIC).
  Anche `restart` ripristina il VIC bank 0, cosi' uscendo con SPACE il menu
  non viene mostrato con il bank lasciato dal SID.
- La lista dei SID viene preparata durante la scansione della directory, come
  quella dei Koala.
- Lo slideshow Koala standalone accetta F7 (successivo, comando 61) e F1
  (precedente, comando 54), oltre a SPACE per uscire.
- I nomi file con basename >37 caratteri sono stati accorciati sulla SD
  (84 file: 72 SID, 12 KOA), preservando estensione e suffisso _4000/_9000.
- I testi visibili del programma sono stati convertiti in inglese.
- Tra un'immagine Koala e l'altra il cursore KERNAL resta spento durante lo
  schermo nero.
- La lista dei Koala viene costruita durante la scansione della directory
  (`buildDirectoryListing`); il comando 62 non rilegge piu' la SD.
- La lista dei SEQ viene letta da `manifest.txt`/`seqlist.txt` per evitare la
  scansione lenta delle 681 entry.
- Tra un'immagine Koala e l'altra lo schermo viene pulito/annerito
  (`slide_text_mode` chiama `clear_screen_text`) per evitare caratteri residui
  durante il passaggio bitmap -> testo -> bitmap.

---

## 15. Sessione 2026-11-17 (serale) — EPROM, PR e repository pubblico

Riepilogo della serata: dalla EPROM che non partiva al repository pubblico con
documentazione completa. Tutto verificato su hardware reale.

### 15.1 Schermo blu con bordo celeste: banco EPROM e jumper

Sintomo: accensione con la 27C512 programmata, nessun avvio della cartuccia.

Dalla netlist KiCad (`c64esp32crt1.1_kicad7/ESP32Hardware.kicad_pcb`):

| Segnale EPROM | Collegamento |
|---|---|
| pin 20 `/CE` | GND fisso |
| pin 22 `/OE` | jumper **J9**: pin1 `/ROML`, pin3 `/ROMH` |
| pin 26 `A13` | pull-up `R9` -> +5V + jumper **J4** -> GND |
| pin 27 `A14` | pull-up `R10` -> +5V + jumper **J5** -> GND |
| pin 1 `A15` | pull-up `R11` -> +5V + jumper **J6** -> GND |

Quindi **senza jumper la EPROM legge il banco 7** (`0xE000`): se il bootstrap
sta solo nel banco 0 la C64 non trova `CBM80` e va al BASIC normale.

Attenzioni emerse:
- `J3` (EXROM -> GND) tiene la cartuccia sempre mappata e rende inutile il
  comando 112: in uso normale va **aperto**;
- `J9` deve stare su **ROM-L** (pin 1-2) per una cartuccia 8K a `$8000-$9FFF`;
- `J2` (`GAME`) va **aperto**.

### 15.2 `tools/make_eprom_image.sh`

Nuovo script: immagine 64K per la 27C512 da un binario da 8K.

```bash
tools/make_eprom_image.sh 0 c64esp8kBOOT.bin        # banco 0, resto 0xFF
tools/make_eprom_image.sh all c64esp8kBOOT.bin      # replicato negli 8 banchi
```

La modalita' `all` rende la EPROM indipendente dai jumper: e' quella usata per
la cartuccia definitiva. Programmazione con TL866II+:

```bash
minipro -p 'W27C512@DIP28' -z
minipro -p 'W27C512@DIP28' -w W27C512_c64esp8kBOOT.bin
minipro -p 'W27C512@DIP28' -r /tmp/verify.bin && cmp /tmp/verify.bin W27C512_c64esp8kBOOT.bin
```

Contatti del ZIF: piu' volte `Bad contact on pin:1` / `pin:25..28` -> basta
riaprire la leva, raddrizzare i piedini e reinserire; il read-back deve
combaciare byte per byte.

### 15.3 Build di debug del bootstrap (`-DDBGBOOT`)

Per capire dove si fermava il loader:

```bash
acme -DDBGBOOT=1 -o c64esp8kBOOT_dbg.bin bootstrap.asm
```

Segnali a schermo (colore del bordo + carattere): bordo bianco/sfondo nero =
EPROM mappata; `2` ciano = loader a `$C000`; `3` verde = risposta al comando
112; `4` giallo = nome file inviato; `5` viola = PRG ricevuto; `6` verde
chiaro = entry point ricevuto. Poi stampa i primi 16 byte a `$0801`, l'ultima
pagina scritta, l'entry point e un **checksum a 16 bit** del PRG (`5A47` per
`espnewsauto.prg`), e aspetta un tasto prima del salto.

### 15.4 Il bug vero: CLR / relink BASIC nel bootstrap

Il bootstrap faceva, subito prima di `jmp (PRG_ENTRY)`:

```asm
        jsr $a659       ; CLR
        jsr $a533       ; relink righe BASIC
```

Il coldstart da cartuccia inizializza **solo il KERNAL** (`IOINIT`, `RAMTAS`,
`RESTOR`, `CINT`): `TXTTAB`/`VARTAB`/`ARYTAB`/`STREND` possono contenere
spazzatura, quindi `CLR`/`LINKPRG` andavano in crash. Sintomo: dopo il
caricamento del PRG lo schermo restava fermo (i marker `23456` non venivano
nemmeno cancellati), nessuna reazione ai tasti, menu mai avviato.

Rimosse entrambe le chiamate: il PRG fa da solo l'init BASIC che gli serve.
Con il fix il menu parte dalla EPROM bootstrap.

Aggiunta anche una nota nel sorgente e nei documenti, per non reintrodurle.

### 15.5 Immagine definitiva

```bash
acme bootstrap.asm                                  # c64esp8kBOOT.bin (8192 byte)
tools/make_eprom_image.sh all c64esp8kBOOT.bin       # W27C512_c64esp8kBOOT.bin
```

| File | Dimensione | sha256 |
|---|---:|---|
| `c64esp8kBOOT.bin` | 8192 | `66cd34285219a18c6cb903c7d49a060a4538c5548049e8c76df01f9cb417fb5e` |
| `W27C512_c64esp8kBOOT.bin` | 65536 | `193b32b287a80974e57efec42c1fc14c13755ef5dd729ce62d4335ac7cd28ea8` |

Scritta e verificata (read-back identico, banchi 0-7 tutti uguali al binario).

### 15.6 Documentazione e immagini

- `README.md` **riscritto in inglese**: funzioni, hardware/pinout, capitolo
  **Schematic**, menu e tasti, **Screenshot**, jumper e banchi EPROM, flusso di
  avvio, firmware ESP32, `secrets.h` (WiFi/OpenWeather/NTP/NewsAPI), SD,
  formato Koala, rilocazione SID (`$4000`/`$9000` + maschere), arte PETSCII,
  programmazione 27C512, comandi di build, layout memoria, troubleshooting.
- **Screenshot reali** (snapshot VLC del video del C64) in
  `docs/screenshots/`: `main.jpg`, `dir.jpg`, `supermon.jpg`, `news-src.jpg`,
  `news-bg.jpg`, `news-petscii.jpg`, `weather.jpg`, `sid.jpg`, disposte in una
  tabella 2x4 da 420 px. `tools/render_menu.py` resta come fallback
  (`docs/renders/`, ignorato da git).
- Foto cartuccia `sideUp1/sideUp2/sideBottom` ridotte a 520 px e messe in una
  tabella a tre colonne con didascalie; `schematic.jpg` da 1094 KB a 288 KB.
- La schermata meteo e' stata **ritoccata a pixel**: nome della citta'
  sostituito con `ROMA` (glifi presi dalla riga sottostante della stessa foto),
  per non pubblicare la localita' dell'autore.

### 15.7 Privacy: coordinate spostate in `secrets.h`

- Rimosse dal firmware `String lat/lon` con le coordinate di casa.
- Ora si usano `openweatherLat` / `openweatherLon` da `secrets.h`
  (placeholder `xxxxx` in `secrets.example.h`).
- `weatherLocationConfigured()`: se non configurate il firmware non chiama
  l'API e stampa `Meteo: lat/lon non configurate in secrets.h`.

### 15.8 Git: PR, merge e repository pubblico

| Evento | Commit |
|---|---|
| PR #1 `pi-work -> main` (13 commit) squash-merged | `2a27359` |
| PR #2 (privacy coordinate + screenshot ritoccato) squash-merged | `4cedc61` |
| `pi-work` riallineato a `main` dopo ogni merge | `4cedc61` |

- Visibilita' cambiata a **public** con
  `gh api -X PATCH repos/matt199394/c64esp32cart -f visibility=public`
  (in gh 2.46 il flag `--accept-visibility-change-consequences` non esiste).
- **Issues abilitate** (`-F has_issues=true`).
- Verificato in anonimo: `secrets.h` e `firmware_backup/` rispondono 404.

### 15.9 Da fare alla prossima sessione

1. **Ruotare le credenziali** ancora nella storia Git ora pubblica:
   password WiFi `Home&Life SuperWiFi-0B6D`, OpenWeather `be8d2977…`,
   NewsAPI `a8255caf…` (commit `03bf297`, `346eeb9`, `37bf9bb`, `2dded14`),
   poi aggiornare `c64esp32Cart/secrets.h`.
2. **Riflashare l'ESP32** (ultima modifica: lat/lon in `secrets.h`).
3. **Fix cosmetico directory**: `_` mostrato come `←` (`asciiToPET()` lascia
   passare ASCII `$5F`, che in PETSCII e' la freccia) -> mappare su `$A4` o
   ripiegare su `-`.
4. Test funzionali rimasti (uscita SID con SPACE, paginazione directory,
   sfondi NEWS, Supermon, nomi lunghi).
5. Manca uno screenshot reale della pagina **HELP**.

---

## 16. 2026-11-21 (sera) — BOOTSD su EEPROM e EXROM solo col comando 112

### 16.1 Immagine 64K del bootstrap con menu

Il bootstrap con il menu `1) menu senza sam` / `2) menu con sam` (loader in
`boot_loader.inc`) e' gia' quello che finisce nella EPROM: `bootstrap.asm`
include lo stesso `.inc` di `bootstrap_sd.asm`, quindi non serve "scrivere
`boot_sd.prg`" nella EPROM (un PRG a `$0801` non puo' autostartare da
`$8000`: serve la firma `CBM80`, che sta solo in `bootstrap.asm`).

Build riverificata in questa sessione:

```bash
acme bootstrap.asm                                  # c64esp8kBOOT.bin
tools/make_eprom_image.sh all c64esp8kBOOT.bin /tmp/W27_new.bin
cmp /tmp/W27_new.bin W27C512_c64esp8kBOOT.bin       # identici
```

| File | Byte | sha256 |
|---|---:|---|
| `c64esp8kBOOT.bin` | 8192 | `9f4b78c6493092b0a70f74306c7dccf54ec33789fa10fd00f1e82a005f424110` |
| `W27C512_c64esp8kBOOT.bin` | 65536 | `15460daa1b6280700cea6f7fd08c1a61c5664e4adcc87e5ec3d3a5e3c95e2d09` |

Verificato anche che i banchi 0-7 sono identici agli 8K e che le stringhe del
menu sono presenti nel binario. **Resta solo da programmare la 27C512**:

```bash
minipro -p 'W27C512@DIP28' -z
minipro -p 'W27C512@DIP28' -w W27C512_c64esp8kBOOT.bin
minipro -p 'W27C512@DIP28' -r /tmp/W27C512_verify.bin
cmp /tmp/W27C512_verify.bin W27C512_c64esp8kBOOT.bin
```

### 16.2 Firmware: EXROM resta basso finche' non arriva il comando 112

Problema: il setup faceva `delay(1500)` + `CLED LOW` dopo
`buildDirectoryList()`. Se la C64 partiva in ritardo (o l'ESP32 finiva l'init
prima), la EPROM spariva mentre il `coldstart` la stava ancora eseguendo ->
C64 al BASIC senza cartuccia.

Modifiche in `c64esp32Cart/c64esp32Cart.ino`:

- `exromMapped` e funzione `exromRelease(reason)` che alza EXROM (`CLED LOW`)
  una volta sola e logga `EXROM alto (<motivo>): $8000-$9FFF torna RAM`;
- setup: `CLED HIGH` (EPROM mappata) resta, `delay(1500)+CLED LOW` **rimossi**;
  al loro posto log `EXROM basso: EPROM mappata, attendo il comando 112`;
- `case 112` usa `exromRelease("comando 112")`;
- **niente timeout** (prima avevo messo un fallback a 60 s poi tolto): in
  banco l'ESP32 e' alimentato via USB e la C64 si accende dopo; con un
  timeout la EPROM spariva e la cartuccia non partiva piu'. Alzare EXROM a
  mano (a boot ultimato) sarebbe comunque innocuo, il loader e il menu
  girano gia' a `$C000`.

Compila: 87% flash / 21% RAM (`--build-property build.partitions=no_ota`).

### 16.3 Da verificare su hardware (quando la scheda e' collegata)

1. Log atteso, nell'ordine: `EXROM basso: EPROM mappata, attendo il comando
   112` -> `done!` -> (comparsa del menu di avvio) -> `CMD 112` ->
   `EXROM alto (comando 112): $8000-$9FFF torna RAM` -> `CMD 51` ->
   `PRG: ... load $0801, entry ...` -> `CMD 60`.
2. Con la EPROM vecchia (senza menu) il firmware nuovo deve funzionare lo
   stesso (manda 112 subito).
3. Senza cartuccia inserita: nessun `CMD 112`, dopo 60 s deve comparire
   `EXROM alto (timeout, nessun comando 112)`.

Nota: `bootsd` nella root e' un build vecchio (749 byte, senza menu) e non e'
tracciato: si puo' cancellare, il sorgente `boot_sd.prg` si rigenera con
`acme bootstrap_sd.asm`.

### 16.4 Pulsante di reset della cartuccia (net iRST)

Osservato su hardware: il pulsante **RESET del modulo ESP32** resetta anche la
C64 (setup pulsa `GPIO21`) e la cartuccia riparte dal menu del bootstrap; il
pulsante **sulla cartuccia** invece non faceva nulla.

Analisi dell'hardware (`c64esp32crt1.1_kicad7/ESP32Hardware.kicad_pcb`, net del
PCB):

| Net | Collegato a |
|---|---|
| `iRST` | `SW1` (pulsante), `U5` pad 37 = **GPIO15** — e basta |
| `oRST` | `U5` pad 27 = GPIO21, ingresso di `U7` (74LS14) -> C64 `/RESET` |

Quindi il pulsante della cartuccia **non e' cablato al /RESET della C64**:
senza firmware non puo' fare niente. Va gestito nell'ESP32.

Implementazione (`c64esp32Cart/c64esp32Cart.ino`):

- `pollResetButton()` chiamata a ogni giro del `loop`:
  - **polling** con debounce (40 ms di livello basso continuo, poi riarmo),
    niente `attachInterrupt`: l'ISR su GPIO15 (pin di strapping MTDO) era la
    causa dei riavvii spuri durante l'audio a 11 kHz;
  - alla pressione logga `RESET: pulsante della cartuccia premuto`, mette la
    C64 in reset (`digitalWrite(oC64RST, !invert_Reset)`), aspetta 50 ms e fa
    `ESP.restart()`;
  - al riavvio `setup()` rimappa la EPROM (`CLED HIGH`) e ripulsa il reset
    della C64 (250 ms) -> si riparte dal menu del bootstrap.
- `isr_reset()` resta nel file ma non e' piu' agganciata (riferimento storico).
- Limite noto: durante una frase SAM (`samPlay` blocca il `loop`) il pulsante
  non viene letto; fuori da quei casi risponde subito.

Compila 87% flash / 21% RAM, flashato.

### 16.5 Menu di avvio a schermo pieno + HELP della build SAM

Richiesta di Matteo: cornice e colori come gli altri menu, titolo
`C64 ESP32 CARTRIDGE` centrato in giallo, sotto `BY M.ANGELINI` in grigio
chiaro, testi italiani; e HELP della build **con SAM** allineato alle sue 5 voci
(era ancora quello del menu completo).

**Menu di avvio.** Prima era stampato con CHROUT (`msg_menu`), adesso e' un
template 40x25:

- `tools/make_boot_menu.py` -> `boot_menu.inc`: 1024 byte di screen code +
  1024 di colori (padding a 1024 per copiare 4 pagine intere sia in
  `$0400-$07FF` sia in `$D800-$DBFF`, entrambe aree da 1K);
- `boot_loader.inc`: `menu_boot` mette sfondo nero, bordo azzurro,
  `$cc=$01` (cursore non lampeggiante), `copy_menu_screen` +
  `copy_menu_colors`, attende `1`/`2`, poi `PLOT` a riga 15 colonna 6 e stampa
  `CARICO ESPNEWSAUTO.PRG` / `CARICO ESPSAMAUTO.PRG`;
- **CHROUT scrive anche la color RAM** usando `$0286` (colore corrente, dopo
  CINT e' 14 = azzurro): per avere la riga bianca il loader imposta `$0286=1`
  prima della PLOT;
- il template sta **fuori** dal blocco `!pseudopc $c000` in `bootstrap.asm` /
  `bootstrap_sd.asm`: il loader lo legge dal suo indirizzo reale (EPROM
  `$82xx` oppure PRG caricato dalla SD), come `payload_rom` per il codice.
  Inserito dopo `BOOT_LEN` per non alterarlo.
- dimensioni: loader+template stanno in ~2,1 KB degli 8K della EPROM;
  `boot_sd.prg` passa da 743 a 2718 byte.
- prova in emulatore (senza ESP32): `x64sc -cart8 c64esp8kBOOT.bin`, con
  `-keybuf "\x31"`/`"\x32"` per vedere la riga `CARICO ...`.

**HELP SAM.** Aggiunto `helpTextSam` in `c64esp32Cart.ino`; `case 63` sceglie il
testo in base a `samVariant` (impostato dal comando 113, cioe' prima che il menu
possa chiedere l'HELP). Elenca SD CARD, HELP, BASIC, SAM TALK, SAM NEWS;
niente SUPERMON, CLOCK/WEATHER, KOALA, NEWS, SLIDESHOW.

Compila 87% flash / 21% RAM, flashato.
