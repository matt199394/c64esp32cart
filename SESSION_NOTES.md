# C64 ESP32 Cart — note sessione `pi-work`

Ultimo aggiornamento: 2026-11-15  
Branch di sviluppo: `pi-work`  
`main`: versione stabile, il merge verrà fatto solo alla PR finale.

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
  - lista Koala in RAM
  - cache `koalist.txt`
  - SID di sottofondo con pausa durante il trasferimento NMI
- Audio test separato (vedi sotto)

### Comandi protocollo utili

```text
49 = directory
50 = Supermon+
51 = ricevi nome file / directory
52 = meteo/ora
53 = file PRG/SID/Koala
55 = dati SID
56 = info SID
57 = charset NEWS
58 = NEWS
60 = entry point PRG
61 = prossimo Koala slideshow
62 = reset lista slideshow
70 = avvia audio test
71 = ferma audio test
```

---

## 3. Programma C64 — `c64esp32cart.asm`

Build cartuccia ROM:

```bash
acme -DCART=1 c64esp32cart.asm
# -> c64esp8kROM.bin
```

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
*** by M.Angelini C000k ***
```

SHA256 ultimo PRG:

```text
400c5a00a55b4d9b323f456ff3e637fb699ea73eee37da18fd9df7dec0a1e625
```

### Mappa memoria C64 usata

```text
$0400-$07E7  screen RAM
$0800-$0FFF  charset NEWS 2x2
$1000-$3EFF  lista directory (max 12032 byte)
$2000-$3F3F  bitmap Koala (VIC bank 0, base $2000)
$6000-$8710  sorgente Koala
$8800-$8FD0  testo NEWS
$C000-$CE81  codice (build C000k)
$D800-$DBE7  color RAM
```

### Modifiche C64 principali

- codice spostato da `$C400` a `$C000` per spazio
- routine Koala corretta:
  - bitmap + video matrix + color RAM
  - `$D011/$D018/$D016` corretti
  - auto-display dopo load
- cursore disabilitato nel menu, riabilitato in BASIC
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

Il repository GitHub è ora **privato**.

Branch di lavoro:

```text
pi-work
```

`main` resta stabile; il merge verrà fatto con una PR quando il progetto sarà pronto.

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
- codice: `$C000-$CFEF`

---

## 13. Slideshow SEQ standalone

Nuova voce di menu: `9.SEQ`.

- Avanzamento automatico ogni ~5 s (250 jiffy PAL).
- `F7` = immagine successiva, `F1` = precedente, `SPACE` = torna al menu.
- Le immagini vengono lette da `/petscii-seq` sulla SD dell'ESP32.
- Comandi C64 -> ESP32: `89` prima immagine, `87` successiva, `88` precedente.
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

Attenzione: il codice C64 arriva a `$CFF9`, quindi restano solo 7 byte liberi
nella pagina `$C000-$CFFF`.
