# Test comunicazione C64 <-> ESP32

Questa cartella contiene un test **minimalista** per isolare il bus tra C64 e
ESP32. Non usa WiFi, SD, meteo, NEWS o scroller: l'ESP32 fa solo echo dei byte
ricevuti e li stampa sul monitor seriale.

## 1. Firmware ESP32

Scheda:

- MH ET LIVE ESP32MiniKit / ESP32-WROOM-32
- FQBN: `esp32:esp32:mhetesp32minikit`

Compila e carica:

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit comms_test
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit comms_test
```

Monitor seriale:

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

All'avvio deve apparire:

```text
=== C64 <-> ESP32 minimal comm test ===
Modalita': echo di ogni byte scritto dal C64 in $DE00.
Il byte viene rimandato al C64 via NMI e letto da $DF00.
...
READY
```

Durante il test, per ogni byte:

```text
RX 00
TX 00 OK
RX 01
TX 01 OK
...
```

Se compare `TIMEOUT (io2)`, il C64 non ha letto `$DF00` / non ha risposto
all'NMI.

## 2. Programma C64

Build come PRG BASIC (caricamento normale, `RUN`):

```bash
cd comms_test
acme c64_comm_test.asm
# -> c64_comm_test.prg
```

Sul C64:

```basic
LOAD"c64_comm_test.prg",8
RUN
```

Oppure build diretta a `$C400` (se preferisci il caricamento a `$C400`):

```bash
acme -DC400=1 c64_comm_test.asm
# -> c64_comm_test_c400.prg
```

```basic
LOAD"c64_comm_test_c400.prg",8,1
SYS 50176
```

## 3. Cosa fa il test

Invia all'ESP32:

- tutti i valori da `0` a `255`
- 32 volte `$55`
- 32 volte `$AA`
- 8 volte `$00`
- 8 volte `$FF`

Per ogni byte aspetta l'echo, lo confronta e conta gli errori.

Sullo schermo del C64 appare un riepilogo tipo:

```text
c64 <-> esp32 comm test

test completato
tx: 336  errori: 0
ultimo tx: $FF  rx: $FF  tipo: 0

comunicazione ok

premi un tasto: ripeti, q = esci
```

`tipo`:

- `0` = nessun errore
- `1` = timeout in attesa che l'ESP32 sia pronta (`D7` alto)
- `2` = timeout in attesa dell'echo / NMI
- `3` = byte ricevuto diverso da quello inviato

`bit errati` e' la maschera XOR di tutti i byte sbagliati:

- `$01` -> D0 / GPIO4 / 74LS244 A0-Y0
- `$02` -> D1 / GPIO33
- `$04` -> D2 / GPIO14
- `$08` -> D3 / GPIO27
- `$10` -> D4 / GPIO13
- `$20` -> D5 / GPIO22
- `$40` -> D6 / GPIO17
- `$80` -> D7 / GPIO26

Esempio: se il C64 riceve `$A4` invece di `$A5`, la maschera sara'
`$01` (bit 0). In quel caso il problema e' fisico sul ramo D0, non nel
protocollo.

## 4. Note

- Il test usa l'IRQ standard del Kernal e un NMI dedicato.
- Il vettore NMI viene puntato al test; dopo `q` non viene ripristinato, ma il
  test resta in memoria fino al reset/ricaricamento.
- Se il C64 non vede nulla, controlla prima il monitor seriale dell'ESP32:
  se non compaiono righe `RX ...`, il problema è nel senso C64 -> ESP32
  (`$DE00`, `IO1`, `74HC597`, `sdata`/`sclk`/`pload`).
- Se compaiono `RX` ma non `TX ... OK`, il problema è nel senso ESP32 -> C64
  (`$DF00`, `IO2`, NMI, `74LS244`).


## 5. Diagnostica statica dei bit dati

Dal monitor seriale dell'ESP32 si possono inviare questi comandi:

- `0` ... `7` = mette alto solo il bit D0...D7 corrispondente e lo mantiene
- `a` = tutti i bit a 0 (`$00`)
- `f` = tutti i bit a 1 (`$FF`)
- `e` = torna in modalita' echo

Esempio per verificare D0:

```text
0
STATIC 01 (D0 alto)
```

Con il tester misurare:

- pin ESP32 **GPIO4 / IO_04**: deve essere alto (~3.3 V)
- pin **A0** del 74LS244: deve essere alto
- con il C64 che legge `$DF00`, pin **Y0** del 74LS244: deve essere alto

Se GPIO4 e' alto ma A0 e' basso, il problema e' la pista/saldatura tra ESP32
e 74LS244. Se A0 e' alto ma il C64 legge D0 basso, controllare Y0, la pista
fino al connettore C64 e il 74LS244 stesso.
