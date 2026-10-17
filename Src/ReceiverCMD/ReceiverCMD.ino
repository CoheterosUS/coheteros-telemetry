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
#define FRAME_TIMEOUT 200

SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E32 e32(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

struct FrameParser {
  uint8_t buf[CMD_MAX_FRAME];
  uint8_t idx;
  uint8_t expected;
  unsigned long lastByte;
};

FrameParser loraParser;
FrameParser serialParser;

uint8_t txBuf[CMD_MAX_FRAME];
uint8_t txLen = 0;
bool frameReady = false;

// Returns the frame length once p.buf holds a complete valid frame, 0 otherwise.
uint8_t feedByte(FrameParser &p, uint8_t b) {
  unsigned long now = millis();
  if (p.idx != 0 && now - p.lastByte > FRAME_TIMEOUT) {
    p.idx = 0;
    p.expected = 0;
  }
  p.lastByte = now;

  if (p.idx == 0 && b != 0xFE) return 0;
  if (p.idx == 1 && b != 0xCA) {
    p.idx = 0;
    p.expected = 0;
    return 0;
  }

  p.buf[p.idx++] = b;

  if (p.idx == 4) {
    if (p.buf[3] > CMD_MAX_FRAME - 5) {
      p.idx = 0;
      p.expected = 0;
      return 0;
    }
    p.expected = 5 + p.buf[3];
  }

  if (p.expected != 0 && p.idx == p.expected) {
    uint8_t len = p.expected;
    p.idx = 0;
    p.expected = 0;
    return p.buf[len - 1] == 0xBE ? len : 0;
  }
  return 0;
}

bool loraCfgMatches(const Configuration &c) {
  return c.ADDH == 0x00 && c.ADDL == 0x01 && c.CHAN == 0x06 &&
         c.SPED.uartBaudRate == UART_BPS_9600 &&
         c.SPED.airDataRate == AIR_DATA_RATE_011_48 &&
         c.OPTION.transmissionPower == POWER_10 &&
         c.OPTION.fec == FEC_1_ON &&
         c.OPTION.fixedTransmission == FT_TRANSPARENT_TRANSMISSION &&
         c.OPTION.wirelessWakeupTime == WAKE_UP_250 &&
         c.OPTION.ioDriveMode == IO_D_MODE_PUSH_PULLS_PULL_UPS;
}

bool configureLoRa() {
  ResponseStructContainer rsc = e32.getConfiguration();
  if (rsc.status.code != 1) {
    rsc.close();
    return false;
  }

  Configuration cfg = *(Configuration*)rsc.data;
  rsc.close();

  if (loraCfgMatches(cfg)) return true;

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

  return e32.setConfiguration(cfg, WRITE_CFG_PWR_DWN_SAVE).code == 1;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_AUX, INPUT);
  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);

  loraSerial.begin(9600);
  e32.begin();

  bool loraOk = false;
  for (uint8_t i = 0; i < 6 && !loraOk; i++) {
    loraOk = configureLoRa();
    if (!loraOk) delay(500);
  }
  Serial.println(loraOk ? F("[OK] LoRa configured") : F("[ERR] LoRa config failed"));

  digitalWrite(PIN_M0, LOW);
  digitalWrite(PIN_M1, LOW);
  delay(50);

  Serial.println(F("[BOOT] ReceiverCMD ready"));
}

void loop() {
  // --- Downlink: LoRa -> Serial (command frames) ---
  while (loraSerial.available()) {
    uint8_t len = feedByte(loraParser, loraSerial.read());
    if (len) Serial.write(loraParser.buf, len);
  }

  // --- Uplink: Serial -> LoRa (command frames) ---
  while (Serial.available()) {
    uint8_t len = feedByte(serialParser, Serial.read());
    if (len) {
      memcpy(txBuf, serialParser.buf, len);
      txLen = len;
      frameReady = true;
    }
  }

  if (frameReady && digitalRead(PIN_AUX) == HIGH) {
    loraSerial.write(txBuf, txLen);
    frameReady = false;
  }
}
