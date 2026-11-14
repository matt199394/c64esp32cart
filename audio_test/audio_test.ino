/*
 * C64 <-> ESP32 - test audio 4 bit / 11 kHz
 *
 * Scheda: MH ET LIVE ESP32MiniKit / ESP32-WROOM-32
 * FQBN:   esp32:esp32:mhetesp32minikit
 *
 * Protocollo:
 *   C64 scrive 70 in $DE00  -> parte il tono
 *   ESP32 invia campioni PCM 8 bit; il C64 usa il nibble alto
 *   C64 scrive 71 in $DE00  -> stop
 *
 * Ogni campione viene inviato con un NMI breve; il C64 lo legge da $DF00,
 * prende i 4 bit alti e li scrive in $D418 (volume SID).
 */

#include <Arduino.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Pin della cartuccia
// ---------------------------------------------------------------------------
#define oC64D0 GPIO_NUM_4
#define oC64D1 GPIO_NUM_33
#define oC64D2 GPIO_NUM_14
#define oC64D3 GPIO_NUM_27
#define oC64D4 GPIO_NUM_13
#define oC64D5 GPIO_NUM_22
#define oC64D6 GPIO_NUM_17
#define oC64D7 GPIO_NUM_26

#define oC64RST GPIO_NUM_21
#define oC64NMI GPIO_NUM_32
#define CLED    GPIO_NUM_2

#define sclk    GPIO_NUM_25
#define pload   GPIO_NUM_16

#define C64IO1  GPIO_NUM_35
#define sdata   GPIO_NUM_34
#define C64IO2  GPIO_NUM_36

volatile bool dataFromC64 = false;
volatile bool io2 = false;
volatile uint8_t ch = 0;

uint8_t sine4[32];
uint32_t sentSamples = 0;
uint32_t failedSamples = 0;

// ---------------------------------------------------------------------------
// Bus verso il C64
// ---------------------------------------------------------------------------
void outByte(uint8_t c) {
  gpio_set_level(oC64D0, c & 0x01);
  gpio_set_level(oC64D1, c & 0x02);
  gpio_set_level(oC64D2, c & 0x04);
  gpio_set_level(oC64D3, c & 0x08);
  gpio_set_level(oC64D4, c & 0x10);
  gpio_set_level(oC64D5, c & 0x20);
  gpio_set_level(oC64D6, c & 0x40);
  gpio_set_level(oC64D7, c & 0x80);
}

void IRAM_ATTR isr_io1() {
  digitalWrite(oC64D7, LOW);
  digitalWrite(pload, HIGH);
  ch = shiftIn(sdata, sclk, MSBFIRST);
  dataFromC64 = true;
  digitalWrite(pload, LOW);
}

void IRAM_ATTR isr_io2() {
  io2 = true;
}

void triggerNMI_fast() {
  digitalWrite(oC64NMI, HIGH);
  delayMicroseconds(8);          // impulso breve per l'audio
  digitalWrite(oC64NMI, LOW);
}

bool sendSample(uint8_t b) {
  outByte(b);
  io2 = false;
  triggerNMI_fast();

  uint32_t start = micros();
  while (!io2) {
    if ((micros() - start) > 2000) {
      return false;
    }
    delayMicroseconds(1);
  }
  io2 = false;
  return true;
}

// ---------------------------------------------------------------------------
// Tono di test: sinusoide 440 Hz, 11 kHz, 4 bit
// ---------------------------------------------------------------------------
void runAudioTest() {
  Serial.println("AUDIO: start 440Hz 11kHz 4bit SQUARE");
  uint32_t next = micros();
  uint16_t phase = 0;
  const uint16_t step = (uint16_t)((440.0 * 65536.0) / 11000.0); // 2621
  sentSamples = 0;
  failedSamples = 0;

  while (true) {
    // comando di stop dal C64?
    if (dataFromC64) {
      dataFromC64 = false;
      if (ch == 71) {
        break;
      }
    }

    next += 91;                   // ~11 kHz
    int32_t wait = (int32_t)(next - micros());
    if (wait > 0) {
      delayMicroseconds(wait);
    }

    // Onda quadra 0/15: molta più energia della sinusoide, utile per il test.
    uint8_t v = (phase & 0x8000) ? 15 : 0;
    phase += step;

    if (sendSample(v << 4)) {     // nibble alto
      sentSamples++;
    } else {
      failedSamples++;
      if (failedSamples > 50) {
        Serial.println("AUDIO: timeout, stop");
        break;
      }
    }
    if ((sentSamples & 0x3FF) == 0) {
      gpio_set_level(CLED, !gpio_get_level(CLED));
    }
  }
  Serial.printf("AUDIO: stop, samples=%lu fail=%lu\n",
                (unsigned long)sentSamples, (unsigned long)failedSamples);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  pinMode(CLED, OUTPUT);
  digitalWrite(CLED, HIGH);

  pinMode(oC64D0, OUTPUT);
  pinMode(oC64D1, OUTPUT);
  pinMode(oC64D2, OUTPUT);
  pinMode(oC64D3, OUTPUT);
  pinMode(oC64D4, OUTPUT);
  pinMode(oC64D5, OUTPUT);
  pinMode(oC64D6, OUTPUT);
  pinMode(oC64D7, OUTPUT);
  digitalWrite(oC64D7, LOW);

  pinMode(oC64RST, OUTPUT);
  digitalWrite(oC64RST, HIGH);

  pinMode(oC64NMI, OUTPUT);
  digitalWrite(oC64NMI, LOW);

  pinMode(pload, OUTPUT);
  digitalWrite(pload, LOW);

  pinMode(sclk, OUTPUT);
  digitalWrite(sclk, LOW);

  pinMode(sdata, INPUT);
  pinMode(C64IO1, INPUT);
  pinMode(C64IO2, INPUT);

  attachInterrupt(C64IO1, isr_io1, RISING);
  attachInterrupt(C64IO2, isr_io2, FALLING);

  // tabella sinusoidale 0..15
  for (int i = 0; i < 32; i++) {
    float s = sinf((2.0 * PI * i) / 32.0);
    sine4[i] = (uint8_t)(s * 7.5 + 7.5 + 0.5);
  }

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== C64 ESP32 audio test 11kHz 4bit ===");
  Serial.println("Attendo comando 70 dal C64...");
}

void loop() {
  if (!dataFromC64) {
    return;
  }
  dataFromC64 = false;
  uint8_t cmd = ch;

  if (cmd == 70) {
    runAudioTest();
  } else if (cmd == 71) {
    Serial.println("AUDIO: stop da C64");
  } else {
    Serial.printf("CMD %u ignorato\n", cmd);
  }
}
