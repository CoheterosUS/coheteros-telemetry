// ***** ESP32 COMMS HUB (Protocolo Coheteros) *****
#include "LoRa_E32.h"
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <Wire.h>


// Pines LoRa EBYTE (UART2)
#define PIN_RX_LORA 16
#define PIN_TX_LORA 17
#define PIN_AUX 4
#define PIN_M0 25
#define PIN_M1 26

// Pines Controlador de Vuelo STM32 (UART1)
#define PIN_RX_CV 19
#define PIN_TX_CV 18

#define TELEMETRY_SIZE 52
#define TELEMETRY_INTERVAL_MS 1000
#define CMD_LISTEN_WINDOW_MS 50

LoRa_E32 e32ttl(&Serial2, PIN_AUX, PIN_M0, PIN_M1);
HardwareSerial SerialCV(1);
SFE_UBLOX_GNSS miGPS;

unsigned long ultimoGPS = 0;
unsigned long ultimoTX = 0;

uint8_t telBuffer[TELEMETRY_SIZE];
uint8_t telRxIdx = 0;
bool telReady = false;

static void waitAuxHigh() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_AUX) == LOW) {
    if (millis() - t0 > 200)
      break;
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
  delay(500);
  Serial.println(F("\n[BOOT] Emitter starting..."));

  pinMode(PIN_AUX, INPUT);

  Serial.println(F("[BOOT] LoRa UART init..."));
  Serial2.begin(9600, SERIAL_8N1, PIN_RX_LORA, PIN_TX_LORA);
  e32ttl.begin();

  // Configure LoRa module (library manages M0/M1 automatically on ESP32)
  Serial.println(F("[BOOT] LoRa configuring..."));
  ResponseStructContainer rsc = e32ttl.getConfiguration();
  if (rsc.status.code == 1) {
    Configuration configuration = *(Configuration*) rsc.data;
    rsc.close();

    configuration.ADDH = 0x00;
    configuration.ADDL = 0x01;
    configuration.CHAN = 0x06;
    configuration.SPED.uartBaudRate = UART_BPS_9600;
    configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
    configuration.OPTION.transmissionPower = POWER_10;
    configuration.OPTION.fec = FEC_1_ON;
    configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
    configuration.OPTION.wirelessWakeupTime = WAKE_UP_250;
    configuration.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;

    ResponseStatus rs = e32ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == 1) {
      Serial.println(F("[BOOT] LoRa configured OK"));
    } else {
      Serial.print(F("[BOOT] LoRa config FAILED: "));
      Serial.println(rs.getResponseDescription());
    }
  } else {
    Serial.println(F("[BOOT] LoRa config read FAILED"));
    rsc.close();
  }

  Serial.println(F("[BOOT] LoRa OK"));

  Serial.println(F("[BOOT] CV UART init..."));
  SerialCV.begin(115200, SERIAL_8N1, PIN_RX_CV, PIN_TX_CV);
  Serial.println(F("[BOOT] CV OK"));

  Serial.println(F("[BOOT] GPS init..."));
  Wire.begin(22, 21);
  if (miGPS.begin()) {
    miGPS.setI2COutput(COM_TYPE_UBX);
    miGPS.setDynamicModel(DYN_MODEL_AIRBORNE4g);
    miGPS.setNavigationFrequency(1);
    miGPS.setAutoPVT(true);
    Serial.println(F("[BOOT] GPS OK"));
  } else {
    Serial.println(F("[BOOT] GPS NOT FOUND"));
  }

  Serial.println(F("[BOOT] Ready"));
}

void loop() {
  // ========================================================
  // 1. BUFFER TELEMETRY FROM CV (non-blocking accumulation)
  // ========================================================
  while (SerialCV.available()) {
    uint8_t b = SerialCV.read();

    if (telRxIdx == 0) {
      if (b == 0xFE)
        telBuffer[telRxIdx++] = b;
      continue;
    }

    if (telRxIdx == 1) {
      if (b == 0xCA) {
        telBuffer[telRxIdx++] = b;
      } else {
        telRxIdx = 0;
      }
      continue;
    }

    telBuffer[telRxIdx++] = b;

    if (telRxIdx == TELEMETRY_SIZE) {
      if (telBuffer[TELEMETRY_SIZE - 1] == 0xBE) {
        Serial.write(telBuffer, TELEMETRY_SIZE);
        telReady = true;
      }
      telRxIdx = 0;
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
  if (telReady && digitalRead(PIN_AUX) == HIGH &&
      (millis() - ultimoTX >= TELEMETRY_INTERVAL_MS)) {
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
  // 4. GPS: I2C -> ESP32 -> Trama (24B) -> CV (non-blocking)
  // ========================================================
  if (millis() - ultimoGPS >= 1000) {
    ultimoGPS = millis();

    if (miGPS.getPVT(0)) {
      uint32_t unix_time = miGPS.getHour() * 3600UL + miGPS.getMinute() * 60UL +
                           miGPS.getSecond();
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
