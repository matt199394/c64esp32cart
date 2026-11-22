# PROJECT_STATE — C64 ESP32 Cart

Aggiornato: **2026-11-22** (sessione EPROM/EXROM/reset/menu di avvio:
`SESSION_NOTES.md` sez. 16.1-16.6, regole operative in `AGENTS.md`)  
Branch di sviluppo: **`pi-work`**  
`main`: stabile, contiene il merge delle PR #1 e #2; il repository è **pubblico**.

Questo file è il riepilogo operativo per riprendere il progetto in una nuova
sessione. Le regole e i comandi rapidi restano anche in [`AGENTS.md`](AGENTS.md);
il diario esteso è in [`SESSION_NOTES.md`](SESSION_NOTES.md) (la serata del
2026-11-17 è nella sezione 15).

---

## Ripresa in 30 secondi

- **Il progetto funziona su hardware reale** (2026-11-22): 27C512 programmata
  con il bootstrap nuovo negli 8 banchi, la C64 autostarta sul menu di avvio
  a schermo pieno, la voce `1` carica `espnewsauto.prg` (news con immagini
  PETSCII che scorrono), la voce `2` carica `espsamauto.prg` (menu SAM a 5
  voci, SAM TALK e SAM NEWS funzionanti).
- **Menu di avvio** (nella EPROM): cornice azzurra come gli altri menu, titolo
  `C64 ESP32 CARTRIDGE` giallo, `BY M.ANGELINI` grigio chiaro,
  `CHOOSE THE MENU TO LOAD`, `1) MENU WITHOUT SAM`, `2) MENU WITH SAM`,
  `PRESS 1 OR 2`; dopo la scelta stampa `LOADING ESPNEWSAUTO.PRG`. Template
  40x25 generato da `tools/make_boot_menu.py` -> `boot_menu.inc`.
- **EXROM**: il firmware la tiene **bassa** (EPROM mappata) all'avvio e la alza
  **solo** al comando 112 del loader, senza timeout (con l'ESP32 alimentato
  via USB e la C64 accesa dopo, la EPROM deve restare leggibile). Un `112`
  "spurio" (bus flottante con C64 spenta) viene annullato se entro 5 s non
  arriva il comando 51/53.
- **Pulsante di reset della cartuccia**: funziona. Il net `iRST` va solo a
  GPIO15 (non al `/RESET` della C64): il firmware lo legge in polling con
  debounce e fa reset di ESP32 + C64 -> si riparte dal menu del bootstrap.
- Firmware ESP32: **87% flash / 21% RAM** (`--build-property
  build.partitions=no_ota`), flashato su `/dev/ttyACM0`.
- Ultima build C64 completa: codice fino a `$CFFB` (5 byte liberi in
  `$C000-$CFFF`): ogni nuova funzione richiede refactoring.
- La SD della cartuccia è la **7.4G label `8074-0EDB`** (mount tipico
  `/run/media/matteo/8074-0EDB`): root con `espnewsauto.prg`,
  `espsamauto.prg`, `samplayer.prg`, `BOOTSD`.
- **Prossimo lavoro** (in ordine): (a) ruotare le credenziali rimaste nella
  storia Git pubblica; (b) fix cosmetico `_` -> `←` nella lista directory;
  (c) screenshot reale della pagina HELP; (d) test rimasti (vedi
  [Test da fare](#test-da-fare--prossimi-passi)); (e) PR `pi-work -> main`.
- Repository: <https://github.com/matt199394/c64esp32cart> (pubblico, Issues
  abilitate). `main` = `4cedc61`, `pi-work` avanti di vari commit (sessione
  SAM + questa).
- **2026-11-17: bootstrap provato su hardware reale e funzionante**: la C64
  autostarta da EPROM, il loader alza EXROM (comando 112), carica
  `espnewsauto.prg` dalla SD e parte il menu.
- Bug trovato e corretto nel bootstrap: il salto al PRG chiamava `CLR`
  (`$a659`) e il relink BASIC (`$a533`) **prima** che i puntatori BASIC
  fossero inizializzati (il coldstart da cartuccia fa solo IOINIT/RAMTAS/
  RESTOR/CINT). Risultato: crash silenzioso, schermo fermo, menu mai
  avviato. Ora il bootstrap salta direttamente all'entry point e il PRG fa
  da solo l'init BASIC che gli serve.
- **27C512 programmata e verificata** con `W27C512_c64esp8kBOOT.bin`
  (64K, bootstrap pulito in tutti e 8 i banchi, sha256
  `193b32b287a80974e57efec42c1fc14c13755ef5dd729ce62d4335ac7cd28ea8`),
  read-back dal TL866II+ identico byte per byte. Generata con
  `tools/make_eprom_image.sh all c64esp8kBOOT.bin`.
- Prossimi test: funzioni NEWS/SID/directory/slideshow con il bootstrap, poi
  PR finale `pi-work → main`.

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
| `c64esp8kBOOT.bin` | 8192 | `75847448759a74fe6a7731b86d5aa9ac4111240909068a71180211aa7c986b2a` |
| `W27C512_c64esp8kBOOT.bin` | 65536 | `c5b8f9275a98adc3019e947ea9a56855c7af0015349540ba96bd4ac6cc3f7ac3` |
| `boot_sd.prg` | 2725 | `b09fe48eb70269e224ecd8ad77ff259e123bc1c6f4140f5fb1cf1930b8c20fe0` |
| `espnewsauto.prg` | 4160 | `9324f1a2e2cd6c83b1f5dcef50176a8097a32e63f060f158610cdc252c2b0f76` |
| `espsamauto.prg` | 4011 | `b7b5532a9ecf8c47f5f8904f0690c4d27fc3aadb571ac561265d4c7a8d1d56a3` |
| `samplayer.prg` | 1340 | `d2fa82023ee3c19876818d7c50b9b5750e425ec1fe2381a35d97f72006d89163` |

`c64esp8kBOOT.bin` e' il bootstrap + menu di avvio (loader + template
`boot_menu.inc`, ~2,1 KB su 8 KB); l'immagine 64K e' lo stesso contenuto
replicato nei banchi 0-7 (`tools/make_eprom_image.sh all`).

`espnewsauto.prg`, `espsamauto.prg` e `samplayer.prg` sono i PRG nella root
della SD devono combaciare: dopo una ricompilazione ricopiarli e verificare con
`sha256sum`.

### Programmazione 27C512 (TL866II+)

```bash
# immagine 64K: bootstrap replicato in tutti e 8 i banchi (jumper-proof)
tools/make_eprom_image.sh all c64esp8kBOOT.bin  # -> W27C512_c64esp8kBOOT.bin

minipro -p 'W27C512@DIP28' -z                                 # pin/blank check
minipro -p 'W27C512@DIP28' -w W27C512_c64esp8kBOOT.bin        # write + verify
minipro -p 'W27C512@DIP28' -r /tmp/W27C512_verify.bin         # read-back
cmp /tmp/W27C512_verify.bin W27C512_c64esp8kBOOT.bin
```

Immagine master: 65536 byte, sha256
`c5b8f9275a98adc3019e947ea9a56855c7af0015349540ba96bd4ac6cc3f7ac3`
(programmata e verificata il 2026-11-22, read-back identico).
Se il pin test segnala `Bad contact on pin:...` (tipicamente 25-28 o 1),
la EPROM e' nel ZIF solo parzialmente: riaprire la leva, raddrizzare i
piedini e ripetere.

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

Il firmware tiene `CLED HIGH` (EXROM basso, EPROM mappata) dal setup e la
alza **solo** quando il loader manda il comando 112, cioe' dopo aver copiato
se' stesso a `$C000`. Il vecchio `delay(1500)` + `CLED LOW` e' stato tolto
(poteva togliere la ROM mentre la C64 la stava ancora leggendo). Nessun
timeout: con l'ESP32 alimentato via USB la EPROM deve restare mappata anche
se la C64 viene accesa minuti dopo.

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
- `coldstart` inizializza; `warmstart` copia un loader di 338 byte
  (`BOOT_LEN=$152`) a `$C000`.
- Il loader:
  1. imposta il vettore NMI;
  2. invia comando 112 → EXROM alto;
  3. invia il nome `espnewsauto.prg` (screen code) con comando 51;
  4. carica il PRG con comando 53 a `$0801`;
  5. chiede l'entry point con comando 60;
  6. `$01=$37` e salta all'entry (stub BASIC `SYS 2061`) **senza** chiamare
     `CLR`/relink BASIC: il coldstart da cartuccia non inizializza i
     puntatori BASIC e quelle chiamate mandavano il loader in crash
     (trovato e corretto il 2026-11-17 su hardware reale).
- Il PRG caricato copia il payload completo a `$C000` e sovrascrive il loader.
- La EPROM non contiene più il menu completo: si aggiorna solo la SD.
- Build di debug: `acme -DDBGBOOT=1 -o c64esp8kBOOT_dbg.bin bootstrap.asm`
  (marker `2`..`6`, dump a `$0801`, entry point, checksum, attesa tasto).

---

## Test da fare / prossimi passi

**Fatto (2026-11-17):** bootstrap provato su hardware reale, EPROM programmata
e verificata; menu avviato dal bootstrap; PR #1 `pi-work → main` mergiata con
squash (`2a27359`) e `pi-work` riallineato a `main`.

**Fatto (2026-11-21/22):** menu SAM a 5 voci, SAM TALK e SAM NEWS (karaoke,
fonte selezionabile) su hardware; bootstrap con menu di avvio 1/2 a schermo
pieno (cornice, titolo giallo) programmato e verificato; EXROM mappata fino al
comando 112; pulsante di reset della cartuccia funzionante; HELP separato per
la build SAM; testi del menu di avvio in inglese.

1. **Fix cosmetico lista directory**: nella directory i caratteri `_` dei nomi
   file vengono mostrati come `←` (es. `all←gif←koa←split` invece di
   `all_gif_koa_split`). Causa: `asciiToPET()` in `c64esp32Cart.ino` lascia
   passare ASCII `$5F`, che in PETSCII è la freccia sinistra. Mappare `_` su
   `$A4` (underscore PETSCII) o ripiegare su `-`; verificare a schermo su
   hardware reale, poi ricompilare e riflashare l'ESP32.
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
7. Manca uno screenshot reale della schermata HELP (`docs/screenshots/`).
8. Repository reso pubblico: prima di pubblicare controllare la storia Git
   (le vecchie credenziali WiFi/API key sono ancora nei commit passati) e
   ruotare le chiavi.

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

- `main`: stabile. Contiene i merge (squash) delle PR #1 (`2a27359`) e #2
  (`4cedc61`).
- `pi-work`: branch di sviluppo. Contiene la sessione SAM (`25e6be9`) e questa
  sessione: EXROM/BOOTSD (`df9d2a0`, `efac6a3`, `4cf04d9`), pulsante di reset
  (`5315b7c`), menu di avvio a schermo pieno + HELP SAM (`fde4913`), testi in
  inglese (`ada7fe5`), righe del menu SAM centrate (`0d17d74`).
- Regola: `main` si aggiorna **solo** con PR da `pi-work` (squash merge); dopo
  il merge `pi-work` va riallineato:
  `git reset --hard origin/main && git push --force-with-lease`.
- Push di sviluppo: `git push origin pi-work`.
- Storia: la cronologia dettagliata pre-squash è nelle PR #1 e #2 su GitHub.
- Non committare mai i sorgenti di SAM (`sam.c/h`, `render.*`, `reciter.*`,
  `*Tabs.h`, `debug.*`: software commerciale), `c64esp32Cart/secrets.h` né
  `firmware_backup/`.
- I binari di build nella root sono in `.gitignore` (inclusi
  `c64esp8kBOOT.bin` e `W27C512_*.bin`); `boot_sd.prg` **e'** tracciato
  (artefatto di riferimento del loader). I sorgenti e questo riepilogo sì.
