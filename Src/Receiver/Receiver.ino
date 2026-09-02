// ***** GROUND STATION (Protocolo Coheteros) *****
// Bidirectional passthrough: LoRa <-> Serial (UI)
// Telemetry: LoRa 52B -> Serial (UI reads and decodes)
// Commands:  Serial 5B  -> LoRa   (UI builds and sends)

#include <SoftwareSerial.h>
#include "LoRa_E32.h"

#define PIN_RX 2   // Connect to E32 TX
#define PIN_TX 3   // Connect to E32 RX
#define PIN_AUX 4
#define PIN_M0 5
#define PIN_M1 6

#define TELEMETRY_SIZE 52
#define COMMAND_SIZE 5

SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E32 e32ttl(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

uint8_t telRxBuf[TELEMETRY_SIZE];
uint8_t telRxIdx = 0;

uint8_t pendingCmd[COMMAND_SIZE];
bool cmdPending = false;

static void waitAuxHigh() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_AUX) == LOW) {
    if (millis() - t0 > 200) break;
  }
}

static void trySendPendingCmd() {
  if (!cmdPending) return;
  if (digitalRead(PIN_AUX) != HIGH) return;

  e32ttl.sendMessage(pendingCmd, COMMAND_SIZE);
  waitAuxHigh();
  cmdPending = false;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_AUX, INPUT);
  loraSerial.begin(9600);
  e32ttl.begin();
}

void loop() {
  // ========================================================
  // 1. TELEMETRY: LoRa -> Serial (non-blocking accumulation)
  // ========================================================
  while (loraSerial.available()) {
    uint8_t b = loraSerial.read();

    if (telRxIdx == 0) {
      if (b == 0xFE) telRxBuf[telRxIdx++] = b;
      continue;
    }

    if (telRxIdx == 1) {
      if (b == 0xCA) {
        telRxBuf[telRxIdx++] = b;
      } else {
        telRxIdx = 0;
      }
      continue;
    }

    telRxBuf[telRxIdx++] = b;

    if (telRxIdx == TELEMETRY_SIZE) {
      if (telRxBuf[TELEMETRY_SIZE - 1] == 0xBE) {
        Serial.write(telRxBuf, TELEMETRY_SIZE);
      }
      telRxIdx = 0;

      trySendPendingCmd();
    }
  }

  // ========================================================
  // 2. COMMANDS: Serial -> buffer (sent after next telemetry RX)
  // ========================================================
  if (Serial.available() >= COMMAND_SIZE) {
    uint8_t cmd[COMMAND_SIZE];
    Serial.readBytes(cmd, COMMAND_SIZE);

    if (cmd[0] == 0xFE && cmd[1] == 0xCA && cmd[4] == 0xBE) {
      memcpy(pendingCmd, cmd, COMMAND_SIZE);
      cmdPending = true;

      trySendPendingCmd();
    }
  }
}
