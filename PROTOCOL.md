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
| 3     | BURN             |
| 4     | PASSIVE_BURNOUT  |
| 5     | ACTIVE_BURNOUT   |
| 6     | APOGEE           |
| 7     | PARACHUTE        |
| 8     | LANDED           |
| 9     | GROUND_ABORT     |
| 10    | DESCENT_ABORT    |

## CommandType (Enum)

| Value  | Name                 | Notes                     |
|--------|----------------------|---------------------------|
| `0x00` | COMMAND_NONE         |                           |
| `0x01` | COMMAND_RESET        |                           |
| `0x02` | COMMAND_GROUND_ABORT |                           |
| `0x03` | COMMAND_CALIBRATION  |                           |
| `0x04` | COMMAND_DROGUE       |                           |
| `0x05` | COMMAND_LANDED       |                           |
| `0x10` | COMMAND_HIL_DATA     | Excluded From LastCommand |
| `0x20` | COMMAND_GPS_DATA     | Excluded From LastCommand |

## SystemFaultFlags (Bitmask)

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

## RelayState (Bitmask)

| Bit | Flag             |
|-----|------------------|
| 0   | DROGUE_FIRED     |
| 1   | PARACHUTE_FIRED  |

## Flight Record (Internal, Packed)

| Field          | Type   | Unit                | Notes            |
|----------------|--------|---------------------|------------------|
| Sync           | uint16 |                     | `0xCAFE`         |
| Tick           | uint32 | ms                  | FreeRTOS Tick    |
| AccelX         | float  | m/s²                |                  |
| AccelY         | float  | m/s²                |                  |
| AccelZ         | float  | m/s²                |                  |
| GyroX          | float  | Degrees/s           |                  |
| GyroY          | float  | Degrees/s           |                  |
| GyroZ          | float  | Degrees/s           |                  |
| MagX           | float  | Milligauss          |                  |
| MagY           | float  | Milligauss          |                  |
| MagZ           | float  | Milligauss          |                  |
| PressurePa     | float  | Pascals             |                  |
| TemperatureC   | float  | Celsius             |                  |
| Latitude       | int32  | Degrees × 10^7      |                  |
| Longitude      | int32  | Degrees × 10^7      |                  |
| GPSAltitude    | float  | Meters              |                  |
| UnixTime       | uint32 | Epoch Seconds       |                  |
| Milliseconds   | uint16 | 0–999               |                  |
| Satellites     | uint8  | Count               |                  |
| BaroAltitude   | float  | Meters              |                  |
| BaroVelocity   | float  | m/s                 |                  |
| VelX           | float  | m/s                 |                  |
| VelY           | float  | m/s                 |                  |
| VelZ           | float  | m/s                 |                  |
| FaultFlags     | uint32 | Bitmask             | SystemFaultFlags |
| BatteryVoltage | float  | Volts               |                  |
| State          | uint8  | SystemState         |                  |
| RelayState     | uint8  | Bitmask             | RelayState       |
| LastCommand    | uint8  | CommandType         | Persists         |
| SyncEnd        | uint8  |                     | `0xBE`           |

## Command Frame (Structure, Packed)

Received over UART from external board (ESP32/Arduino).

| Offset | Size | Type  | Value  | Description         |
|--------|------|-------|--------|---------------------|
| 0      | 1    | uint8 | `0xFE` | Sync LSB            |
| 1      | 1    | uint8 | `0xCA` | Sync MSB            |
| 2      | 1    | uint8 | CMD    | CommandType         |
| 3      | 1    | uint8 | `0x00` | Payload Length (0)  |
| 4      | 1    | uint8 | `0xBE` | Footer              |

## GPS Data Frame (Structure, Packed)

Command `0x20` (COMMAND_GPS_DATA). Received over UART from external board (ESP32/Arduino).

| Offset | Size | Type   | Value  | Description          |
|--------|------|--------|--------|----------------------|
| 0      | 1    | uint8  | `0xFE` | Sync LSB             |
| 1      | 1    | uint8  | `0xCA` | Sync MSB             |
| 2      | 1    | uint8  | `0x20` | COMMAND_GPS_DATA     |
| 3      | 1    | uint8  | `0x13` | Payload Length (19)  |
| 4–22   |      |        |        | GPS Payload          |
| 23     | 1    | uint8  | `0xBE` | Footer               |

## GPS Payload (Structure, Packed)

| Offset | Size | Type   | Field        | Unit             |
|--------|------|--------|--------------|------------------|
| 0      | 4    | uint32 | UnixTime     | Epoch Seconds    |
| 4      | 2    | uint16 | Milliseconds | 0–999            |
| 6      | 4    | int32  | Latitude     | Degrees × 10^7   |
| 10     | 4    | int32  | Longitude    | Degrees × 10^7   |
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
| 4–47   |      |        |        | HIL Payload          |
| 48     | 1    | uint8  | `0xBE` | Footer               |

## HIL Payload (Structure, Packed)

| Offset | Size | Type    | Field        | Unit       |
|--------|------|---------|--------------|------------|
| 0      | 4    | float32 | AccelX       | m/s²       |
| 4      | 4    | float32 | AccelY       | m/s²       |
| 8      | 4    | float32 | AccelZ       | m/s²       |
| 12     | 4    | float32 | GyroX        | Degrees/s  |
| 16     | 4    | float32 | GyroY        | Degrees/s  |
| 20     | 4    | float32 | GyroZ        | Degrees/s  |
| 24     | 4    | float32 | MagX         | Milligauss |
| 28     | 4    | float32 | MagY         | Milligauss |
| 32     | 4    | float32 | MagZ         | Milligauss |
| 36     | 4    | float32 | PressurePa   | Pascals    |
| 40     | 4    | float32 | TemperatureC | Celsius    |

## Wire Telemetry Packet (Structure, Packed)

| Offset | Size | Type   | Field            | Encoding          |
|--------|------|--------|------------------|-------------------|
| 0      | 2    | uint16 | Sync             | `0xCAFE`          |
| 2      | 4    | uint32 | Tick             | Raw               |
| 6      | 2    | int16  | AccelX           | Truncated         |
| 8      | 2    | int16  | AccelY           | Truncated         |
| 10     | 2    | int16  | AccelZ           | Truncated         |
| 12     | 2    | int16  | GyroX            | Truncated         |
| 14     | 2    | int16  | GyroY            | Truncated         |
| 16     | 2    | int16  | GyroZ            | Truncated         |
| 18     | 2    | int16  | PressurePa       | ÷10               |
| 20     | 1    | int8   | TemperatureC     | Truncated         |
| 21     | 4    | int32  | Latitude         | ×10^7             |
| 25     | 4    | int32  | Longitude        | ×10^7             |
| 29     | 4    | int32  | GPSAltitude      | ×100              |
| 33     | 1    | uint8  | Satellites       | Raw               |
| 34     | 4    | int32  | BaroAltitude     | ×100              |
| 38     | 4    | int32  | BaroVelocity     | ×100              |
| 42     | 4    | uint32 | Flags            | Bitmask           |
| 46     | 2    | int16  | BatteryVoltage   | ×10               |
| 48     | 1    | uint8  | State            | Enum              |
| 49     | 1    | uint8  | RelayState       | Bitmask           |
| 50     | 1    | uint8  | LastCommand      | Enum              |
| 51     | 1    | uint8  | SyncEnd          | `0xBE`            |

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
| 50     | 4    | int32   | Latitude       | ×10^7                |
| 54     | 4    | int32   | Longitude      | ×10^7                |
| 58     | 4    | float32 | GPSAltitude    | Native               |
| 62     | 4    | uint32  | UnixTime       | Epoch Seconds        |
| 66     | 2    | uint16  | Milliseconds   | 0–999                |
| 68     | 1    | uint8   | Satellites     | Raw                  |
| 69     | 4    | uint32  | Flags          | Bitmask              |
| 73     | 4    | float32 | BatteryVoltage | Native               |
| 77     | 1    | uint8   | State          | Enum                 |
| 78     | 1    | uint8   | RelayState     | Bitmask              |
| 79     | 1    | uint8   | LastCommand    | Enum                 |
| 80     | 1    | uint8   | SyncEnd        | `0xBE`               |
