# PROJECT_STATE — C64 ESP32 Cart

Aggiornato: **2026-11-17**  
Branch di sviluppo: **`pi-work`**  
`main`: stabile; il merge `pi-work → main` va fatto solo alla PR finale.

Questo file è il riepilogo operativo per riprendere il progetto in una nuova
sessione. Le regole e i comandi rapidi restano anche in [`AGENTS.md`](AGENTS.md);
il diario esteso è in [`SESSION_NOTES.md`](SESSION_NOTES.md).

---

## Ripresa in 30 secondi

- Firmware ESP32 principale: `c64esp32Cart`, già compilato e caricato su
  `/dev/ttyACM0` con FQBN `esp32:esp32:mhetesp32minikit`.
- Ultima build C64 completa: codice fino a `$CFFB` (5 byte liberi in
  `$C000-$CFFF`).
- La SD della cartuccia è la **7.4G label `8074-0EDB`** (mount tipico
  `/run/media/matteo/8074-0EDB`). Alla fine della sessione era stata scollegata.
- Sulla SD devono esserci `espnewsauto.prg` (root) e i file rinominati; i PRG
  principali sono stati copiati con gli hash indicati sotto.
- È stato aggiunto il **bootstrap EPROM 8K**: `bootstrap.asm` →
  `c64esp8kBOOT.bin` (8192 byte). Va programmato nella 27C512 e provato su
  hardware reale.
- Prossimo test consigliato: accensione con EPROM bootstrap, verifica EXROM,
  caricamento automatico di `espnewsauto.prg`, poi test funzioni NEWS/SID/
  directory/slideshow.

---

## Hardware

- MH ET LIVE ESP32MiniKit / ESP32-WROOM-32.
- FQBN: `esp32:esp32:mhetesp32minikit`; porta USB: `/dev/ttyACM0`.
- SD ESP32: label `8074-0EDB`, 7.4G; root con `espnewsauto.prg` e cartelle
  (`C64Music_safe_reloc`, `all_gif_koa_split`, `petscii-art`, `petscii-seq`,
  ecc.).
- EPROM: 27C512 (64K), con jumper per selezionare il banco da 8K.
- EXROM: controllato da **ESP32 GPIO2 / `CLED`** tramite transistor BC547:
  - `CLED HIGH` → transistor ON → EXROM basso → EPROM `$8000-$9FFF` mappata;
  - `CLED LOW` → transistor OFF → EXROM alto → EPROM rimossa, resta RAM.
- Il vecchio ponticello EXROM che teneva sempre mappata la ROM esterna è stato
  rimosso.
- La scheda precedente aveva GPIO4 in corto: sostituita e da non usare.

---

## Comandi di build

### Firmware ESP32

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit c64esp32Cart
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit c64esp32Cart
```

### C64 — tre target

```bash
# ROM 8K / menu completo incorporato (build storica)
acme -DCART=1 c64esp32cart.asm
# -> c64esp8kROM.bin

# PRG diretto a $C000 (caricamento ,8,1 + SYS 49152)
acme -DCART=1 -DC400=1 c64esp32cart.asm
# -> c64esp32menu_c000_cart.prg

# PRG auto-avviante da $0801 (LOAD + RUN)
acme -DCART=1 -DPRGAUTO=1 c64esp32cart.asm
# -> espnewsauto.prg
```

### Bootstrap EPROM 8K (consigliato)

```bash
acme bootstrap.asm
# -> c64esp8kBOOT.bin (8192 byte)
```

Inserire gli 8192 byte nel file 64K della 27C512 nel banco selezionato dai
jumper, come si faceva con `c64esp8kROM.bin`.

---

## Binari e hash correnti

| File | Dimensione | SHA-256 |
|---|---:|---|
| `c64esp32menu_c000_cart.prg` | 4097 | `3d17e8a7bd14599eca27f6176c48fbaaffbf4a548c7d8b70591159951a58ffb2` |
| `espnewsauto.prg` | 4160 | `5f4b00d150778e61429d6688201126850869142ae19d5d704ec90c39660469ea` |
| `c64esp8kROM.bin` | 8192 | `5b086cdf195fda9811d42f1ca084c2826aa35b491d80268dbe9a89931f047da0` |
| `c64esp8kBOOT.bin` | 8192 | `5c1a5c7be4037f2392b4cc8a29040862c3ec75550cbfff5f40f71a43c51ef6d5` |

`c64esp32menu_c000_cart.prg` e `espnewsauto.prg` sono gli stessi file usati per
la SD (quando la SD è inserita, verificare con `sha256sum`).

`c64esp8kROM.bin` è la build storica con tutto il menu incorporato.
La soluzione EPROM consigliata è `c64esp8kBOOT.bin`.

---

## Layout memoria C64

| Area | Uso |
|---|---|
| `$0400-$07E7` | screen RAM |
| `$0800-$0FFF` | charset NEWS / font 2x2 |
| `$1000-$3EFF` | lista directory (max 12032 byte) |
| `$2000-$3F3F` | bitmap Koala |
| `$6000-$8710` | sorgente Koala |
| `$8800-$8FD0` | testo NEWS |
| `$9000-$97CF` | grafica NEWS |
| `$9800-$9FCF` | template menu (2000 byte) |
| `$A000-$BFFF` | staging SEQ/HELP/schermata SID |
| `$C000-$CFFF` | codice C64 (build principale fino a `$CFFB`) |
| `$D800-$DBE7` | color RAM |

La pagina `$C000-$CFFF` è quasi piena: 5 byte liberi. Qualsiasi nuova funzione
C64 richiede refactoring o spostamento di codice.

---

## Protocollo ESP32 (comandi principali)

- `49` directory
- `50` Supermon+
- `51` ricevi stringa (nome file/directory)
- `52` schermata ora/meteo → `$A000`
- `53` invia file (PRG/SID/Koala) al C64
- `54` Koala precedente
- `55` dati SID
- `56` schermata SID player → `$A000`
- `57` charset NEWS
- `58` NEWS
- `60` entry point PRG
- `61` Koala successivo
- `62` reset lista Koala
- `63` HELP → `$A000`
- `64` template menu → `$9800`
- `65` schermata fonte NEWS → `$A000`
- `66` schermata bordo directory → `$A000`
- `67` schermata sfondo NEWS → `$A000`
- `68` SID successivo
- `69` SID precedente
- `70..85` selezione fonte NEWS
- `86..89` art/SEQ
- `90..105` NEWS + art
- **`112` bootstrap: EXROM alto** (`CLED LOW`), usato dal loader EPROM

Il setup ESP32 tiene `CLED HIGH` per far partire la EPROM, poi dopo l'init
(aspetta 1.5 s) e `buildDirectoryList()` alza EXROM come fallback. Il loader
EPROM invia comunque il comando 112 appena possibile.

---

## Stato funzionalità

### Menu e interfaccia
- Template grafico 40x25 generato dall'ESP32, cornice, titolo centrato, voci a
  sinistra, color wash sulla riga selezionata.
- Navigazione frecce SU/GIU + RETURN.
- Template menu in `$9800`, ricaricato se un SID/PRG lo sovrascrive.
- Supermon: prima del salto a `$A000` viene pulito lo schermo, quindi non si
  sovrappone più alle scritte del menu.
- La voce `KOALA` del menu usa `koala_once` (mostra l'immagine caricata per
  ~4 s o fino a SPACE); il codice `koala` storico è stato rimosso.

### Directory SD
- Cornice, testo a colonna 2, 22 righe per pagina.
- F1/F7 pagina precedente/successiva; `Q` esce.
- `read_from_screen` legge dal col 2.
- I puntatori `ptr1` vengono salvati/ripristinati attorno alla copia del
  template: F1/F7 ripartono dal punto corretto.
- La scansione si ferma quando la lista raggiunge 12032 byte (evita timeout su
  root enormi).
- I nomi file con basename >37 caratteri sono stati accorciati sulla SD:
  84 file (72 SID in `C64Music_safe_reloc`, 12 KOA in `all_gif_koa_split`).
  Ora non ci sono basename >37, quindi le righe non vanno più a capo.
- I file `directory.txt` e `koalist.txt` sono filtrati dalla lista.

### PRG e SID
- PRG caricati dalla directory: prima del salto vengono ripristinati
  `$01=$37`, CLR/relink BASIC e cursore normale.
- SID: schermata info 40x25 a `$A000`; F1/F7 SID precedente/successivo,
  SPACE ferma e torna al menu.
- Dopo un SID, prima della schermata successiva vengono ripristinati VIC
  (`$d011/$d018/$d016`, VIC bank 0) e `$01=$36` per leggere il template da
  `$A000` in RAM (con `$01=$37` si leggerebbe la ROM BASIC).
- La lista SID viene preparata durante la scansione della directory.
- Se alla richiesta `PLAY LOADED SID? Y/N` si risponde **N**, il SID non parte
  (controllo `sid_choice` anche in NEWS).

### Slideshow
- Koala standalone: auto-avanzamento, F7 successivo, F1 precedente, SPACE esce.
- SEQ standalone: auto-avanzamento, F7/F1, SPACE esce; SID in pausa durante
  l'NMI.
- Koala slideshow: cache lista preparata durante la scansione della directory.

### NEWS
- Fonti multiple (ANSA, Rai News, Repubblica, Il Sole 24 Ore, Google News,
  NewsAPI, BBC World, Al Jazeera, SCMP, CGTN World).
- Sottomenu fonte con frecce/RETURN; poi sottomenu sfondo (comando 67):
  PETSCII, KOALA, CLOCK/WEATHER.
- KOALA come sfondo NEWS usa lo slideshow dei Koala della directory corrente;
  se non ce ne sono, ripiega su PETSCII.
- NEWS: Koala ogni ~60 s, PETSCII ogni ~30 s.
- Meteo come sfondo NEWS: snapshot, **nessun refresh periodico**.
- Meteo/ora standalone: refresh ogni ~5 s.
- Lo scroller usa il font 2x2 in `$0800`; il top usa il charset ROM.

### EPROM bootstrap
- `bootstrap.asm` produce `c64esp8kBOOT.bin` 8192 byte.
- Il C64 parte dalla EPROM a `$8000` (CBM80).
- `coldstart` inizializza; `warmstart` copia un loader di 344 byte
  (`BOOT_LEN=$158`) a `$C000`.
- Il loader:
  1. imposta il vettore NMI;
  2. invia comando 112 → EXROM alto;
  3. invia il nome `espnewsauto.prg` (screen code) con comando 51;
  4. carica il PRG con comando 53 a `$0801`;
  5. chiede l'entry point con comando 60;
  6. `$01=$37`, CLR/relink, salta all'entry (stub BASIC `SYS 2061`).
- Il PRG caricato copia il payload completo a `$C000` e sovrascrive il loader.
- La EPROM non contiene più il menu completo: si aggiorna solo la SD.

---

## Test da fare / prossimi passi

1. **Bootstrap su hardware reale**:
   - programmare la 27C512 con `c64esp8kBOOT.bin` nel banco corretto;
   - accendere e verificare che EXROM venga alzato e che `espnewsauto.prg`
     parta dalla root della SD;
   - se non parte, controllare: comando 112 nel firmware, file sulla SD,
     temporizzazione EXROM, banco EPROM.
2. Verificare uscita dal player SID con SPACE: il menu deve riapparire subito
   senza caratteri casuali.
3. Verificare directory F1/F7: la seconda pagina deve partire dalla riga
   corretta senza scorrere caratteri casuali.
4. Verificare NEWS:
   - sfondo Koala: slideshow della directory, ~60 s;
   - sfondo PETSCII: ~30 s;
   - sfondo meteo: snapshot, nessun refresh;
   - meteo standalone: refresh ~5 s;
   - risposta N al SID: nessun SID in sottofondo.
5. Verificare Supermon: schermo pulito, nessuna sovrapposizione col menu.
6. Verificare che i nomi file lunghi non mandino più a capo la directory.
7. Solo dopo i test: PR finale `pi-work → main`.

---

## File di riferimento

- [`AGENTS.md`](AGENTS.md) — contesto operativo e comandi.
- [`SESSION_NOTES.md`](SESSION_NOTES.md) — diario esteso.
- [`c64esp32cart.asm`](c64esp32cart.asm) — sorgente C64 completo.
- [`bootstrap.asm`](bootstrap.asm) — bootstrap EPROM 8K.
- [`c64esp32Cart/c64esp32Cart.ino`](c64esp32Cart/c64esp32Cart.ino) — firmware ESP32.
- `c64esp32Cart/secrets.example.h` — template credenziali; **non** committare
  `secrets.h`.
- `tools/` — script SID/SEQ/Televideo.
- `comms_test/`, `audio_test/` — test separati, non attivi.

---

## Git

- Branch: `pi-work`.
- Push di sviluppo: `git push origin pi-work`.
- Non committare mai `c64esp32Cart/secrets.h` né `firmware_backup/`.
- I binari di build nella root sono in `.gitignore` (incluso
  `c64esp8kBOOT.bin`); i sorgenti e questo riepilogo sì.
