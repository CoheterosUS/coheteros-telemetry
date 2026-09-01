// ***** ESP32 FAKE TELEMETRY + REAL GPS *****
// Sends 52-byte Wire Telemetry packets via LoRa to ground station.
// Sensor data is simulated. GPS data is real from ZOE-M8Q.

#include <Wire.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include "LoRa_E32.h"

#define PIN_RX_LORA 16
#define PIN_TX_LORA 17
#define PIN_AUX 4
#define PIN_M0 25
#define PIN_M1 26

#define TELEMETRY_SIZE 52

LoRa_E32 e32ttl(&Serial2, PIN_AUX, PIN_M0, PIN_M1);
SFE_UBLOX_GNSS miGPS;

int32_t gpsLatitude = 0;
int32_t gpsLongitude = 0;
int32_t gpsAltitude = 0;
uint8_t gpsSatellites = 0;

unsigned long lastTx = 0;

void packInt16(uint8_t* buf, int16_t val) {
  memcpy(buf, &val, 2);
}

void packInt32(uint8_t* buf, int32_t val) {
  memcpy(buf, &val, 4);
}

void packUint32(uint8_t* buf, uint32_t val) {
  memcpy(buf, &val, 4);
}

void setup() {
  Serial.begin(115200);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX_LORA, PIN_TX_LORA);
  e32ttl.begin();

  Wire.begin(21, 22);
  if (miGPS.begin()) {
    miGPS.setI2COutput(COM_TYPE_UBX);
    miGPS.setDynamicModel(DYN_MODEL_AIRBORNE4g);
    miGPS.setNavigationFrequency(10);
    Serial.println(F("GPS OK"));
  } else {
    Serial.println(F("GPS FAIL — continuing without GPS"));
  }

  randomSeed(analogRead(34));
  Serial.println(F("--- FAKE TELEMETRY READY ---"));
}

void loop() {
  if (miGPS.getPVT()) {
    gpsLatitude = miGPS.getLatitude();
    gpsLongitude = miGPS.getLongitude();
    gpsAltitude = miGPS.getAltitude() / 10;  // mm -> ×100 m
    gpsSatellites = miGPS.getSIV();
  }

  if (millis() - lastTx < 100) return;
  lastTx = millis();

  uint8_t pkt[TELEMETRY_SIZE];
  memset(pkt, 0, TELEMETRY_SIZE);

  // Sync (offset 0-1)
  pkt[0] = 0xFE;
  pkt[1] = 0xCA;

  // Tick (offset 2-5, uint32 raw)
  uint32_t tick = millis();
  packUint32(pkt + 2, tick);

  // AccelXYZ — fake (offset 6-11, int16 truncated)
  packInt16(pkt + 6,  (int16_t)random(-5, 5));        // AccelX
  packInt16(pkt + 8,  (int16_t)random(-5, 5));        // AccelY
  packInt16(pkt + 10, (int16_t)9);                     // AccelZ (~9 m/s²)

  // GyroXYZ — fake (offset 12-17, int16 truncated)
  packInt16(pkt + 12, (int16_t)random(-1, 1));         // GyroX
  packInt16(pkt + 14, (int16_t)random(-1, 1));         // GyroY
  packInt16(pkt + 16, (int16_t)random(-1, 1));         // GyroZ

  // PressurePa (offset 18-19, int16 ÷10)
  packInt16(pkt + 18, (int16_t)10132);                 // 101325 Pa ÷ 10

  // TemperatureC (offset 20, int8 truncated)
  pkt[20] = (int8_t)25;                                // 25 °C

  // GPS — real (offset 21-33)
  packInt32(pkt + 21, gpsLatitude);                    // Latitude (deg ×10^7)
  packInt32(pkt + 25, gpsLongitude);                   // Longitude (deg ×10^7)
  packInt32(pkt + 29, gpsAltitude);                    // GPSAltitude (m ×100)
  pkt[33] = gpsSatellites;                             // Satellites

  // BaroAltitude (offset 34-37, int32 ×100)
  packInt32(pkt + 34, random(0, 10000));

  // BaroVelocity (offset 38-41, int32 ×100)
  packInt32(pkt + 38, random(-500, 500));

  // FaultFlags (offset 42-45, uint32 bitmask)
  packUint32(pkt + 42, 0x00000000);

  // BatteryVoltage (offset 46-47, int16 ×10)
  packInt16(pkt + 46, (int16_t)(37 + random(-1, 1)));  // ~3.7 V

  // State (offset 48)
  pkt[48] = 0;   // IDLE

  // RelayState (offset 49)
  pkt[49] = 0;

  // LastCommand (offset 50)
  pkt[50] = 0;   // COMMAND_NONE

  // Footer (offset 51)
  pkt[51] = 0xBE;

  ResponseStatus rs = e32ttl.sendMessage(pkt, TELEMETRY_SIZE);

  if (rs.code == 1) {
    Serial.print(F("TX | Sat:"));
    Serial.print(gpsSatellites);
    Serial.print(F(" Lat:"));
    Serial.print(gpsLatitude);
    Serial.print(F(" Lon:"));
    Serial.println(gpsLongitude);
  } else {
    Serial.print(F("TX ERR: "));
    Serial.println(rs.getResponseDescription());
  }
}
