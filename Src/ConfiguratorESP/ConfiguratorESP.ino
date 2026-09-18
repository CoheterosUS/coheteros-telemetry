#include "LoRa_E32.h"

// Matches Emisor_Modified pin layout
#define PIN_RX_LORA 16
#define PIN_TX_LORA 17
#define PIN_AUX 4
#define PIN_M0 25
#define PIN_M1 26

LoRa_E32 e32ttl(&Serial2, PIN_AUX, PIN_M0, PIN_M1);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX_LORA, PIN_TX_LORA);
  e32ttl.begin();

  Serial.println(F("--- INTENTO DE CONFIGURACION ---"));

  // 1. Leemos la configuración actual
  ResponseStructContainer rsc = e32ttl.getConfiguration();
  Configuration configuration;
  
  if (rsc.status.code == 1) {
    configuration = *(Configuration*) rsc.data;
    Serial.println(F("Lectura previa OK. Aplicando nuevos parámetros..."));
    rsc.close(); // Cerramos la lectura
  } else {
    Serial.println(F("Fallo de lectura. Revisa cableado RX/TX."));
    return; // Detenemos el código aquí para no sobreescribir basura
  }

  // 2. Modificamos usando los parámetros estructurados de la librería
  configuration.ADDH = 0x00;
  configuration.ADDL = 0x01;
  configuration.CHAN = 0x06; 
  
  // Usamos las macros nativas de la librería para mayor seguridad
  configuration.SPED.uartBaudRate = UART_BPS_9600;
  configuration.SPED.airDataRate = AIR_DATA_RATE_010_24; 
  configuration.OPTION.transmissionPower = POWER_10; 
  configuration.OPTION.fec = FEC_1_ON;
  configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
  configuration.OPTION.wirelessWakeupTime = WAKE_UP_250;
  configuration.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;

  // 3. GUARDAMOS EN MEMORIA (La librería se encarga de M0, M1 y AUX automáticamente)
  ResponseStatus rs = e32ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
  
  if (rs.code == 1) {
    Serial.println(F("====================================="));
    Serial.println(F("¡CONFIGURACIÓN GUARDADA CON ÉXITO!"));
    Serial.println(F("El módulo ya está listo para el vuelo."));
    Serial.println(F("====================================="));
  } else {
    Serial.print(F("Error al guardar: "));
    Serial.println(rs.getResponseDescription());
  }
}

void loop() {
  // Bucle vacío
}