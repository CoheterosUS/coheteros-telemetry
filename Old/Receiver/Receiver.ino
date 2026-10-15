// ***** GROUND STATION (Protocolo Coheteros) *****
// Bidirectional passthrough: LoRa <-> Serial (UI)
// Telemetry: LoRa 52B -> Serial (UI reads and decodes)
// Commands:  Serial 5B  -> LoRa   (UI builds and sends)

#include <SoftwareSerial.h>
#include "LoRa_E32.h"

#define PIN_RX 8   // Connect to E32 TX
#define PIN_TX 9   // Connect to E32 RX
#define PIN_AUX 4
#define PIN_M0 5
#define PIN_M1 6

#define TELEMETRY_SIZE 52
#define COMMAND_SIZE 5
#define LAST_COMMAND_OFFSET 50
#define CMD_RETRY_INTERVAL_MS 1000

SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E32 e32ttl(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

uint8_t telRxBuf[TELEMETRY_SIZE];
uint8_t telRxIdx = 0;

uint8_t pendingCmd[COMMAND_SIZE];
bool cmdPending = false;
unsigned long lastCmdSendMs = 0;
unsigned long cmdDeadlineMs = 0;
#define CMD_RETRY_DURATION_MS 5000

void setup() {
  Serial.begin(115200);
  pinMode(PIN_AUX, INPUT);
  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);

  loraSerial.begin(9600);
  e32ttl.begin();

  // Enter configuration mode: M0=HIGH, M1=HIGH
  digitalWrite(PIN_M0, HIGH);
  digitalWrite(PIN_M1, HIGH);
  delay(50);

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

  // Exit configuration mode: M0=LOW, M1=LOW (normal/transparent)
  digitalWrite(PIN_M0, LOW);
  digitalWrite(PIN_M1, LOW);
  delay(50);
}

void loop() {
  // ========================================================
  // 1. TELEMETRY: LoRa -> Serial (non-blocking accumulation)
  // ========================================================
  while (loraSerial.available()) {
    uint8_t b = loraSerial.read();

    if (telRxIdx == 0) {
      if (b == 0xFE) telRxBuf[telRxIdx++] = b;
      continue;
    }

    if (telRxIdx == 1) {
      if (b == 0xCA) {
        telRxBuf[telRxIdx++] = b;
      } else {
        telRxIdx = 0;
      }
      continue;
    }

    telRxBuf[telRxIdx++] = b;

    if (telRxIdx == TELEMETRY_SIZE) {
      if (telRxBuf[TELEMETRY_SIZE - 1] == 0xBE) {
        Serial.write(telRxBuf, TELEMETRY_SIZE);
      }
      telRxIdx = 0;
    }
  }

  // ========================================================
  // 2. COMMANDS: Serial -> LoRa (AUX-gated)
  // ========================================================
  if (Serial.available() >= COMMAND_SIZE && digitalRead(PIN_AUX) == HIGH) {
    uint8_t cmd[COMMAND_SIZE];
    Serial.readBytes(cmd, COMMAND_SIZE);

    if (cmd[0] == 0xFE && cmd[1] == 0xCA && cmd[4] == 0xBE) {
      if (cmd[2] == 0x10 || cmd[2] == 0x20) {
        if (digitalRead(PIN_AUX) == HIGH) {
          e32ttl.sendMessage(cmd, COMMAND_SIZE);
        }
      } else {
        memcpy(pendingCmd, cmd, COMMAND_SIZE);
        cmdPending = true;
        lastCmdSendMs = 0;
        cmdDeadlineMs = millis() + CMD_RETRY_DURATION_MS;
      }
    }
  }

  if (cmdPending) {
    if (millis() >= cmdDeadlineMs) {
      cmdPending = false;
    } else if (digitalRead(PIN_AUX) == HIGH &&
               (millis() - lastCmdSendMs >= CMD_RETRY_INTERVAL_MS)) {
      e32ttl.sendMessage(pendingCmd, COMMAND_SIZE);
      lastCmdSendMs = millis();
    }
  }
}
