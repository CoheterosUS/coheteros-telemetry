// ***** ESP32 COMMS HUB (Protocolo Coheteros) *****
#include <Wire.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include "LoRa_E32.h"

// Pines LoRa EBYTE (UART2)
#define PIN_RX_LORA 16
#define PIN_TX_LORA 17
#define PIN_AUX 4
#define PIN_M0 25
#define PIN_M1 26

// Pines Controlador de Vuelo STM32 (UART1)
#define PIN_RX_CV 18
#define PIN_TX_CV 19

#define TELEMETRY_SIZE 52
#define TELEMETRY_INTERVAL_MS 1000
#define CMD_LISTEN_WINDOW_MS 50

LoRa_E32 e32ttl(&Serial2, PIN_AUX, PIN_M0, PIN_M1);
HardwareSerial SerialCV(1);
SFE_UBLOX_GNSS miGPS;

unsigned long ultimoGPS = 0;
unsigned long ultimoTX = 0;

uint8_t telBuffer[TELEMETRY_SIZE];
bool telReady = false;

static void waitAuxHigh() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_AUX) == LOW) {
    if (millis() - t0 > 200) break;
  }
}

static void checkAndForwardCommands() {
  while (Serial2.available() >= 5) {
    if (Serial2.peek() != 0xFE) {
      Serial2.read();
      continue;
    }

    uint8_t cmd[5];
    Serial2.readBytes(cmd, 5);

    Serial.print(F("CMD bytes: "));
    for (int i = 0; i < 5; i++) {
      Serial.print(cmd[i], HEX);
      Serial.print(' ');
    }
    Serial.println();

    if (cmd[1] == 0xCA && cmd[4] == 0xBE) {
      SerialCV.write(cmd, 5);
      Serial.print(F("CMD FWD: 0x"));
      Serial.println(cmd[2], HEX);
    } else {
      Serial.println(F("CMD INVALID frame"));
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_AUX, INPUT);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX_LORA, PIN_TX_LORA);
  e32ttl.begin();

  SerialCV.begin(115200, SERIAL_8N1, PIN_RX_CV, PIN_TX_CV);

  Wire.begin(21, 22);
  if (miGPS.begin()) {
    miGPS.setI2COutput(COM_TYPE_UBX);
    miGPS.setDynamicModel(DYN_MODEL_AIRBORNE4g);
    miGPS.setNavigationFrequency(1);
  }
}

void loop() {
  // ========================================================
  // 1. BUFFER TELEMETRY FROM CV (non-blocking)
  // ========================================================
  if (SerialCV.available() >= TELEMETRY_SIZE) {
    if (SerialCV.peek() == 0xFE) {
      SerialCV.readBytes(telBuffer, TELEMETRY_SIZE);
      if (telBuffer[1] == 0xCA && telBuffer[TELEMETRY_SIZE - 1] == 0xBE) {
        telReady = true;
      }
    } else {
      SerialCV.read();
    }
  }

  // ========================================================
  // 2. COMMANDS: LoRa -> CV (checked every iteration)
  // ========================================================
  if (digitalRead(PIN_AUX) == HIGH) {
    checkAndForwardCommands();
  }

  // ========================================================
  // 3. TELEMETRY TX: rate-limited, AUX-gated, with listen window
  // ========================================================
  if (telReady && digitalRead(PIN_AUX) == HIGH && (millis() - ultimoTX >= TELEMETRY_INTERVAL_MS)) {
    e32ttl.sendMessage(telBuffer, TELEMETRY_SIZE);
    ultimoTX = millis();
    telReady = false;

    waitAuxHigh();

    unsigned long listenStart = millis();
    while (millis() - listenStart < CMD_LISTEN_WINDOW_MS) {
      checkAndForwardCommands();
    }
  }

  // ========================================================
  // 4. GPS: I2C -> ESP32 -> Trama (24B) -> CV
  // ========================================================
  if (millis() - ultimoGPS >= 1000) {
    ultimoGPS = millis();

    if (miGPS.getPVT()) {
      uint32_t unix_time = miGPS.getHour() * 3600UL + miGPS.getMinute() * 60UL + miGPS.getSecond();
      uint16_t milliseconds = miGPS.getMillisecond();
      int32_t latitude = miGPS.getLatitude();
      int32_t longitude = miGPS.getLongitude();
      int32_t altitude_mm = miGPS.getAltitude();
      uint8_t satellites = miGPS.getSIV();

      uint8_t frame[24];
      frame[0] = 0xFE;
      frame[1] = 0xCA;
      frame[2] = 0x20;
      frame[3] = 0x13;

      memcpy(frame + 4, &unix_time, 4);
      memcpy(frame + 8, &milliseconds, 2);
      memcpy(frame + 10, &latitude, 4);
      memcpy(frame + 14, &longitude, 4);
      memcpy(frame + 18, &altitude_mm, 4);
      frame[22] = satellites;
      frame[23] = 0xBE;

      SerialCV.write(frame, 24);
    }
  }
}
