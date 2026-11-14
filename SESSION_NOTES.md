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
