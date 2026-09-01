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

void setup() {
  Serial.begin(115200);
  loraSerial.begin(9600);
  e32ttl.begin();
}

void loop() {
  // ========================================================
  // 1. TELEMETRY: LoRa (95B) -> Serial
  // ========================================================
  if (e32ttl.available() > 1) {
    ResponseStructContainer rsc = e32ttl.receiveMessage(TELEMETRY_SIZE);
    if (rsc.status.code == 1) {
      uint8_t* buf = (uint8_t*)rsc.data;

      if (buf[0] == 0xFE && buf[1] == 0xCA && buf[51] == 0xBE) {
        Serial.write(buf, TELEMETRY_SIZE);
      }
    }
    rsc.close();
  }

  // ========================================================
  // 2. COMMANDS: Serial (5B) -> LoRa
  // ========================================================
  if (Serial.available() >= COMMAND_SIZE) {
    uint8_t cmd[COMMAND_SIZE];
    Serial.readBytes(cmd, COMMAND_SIZE);

    if (cmd[0] == 0xFE && cmd[1] == 0xCA && cmd[4] == 0xBE) {
      e32ttl.sendMessage(cmd, COMMAND_SIZE);
    }
  }
}
