// ***** EMISOR *****
// Módulo: EBYTE E32 900T30D + GPS ZOE-M8Q
// Microcontrolador: ESP32 (WROOM)

#include "LoRa_E32.h"
#include <Wire.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

// Pines seguros y estándar para el ESP32
#define PIN_RX 16   // Conectar al TX del E32
#define PIN_TX 17   // Conectar al RX del E32
#define PIN_AUX 4   

// ¡Pines movidos para liberar el I2C (21 y 22)!
#define PIN_M0 25   
#define PIN_M1 26   

LoRa_E32 e32ttl(&Serial2, PIN_AUX, PIN_M0, PIN_M1);
SFE_UBLOX_GNSS miGPS; // Instancia del GPS

struct __attribute__((packed)) PaqueteTelemetria {
    uint8_t header1;         
    uint8_t header2;         
    int32_t altitude;        
    uint8_t gpsAltitude;     // CUIDADO: Máximo 255. Se desbordará en vuelo real.
    uint8_t flightStatus;    
    int16_t accX;            
    int16_t accY;            
    int16_t accZ;            
    int16_t gyroX;           
    int16_t gyroY;           
    int16_t gyroZ;           
    int16_t roll;            
    int16_t pitch;           
    uint16_t yaw;            
    int32_t gpsLatitude;     
    int32_t gpsLongitude;    
    int16_t batteryVoltage;  
    int16_t temperature;     
    uint8_t timestamp;       
};

PaqueteTelemetria misDatos; 
unsigned long tiempoAnterior = 0;

void setup() {
  Serial.begin(9600); 
  delay(1000);

  Serial2.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX);
  e32ttl.begin(); 

  // Inicialización del I2C y el GPS
  Wire.begin(21, 22); // SDA = 21, SCL = 22
  
  if (miGPS.begin() == false) {
    Serial.println(F("Fallo I2C: ZOE-M8Q no detectado."));
    while (1); 
  }
  
  // Configuración de cohetería (Airborne)
  miGPS.setI2COutput(COM_TYPE_UBX); 
  miGPS.setDynamicModel(DYN_MODEL_AIRBORNE4g); 
  miGPS.setNavigationFrequency(10); // 10 Hz

  randomSeed(analogRead(34));
  misDatos.header1 = 0xAA;
  misDatos.header2 = 0xBB;

  Serial.println(F("--- SISTEMA DE TELEMETRIA COHETEROS LISTO ---"));
}

void loop() {
  unsigned long tiempoActual = millis();
  
  if (tiempoActual - tiempoAnterior >= 600) { 
    tiempoAnterior = tiempoActual;

    // 1. Extraemos los datos reales del GPS
    if (miGPS.getPVT()) { // Comprueba si hay un paquete de navegación válido
      // La librería devuelve coordenadas en grados * 10^7 (ideal para int32_t)
      misDatos.gpsLatitude = miGPS.getLatitude(); 
      misDatos.gpsLongitude = miGPS.getLongitude(); 
      
      // La librería devuelve altitud en milímetros. Pasamos a metros.
      // Limitamos a 255 para evitar comportamientos erráticos con tu uint8_t
      long altMetros = miGPS.getAltitude() / 1000;
      misDatos.gpsAltitude = (altMetros > 255) ? 255 : (uint8_t)altMetros; 
    }

    // 2. Simulamos el resto de los sensores para mantener la trama llena
    misDatos.altitude = random(100000, 300000);       
    misDatos.flightStatus = 2;                     
    misDatos.accX = random(-2000, 2000);                
    misDatos.accY = random(-2000, 2000);
    misDatos.accZ = random(-2000, 2000);
    misDatos.gyroX = random(-1000, 1000);               
    misDatos.gyroY = random(-1000, 1000);
    misDatos.gyroZ = random(-1000, 1000);
    misDatos.roll = random(-900, 900);                 
    misDatos.pitch = random(-900, 900);
    misDatos.yaw = random(0, 36000);
    misDatos.batteryVoltage = random(700, 840);      
    misDatos.temperature = random(150, 450);         
    misDatos.timestamp++;                          

    // 3. Enviamos el paquete
    ResponseStatus rs = e32ttl.sendMessage((void *)&misDatos, sizeof(misDatos));    

    if (rs.code == 1) {
      Serial.print(F("Paquete #"));
      Serial.print(misDatos.timestamp);
      Serial.print(F(" | Lat: ")); Serial.print(misDatos.gpsLatitude);
      Serial.print(F(" | Lon: ")); Serial.println(misDatos.gpsLongitude);
    } else {
      Serial.print(F("Error TX: "));
      Serial.println(rs.getResponseDescription());
    }
  }
}