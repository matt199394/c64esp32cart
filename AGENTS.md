# AGENTS.md — C64 ESP32 Cart

> **Ripresa rapida:** all'avvio leggere [`PROJECT_STATE.md`](PROJECT_STATE.md)
> (riepilogo aggiornato al 2026-11-17: stato, hash, bootstrap EPROM, test da
> fare). Il diario esteso è in [`SESSION_NOTES.md`](SESSION_NOTES.md).

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

### Bootstrap EPROM 8K (consigliato per la 27C512)

```bash
acme bootstrap.asm
# -> c64esp8kBOOT.bin (8192 byte, da inserire nel file 64K della 27C512)
```

Il bootstrap:
- parte dalla EPROM a `$8000` (CBM80);
- copia un loader di ~344 byte a `$C000`;
- invia il comando ESP32 **112** per alzare EXROM (GPIO2/CLED -> transistor):
  la EPROM `$8000-$9FFF` viene rimossa e resta solo RAM;
- carica `espnewsauto.prg` dalla SD dell'ESP32 a `$0801`;
- salta all'entry point del PRG.

Requisiti: firmware ESP32 con comando 112 e `espnewsauto.prg` nella root della SD.
Il setup ESP32 tiene EXROM basso per 1.5 s dopo l'init come fallback, poi lo alza;
il loader invia comunque il comando 112 appena pronto.

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
