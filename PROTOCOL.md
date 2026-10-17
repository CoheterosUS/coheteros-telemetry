# Protocol Specification

Little-Endian

float: 4 bytes, IEEE 754

int8: 1 byte, Two's Complement
int16: 2 bytes, Two's Complement
int32: 4 bytes, Two's Complement

uint8: 1 byte
uint16: 2 bytes
uint32: 4 bytes

Any enum, bitmask, or structure is subject to change without notice.
Any agent that reads this file should ask any necessary clarifying questions whenever information is missing, ambiguous, incomplete, or required to proceed correctly.
Any agent that reads this file should not assume that any information is correct, complete, or up-to-date unless it is explicitly stated to be so.

## SystemState (Enum)

| Value | Name             |
|-------|------------------|
| 0     | IDLE             |
| 1     | CALIBRATION      |
| 2     | PRELAUNCH        |
| 3     | BOOST            |
| 4     | COAST            |
| 5     | ACTIVE_CONTROL   |
| 6     | APOGEE           |
| 7     | MAIN_PARACHUTE   |
| 8     | LANDED           |
| 9     | GROUND_ABORT     |
| 10    | DESCENT_ABORT    |
| 11    | ASCENT_ABORT     |
| 12    | DEEP_CALIBRATION |

## CommandType (Enum)

| Value  | Name                  | Notes                     |
|--------|-----------------------|---------------------------|
| `0x00` | COMMAND_NONE          |                           |
| `0x01` | COMMAND_RESET         |                           |
| `0x02` | COMMAND_GROUND_ABORT  |                           |
| `0x03` | COMMAND_CALIBRATION   | Requires 4-byte payload: pitch angle (float, radians). Ignored without pitch. |
| `0x04` | COMMAND_DROGUE        |                           |
| `0x05` | COMMAND_LANDED        |                           |
| `0x06` | COMMAND_DEEP_CALIBRATION | 1-byte payload: face index (1-6) |
| `0x07` | COMMAND_REQUEST_TELEM_A | FC responds with COMMAND_RESPONSE_TELEM_A |
| `0x08` | COMMAND_REQUEST_TELEM_B | FC responds with COMMAND_RESPONSE_TELEM_B |
| `0x09` | COMMAND_HIL_START     | Start HIL from SD (HIL.BIN), IDLE only |
| `0x10` | COMMAND_HIL_DATA      | Excluded From LastCommand |
| `0x20` | COMMAND_GPS_DATA      | Excluded From LastCommand |
| `0x80` | COMMAND_RESPONSE_TELEM_A | Response only, FC to ground |
| `0x81` | COMMAND_RESPONSE_TELEM_B | Response only, FC to ground |

## SystemFaultFlags (Bitmask)

Each bit is set (1) when that fault occurs. Bits default to 0 (no fault).

| Bit | Flag                              |
|-----|-----------------------------------|
| 0   | BMP280_MODE_IDLE_FAILED           |
| 1   | BMP280_MODE_PERFORMANCE_FAILED    |
| 2   | BMP581_MODE_IDLE_FAILED           |
| 3   | BMP581_MODE_PERFORMANCE_FAILED    |
| 4   | IIM42653_MODE_IDLE_FAILED         |
| 5   | IIM42653_MODE_PERFORMANCE_FAILED  |
| 6   | IIS2MDCTR_MODE_IDLE_FAILED        |
| 7   | IIS2MDCTR_MODE_PERFORMANCE_FAILED |
| 8   | SD_MOUNT_FAILED                   |
| 9   | SD_OPEN_FAILED                    |
| 10  | W25Q_JEDEC_ID_FAILED              |
| 11  | W25Q_INIT_FAILED                  |
| 12  | W25Q_CAL_WRITE_FAILED             |
| 13  | SIX_POINT_CAL_FAILED              |

### Deep Calibration (bits 16-21)

Present in Flags during STATE_DEEP_CALIBRATION. Zero otherwise.

| Bits  | Field              | Description                                         |
|-------|--------------------|-----------------------------------------------------|
| 16-18 | FacesCaptured      | Count of faces captured (0-6)                       |
| 19-21 | CurrentFace + 1    | 0 = none detected, 1-6 = face index being measured  |

## RelayState (Bitmask)

| Bit | Flag             |
|-----|------------------|
| 0   | DROGUE_FIRED     |
| 1   | PARACHUTE_FIRED  |

## Flight Record (Internal, Packed)

| Field              | Type   | Unit                | Notes            |
|--------------------|--------|---------------------|------------------|
| Sync               | uint16 |                     | `0xCAFE`         |
| Tick               | uint32 | ms                  | FreeRTOS Tick    |
| AccelX             | float  | m/s^2               |                  |
| AccelY             | float  | m/s^2               |                  |
| AccelZ             | float  | m/s^2               |                  |
| GyroX              | float  | Degrees/s           |                  |
| GyroY              | float  | Degrees/s           |                  |
| GyroZ              | float  | Degrees/s           |                  |
| MagX               | float  | Milligauss          |                  |
| MagY               | float  | Milligauss          |                  |
| MagZ               | float  | Milligauss          |                  |
| PressurePa         | float  | Pascals             |                  |
| TemperatureC       | float  | Celsius             |                  |
| Latitude           | int32  | Degrees x 10^7      |                  |
| Longitude          | int32  | Degrees x 10^7      |                  |
| GPSAltitude        | float  | Meters              |                  |
| UnixTime           | uint32 | Epoch Seconds       |                  |
| Milliseconds       | uint16 | 0-999               |                  |
| Satellites         | uint8  | Count               |                  |
| BarometricAltitude | float  | Meters              |                  |
| BarometricVelocity | float  | m/s                 |                  |
| GPSVelocity        | float  | m/s                 |                  |
| VelX               | float  | m/s                 | Kalman NED       |
| VelY               | float  | m/s                 | Kalman NED       |
| VelZ               | float  | m/s                 | Kalman NED       |
| PosX               | float  | m                   | Kalman NED       |
| PosY               | float  | m                   | Kalman NED       |
| PosZ               | float  | m                   | Kalman NED       |
| QuatW              | float  |                     | Kalman scalar    |
| QuatX              | float  |                     | Kalman vector    |
| QuatY              | float  |                     | Kalman vector    |
| QuatZ              | float  |                     | Kalman vector    |
| BodyAccelX         | float  | m/s^2               |                  |
| BodyAccelY         | float  | m/s^2               |                  |
| BodyAccelZ         | float  | m/s^2               |                  |
| BodyGyroX          | float  | Degrees/s           |                  |
| BodyGyroY          | float  | Degrees/s           |                  |
| BodyGyroZ          | float  | Degrees/s           |                  |
| Flags              | uint32 | Bitmask             | Bits 0-13: SystemFaultFlags, Bits 16-21: Deep Calibration |
| BatteryVoltage     | float  | Volts               |                  |
| State              | uint8  | SystemState         |                  |
| RelayState         | uint8  | Bitmask             | RelayState       |
| LastCommand        | uint8  | CommandType         | Persists         |
| SyncEnd            | uint8  |                     | `0xBE`           |

## Command Frame (Structure, Packed)

Used in both directions: inbound (ESP32/Arduino to FC) and outbound (FC telemetry responses).

| Offset | Size | Type  | Value  | Description         |
|--------|------|-------|--------|---------------------|
| 0      | 1    | uint8 | `0xFE` | Sync LSB            |
| 1      | 1    | uint8 | `0xCA` | Sync MSB            |
| 2      | 1    | uint8 | CMD    | CommandType         |
| 3      | 1    | uint8 | LEN    | Payload Length      |
| 4      | LEN  |       |        | Payload (optional)  |
| 4+LEN  | 1    | uint8 | `0xBE` | Footer              |

## Telemetry Request Payload

Commands `0x07` (COMMAND_REQUEST_TELEM_A) and `0x08` (COMMAND_REQUEST_TELEM_B). Ground station sends a 1-byte payload containing a sequence number. FC echoes this SEQ in the response packet for ACK matching.

| Offset | Size | Type  | Field | Values  |
|--------|------|-------|-------|---------|
| 0      | 1    | uint8 | SEQ   | 0-255   |

## Calibration Command Payload

Command `0x03` (COMMAND_CALIBRATION). Requires a 4-byte payload containing the launch rail pitch angle. Commands without the pitch payload are ignored.

| Offset | Size | Type    | Field          | Unit    |
|--------|------|---------|----------------|---------|
| 0      | 4    | float32 | PitchAngleRad  | Radians |

Pitch is the angle between the rocket's longitudinal axis and the horizontal plane. Vertical = pi/2.

## Deep Calibration Command Payload

Command `0x06` (COMMAND_DEEP_CALIBRATION). Requires a 1-byte payload selecting the face to capture. Board must be held still on the specified face before sending.

| Offset | Size | Type  | Field     | Values  |
|--------|------|-------|-----------|---------|
| 0      | 1    | uint8 | FaceIndex | 1-6     |

| Value | Face |
|-------|------|
| 1     | +X up |
| 2     | -X up |
| 3     | +Y up |
| 4     | -Y up |
| 5     | +Z up |
| 6     | -Z up |

Accepted in STATE_IDLE or STATE_CALIBRATION. Enters STATE_DEEP_CALIBRATION and samples the specified face. After sampling, waits for the next face command. Faces can be sent in any order. Resampling a face overwrites its previous data. When all 6 unique faces have been captured, computes calibration, saves to flash, and returns to STATE_IDLE.

## GPS Data Frame (Structure, Packed)

Command `0x20` (COMMAND_GPS_DATA). Received over UART from external board (ESP32/Arduino).

| Offset | Size | Type   | Value  | Description          |
|--------|------|--------|--------|----------------------|
| 0      | 1    | uint8  | `0xFE` | Sync LSB             |
| 1      | 1    | uint8  | `0xCA` | Sync MSB             |
| 2      | 1    | uint8  | `0x20` | COMMAND_GPS_DATA     |
| 3      | 1    | uint8  | `0x13` | Payload Length (19)  |
| 4-22   |      |        |        | GPS Payload          |
| 23     | 1    | uint8  | `0xBE` | Footer               |

## GPS Payload (Structure, Packed)

| Offset | Size | Type   | Field        | Unit             |
|--------|------|--------|--------------|------------------|
| 0      | 4    | uint32 | UnixTime     | Epoch Seconds    |
| 4      | 2    | uint16 | Milliseconds | 0-999            |
| 6      | 4    | int32  | Latitude     | Degrees x 10^7   |
| 10     | 4    | int32  | Longitude    | Degrees x 10^7   |
| 14     | 4    | int32  | Altitude     | Millimeters      |
| 18     | 1    | uint8  | Satellites   | Count            |

## HIL Data Frame (Structure, Packed)

Command `0x10` (COMMAND_HIL_DATA). Received over UART from external device (Laptop, ESP32, Arduino, etc.).

| Offset | Size | Type   | Value  | Description          |
|--------|------|--------|--------|----------------------|
| 0      | 1    | uint8  | `0xFE` | Sync LSB             |
| 1      | 1    | uint8  | `0xCA` | Sync MSB             |
| 2      | 1    | uint8  | `0x10` | COMMAND_HIL_DATA     |
| 3      | 1    | uint8  | `0x2C` | Payload Length (44)  |
| 4-47   |      |        |        | HIL Payload          |
| 48     | 1    | uint8  | `0xBE` | Footer               |

## HIL Payload (Structure, Packed)

| Offset | Size | Type    | Field        | Unit       |
|--------|------|---------|--------------|------------|
| 0      | 4    | float32 | AccelX       | m/s^2      |
| 4      | 4    | float32 | AccelY       | m/s^2      |
| 8      | 4    | float32 | AccelZ       | m/s^2      |
| 12     | 4    | float32 | GyroX        | Degrees/s  |
| 16     | 4    | float32 | GyroY        | Degrees/s  |
| 20     | 4    | float32 | GyroZ        | Degrees/s  |
| 24     | 4    | float32 | MagX         | Milligauss |
| 28     | 4    | float32 | MagY         | Milligauss |
| 32     | 4    | float32 | MagZ         | Milligauss |
| 36     | 4    | float32 | PressurePa   | Pascals    |
| 40     | 4    | float32 | TemperatureC | Celsius    |

## Wire Telemetry A (CMD `0x80`, 54 bytes)

Sent by FC in response to `COMMAND_REQUEST_TELEM_A`. SEQ echoes the request's SEQ for ACK.

| Offset | Size | Type    | Field              | Encoding            |
|--------|------|---------|--------------------|---------------------|
| 0      | 2    | uint16  | Sync               | `0xCAFE`            |
| 2      | 1    | uint8   | CMD                | `0x80`              |
| 3      | 1    | uint8   | LEN                | `0x31` (49)         |
| 4      | 1    | uint8   | SEQ                | Echoed from request |
| 5      | 4    | uint32  | Tick               | Raw                 |
| 9      | 2    | int16   | BodyAccelX         | x100                |
| 11     | 2    | int16   | BodyAccelY         | x100                |
| 13     | 2    | int16   | BodyAccelZ         | x100                |
| 15     | 2    | int16   | BodyGyroX          | x10                 |
| 17     | 2    | int16   | BodyGyroY          | x10                 |
| 19     | 2    | int16   | BodyGyroZ          | x10                 |
| 21     | 2    | int16   | QuatW              | x10000              |
| 23     | 2    | int16   | QuatX              | x10000              |
| 25     | 2    | int16   | QuatY              | x10000              |
| 27     | 2    | int16   | QuatZ              | x10000              |
| 29     | 1    | uint8   | State              | Enum                |
| 30     | 4    | float32 | BarometricAltitude | Native              |
| 34     | 4    | float32 | PosZ               | Native, Kalman NED  |
| 38     | 4    | float32 | VelZ               | Native, Kalman NED  |
| 42     | 4    | float32 | KalmanPZ           | P[2,2], pos-Z var   |
| 46     | 4    | uint32  | Flags              | Bitmask             |
| 50     | 1    | uint8   | RelayState         | Bitmask             |
| 51     | 1    | uint8   | Satellites         | Raw                 |
| 52     | 1    | uint8   | LastCommand        | Enum                |
| 53     | 1    | uint8   | SyncEnd            | `0xBE`              |

## Wire Telemetry B (CMD `0x81`, 54 bytes)

Sent by FC in response to `COMMAND_REQUEST_TELEM_B`. SEQ echoes the request's SEQ for ACK.

| Offset | Size | Type    | Field              | Encoding            |
|--------|------|---------|--------------------|---------------------|
| 0      | 2    | uint16  | Sync               | `0xCAFE`            |
| 2      | 1    | uint8   | CMD                | `0x81`              |
| 3      | 1    | uint8   | LEN                | `0x31` (49)         |
| 4      | 1    | uint8   | SEQ                | Echoed from request |
| 5      | 4    | uint32  | Tick               | Raw                 |
| 9      | 2    | int16   | BodyAccelX         | x100                |
| 11     | 2    | int16   | BodyAccelY         | x100                |
| 13     | 2    | int16   | BodyAccelZ         | x100                |
| 15     | 2    | int16   | BodyGyroX          | x10                 |
| 17     | 2    | int16   | BodyGyroY          | x10                 |
| 19     | 2    | int16   | BodyGyroZ          | x10                 |
| 21     | 2    | int16   | QuatW              | x10000              |
| 23     | 2    | int16   | QuatX              | x10000              |
| 25     | 2    | int16   | QuatY              | x10000              |
| 27     | 2    | int16   | QuatZ              | x10000              |
| 29     | 1    | uint8   | State              | Enum                |
| 30     | 4    | int32   | Latitude           | x10^7               |
| 34     | 4    | int32   | Longitude          | x10^7               |
| 38     | 4    | float32 | GPSAltitude        | Native              |
| 42     | 4    | uint32  | UnixTime           | Epoch Seconds       |
| 46     | 4    | float32 | PressurePa         | Native              |
| 50     | 2    | int16   | TemperatureC       | x100                |
| 52     | 1    | uint8   | BatteryVoltage     | x10                 |
| 53     | 1    | uint8   | SyncEnd            | `0xBE`              |

## Wire SD Log Record (Structure, Packed)

| Offset | Size | Type    | Field          | Encoding             |
|--------|------|---------|----------------|----------------------|
| 0      | 2    | uint16  | Sync           | `0xCAFE`             |
| 2      | 4    | uint32  | Tick           | Raw                  |
| 6      | 4    | float32 | AccelX         | Native               |
| 10     | 4    | float32 | AccelY         | Native               |
| 14     | 4    | float32 | AccelZ         | Native               |
| 18     | 4    | float32 | GyroX          | Native               |
| 22     | 4    | float32 | GyroY          | Native               |
| 26     | 4    | float32 | GyroZ          | Native               |
| 30     | 4    | float32 | MagX           | Native               |
| 34     | 4    | float32 | MagY           | Native               |
| 38     | 4    | float32 | MagZ           | Native               |
| 42     | 4    | float32 | PressurePa     | Native               |
| 46     | 4    | float32 | TemperatureC   | Native               |
| 50     | 4    | int32   | Latitude       | x10^7                |
| 54     | 4    | int32   | Longitude      | x10^7                |
| 58     | 4    | float32 | GPSAltitude    | Native               |
| 62     | 4    | uint32  | UnixTime       | Epoch Seconds        |
| 66     | 1    | uint8   | Satellites     | Raw                  |
| 67     | 4    | float32 | VelX           | Native, Kalman NED   |
| 71     | 4    | float32 | VelY           | Native, Kalman NED   |
| 75     | 4    | float32 | VelZ           | Native, Kalman NED   |
| 79     | 4    | float32 | PosX           | Native, Kalman NED   |
| 83     | 4    | float32 | PosY           | Native, Kalman NED   |
| 87     | 4    | float32 | PosZ           | Native, Kalman NED   |
| 91     | 4    | float32 | QuatW          | Native, scalar       |
| 95     | 4    | float32 | QuatX          | Native, vector       |
| 99     | 4    | float32 | QuatY          | Native, vector       |
| 103    | 4    | float32 | QuatZ          | Native, vector       |
| 107    | 4    | float32 | KalmanPZ       | P[2,2], pos-Z var    |
| 111    | 4    | uint32  | Flags          | Bitmask              |
| 115    | 4    | float32 | BatteryVoltage | Native               |
| 119    | 1    | uint8   | State          | Enum                 |
| 120    | 1    | uint8   | RelayState     | Bitmask              |
| 121    | 1    | uint8   | LastCommand    | Enum                 |
| 122    | 1    | uint8   | SyncEnd        | `0xBE`               |

## Wire Flash Log Record (Structure, Packed)

Stored on W25Q32JV external flash at 10 Hz. 8 records per 256-byte page. Flight boundaries marked by a marker record where State = `0xFF` and all sensor fields are zero. Marker is page-aligned (occupies first 32 bytes of a 256-byte page, rest is `0xFF` padding).

| Offset | Size | Type    | Field    | Encoding |
|--------|------|---------|----------|----------|
| 0      | 2    | uint16  | Sync     | `0xCAFE` |
| 2      | 4    | uint32  | Tick     | Raw      |
| 6      | 4    | float32 | AccelX   | Native   |
| 10     | 4    | float32 | AccelY   | Native   |
| 14     | 4    | float32 | AccelZ   | Native   |
| 18     | 4    | float32 | GyroX    | Native   |
| 22     | 4    | float32 | GyroY    | Native   |
| 26     | 4    | float32 | GyroZ    | Native   |
| 30     | 1    | uint8   | State    | Enum     |
| 31     | 1    | uint8   | SyncEnd  | `0xBE`   |
