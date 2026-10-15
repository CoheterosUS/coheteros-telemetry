// Receiver — LoRa telemetry to Serial (Protocolo Coheteros)
// Ground Station: LoRa -> Arduino -> Serial (UI reads and decodes)
#include <SoftwareSerial.h>
#include "LoRa_E32.h"

#define PIN_RX  8
#define PIN_TX  9
#define PIN_AUX 4
#define PIN_M0  5
#define PIN_M1  6

#define TEL_SIZE 52

SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E32 e32(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

uint8_t rxBuf[TEL_SIZE];
uint8_t rxIdx = 0;

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
  cfg.SPED.airDataRate = AIR_DATA_RATE_010_24;
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

  Serial.println(F("[BOOT] ReceiverNew ready"));
}

void loop() {
  // Accumulate telemetry frames from LoRa (0xFE 0xCA ... 0xBE, 52 bytes)
  while (loraSerial.available()) {
    uint8_t b = loraSerial.read();

    if (rxIdx == 0 && b != 0xFE) continue;
    if (rxIdx == 1 && b != 0xCA) { rxIdx = 0; continue; }

    rxBuf[rxIdx++] = b;

    if (rxIdx == TEL_SIZE) {
      if (rxBuf[TEL_SIZE - 1] == 0xBE) {
        Serial.write(rxBuf, TEL_SIZE);
      }
      rxIdx = 0;
    }
  }
}
