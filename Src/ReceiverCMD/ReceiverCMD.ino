// ReceiverCMD — LoRa telemetry to Serial + Serial commands to LoRa
// Ground Station: LoRa -> Arduino -> Serial (telemetry downlink)
//                 Serial -> Arduino -> LoRa (command uplink)
#include <SoftwareSerial.h>
#include "LoRa_E32.h"

#define PIN_RX  8
#define PIN_TX  9
#define PIN_AUX 4
#define PIN_M0  5
#define PIN_M1  6

#define CMD_MAX_FRAME 64

SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E32 e32(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

uint8_t loraBuf[CMD_MAX_FRAME];
uint8_t loraIdx = 0;
uint8_t loraExpected = 0;

uint8_t serialBuf[CMD_MAX_FRAME];
uint8_t serialIdx = 0;
uint8_t serialExpected = 0;

void configureLoRa() {
  digitalWrite(PIN_M0, HIGH);
  digitalWrite(PIN_M1, HIGH);
  delay(50);

  ResponseStructContainer rsc = e32.getConfiguration();
  if (rsc.status.code != 1) {
    Serial.println(F("[ERR] LoRa config read failed"));
    rsc.close();
    return;
  }

  Configuration cfg = *(Configuration*)rsc.data;
  rsc.close();

  cfg.ADDH = 0x00;
  cfg.ADDL = 0x01;
  cfg.CHAN = 0x06;
  cfg.SPED.uartBaudRate = UART_BPS_9600;
  cfg.SPED.airDataRate = AIR_DATA_RATE_011_48;
  cfg.OPTION.transmissionPower = POWER_10;
  cfg.OPTION.fec = FEC_1_ON;
  cfg.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
  cfg.OPTION.wirelessWakeupTime = WAKE_UP_250;
  cfg.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;

  ResponseStatus rs = e32.setConfiguration(cfg, WRITE_CFG_PWR_DWN_SAVE);
  Serial.println(rs.code == 1 ? F("[OK] LoRa configured") : F("[ERR] LoRa config write failed"));
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_AUX, INPUT);
  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);

  loraSerial.begin(9600);
  e32.begin();
  configureLoRa();

  digitalWrite(PIN_M0, LOW);
  digitalWrite(PIN_M1, LOW);
  delay(50);

  Serial.println(F("[BOOT] ReceiverCMD ready"));
}

void loop() {
  // --- Downlink: LoRa -> Serial (command frames) ---
  while (loraSerial.available()) {
    uint8_t b = loraSerial.read();

    if (loraIdx == 0 && b != 0xFE) continue;
    if (loraIdx == 1 && b != 0xCA) { loraIdx = 0; continue; }

    loraBuf[loraIdx++] = b;

    if (loraIdx == 4) {
      if (loraBuf[3] > CMD_MAX_FRAME - 5) {
        loraIdx = 0;
        continue;
      }
      loraExpected = 5 + loraBuf[3];
    }

    if (loraExpected != 0 && loraIdx == loraExpected) {
      if (loraBuf[loraExpected - 1] == 0xBE) {
        Serial.write(loraBuf, loraExpected);
      }
      loraIdx = 0;
      loraExpected = 0;
    }
  }

  // --- Uplink: Serial -> LoRa (command frames) ---
  while (Serial.available()) {
    uint8_t b = Serial.read();

    if (serialIdx == 0 && b != 0xFE) continue;
    if (serialIdx == 1 && b != 0xCA) { serialIdx = 0; continue; }

    serialBuf[serialIdx++] = b;

    if (serialIdx == 4) {
      if (serialBuf[3] > CMD_MAX_FRAME - 5) {
        serialIdx = 0;
        continue;
      }
      serialExpected = 5 + serialBuf[3];
    }

    if (serialExpected != 0 && serialIdx == serialExpected) {
      if (serialBuf[serialExpected - 1] == 0xBE) {
        while (digitalRead(PIN_AUX) == LOW) {}
        e32.sendMessage(serialBuf, serialExpected);
      }
      serialIdx = 0;
      serialExpected = 0;
    }
  }
}
