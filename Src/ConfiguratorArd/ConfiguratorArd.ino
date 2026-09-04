#include "LoRa_E32.h"
#include <SoftwareSerial.h>


#define PIN_RX_LORA 3
#define PIN_TX_LORA 2
#define PIN_AUX 7
#define PIN_M0 5
#define PIN_M1 6

SoftwareSerial loraSerial(PIN_RX_LORA, PIN_TX_LORA);
LoRa_E32 e32ttl(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

void setup() {
  Serial.begin(9600);
  delay(1000);

  loraSerial.begin(9600);
  e32ttl.begin();

  Serial.println(F("--- INTENTO DE CONFIGURACION ---"));

  ResponseStructContainer rsc = e32ttl.getConfiguration();
  Configuration configuration;

  if (rsc.status.code == 1) {
    configuration = *(Configuration *)rsc.data;
    Serial.println(F("Lectura previa OK. Aplicando nuevos parámetros..."));
    rsc.close();
  } else {
    Serial.println(F("Fallo de lectura. Revisa cableado RX/TX."));
    return;
  }

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

  ResponseStatus rs =
      e32ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);

  if (rs.code == 1) {
    Serial.println(F("====================================="));
    Serial.println(F("CONFIGURACION GUARDADA CON EXITO!"));
    Serial.println(F("El modulo ya esta listo para el vuelo."));
    Serial.println(F("====================================="));
  } else {
    Serial.print(F("Error al guardar: "));
    Serial.println(rs.getResponseDescription());
  }
}

void loop() {}
