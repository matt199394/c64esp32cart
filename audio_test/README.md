# Test audio C64 <-> ESP32

Test del solo trasferimento audio: l'ESP32 genera una sinusoide 440 Hz a
11 kHz, il C64 prende il nibble alto di ogni campione e lo scrive nel volume
SID `$D418` (4 bit).

## 1. Firmware ESP32

Compila e carica:

```bash
arduino-cli compile --fqbn esp32:esp32:mhetesp32minikit audio_test
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit audio_test
```

Monitor seriale:

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

All'avvio:

```text
=== C64 ESP32 audio test 11kHz 4bit ===
Attendo comando 70 dal C64...
```

Premendo SPAZIO sul C64:

```text
AUDIO: start 440Hz 11kHz 4bit
...
AUDIO: stop, samples=... fail=...
```

## 2. Programma C64

```bash
cd audio_test
acme c64_audio_test.asm
# -> c64_audio_test.prg
```

Sul C64:

```basic
LOAD"c64_audio_test.prg",8
RUN
```

Il programma:

1. salva il vettore NMI
2. installa un handler NMI minimale
3. invia il comando `70` all'ESP32
4. aspetta SPAZIO
5. invia `71`, azzera il volume e ripristina il NMI

Handler NMI:

```asm
audio_nmi
        bit $dd0d
        pha
        lda $df00
        lsr
        lsr
        lsr
        lsr
        sta $d418
        pla
        rti
```

## 3. Note

- Il volume SID è a 4 bit: 16 livelli.
- Il nibble alto del campione 8 bit è il valore usato.
- Il protocollo è grezzo: nessun header, un NMI breve per campione.
- Se compare `AUDIO: timeout`, il C64 non sta rispondendo in tempo: si può
  aumentare leggermente l'impulso NMI o ridurre il ritmo.
- Dopo il test, per tornare al firmware principale:

```bash
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:mhetesp32minikit c64esp32Cart
```
