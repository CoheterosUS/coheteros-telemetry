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
#define GPS_INTERVAL  1000
#define GPS_SDA       32
#define GPS_SCL       33
LoRa_E32 e32(&Serial2, PIN_AUX, PIN_M0, PIN_M1);
HardwareSerial SerialCV(1);
SFE_UBLOX_GNSS gps;

uint8_t cvBuf[CMD_MAX_FRAME];
uint8_t cvIdx = 0;
uint8_t cvExpected = 0;
uint8_t txBuf[CMD_MAX_FRAME];
uint8_t txLen = 0;
bool frameReady = false;

uint8_t loraBuf[CMD_MAX_FRAME];
uint8_t loraIdx = 0;
uint8_t loraExpected = 0;

unsigned long lastGps = 0;

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
  delay(500);
  Serial.println(F("\n[BOOT] EmitterCMD"));

  pinMode(PIN_AUX, INPUT);
  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX_LORA, PIN_TX_LORA);
  e32.begin();
  configureLoRa();

  digitalWrite(PIN_M0, LOW);
  digitalWrite(PIN_M1, LOW);
  delay(50);

  SerialCV.begin(115200, SERIAL_8N1, PIN_RX_CV, PIN_TX_CV);

  Wire.begin(GPS_SDA, GPS_SCL);
  if (gps.begin()) {
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
    uint8_t b = SerialCV.read();

    if (cvIdx == 0 && b != 0xFE) continue;
    if (cvIdx == 1 && b != 0xCA) { cvIdx = 0; continue; }

    cvBuf[cvIdx++] = b;

    if (cvIdx == 4) {
      if (cvBuf[3] > CMD_MAX_FRAME - 5) {
        cvIdx = 0;
        continue;
      }
      cvExpected = 5 + cvBuf[3];
    }

    if (cvExpected != 0 && cvIdx == cvExpected) {
      if (cvBuf[cvExpected - 1] == 0xBE) {
        memcpy(txBuf, cvBuf, cvExpected);
        txLen = cvExpected;
        frameReady = true;
      }
      cvIdx = 0;
      cvExpected = 0;
    }
  }

  if (frameReady && digitalRead(PIN_AUX) == HIGH) {
    e32.sendMessage(txBuf, txLen);
    frameReady = false;
    Serial.write(txBuf, txLen);
  }

  // --- Uplink: LoRa -> CV (command frames) ---
  while (Serial2.available()) {
    uint8_t b = Serial2.read();

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
        SerialCV.write(loraBuf, loraExpected);
        Serial.print(F("[CMD] rx id=0x"));
        Serial.println(loraBuf[2], HEX);
      }
      loraIdx = 0;
      loraExpected = 0;
    }
  }

  // --- GPS -> CV at 1Hz ---
  if (millis() - lastGps >= GPS_INTERVAL) {
    lastGps = millis();
    gps.getPVT(0);

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
