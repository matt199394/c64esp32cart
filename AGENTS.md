# AGENTS.md — C64 ESP32 Cart

Contesto operativo per Pi. Per il diario completo della sessione vedi
[`SESSION_NOTES.md`](SESSION_NOTES.md).

## Stato del progetto

- Branch di sviluppo: `pi-work`
- `main`: stabile
- Il merge `pi-work → main` va fatto solo alla PR finale
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
- Repository GitHub privato
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
  - staging NEWS + art/SEQ `$8800-$BFFF`
  - codice `$C000-$CE81`
- I file interni `directory.txt` e `koalist.txt` sono filtrati dalla lista

## Funzioni già presenti

- WiFi opzionale/non bloccante
- SD con lettura diretta in RAM (niente file temporaneo per la directory)
- directory con F1/F7, sottodirectory, `/..`
- Koala auto-display
- slideshow Koala (`8.Koala slideshow`) con cache `koalist.txt`
- SID di sottofondo nello slideshow, in pausa durante l'NMI
- NEWS con Koala di sfondo e testo scorrevole
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

## Prossimi passi

1. Webradio: decoder MP3/AAC ESP32, PCM mono 11 kHz, nibble alto → `$D418`
2. Eventuale conversione Televideo direttamente sull'ESP32
3. Rifinitura pausa musicale durante il caricamento immagini
4. PR finale `pi-work → main`

## File di riferimento

- `SESSION_NOTES.md`: diario completo della sessione
- `c64esp32cart.asm`: sorgente C64
- `c64esp32Cart/c64esp32Cart.ino`: firmware principale
- `comms_test/`: test comunicazione
- `audio_test/`: test audio 4 bit
- `tools/`: script di supporto
