# TTGO T-Beam Sensor Node - Complete Project Documentation

**Author:** Markus van Kempen  
**Email:** markus.van.kempen@gmail.com  
**Organization:** Research | Floor 7½ 🏢🤏  
**Motto:** "No bug too small, no syntax too weird."

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Hardware Specifications](#hardware-specifications)
3. [Software Architecture](#software-architecture)
4. [Pin Configuration](#pin-configuration)
5. [Feature Documentation](#feature-documentation)
6. [Code Structure](#code-structure)
7. [Development Notes](#development-notes)
8. [Troubleshooting History](#troubleshooting-history)
9. [Future Enhancements](#future-enhancements)

---

## Project Overview

### Purpose
Multi-sensor IoT node for environmental monitoring and GPS tracking with dual connectivity:
- **LoRaWAN**: Long-range, low-power telemetry to TTN/Helium network
- **WiFi**: Local web dashboard for real-time monitoring and configuration

### Key Features
- Real-time environmental monitoring (BME280)
- GPS tracking with satellite lock detection
- LoRaWAN OTAA connectivity (US915)
- WiFi web dashboard with Google Maps
- OLED display with anti-burn-in
- Serial command interface
- JSON API endpoint

### Target Hardware
- **Board**: TTGO T-Beam T22 V1.1 (20191212 variant)
- **MCU**: ESP32 (Dual-core Xtensa LX6)
- **LoRa**: SX1276 (868/915 MHz)
- **GPS**: NEO-6M or NEO-8M
- **Sensors**: BME280 (I2C)
- **Display**: SSD1306 OLED 128x64 (I2C)
- **PMU**: AXP192 power management

---

## Hardware Specifications

### ESP32 Specifications
- **CPU**: Dual-core Xtensa LX6 @ 240 MHz
- **RAM**: 520 KB SRAM
- **Flash**: 4 MB (typical)
- **WiFi**: 802.11 b/g/n (2.4 GHz)
- **Bluetooth**: BLE 4.2
- **GPIO**: 34 pins (some shared)

### Power Management (AXP192)
- **Input**: USB-C or 18650 battery
- **Output Rails**:
  - DCDC1: 3.3V (ESP32)
  - DCDC2: Variable (not used)
  - LDO2: 3.3V (LoRa)
  - LDO3: 3.3V (GPS) - **Critical for GPS operation**
  - EXTEN: External enable

### Sensor Specifications

#### BME280 Environmental Sensor
- **Interface**: I2C (address 0x76 or 0x77)
- **Measurements**:
  - Temperature: -40°C to +85°C (±1°C accuracy)
  - Humidity: 0-100% RH (±3% accuracy)
  - Pressure: 300-1100 hPa (±1 hPa accuracy)
- **Update Rate**: 2 seconds (configurable)

#### GPS Module (NEO-6M/8M)
- **Interface**: UART (9600 baud)
- **Protocol**: NMEA 0183
- **Channels**: 50 (tracking)
- **Cold Start**: 26-30 seconds (typical)
- **Warm Start**: 1 second
- **Accuracy**: 2.5m CEP (50%)
- **Update Rate**: 1 Hz (default)

#### SX1276 LoRa Radio
- **Frequency**: 902-928 MHz (US915)
- **Modulation**: LoRa, FSK, GFSK, MSK, GMSK, OOK
- **TX Power**: +20 dBm max
- **RX Sensitivity**: -148 dBm
- **Range**: 2-15 km (line of sight)

---

## Software Architecture

### Main Components

```
┌─────────────────────────────────────────────────────────┐
│                     Main Loop                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐             │
│  │   GPS    │  │  Sensors │  │  Display │             │
│  │  Parser  │  │  Reader  │  │  Update  │             │
│  └──────────┘  └──────────┘  └──────────┘             │
└─────────────────────────────────────────────────────────┘
         │              │              │
         ▼              ▼              ▼
┌─────────────────────────────────────────────────────────┐
│              Data Processing Layer                      │
│  ┌──────────────┐  ┌──────────────┐                   │
│  │  CayenneLPP  │  │  JSON API    │                   │
│  │   Encoder    │  │  Generator   │                   │
│  └──────────────┘  └──────────────┘                   │
└─────────────────────────────────────────────────────────┘
         │                      │
         ▼                      ▼
┌──────────────┐      ┌──────────────────┐
│   LoRaWAN    │      │   WiFi Web       │
│   LMIC       │      │   Server         │
│   Stack      │      │   (Async)        │
└──────────────┘      └──────────────────┘
```

### Threading Model
- **Core 0**: WiFi/Bluetooth stack (Arduino)
- **Core 1**: Main application loop
- **Async**: Web server (non-blocking)
- **LMIC**: LoRaWAN timing (cooperative)

### Memory Management
- **Heap**: ~330 KB free (typical)
- **Stack**: 8 KB per task
- **PSRAM**: Not used
- **Flash**: ~1.5 MB used (code + libraries)

---

## Pin Configuration

### I2C Bus (Wire)
```
SDA: GPIO 21
SCL: GPIO 22
Clock: 100 kHz

Devices:
- 0x34: AXP192 (Power Management)
- 0x3C: SSD1306 (OLED Display)
- 0x51: PCF8563 (RTC - optional)
- 0x76/0x77: BME280 (Sensor)
```

### GPS UART (Serial1)
```
RX: GPIO 34 (GPS TX → ESP32 RX)
TX: GPIO 12 (ESP32 TX → GPS RX)
Baud: 9600
Config: 8N1
```

### LoRa SPI
```
SCK:  GPIO 5
MISO: GPIO 19
MOSI: GPIO 27
NSS:  GPIO 18 (Chip Select)
RST:  GPIO 23 (Reset)
DIO0: GPIO 26 (IRQ)
DIO1: GPIO 33 (IRQ)
DIO2: GPIO 32 (IRQ)
```

### Special Pins
```
GPIO 0:  Boot button (WiFi reset)
GPIO 13: GPS reset (alternative - not used)
GPIO 15: GPS reset (alternative - not used)
GPIO 4:  AXP192 IRQ (not used)
```

### Pin Usage Notes
- **GPIO 34**: Input only (no pullup)
- **GPIO 0**: Boot mode selection (pulled high)
- **Strapping pins**: 0, 2, 5, 12, 15 (be careful)

---

## Feature Documentation

### 1. Environmental Monitoring (BME280)

**Implementation**: `updateSensorReadings()`

**Features**:
- Automatic retry on read failure (3 attempts)
- Range validation (prevents sensor errors)
- Altitude calculation from pressure
- 2-second update interval

**Data Flow**:
```
BME280 → I2C → ESP32 → Validation → Global Variables
                                   ↓
                            LoRa Payload / Web API
```

**Troubleshooting**:
- Check I2C address (0x76 vs 0x77)
- Verify I2C pullup resistors
- Monitor for I2C bus errors

### 2. GPS Tracking

**Implementation**: `loop()` GPS parser section

**Critical Fix (T22 V1.1 20191212)**:
- GPIO reset was **disabled** - it prevented GPS LED from turning on
- AXP192 LDO3 must be enabled for GPS power
- UBX NMEA re-enable commands were **disabled** - they interfered with operation

**Data Flow**:
```
GPS Module → UART → TinyGPS++ → Validation → Global Variables
                                            ↓
                                    LoRa / Web / Display
```

**Satellite Acquisition**:
- Cold start: 3-10 minutes (outdoors)
- Warm start: 30-60 seconds
- Requires clear sky view
- Indoor operation: Not possible

**NMEA Sentences Processed**:
- `$GPGGA`: Position, altitude, fix quality
- `$GPRMC`: Position, speed, date/time
- `$GPGSV`: Satellites in view
- `$GPGSA`: DOP and active satellites

### 3. LoRaWAN Connectivity

**Implementation**: LMIC library with custom configuration

**Network**: The Things Network (TTN) or Helium

**Join Method**: OTAA (Over-The-Air Activation)
- More secure than ABP
- Automatic session key generation
- Network server assigns DevAddr

**Payload Format**: CayenneLPP
- Channel 1: Temperature (°C)
- Channel 2: Humidity (%)
- Channel 3: Pressure (hPa)
- Channel 4: Altitude (m) - analog input
- Channel 5: GPS (lat/lon/alt)
- Channel 6: HDOP - analog input
- Channel 7: Satellite count - analog input
- Channel 8: GPS time - analog input

**Uplink Interval**: 60 seconds (configurable)

**Region**: US915 (North America)
- Sub-band 2 (channels 8-15)
- 125 kHz bandwidth
- SF7-SF10 adaptive data rate

### 4. WiFi Web Dashboard

**Implementation**: ESPAsyncWebServer + WiFiManager

**Features**:
- Automatic AP mode if no credentials
- Captive portal for configuration
- Real-time data updates (1 second)
- Google Maps integration
- Responsive design
- JSON API endpoint

**Endpoints**:
```
GET /              → HTML dashboard
GET /api/data      → JSON sensor data
GET /reset-wifi    → Reset WiFi credentials
```

**WiFi Manager**:
- AP SSID: `TTGO-T-Beam-Setup`
- AP Password: `12345678`
- Timeout: 3 minutes
- Boot button reset: Hold GPIO 0 during power-on

**Security Considerations**:
- No authentication (local network only)
- Change default AP password for production
- Consider adding HTTPS for internet exposure
- Restrict Google Maps API key

### 5. OLED Display

**Implementation**: Adafruit SSD1306 library

**Display Pages** (6-second rotation):
1. **Environment**: Temp, humidity, pressure
2. **GPS Fix**: Lat/lon, satellites, HDOP
3. **GPS Time**: UTC date/time
4. **LoRa**: Join status, uplinks, RSSI

**Anti-Burn-In Protection**:
- Page rotation: 6 seconds
- Pixel shift: 12 seconds (±1 pixel X/Y)
- Display sleep: 8 seconds every 60 seconds

**Burn-In Prevention Strategy**:
```
Time 0s:    Page 0, Shift (0,0)
Time 6s:    Page 1, Shift (0,0)
Time 12s:   Page 2, Shift (1,0)
Time 18s:   Page 3, Shift (1,0)
Time 24s:   Page 0, Shift (1,1)
...
Time 60s:   Display OFF for 8 seconds
Time 68s:   Display ON, continue rotation
```

---

## Code Structure

### File Organization
```
NewCodeForOldTTGO/
├── platformio.ini              # Build configuration
├── README.md                   # Quick start guide
├── WIFI_SETUP.md              # WiFi setup instructions
├── PROJECT_DOCUMENTATION.md   # This file
├── include/
│   └── lorawan_secrets.h      # LoRaWAN keys (not in git)
├── src/
│   └── main.cpp               # Main application (1600+ lines)
└── lib/                       # Custom libraries (if any)
```

### Main.cpp Structure
```cpp
// Lines 1-70: Header and includes
// Lines 71-130: Constants and configuration
// Lines 131-170: Global objects and variables
// Lines 171-500: Helper functions
// Lines 501-700: LoRaWAN functions
// Lines 701-900: Display functions
// Lines 901-1100: Sensor functions
// Lines 1101-1400: WiFi and web server functions
// Lines 1401-1500: Setup function
// Lines 1501-1700: Main loop
```

### Key Functions

#### Setup Sequence
```cpp
setup() {
  1. Serial.begin(115200)
  2. Wire.begin() - I2C initialization
  3. setupAxp192PowerForGps() - Enable GPS power
  4. display.begin() - OLED initialization
  5. startGpsSerial() - GPS UART start
  6. initBme() - BME280 initialization
  7. setupLoRa() - LoRaWAN join
  8. setupWiFi() - WiFi connection
}
```

#### Main Loop Flow
```cpp
loop() {
  1. Read GPS data (non-blocking)
  2. Sync system clock from GPS
  3. Update sensor readings (2s interval)
  4. Queue LoRa uplink (60s interval)
  5. Rotate display page (6s interval)
  6. Handle display sleep/wake
  7. Log system status (15s interval)
  8. Run LMIC scheduler
  9. Handle serial commands
  10. delay(5ms)
}
```

---

## Development Notes

### Lessons Learned

#### GPS Issues (T22 V1.1 20191212)
**Problem**: GPS not acquiring satellites despite NMEA stream present

**Root Cause**: GPIO reset pin (15 or 13) was interfering with GPS module operation

**Solution**: 
- Disabled GPIO reset entirely
- Rely only on AXP192 LDO3 power cycling
- Disabled UBX NMEA re-enable commands

**Key Indicator**: Blue GPS LED not turning on = GPS module not running properly

#### WiFi + LoRaWAN Coexistence
**Challenge**: Both use radio, potential interference

**Solution**:
- Async web server (non-blocking)
- LMIC runs in cooperative scheduler
- No timing conflicts observed
- WiFi on 2.4 GHz, LoRa on 915 MHz (different bands)

#### Memory Management
**Challenge**: Large HTML page + JSON library

**Solution**:
- Use raw string literals for HTML
- ArduinoJson v7 (more efficient)
- Async server (no blocking)
- Monitor heap usage: ~330 KB free typical

### Best Practices

1. **Always check AXP192 power rails** before debugging GPS
2. **Test GPS outdoors** - indoor testing is futile
3. **Monitor serial output** - it tells you everything
4. **Use proper antenna** - GPS and LoRa need correct antennas
5. **Document pin changes** - T-Beam variants differ
6. **Version control secrets** - never commit LoRaWAN keys

### Common Pitfalls

1. **Wrong GPS pins** - RX/TX swapped on some boards
2. **Missing LDO3 enable** - GPS won't work without power
3. **Indoor GPS testing** - waste of time
4. **Wrong LoRa frequency** - US915 vs EU868
5. **Antenna on wrong connector** - GPS vs LoRa confusion
6. **Boot button conflicts** - GPIO 0 strapping pin

---

## Troubleshooting History

### Issue 1: GPS No Satellite Lock
**Date**: 2026-03-17  
**Symptoms**: NMEA stream present, 0 satellites, HDOP 99.99  
**Diagnosis**: GPS module not searching (blue LED off)  
**Solution**: Disabled GPIO reset, GPS LED turned on, satellites acquired  
**Prevention**: Don't use GPIO reset on T22 V1.1 20191212  

### Issue 2: WiFi Manager Timeout
**Symptoms**: Device restarts after 3 minutes in AP mode  
**Diagnosis**: Normal behavior - timeout reached  
**Solution**: Increase timeout or disable timeout  
**Prevention**: Configure WiFi within 3 minutes  

### Issue 3: Web Dashboard Not Loading
**Symptoms**: Can't access dashboard at IP address  
**Diagnosis**: Firewall blocking port 80  
**Solution**: Allow port 80 or test from same device  
**Prevention**: Check network connectivity first  

---

## Future Enhancements

### Planned Features
1. **MQTT Integration** - Publish to MQTT broker
2. **SD Card Logging** - Store sensor data locally
3. **Battery Monitoring** - Display battery level
4. **OTA Updates** - Update firmware over WiFi
5. **Bluetooth BLE** - Mobile app connectivity
6. **Deep Sleep Mode** - Power saving when on battery
7. **Geofencing** - Alerts when leaving area
8. **Data Visualization** - Charts on web dashboard

### Hardware Upgrades
1. **External GPS Antenna** - Better reception
2. **Solar Panel** - Continuous outdoor operation
3. **Weatherproof Enclosure** - Outdoor deployment
4. **Additional Sensors** - CO2, PM2.5, UV, etc.

### Software Improvements
1. **Web Authentication** - Password protect dashboard
2. **HTTPS Support** - Secure web interface
3. **Database Integration** - Store historical data
4. **Alert System** - Email/SMS notifications
5. **Multi-language Support** - Internationalization

---

## Contributing

### Code Style
- Use descriptive variable names
- Comment complex logic
- Follow existing formatting
- Document all functions
- Test before committing

### Pull Request Process
1. Fork the repository
2. Create feature branch
3. Make changes with tests
4. Update documentation
5. Submit pull request

### Bug Reports
Include:
- Hardware variant (T22 V1.1 date code)
- Serial monitor output
- Steps to reproduce
- Expected vs actual behavior

---

## License

MIT License (or your preferred license)

---

## Contact

**Author**: Markus van Kempen  
**Email**: markus.van.kempen@gmail.com  
**Organization**: Research | Floor 7½ 🏢🤏  

**Motto**: "No bug too small, no syntax too weird."

---

*Last Updated: 2026-03-18*  
*Version: 2.1*  
*Document Revision: 1*