// ***** ESP32 FAKE TELEMETRY + REAL GPS *****
// Sends 52-byte Wire Telemetry packets via LoRa to ground station.
// Sensor data is simulated. GPS data is real from ZOE-M8Q.
// Receives commands from ground and reflects them in LastCommand field.

#include <Wire.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include "LoRa_E32.h"

#define PIN_RX_LORA 16
#define PIN_TX_LORA 17
#define PIN_AUX 4
#define PIN_M0 25
#define PIN_M1 26

#define TELEMETRY_SIZE 52
#define TELEMETRY_INTERVAL_MS 1000
#define CMD_LISTEN_WINDOW_MS 50

LoRa_E32 e32ttl(&Serial2, PIN_AUX, PIN_M0, PIN_M1);
SFE_UBLOX_GNSS miGPS;

int32_t gpsLatitude = 0;
int32_t gpsLongitude = 0;
int32_t gpsAltitude = 0;
uint8_t gpsSatellites = 0;

unsigned long lastTx = 0;
uint8_t lastCommand = 0;

void packInt16(uint8_t* buf, int16_t val) {
  memcpy(buf, &val, 2);
}

void packInt32(uint8_t* buf, int32_t val) {
  memcpy(buf, &val, 4);
}

void packUint32(uint8_t* buf, uint32_t val) {
  memcpy(buf, &val, 4);
}

static void waitAuxHigh() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_AUX) == LOW) {
    if (millis() - t0 > 200) break;
  }
}

static void checkCommands() {
  while (Serial2.available() >= 5) {
    if (Serial2.peek() != 0xFE) {
      Serial2.read();
      continue;
    }

    uint8_t cmd[5];
    Serial2.readBytes(cmd, 5);

    if (cmd[1] == 0xCA && cmd[4] == 0xBE) {
      lastCommand = cmd[2];
      Serial.print(F("CMD RX: 0x"));
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

  Wire.begin(21, 22);
  if (miGPS.begin()) {
    miGPS.setI2COutput(COM_TYPE_UBX);
    miGPS.setDynamicModel(DYN_MODEL_AIRBORNE4g);
    miGPS.setNavigationFrequency(1);
    miGPS.setAutoPVT(true);
    Serial.println(F("GPS OK"));
  } else {
    Serial.println(F("GPS FAIL — continuing without GPS"));
  }

  randomSeed(analogRead(34));
  Serial.println(F("--- FAKE TELEMETRY READY ---"));
}

void loop() {
  // ========================================================
  // 1. GPS: non-blocking poll
  // ========================================================
  if (miGPS.getPVT(0)) {
    gpsLatitude = miGPS.getLatitude();
    gpsLongitude = miGPS.getLongitude();
    gpsAltitude = miGPS.getAltitude() / 10;  // mm -> x100 m
    gpsSatellites = miGPS.getSIV();
  }

  // ========================================================
  // 2. COMMANDS: LoRa -> update lastCommand
  // ========================================================
  if (digitalRead(PIN_AUX) == HIGH) {
    checkCommands();
  }

  // ========================================================
  // 3. TELEMETRY TX: rate-limited, AUX-gated, with listen window
  // ========================================================
  if (digitalRead(PIN_AUX) == HIGH && (millis() - lastTx >= TELEMETRY_INTERVAL_MS)) {
    lastTx = millis();

    uint8_t pkt[TELEMETRY_SIZE];
    memset(pkt, 0, TELEMETRY_SIZE);

    pkt[0] = 0xFE;
    pkt[1] = 0xCA;

    uint32_t tick = millis();
    packUint32(pkt + 2, tick);

    packInt16(pkt + 6,  (int16_t)random(-5, 5));
    packInt16(pkt + 8,  (int16_t)random(-5, 5));
    packInt16(pkt + 10, (int16_t)9);

    packInt16(pkt + 12, (int16_t)random(-1, 1));
    packInt16(pkt + 14, (int16_t)random(-1, 1));
    packInt16(pkt + 16, (int16_t)random(-1, 1));

    packInt16(pkt + 18, (int16_t)10132);
    pkt[20] = (int8_t)25;

    packInt32(pkt + 21, gpsLatitude);
    packInt32(pkt + 25, gpsLongitude);
    packInt32(pkt + 29, gpsAltitude);
    pkt[33] = gpsSatellites;

    packInt32(pkt + 34, random(0, 10000));
    packInt32(pkt + 38, random(-500, 500));

    packUint32(pkt + 42, 0x00000000);

    packInt16(pkt + 46, (int16_t)(37 + random(-1, 1)));

    pkt[48] = 0;   // State: IDLE
    pkt[49] = 0;   // RelayState
    pkt[50] = lastCommand;
    pkt[51] = 0xBE;

    e32ttl.sendMessage(pkt, TELEMETRY_SIZE);

    Serial.print(F("TX | Sat:"));
    Serial.print(gpsSatellites);
    Serial.print(F(" Lat:"));
    Serial.print(gpsLatitude);
    Serial.print(F(" Lon:"));
    Serial.print(gpsLongitude);
    Serial.print(F(" CMD:0x"));
    Serial.println(lastCommand, HEX);

    waitAuxHigh();

    unsigned long listenStart = millis();
    while (millis() - listenStart < CMD_LISTEN_WINDOW_MS) {
      checkCommands();
    }
  }
}
