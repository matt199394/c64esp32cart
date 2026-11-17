/*
 * C64 <-> ESP32 - test minimale di comunicazione
 *
 * Scheda: MH ET LIVE ESP32MiniKit / ESP32-WROOM-32
 * FQBN:   esp32:esp32:mhetesp32minikit
 *
 * Protocollo:
 *   - il C64 scrive un byte in $DE00
 *   - l'ESP32 lo legge, lo stampa sul monitor seriale (115200)
 *   - l'ESP32 rimanda lo stesso byte al C64 via NMI / $DF00
 *   - il C64 confronta il byte ricevuto con quello inviato
 *
 * Non usa WiFi, SD, web server o altre periferiche: serve solo a isolare
 * il bus C64 <-> ESP32.
 */

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin della cartuccia (identici al firmware principale)
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

// ---------------------------------------------------------------------------
// Stato condiviso con gli interrupt
// ---------------------------------------------------------------------------
volatile bool dataFromC64 = false;
volatile bool io2 = false;
volatile uint8_t ch = 0;

uint32_t rxCount = 0;
uint32_t txCount = 0;
uint32_t txTimeout = 0;

bool staticMode = false;
uint8_t staticValue = 0;

// ---------------------------------------------------------------------------
// Uscita dati verso il C64 (74LS244)
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

// ---------------------------------------------------------------------------
// Il C64 ha scritto in $DE00: il 74HC597 ha il byte pronto
// ---------------------------------------------------------------------------
void IRAM_ATTR isr_io1() {
  digitalWrite(oC64D7, LOW);       // flow control: non inviare altri byte
  digitalWrite(pload, HIGH);       // carica il dato parallelo nel shift register
  ch = shiftIn(sdata, sclk, MSBFIRST);
  dataFromC64 = true;
  digitalWrite(pload, LOW);
}

// Il C64 ha letto $DF00: acknowledge del byte inviato
void IRAM_ATTR isr_io2() {
  io2 = true;
}

// ---------------------------------------------------------------------------
// NMI: pulse verso il C64
// ---------------------------------------------------------------------------
void triggerNMI() {
  digitalWrite(oC64NMI, HIGH);
  delayMicroseconds(150);
  digitalWrite(oC64NMI, LOW);
}

// Invia un byte al C64 e aspetta che il C64 legga $DF00.
// Ritorna false se il C64 non risponde entro 5 ms.
bool sendByte(uint8_t b) {
  outByte(b);
  io2 = false;
  triggerNMI();

  uint32_t start = micros();
  while (!io2) {
    if ((micros() - start) > 5000) {
      return false;
    }
    delayMicroseconds(2);
  }

  io2 = false;
  return true;
}

// Stampa solo se il buffer seriale ha spazio: se il monitor non e' aperto
// il debug non rallenta/ blocca il test.
bool canLog() {
  return Serial.availableForWrite() >= 48;
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

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=== C64 <-> ESP32 minimal comm test ===");
  Serial.println("Modalita': echo di ogni byte scritto dal C64 in $DE00.");
  Serial.println("Il byte viene rimandato al C64 via NMI e letto da $DF00.");
  Serial.println("Pin: D0..D7 = 4,33,14,27,13,22,17,26");
  Serial.println("     NMI=32, IO1=35, IO2=36, SDATA=34, SCLK=25, PLOAD=16");
  Serial.println("Comandi: 0..7 = mette un solo bit alto, a=00, f=FF, e=echo");
  Serial.println("READY");
}

// ---------------------------------------------------------------------------
// Loop principale
// ---------------------------------------------------------------------------
void loop() {
  // Comandi di diagnostica dal monitor seriale
  if (Serial.available()) {
    int cmd = Serial.read();

    if (cmd >= '0' && cmd <= '7') {
      int bit = cmd - '0';
      staticValue = (uint8_t)(1 << bit);
      staticMode = true;
      outByte(staticValue);
      Serial.printf("STATIC %02X (D%d alto)\n", staticValue, bit);
      return;
    }

    if (cmd == 'a') {
      staticValue = 0x00;
      staticMode = true;
      outByte(staticValue);
      Serial.println("STATIC 00");
      return;
    }

    if (cmd == 'f') {
      staticValue = 0xFF;
      staticMode = true;
      outByte(staticValue);
      Serial.println("STATIC FF");
      return;
    }

    if (cmd == 'e') {
      staticMode = false;
      gpio_set_level(oC64D7, HIGH);
      Serial.println("ECHO MODE");
      return;
    }
  }

  if (staticMode) {
    return;
  }

  // D7 alto = ESP32 pronta a ricevere il prossimo byte dal C64
  gpio_set_level(oC64D7, HIGH);

  if (!dataFromC64) {
    return;
  }

  dataFromC64 = false;
  gpio_set_level(oC64D7, LOW);

  uint8_t b = ch;
  rxCount++;

  if (canLog()) {
    Serial.printf("RX %02X\n", b);
  }

  bool ok = sendByte(b);

  if (ok) {
    txCount++;
    if (canLog()) {
      Serial.printf("TX %02X OK\n", b);
    }
  } else {
    txTimeout++;
    if (canLog()) {
      Serial.printf("TX %02X TIMEOUT (io2)\n", b);
    }
  }

  // LED: lampeggia ad ogni byte scambiato
  digitalWrite(CLED, !digitalRead(CLED));

  // Riabilita il C64 per il prossimo byte
  gpio_set_level(oC64D7, HIGH);

  if (canLog() && ((rxCount & 0x3F) == 0)) {
    Serial.printf("--- RX=%lu TX=%lu TIMEOUT=%lu ---\n",
                  (unsigned long)rxCount,
                  (unsigned long)txCount,
                  (unsigned long)txTimeout);
  }
}
