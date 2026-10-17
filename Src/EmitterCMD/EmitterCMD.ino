// EmitterCMD — CV telemetry over LoRa + GPS to CV + LoRa commands to CV
// CV (STM32) -> UART1 -> ESP32 -> LoRa -> Ground Station  (telemetry downlink)
// Ground Station -> LoRa -> ESP32 -> UART1 -> CV (STM32)  (command uplink)
// GPS (I2C) -> ESP32 -> UART1 -> CV (STM32)
#include "LoRa_E32.h"
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <Wire.h>

#define PIN_RX_LORA 16
#define PIN_TX_LORA 17
#define PIN_AUX     4
#define PIN_M0      21
#define PIN_M1      22

#define PIN_RX_CV   26
#define PIN_TX_CV   27

#define CMD_MAX_FRAME 64
#define FRAME_TIMEOUT 200
#define GPS_INTERVAL  1000
#define GPS_SDA       32
#define GPS_SCL       33
LoRa_E32 e32(&Serial2, PIN_AUX, PIN_M0, PIN_M1);
HardwareSerial SerialCV(1);
SFE_UBLOX_GNSS gps;

struct FrameParser {
  uint8_t buf[CMD_MAX_FRAME];
  uint8_t idx;
  uint8_t expected;
  unsigned long lastByte;
};

FrameParser cvParser;
FrameParser loraParser;

uint8_t txBuf[CMD_MAX_FRAME];
uint8_t txLen = 0;
bool frameReady = false;

bool gpsOk = false;
unsigned long lastGps = 0;

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
  delay(500);
  Serial.println(F("\n[BOOT] EmitterCMD"));

  pinMode(PIN_AUX, INPUT);
  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX_LORA, PIN_TX_LORA);
  e32.begin();

  // Retry: the module may be powered from a rail that comes up after the ESP32.
  bool loraOk = false;
  for (uint8_t i = 0; i < 6 && !loraOk; i++) {
    loraOk = configureLoRa();
    if (!loraOk) delay(500);
  }
  Serial.println(loraOk ? F("[OK] LoRa configured") : F("[ERR] LoRa config failed"));

  digitalWrite(PIN_M0, LOW);
  digitalWrite(PIN_M1, LOW);
  delay(50);

  SerialCV.setRxBufferSize(1024);
  SerialCV.begin(115200, SERIAL_8N1, PIN_RX_CV, PIN_TX_CV);

  Wire.begin(GPS_SDA, GPS_SCL);
  gpsOk = gps.begin();
  if (gpsOk) {
    gps.setI2COutput(COM_TYPE_UBX);
    gps.setDynamicModel(DYN_MODEL_AIRBORNE4g);
    gps.setNavigationFrequency(1);
    gps.setAutoPVT(true);
    Serial.println(F("[OK] GPS"));
  } else {
    Serial.println(F("[ERR] GPS not found"));
  }

  Serial.println(F("[BOOT] Ready"));
}

void loop() {
  // --- Downlink: CV -> LoRa (command frames) ---
  while (SerialCV.available()) {
    uint8_t len = feedByte(cvParser, SerialCV.read());
    if (len) {
      memcpy(txBuf, cvParser.buf, len);
      txLen = len;
      frameReady = true;
    }
  }

  if (frameReady && digitalRead(PIN_AUX) == HIGH) {
    Serial2.write(txBuf, txLen);
    frameReady = false;
    Serial.write(txBuf, txLen);
  }

  // --- Uplink: LoRa -> CV (command frames) ---
  while (Serial2.available()) {
    uint8_t len = feedByte(loraParser, Serial2.read());
    if (len) {
      SerialCV.write(loraParser.buf, len);
      Serial.print(F("[CMD] rx id=0x"));
      Serial.println(loraParser.buf[2], HEX);
    }
  }

  // --- GPS -> CV at 1Hz ---
  if (gpsOk && millis() - lastGps >= GPS_INTERVAL && gps.getPVT(0)) {
    lastGps = millis();

    uint32_t tod = gps.getHour() * 3600UL + gps.getMinute() * 60UL + gps.getSecond();
    uint16_t ms  = gps.getMillisecond();
    int32_t  lat = gps.getLatitude();
    int32_t  lon = gps.getLongitude();
    int32_t  alt = gps.getAltitude();
    uint8_t  siv = gps.getSIV();

    uint8_t frame[24];
    frame[0] = 0xFE;
    frame[1] = 0xCA;
    frame[2] = 0x20;
    frame[3] = 0x13;
    memcpy(frame + 4,  &tod, 4);
    memcpy(frame + 8,  &ms,  2);
    memcpy(frame + 10, &lat, 4);
    memcpy(frame + 14, &lon, 4);
    memcpy(frame + 18, &alt, 4);
    frame[22] = siv;
    frame[23] = 0xBE;

    SerialCV.write(frame, 24);
  }
}
