# SAM — sorgenti della voce (NON nel repository)

Il firmware `c64esp32Cart` sa far parlare la C64 usando **SAM (Software
Automatic Mouth)** in esecuzione sull'ESP32: i campioni vengono generati a
11025 Hz, ridotti a 4 bit e mandati alla C64 che li suona su `$D418` via NMI
(comandi ESP32 113-119).

SAM però è **software commerciale** (SoftVoice / Don't Ask Software): per
questo i suoi sorgenti **non sono versionati** in questo repository pubblico,
che viene usato solo a livello personale. Sono elencati in `.gitignore` e non
devono mai finire in un commit.

Questo file spiega come rimetterli a posto per compilare la voce, e come
compilare **senza** SAM quando non servono.

---

## 1. File da aggiungere

Vanno copiati in `c64esp32Cart/` (accanto a `c64esp32Cart.ino`):

| File | Ruolo |
|---|---|
| `sam.c`, `sam.h` | parser dei fonemi e motore SAM |
| `render.c`, `render.h` | sintesi (formanti, intonazione) |
| `reciter.c`, `reciter.h` | testo inglese -> fonemi |
| `RenderTabs.h` | tabelle del renderer |
| `ReciterTabs.h` | tabelle del reciter |
| `SamTabs.h` | tabelle dei fonemi |
| `debug.c`, `debug.h` | funzioni di debug richieste da `sam.c`/`render.c` |

Provenienza: il port in C di SAM (progetto `s-macke/SAM`, a sua volta basato
sul SAM originale per C64/Atari/Apple II). Servono le funzioni
`SetInput()`, `SAMMain()`, `GetBuffer()`, `GetBufferLength()` dichiarate in
`sam.h`.

Restano invece versionati i file scritti per questo progetto, che fanno da
ponte e si compilano anche senza SAM:

- `sam_wrap.c` / `sam_wrap.h`: 22050 -> 11025 Hz, 4 bit, lingua (`sam_say_lang`);
- `it2sam.c` / `it2sam.h`: regole di pronuncia italiana -> fonemi SAM.

---

## 2. Due patch obbligatorie ai sorgenti SAM

Senza queste due modifiche il firmware va in crash quando la RAM scarseggia
(successo davvero dopo il download TLS delle news: `malloc` falliva e SAM
scriveva a `NULL+3`).

**a) `sam.c`, funzione `Init()`** — controllare il `malloc`:

```c
buffer = malloc(SAM_BUF_BYTES);
if (buffer == 0) { bufferpos = 0; }   // niente crash: SAMMain() esce con errore
```

**b) `render.c`, funzione `Output8BitAry()`** — non uscire dal buffer:

```c
for (k = 0; k < 5; k++)
    if (bufferpos / 50 + k < SAM_BUF_BYTES) buffer[bufferpos / 50 + k] = ary[k];
```

Controllo rapido:

```bash
grep -n "buffer == 0"            c64esp32Cart/sam.c
grep -n "bufferpos / 50 + k <"   c64esp32Cart/render.c
# oppure: tools/check_sam_sources.sh
```

I due parametri di memoria stanno in `sam_wrap.h` (versionato, non serve
toccarli): `SAM_BUF_SECONDS 2` (buffer ~44 KB: il massimo contiguo dopo il
download TLS) e `SAM_MAX_CHARS 16` (pezzi di frase da ~1,6 s).

---

## 3. Compilare

La macro `ENABLE_SAM` (in testa a `c64esp32Cart.ino`, stessa logica in
`sam_wrap.c`) decide se la voce entra nel firmware:

- **non definita**: `ENABLE_SAM = 1` se `sam.h`/`sam_wrap.h` ci sono, altrimenti
  `0` (autodetect);
- **forzata**: `-DENABLE_SAM=0` oppure `-DENABLE_SAM=1`.

```bash
# con SAM (sorgenti presenti in c64esp32Cart/)
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit \
    --build-property build.partitions=no_ota c64esp32Cart

# senza SAM, anche se i sorgenti ci sono (o se non ci sono affatto)
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit \
    --build-property build.partitions=no_ota \
    --build-property "build.extra_flags=-DENABLE_SAM=0" c64esp32Cart
```

Dimensioni misurate (2026-11-22): **con SAM 88% flash / 21% RAM**,
**senza SAM 85% flash / 20% RAM** (la voce costa ~30 KB di flash e ~4 KB di
RAM).

All'avvio il firmware lo dice nel log seriale:

```
SAM: voce inclusa (ENABLE_SAM=1)
SAM: build SENZA voce (ENABLE_SAM=0, vedi SAM_SOURCES.md)
```

---

## 4. Cosa funziona senza SAM

Tutto il resto: menu, directories SD, PRG, SID, Koala, slideshow, NEWS
(scroller), meteo/ora, Supermon, bootstrap e menu di avvio della EPROM.

Non funziona la voce: scegliendo `2) MENU WITH SAM` nel menu del bootstrap il
PRG `espsamauto.prg` parte comunque, ma SAM TALK / SAM NEWS rispondono con un
blocco vuoto (`sendZeroBlock`) e nel log compare

```
SAM: questa build non include SAM (vedi SAM_SOURCES.md)
```

---

## 5. Prima di committare

```bash
git status --short    # i sorgenti SAM non devono comparire
```

`.gitignore` copre già `c64esp32Cart/sam.c`, `sam.h`, `render.*`, `reciter.*`,
`RenderTabs.h`, `ReciterTabs.h`, `SamTabs.h`, `debug.*`. Se un giorno se ne
aggiunge un altro, va messo lì **prima** di copiarlo nella cartella.
