/*
 * TTGO T-Beam (T22 V1.1 20191212) Sensor Node with WiFi Web Dashboard
 *
 * Author: Markus van Kempen
 * Email: Markus.van.Kempen@gmail.com
 * Organization: Research | Floor 7½ 🏢🤏
 * Motto: "No bug too small, no syntax too weird."
 *
 * Description:
 *   Multi-sensor IoT node with LoRaWAN connectivity and real-time web dashboard.
 *   Integrates BME280 environmental sensor, GPS tracking, OLED display, and
 *   WiFi-enabled web interface with Google Maps integration.
 *
 * Hardware:
 *   - TTGO T-Beam T22 V1.1 (20191212 variant)
 *   - ESP32 microcontroller
 *   - SX1276 LoRa radio (US915)
 *   - BME280 sensor (I2C)
 *   - NEO-6M/8M GPS module (UART)
 *   - SSD1306 OLED display (I2C)
 *   - AXP192 power management IC
 *
 * Features:
 *   - Real-time environmental monitoring (temperature, humidity, pressure, altitude)
 *   - GPS tracking with satellite lock detection
 *   - LoRaWAN OTAA connectivity with CayenneLPP payload
 *   - LoRaWAN downlink command control (WiFi, debug, restart)
 *   - WiFi web dashboard with live sensor data
 *   - OpenStreetMap integration with GPS marker (no API key needed!)
 *   - WiFi Manager for easy configuration
 *   - WiFi network selection UI
 *   - Interactive serial menu system
 *   - Debug mode control (verbose/quiet operation)
 *   - OLED display with anti-burn-in protection
 *   - JSON API endpoint for data access
 *   - LoRaWAN key configuration via serial and web UI
 *   - NVS persistent storage for LoRa keys
 *
 * Debug Mode:
 *   - Normal Mode (debug-off): Shows only important events, minimal output
 *   - Debug Mode (debug-on): Verbose logging for troubleshooting
 *   - Toggle via serial: debug-on / debug-off
 *   - Toggle via downlink: 0x05 (ON) / 0x06 (OFF)
 *   - Always shown: joins, errors, downlinks, user commands
 *
 * Pin Configuration (T22 V1.1 20191212):
 *   I2C: SDA=21, SCL=22
 *   GPS: RX=34, TX=12
 *   LoRa: NSS=18, RST=23, DIO0=26, DIO1=33, DIO2=32
 *   WiFi Reset: GPIO 0 (Boot button)
 *
 * WiFi Features:
 *   - Auto AP mode: SSID="TTGO-T-Beam-Setup", Password="12345678"
 *   - Web dashboard at device IP address
 *   - Serial commands: wifi-status, wifi-reset, help
 *   - JSON API: http://DEVICE_IP/api/data
 *
 * Version: 2.6.0
 * Last Updated: 2026-03-19
 *
 * Version History:
 *   See CHANGELOG.md for complete version history
 *
 * License: Apache License 2.0
 *   See LICENSE file for full license text
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <set>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <TinyGPSPlus.h>
#include <lmic.h>
#include <hal/hal.h>
#include <CayenneLPP.h>
#include <time.h>
#include <sys/time.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include "lorawan_secrets.h"

// -----------------------------------------------------------------------------
// Serial Debug Capture Class
// -----------------------------------------------------------------------------
// Custom Stream class that wraps Serial and captures output to web debug buffer
class DebugSerial : public Stream {
private:
  HardwareSerial* _serial;
  String _lineBuffer;
  
public:
  DebugSerial(HardwareSerial* serial) : _serial(serial), _lineBuffer("") {}
  
  void begin(unsigned long baud) {
    _serial->begin(baud);
  }
  
  size_t write(uint8_t c) override {
    // Write to actual serial
    size_t result = _serial->write(c);
    
    // Capture to buffer
    if (c == '\n') {
      // Complete line - add to debug buffer with timestamp
      if (_lineBuffer.length() > 0) {
        unsigned long now = millis();
        unsigned long seconds = now / 1000;
        unsigned long hours = seconds / 3600;
        unsigned long minutes = (seconds % 3600) / 60;
        unsigned long secs = seconds % 60;
        
        char timestamp[16];
        snprintf(timestamp, sizeof(timestamp), "[%02lu:%02lu:%02lu] ", hours, minutes, secs);
        
        // Add to circular buffer
        extern String debugBuffer[];
        extern int debugBufferIndex;
        extern int debugBufferCount;
        extern unsigned long debugMessageCounter;
        
        debugBuffer[debugBufferIndex] = String(timestamp) + _lineBuffer;
        debugBufferIndex = (debugBufferIndex + 1) % 100; // DEBUG_BUFFER_SIZE
        if (debugBufferCount < 100) {
          debugBufferCount++;
        }
        debugMessageCounter++;
        
        _lineBuffer = "";
      }
    } else if (c == '\r') {
      // Ignore carriage return
    } else {
      // Add character to line buffer
      _lineBuffer += (char)c;
    }
    
    return result;
  }
  
  size_t write(const uint8_t *buffer, size_t size) override {
    size_t n = 0;
    while (size--) {
      n += write(*buffer++);
    }
    return n;
  }
  
  // Stream interface methods
  int available() override { return _serial->available(); }
  int read() override { return _serial->read(); }
  int peek() override { return _serial->peek(); }
  void flush() override { _serial->flush(); }
  
  // Additional HardwareSerial methods
  operator bool() { return (bool)(*_serial); }
};

// Create debug serial wrapper
DebugSerial DebugSerial_instance(&Serial);
#define Serial DebugSerial_instance

// -----------------------------------------------------------------------------
// Firmware Version
// -----------------------------------------------------------------------------
const char* FIRMWARE_VERSION = "2.6.0";
const char* FIRMWARE_DATE = "2026-03-19";

// -----------------------------------------------------------------------------
// Board and peripheral pin configuration
// -----------------------------------------------------------------------------
// Typical TTGO T-Beam I2C pins.
static const uint8_t I2C_SDA_PIN = 21;
static const uint8_t I2C_SCL_PIN = 22;

static const uint8_t OLED_I2C_ADDR = 0x3C;
static const uint8_t OLED_WIDTH = 128;
static const uint8_t OLED_HEIGHT = 64;
static const uint8_t AXP192_I2C_ADDR = 0x34;
static const uint8_t AXP192_REG_POWER_OUTPUT_CONTROL = 0x12;
static const uint8_t RTC_I2C_ADDR = 0x51;
static const uint8_t AXP192_BIT_LDO3 = 0x08;
static const uint32_t I2C_CLOCK_HZ = 100000;

static const uint32_t GPS_BAUD = 9600;
static const uint8_t GPS_RESET_PIN = 13;  // GPIO 13 for T22 V1.1 20191212 GPS reset (alternative)
static const unsigned long GPS_NO_STREAM_LOG_MS = 5000;
static const unsigned long GPS_NO_FIX_LOG_MS = 10000;
static const unsigned long GPS_WARMUP_MS = 180000;
static const bool GPS_RAW_NMEA_DEBUG_ENABLED = true;
static const unsigned long GPS_RAW_NMEA_DEBUG_MS = 30000;
static const bool GPS_UBX_REENABLE_NMEA_AT_BOOT = false;  // Test: disable UBX commands
static const unsigned long LORA_SEND_INTERVAL_MS = 60000;
static const unsigned long CLOCK_RESYNC_MS = 21600000;

static const unsigned long SENSOR_UPDATE_MS = 2000;
static const unsigned long GPS_LDO3_RESET_OFF_MS = 300;
static const unsigned long GPS_LDO3_RESET_ON_SETTLE_MS = 1200;
static const unsigned long DISPLAY_PAGE_MS = 6000;
static const unsigned long DISPLAY_SHIFT_MS = 12000;
static const unsigned long DISPLAY_SLEEP_EVERY_MS = 60000;
static const unsigned long DISPLAY_SLEEP_MS = 8000;
static const unsigned long SERIAL_GPS_LOG_MS = 2500;
static const unsigned long SERIAL_SYSTEM_DEBUG_MS = 15000;
static const unsigned long SERIAL_AXP_DEBUG_MS = 180000;
static const uint8_t BME_READ_RETRIES = 3;

static const float BME_MIN_TEMP_C = -40.0f;
static const float BME_MAX_TEMP_C = 85.0f;
static const float BME_MIN_HUMIDITY = 0.0f;
static const float BME_MAX_HUMIDITY = 100.0f;
static const float BME_MIN_PRESSURE_HPA = 300.0f;
static const float BME_MAX_PRESSURE_HPA = 1100.0f;

static const uint8_t LORA_SCK_PIN = 5;
static const uint8_t LORA_MISO_PIN = 19;
static const uint8_t LORA_MOSI_PIN = 27;
static const uint8_t LORA_NSS_PIN = 18;
static const uint8_t LORA_RST_PIN = 23;
static const uint8_t LORA_DIO0_PIN = 26;
static const uint8_t LORA_DIO1_PIN = 33;
static const uint8_t LORA_DIO2_PIN = 32;
static const uint8_t LORA_PORT = 1;

// WiFi and Web Server Configuration
static const char* WIFI_AP_NAME = "TTGO-T-Beam-Setup";
static const char* WIFI_AP_PASSWORD = "12345678";
static const unsigned long WIFI_CONFIG_TIMEOUT_MS = 180000;  // 3 minutes
static const unsigned long WEB_UPDATE_INTERVAL_MS = 1000;
static const int WIFI_RESET_BUTTON_PIN = 0;  // Boot button for WiFi reset

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;
TinyGPSPlus gps;
HardwareSerial GpsSerial(1);
CayenneLPP lpp(120);
osjob_t sendjob;
AsyncWebServer server(80);
WiFiManager wifiManager;
Preferences preferences;

// LoRaWAN keys - loaded from NVS or defaults from lorawan_secrets.h
uint8_t storedAPPEUI[8];
uint8_t storedDEVEUI[8];
uint8_t storedAPPKEY[16];
bool loraKeysConfigured = false;

// ABP mode support
bool useABP = false;  // false = OTAA (default), true = ABP
uint8_t storedDEVADDR[4];   // Device Address for ABP
uint8_t storedNWKSKEY[16];  // Network Session Key for ABP
uint8_t storedAPPSKEY[16];  // Application Session Key for ABP

const lmic_pinmap lmic_pins = {
  .nss = LORA_NSS_PIN,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = LORA_RST_PIN,
  .dio = {LORA_DIO0_PIN, LORA_DIO1_PIN, LORA_DIO2_PIN}
};

struct GpsPinProfile {
  int rx;
  int tx;
  const char *name;
};

// T22 V1.1 mapping: GPS-TX -> IO34 (ESP RX), GPS-RX -> IO12 (ESP TX).
const GpsPinProfile GPS_PROFILES[] = {
  {34, 12, "T22 V1.1 RX34 TX12"}
};
const uint8_t GPS_PROFILE_COUNT = sizeof(GPS_PROFILES) / sizeof(GPS_PROFILES[0]);

bool bmeFound = false;
bool gpsStreamSeen = false;
bool loraJoined = false;
bool axpFound = false;
bool rtcChipFound = false;
bool clockSynced = false;
bool wifiConnected = false;
bool wifiConfigMode = false;
bool wifiEnabled = true;
bool debugEnabled = false;
bool displayEnabled = true;
float seaLevelPressureHpa = 1013.25f;
unsigned long lastUpdate = 0;
unsigned long lastPageRotate = 0;
unsigned long lastShiftRotate = 0;
unsigned long lastDisplaySleep = 0;
unsigned long displayWakeAt = 0;
unsigned long lastGpsLog = 0;
unsigned long lastGpsNoStreamLog = 0;
unsigned long lastGpsNoFixLog = 0;
unsigned long gpsFirstStreamAt = 0;
unsigned long lastLoraSendAt = 0;
unsigned long lastSystemDebugAt = 0;
unsigned long lastClockSyncAt = 0;
unsigned long lastAxpDebugAt = 0;
unsigned long gpsRawDebugStartAt = 0;
uint8_t displayPage = 0;
uint8_t burnInShiftIndex = 0;
uint8_t gpsProfileIndex = 0;
bool displaySleeping = false;
uint32_t uplinkCounter = 0;
uint32_t downlinkCounter = 0;
String gpsRawLine;

// Last known good GPS position (persisted to NVS)
double lastGoodLat = 0.0;
double lastGoodLon = 0.0;
double lastGoodAlt = 0.0;
uint32_t lastGoodSats = 0;
double lastGoodHdop = 99.99;
uint32_t lastGoodTimestamp = 0; // Unix timestamp when last saved

// Channel enable/disable flags
bool channelEnabled[10] = {true, true, true, true, true, true, true, true, true, false}; // CH1-9 enabled, CH10 (PAX) disabled by default

// PAX counter variables
bool paxCounterEnabled = false;
bool paxWifiScanEnabled = true;  // Enable WiFi scanning by default
bool paxBleScanEnabled = true;   // Enable BLE scanning by default
uint32_t lastPaxScanAt = 0;
uint32_t paxScanInterval = 60000; // 60 seconds default
uint16_t wifiDeviceCount = 0;
uint16_t bleDeviceCount = 0;
uint16_t totalPaxCount = 0;
NimBLEScan* pBLEScan = nullptr;
std::set<std::string> bleDevices; // Track unique BLE devices by address

// Debug console circular buffer
#define DEBUG_BUFFER_SIZE 100
String debugBuffer[DEBUG_BUFFER_SIZE];
int debugBufferIndex = 0;
int debugBufferCount = 0;
unsigned long debugMessageCounter = 0;
String currentDebugLine = "";  // Buffer for building complete lines

float lastTemperature = NAN;
float lastHumidity = NAN;
float lastPressureHpa = NAN;
float lastAltitude = NAN;

const int8_t SHIFT_X[] = {0, 1, -1, 2, -2};
const int8_t SHIFT_Y[] = {0, 1, -1};

void onEvent(ev_t ev);
void queueCayenneUplink();

void sendUbxFrame(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint16_t payloadLen) {
  uint8_t ckA = 0;
  uint8_t ckB = 0;

  auto addChecksum = [&](uint8_t b) {
    ckA = static_cast<uint8_t>(ckA + b);
    ckB = static_cast<uint8_t>(ckB + ckA);
  };

  GpsSerial.write(0xB5);
  GpsSerial.write(0x62);

  GpsSerial.write(msgClass);
  addChecksum(msgClass);
  GpsSerial.write(msgId);
  addChecksum(msgId);

  uint8_t lenL = static_cast<uint8_t>(payloadLen & 0xFF);
  uint8_t lenH = static_cast<uint8_t>((payloadLen >> 8) & 0xFF);
  GpsSerial.write(lenL);
  addChecksum(lenL);
  GpsSerial.write(lenH);
  addChecksum(lenH);

  for (uint16_t i = 0; i < payloadLen; i++) {
    GpsSerial.write(payload[i]);
    addChecksum(payload[i]);
  }

  GpsSerial.write(ckA);
  GpsSerial.write(ckB);
  GpsSerial.flush();
}

void reenableUbxNmeaOutput() {
  if (!GPS_UBX_REENABLE_NMEA_AT_BOOT) {
    return;
  }

  Serial.println("GPS UBX recovery: forcing UART1 NMEA output");

  // CFG-PRT (0x06 0x00): set UART1 to 9600, 8N1, in=UBX+NMEA, out=NMEA.
  const uint8_t cfgPrtUart1[] = {
    0x01, 0x00,             // portID=UART1, reserved
    0x00, 0x00,             // txReady
    0xD0, 0x08, 0x00, 0x00, // mode: 8N1
    0x80, 0x25, 0x00, 0x00, // baud: 9600
    0x03, 0x00,             // inProtoMask: UBX+NMEA
    0x02, 0x00,             // outProtoMask: NMEA
    0x00, 0x00,             // flags
    0x00, 0x00              // reserved
  };
  sendUbxFrame(0x06, 0x00, cfgPrtUart1, sizeof(cfgPrtUart1));
  delay(120);

  // CFG-MSG (0x06 0x01): ensure GGA and RMC are enabled on output.
  const uint8_t cfgMsgGga[] = {0xF0, 0x00, 0x01};
  const uint8_t cfgMsgRmc[] = {0xF0, 0x04, 0x01};
  sendUbxFrame(0x06, 0x01, cfgMsgGga, sizeof(cfgMsgGga));
  delay(80);
  sendUbxFrame(0x06, 0x01, cfgMsgRmc, sizeof(cfgMsgRmc));
  delay(80);

  Serial.println("GPS UBX recovery: commands sent");
}

void logRawNmeaChar(char c, unsigned long now) {
  if (!GPS_RAW_NMEA_DEBUG_ENABLED) {
    return;
  }
  if (gpsRawDebugStartAt == 0 || (now - gpsRawDebugStartAt) > GPS_RAW_NMEA_DEBUG_MS) {
    return;
  }

  if (c == '\r') {
    return;
  }

  if (c == '\n') {
    if (gpsRawLine.length() > 0) {
      Serial.print("NMEA RAW: ");
      Serial.println(gpsRawLine);
      gpsRawLine = "";
    }
    return;
  }

  if (gpsRawLine.length() < 120) {
    gpsRawLine += c;
  }
}

bool i2cDevicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool hasSystemTime() {
  return time(nullptr) > 100000;
}

// Reject placeholder/no-fix date-time so the internal clock is not poisoned by
// invalid GPS frames (for example 2000-00-00 00:00:00).
bool isPlausibleGpsDateTime() {
  if (!gps.date.isValid() || !gps.time.isValid()) {
    return false;
  }

  int year = gps.date.year();
  int month = gps.date.month();
  int day = gps.date.day();
  int hour = gps.time.hour();
  int minute = gps.time.minute();
  int second = gps.time.second();

  if (year < 2024 || year > 2099) {
    return false;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }
  if (hour > 23 || minute > 59 || second > 59) {
    return false;
  }

  // Avoid syncing from placeholder/no-fix streams.
  if (!gps.location.isValid()) {
    return false;
  }
  if (gps.satellites.isValid() && gps.satellites.value() < 1) {
    return false;
  }
  if (gps.hdop.isValid() && gps.hdop.hdop() >= 50.0f) {
    return false;
  }

  return true;
}

// Synchronize ESP32 system UTC clock from GPS once quality is acceptable.
// The clock is refreshed occasionally to limit drift while avoiding spam writes.
void syncSystemClockFromGpsIfNeeded(unsigned long nowMs) {
  if (!isPlausibleGpsDateTime()) {
    return;
  }
  if (gps.date.age() > 2000 || gps.time.age() > 2000) {
    return;
  }
  if (clockSynced && (nowMs - lastClockSyncAt < CLOCK_RESYNC_MS)) {
    return;
  }

  struct tm t = {};
  t.tm_year = gps.date.year() - 1900;
  t.tm_mon = gps.date.month() - 1;
  t.tm_mday = gps.date.day();
  t.tm_hour = gps.time.hour();
  t.tm_min = gps.time.minute();
  t.tm_sec = gps.time.second();

  time_t epoch = mktime(&t);
  struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  settimeofday(&tv, nullptr);

  clockSynced = true;
  lastClockSyncAt = nowMs;
  Serial.print("Clock synced from GPS epoch=");
  Serial.println(static_cast<unsigned long>(epoch));
}

void printSystemUtcToSerial() {
  if (!hasSystemTime()) {
    Serial.print("invalid");
    return;
  }

  time_t now = time(nullptr);
  struct tm utc = {};
  gmtime_r(&now, &utc);
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d UTC",
           utc.tm_year + 1900,
           utc.tm_mon + 1,
           utc.tm_mday,
           utc.tm_hour,
           utc.tm_min,
           utc.tm_sec);
  Serial.print(buffer);
}

void printHexArray(const char *label, const uint8_t *data, size_t len) {
  Serial.print(label);
  Serial.print("=");
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

/**
 * @brief Add message to debug buffer for web console
 *
 * Stores debug messages in a circular buffer that can be accessed
 * via the web debug console. Keeps last 100 messages.
 *
 * @param message Debug message to store
 */
void addToDebugBuffer(const String& message) {
  // Get current timestamp
  unsigned long now = millis();
  unsigned long seconds = now / 1000;
  unsigned long hours = seconds / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;
  
  char timestamp[16];
  snprintf(timestamp, sizeof(timestamp), "[%02lu:%02lu:%02lu] ", hours, minutes, secs);
  
  debugBuffer[debugBufferIndex] = String(timestamp) + message;
  debugBufferIndex = (debugBufferIndex + 1) % DEBUG_BUFFER_SIZE;
  if (debugBufferCount < DEBUG_BUFFER_SIZE) {
    debugBufferCount++;
  }
  debugMessageCounter++;
}

/**
 * @brief Log message to both Serial and web debug buffer
 *
 * @param message Message to log
 */
void logDebug(const String& message) {
  Serial.println(message);
  addToDebugBuffer(message);
}

/**
 * @brief Get all debug messages from buffer as JSON array
 *
 * @return String JSON array of debug messages
 */
String getDebugBufferJson() {
  String json = "[";
  
  if (debugBufferCount > 0) {
    int startIdx = (debugBufferCount < DEBUG_BUFFER_SIZE) ? 0 : debugBufferIndex;
    
    for (int i = 0; i < debugBufferCount; i++) {
      int idx = (startIdx + i) % DEBUG_BUFFER_SIZE;
      if (i > 0) json += ",";
      
      // Escape special characters in JSON
      String msg = debugBuffer[idx];
      msg.replace("\\", "\\\\");
      msg.replace("\"", "\\\"");
      msg.replace("\n", "\\n");
      msg.replace("\r", "\\r");
      msg.replace("\t", "\\t");
      
      json += "\"" + msg + "\"";
    }
  }
  
  json += "]";
  return json;
}

// Boot-time dump for OTAA/session diagnostics.
// Intentionally verbose because this project is in an active debug phase.
void logSecretsAndConfig() {
  Serial.println("================ DEBUG OTAA/CONFIG ================");
  Serial.println("WARNING: printing OTAA keys to serial output");
  printHexArray("APPEUI(little-endian)", APPEUI, sizeof(APPEUI));
  printHexArray("DEVEUI(little-endian)", DEVEUI, sizeof(DEVEUI));
  printHexArray("APPKEY(big-endian)", APPKEY, sizeof(APPKEY));

#if defined(CFG_us915)
  Serial.println("Region=US915");
#elif defined(CFG_eu868)
  Serial.println("Region=EU868");
#else
  Serial.println("Region=UNKNOWN");
#endif

  Serial.print("GPS profile=");
  Serial.println(GPS_PROFILES[gpsProfileIndex].name);
  Serial.print("LoRa pins NSS/RST/DIO0/DIO1/DIO2=");
  Serial.print(LORA_NSS_PIN);
  Serial.print('/');
  Serial.print(LORA_RST_PIN);
  Serial.print('/');
  Serial.print(LORA_DIO0_PIN);
  Serial.print('/');
  Serial.print(LORA_DIO1_PIN);
  Serial.print('/');
  Serial.println(LORA_DIO2_PIN);
  Serial.println("===================================================");
}

// Periodic high-level health snapshot covering sensors, radio and heap state.
void logSystemDetailed(unsigned long now) {
  Serial.println("================ SYSTEM DETAIL ====================");
  Serial.print("uptimeMs=");
  Serial.print(now);
  Serial.print(" freeHeap=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" minFreeHeap=");
  Serial.println(ESP.getMinFreeHeap());

  Serial.print("BME found=");
  Serial.print(bmeFound ? 1 : 0);
  Serial.print(" tempC=");
  Serial.print(lastTemperature, 2);
  Serial.print(" humPct=");
  Serial.print(lastHumidity, 2);
  Serial.print(" pressHpa=");
  Serial.print(lastPressureHpa, 2);
  Serial.print(" altM=");
  Serial.println(lastAltitude, 2);

  Serial.print("GPS stream=");
  Serial.print(gpsStreamSeen ? 1 : 0);
  Serial.print(" chars=");
  Serial.print(gps.charsProcessed());
  Serial.print(" sats=");
  Serial.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
  Serial.print(" fixValid=");
  Serial.print(gps.location.isValid() ? 1 : 0);
  Serial.print(" lat=");
  Serial.print(gps.location.isValid() ? gps.location.lat() : 0.0, 6);
  Serial.print(" lon=");
  Serial.println(gps.location.isValid() ? gps.location.lng() : 0.0, 6);

  Serial.print("AXP found=");
  Serial.print(axpFound ? 1 : 0);
  Serial.print(" rtcChip=");
  Serial.print(rtcChipFound ? 1 : 0);
  Serial.print(" page=");
  Serial.print(displayPage);
  Serial.print(" displaySleeping=");
  Serial.println(displaySleeping ? 1 : 0);

  Serial.print("LoRa joined=");
  Serial.print(loraJoined ? 1 : 0);
  Serial.print(" uplinks=");
  Serial.print(uplinkCounter);
  Serial.print(" opmode=0x");
  Serial.print(LMIC.opmode, HEX);
  Serial.print(" txrxFlags=0x");
  Serial.print(LMIC.txrxFlags, HEX);
  Serial.print(" seqUp=");
  Serial.print(LMIC.seqnoUp);
  Serial.print(" seqDn=");
  Serial.println(LMIC.seqnoDn);
  Serial.print("System UTC=");
  printSystemUtcToSerial();
  Serial.println();
  Serial.println("===================================================");
}

bool axpWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(AXP192_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool axpRead(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(AXP192_I2C_ADDR);
  Wire.write(reg);
  // Use STOP here instead of repeated-start to avoid intermittent
  // i2cWriteReadNonStop errors on some ESP32 Wire implementations.
  if (Wire.endTransmission(true) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(AXP192_I2C_ADDR), 1) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

bool axpSetPowerBits(uint8_t setMask, uint8_t clearMask) {
  uint8_t value = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, value)) {
    return false;
  }
  value |= setMask;
  value &= static_cast<uint8_t>(~clearMask);
  return axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, value);
}

void gpsPowerPulseResetByAxp() {
  if (!axpFound) {
    return;
  }

  Serial.println("GPS reset: pulsing AXP192 LDO3 OFF/ON");
  if (!axpSetPowerBits(0x00, AXP192_BIT_LDO3)) {
    Serial.println("GPS reset: failed to disable LDO3");
    return;
  }
  delay(GPS_LDO3_RESET_OFF_MS);

  if (!axpSetPowerBits(AXP192_BIT_LDO3, 0x00)) {
    Serial.println("GPS reset: failed to enable LDO3");
    return;
  }
  delay(GPS_LDO3_RESET_ON_SETTLE_MS);
  Serial.println("GPS reset: LDO3 restored");
}

void resetGpsModuleViaGpio() {
  Serial.print("GPS reset: toggling GPIO ");
  Serial.print(GPS_RESET_PIN);
  Serial.println(" (T22 V1.1 20191212 fix)");
  
  pinMode(GPS_RESET_PIN, OUTPUT);
  
  // Hold GPS module in reset state
  digitalWrite(GPS_RESET_PIN, LOW);
  delay(200);
  
  // Release reset and allow GPS to boot
  digitalWrite(GPS_RESET_PIN, HIGH);
  delay(500);
  
  Serial.println("GPS reset: GPIO reset complete");
}

void axpPrintReg(uint8_t reg, const char *name) {
  uint8_t v = 0;
  if (axpRead(reg, v)) {
    Serial.print("AXP ");
    Serial.print(name);
    Serial.print(" (0x");
    Serial.print(reg, HEX);
    Serial.print(")=0x");
    if (v < 0x10) {
      Serial.print('0');
    }
    Serial.println(v, HEX);
  } else {
    Serial.print("AXP read fail reg 0x");
    Serial.println(reg, HEX);
  }
}

void logAxp192Detailed() {
  if (!axpFound) {
    Serial.println("AXP192 DETAIL: skipped (AXP not detected)");
    return;
  }

  Serial.println("================ AXP192 DETAIL ====================");
  axpPrintReg(0x00, "PWR_STATUS");
  axpPrintReg(0x01, "CHG_STATUS");
  axpPrintReg(0x02, "MODE_CHG");
  axpPrintReg(0x12, "PWR_OUT_CTRL");
  axpPrintReg(0x28, "LDO23_VOLT");
  axpPrintReg(0x82, "ADC_ENABLE_1");
  axpPrintReg(0x83, "ADC_ENABLE_2");
  axpPrintReg(0x84, "ADC_TS");

  uint8_t pwrOut = 0;
  if (axpRead(0x12, pwrOut)) {
    Serial.print("AXP rails DCDC1=");
    Serial.print((pwrOut & 0x01) ? "ON" : "OFF");
    Serial.print(" DCDC2=");
    Serial.print((pwrOut & 0x10) ? "ON" : "OFF");
    Serial.print(" DCDC3=");
    Serial.print((pwrOut & 0x02) ? "ON" : "OFF");
    Serial.print(" LDO2=");
    Serial.print((pwrOut & 0x04) ? "ON" : "OFF");
    Serial.print(" LDO3=");
    Serial.print((pwrOut & 0x08) ? "ON" : "OFF");
    Serial.print(" EXTEN=");
    Serial.println((pwrOut & 0x40) ? "ON" : "OFF");
  }

  Serial.println("===================================================");
}

void setupAxp192PowerForGps() {
  uint8_t current = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, current)) {
    Serial.println("AXP192 not found on I2C 0x34");
    axpFound = false;
    return;
  }

  axpFound = true;
  Serial.println("================ AXP192 BATTERY SETUP =============");
  Serial.print("AXP192 power reg 0x12 before=0x");
  Serial.println(current, HEX);

  // Match LilyGO T22 GPS sketch: DCDC1, DCDC2, LDO2, LDO3, EXTEN.
  // These rails are required on many T22 boards for GPS to operate reliably.
  const uint8_t requiredBits = 0x5D;
  uint8_t updated = current | requiredBits;

  if (!axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, updated)) {
    Serial.println("AXP192 write failed");
    return;
  }

  uint8_t verify = 0;
  if (axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, verify)) {
    Serial.print("AXP192 power reg 0x12 after =0x");
    Serial.println(verify, HEX);
  }

  // Configure battery charging (Register 0x33)
  // Bit 7: Charging enable (1=enable)
  // Bit 6-5: Target voltage (00=4.1V, 01=4.15V, 10=4.2V, 11=4.36V)
  // Bit 4: Charging end current (0=10%, 1=15%)
  // Bit 3-0: Charging current (0000=100mA to 1101=1320mA, steps of ~100mA)
  // Default: 0xC0 = 11000000 = Charging enabled, 4.2V, 10% end current, 100mA
  // Recommended for 18650: 0xC2 = 11000010 = Charging enabled, 4.2V, 10% end current, 300mA
  uint8_t chargingConfig = 0xC2;  // Enable charging, 4.2V target, 300mA current
  if (axpWrite(0x33, chargingConfig)) {
    Serial.print("AXP192 charging enabled: 0x");
    Serial.println(chargingConfig, HEX);
    Serial.println("  Target: 4.2V, Current: 300mA");
  } else {
    Serial.println("AXP192 charging config write failed");
  }

  // Enable ADC for battery monitoring (Register 0x82)
  // Bit 7: Battery voltage ADC enable
  // Bit 6: Battery current ADC enable
  // Bit 5: ACIN voltage ADC enable
  // Bit 4: ACIN current ADC enable
  // Bit 3: VBUS voltage ADC enable
  // Bit 2: VBUS current ADC enable
  // Bit 1: APS voltage ADC enable
  // Bit 0: TS pin ADC enable
  uint8_t adcEnable1 = 0x83;  // Enable battery voltage, current, and ACIN voltage ADC
  if (axpWrite(0x82, adcEnable1)) {
    Serial.println("AXP192 ADC enabled for battery monitoring");
  }

  // Read and display charging status
  uint8_t chargingStatus = 0;
  if (axpRead(0x01, chargingStatus)) {
    Serial.print("Charging status: 0x");
    Serial.println(chargingStatus, HEX);
    bool isCharging = (chargingStatus & 0x40) != 0;
    bool batteryConnected = (chargingStatus & 0x20) != 0;
    Serial.print("  Battery connected: ");
    Serial.println(batteryConnected ? "YES" : "NO");
    Serial.print("  Charging: ");
    Serial.println(isCharging ? "YES" : "NO");
  }

  Serial.println("===================================================");

  gpsPowerPulseResetByAxp();
}

/**
 * @brief Read battery voltage from AXP192
 *
 * Reads the battery voltage ADC registers and converts to voltage.
 * Battery voltage is stored in registers 0x78 (MSB) and 0x79 (LSB)
 * Resolution: 1.1mV per LSB
 *
 * @return float Battery voltage in volts, or -1.0 if read fails
 */
float readBatteryVoltage() {
  if (!axpFound) return -1.0;
  
  uint8_t msb = 0, lsb = 0;
  if (!axpRead(0x78, msb) || !axpRead(0x79, lsb)) {
    return -1.0;
  }
  
  // Combine MSB and LSB (12-bit value)
  uint16_t raw = (msb << 4) | (lsb & 0x0F);
  
  // Convert to voltage: 1.1mV per LSB
  float voltage = raw * 1.1 / 1000.0;
  
  return voltage;
}

/**
 * @brief Read battery charge/discharge current from AXP192
 *
 * Positive = charging, Negative = discharging
 *
 * @return float Current in mA, or 0.0 if read fails
 */
float readBatteryCurrent() {
  if (!axpFound) return 0.0;
  
  // Read charge current (0x7A, 0x7B)
  uint8_t chargeMsb = 0, chargeLsb = 0;
  if (axpRead(0x7A, chargeMsb) && axpRead(0x7B, chargeLsb)) {
    uint16_t chargeRaw = (chargeMsb << 5) | (chargeLsb & 0x1F);
    float chargeCurrent = chargeRaw * 0.5;  // 0.5mA per LSB
    
    // Read discharge current (0x7C, 0x7D)
    uint8_t dischargeMsb = 0, dischargeLsb = 0;
    if (axpRead(0x7C, dischargeMsb) && axpRead(0x7D, dischargeLsb)) {
      uint16_t dischargeRaw = (dischargeMsb << 5) | (dischargeLsb & 0x1F);
      float dischargeCurrent = dischargeRaw * 0.5;  // 0.5mA per LSB
      
      // Return net current (positive = charging, negative = discharging)
      return chargeCurrent - dischargeCurrent;
    }
  }
  
  return 0.0;
}

/**
 * @brief Check if battery is currently charging
 *
 * @return bool True if charging, false otherwise
 */
bool isBatteryCharging() {
  if (!axpFound) return false;
  
  uint8_t status = 0;
  if (axpRead(0x01, status)) {
    return (status & 0x40) != 0;  // Bit 6 = charging indicator
  }
  
  return false;
}

/**
 * @brief Enable GPS power via AXP192
 *
 * Turns on LDO3 which powers the GPS module
 */
void enableGpsPower() {
  if (!axpFound) return;
  
  uint8_t current = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, current)) {
    return;
  }
  
  // Set LDO3 bit (bit 3) to enable GPS power
  uint8_t updated = current | 0x08;  // LDO3 enable
  
  if (axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, updated)) {
    Serial.println("✓ GPS power enabled (AXP192 LDO3 ON)");
    delay(100); // Allow GPS to power up
  }
}

/**
 * @brief Disable GPS power via AXP192
 *
 * Turns off LDO3 which powers the GPS module
 * This actually powers down the GPS hardware, stopping the LED
 */
void disableGpsPower() {
  if (!axpFound) return;
  
  uint8_t current = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, current)) {
    return;
  }
  
  // Clear LDO3 bit (bit 3) to disable GPS power
  uint8_t updated = current & ~0x08;  // LDO3 disable
  
  if (axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, updated)) {
    Serial.println("✓ GPS power disabled (AXP192 LDO3 OFF) - LED should stop blinking");
  }
}

/**
 * @brief Enable OLED display
 */
void enableDisplay() {
  displayEnabled = true;
  display.ssd1306_command(0xAF); // Display ON
  display.clearDisplay();
  display.display();
  Serial.println("✓ Display enabled");
}

/**
 * @brief Disable OLED display
 */
void disableDisplay() {
  displayEnabled = false;
  display.clearDisplay();
  display.display();
  display.ssd1306_command(0xAE); // Display OFF
  Serial.println("✓ Display disabled (power saving)");
}

void os_getArtEui(u1_t *buf) {
  memcpy(buf, storedAPPEUI, 8);
}

void os_getDevEui(u1_t *buf) {
  memcpy(buf, storedDEVEUI, 8);
}

void os_getDevKey(u1_t *buf) {
  memcpy(buf, storedAPPKEY, 16);
}

void printLoRaStatus() {
  Serial.print("LMIC opmode=0x");
  Serial.print(LMIC.opmode, HEX);
  Serial.print(" joined=");
  Serial.print(loraJoined ? 1 : 0);
  Serial.print(" txrxFlags=0x");
  Serial.print(LMIC.txrxFlags, HEX);
  Serial.print(" seqnoUp=");
  Serial.print(LMIC.seqnoUp);
  Serial.print(" seqnoDn=");
  Serial.println(LMIC.seqnoDn);
}

// Sleep mode variables
bool sleepModeEnabled = false;
uint32_t sleepWakeInterval = 86400; // Default: 24 hours in seconds
unsigned long lastWakeTime = 0;
bool gpsEnabled = true;
uint32_t loraSendIntervalSec = 300; // Default: 5 minutes

/**
 * @brief Process LoRaWAN downlink commands
 *
 * Enhanced protocol with 0xAA prefix for structured commands.
 *
 * Protocol Format: [PREFIX] [CMD] [PARAMS...]
 *
 * Simple Commands (backward compatible):
 * - 0x01: WiFi ON
 * - 0x02: WiFi OFF
 * - 0x03: WiFi Reset
 * - 0x04: Device Restart
 * - 0x05: Debug ON
 * - 0x06: Debug OFF
 *
 * Enhanced Commands (with 0xAA prefix):
 * - AA 01 00: WiFi OFF
 * - AA 01 01: WiFi ON
 * - AA 02 00: GPS OFF
 * - AA 02 01: GPS ON
 * - AA 03 [HH] [HH] [HH] [HH]: Sleep mode with wake interval (4 bytes, seconds in hex)
 *   Example: AA 03 00 01 51 80 = wake every 24 hours (86400 seconds)
 * - AA 04 [HH] [HH] [HH] [HH]: Set LoRa send interval (4 bytes, seconds in hex)
 *   Example: AA 04 00 00 00 3C = send every 60 seconds
 * - AA 05 00: Sleep mode OFF
 * - AA 06 00: Debug OFF
 * - AA 06 01: Debug ON
 *
 * @param data Pointer to downlink payload
 * @param len Length of downlink payload
 */
void processDownlinkCommand(uint8_t* data, uint8_t len) {
  if (len < 1) {
    Serial.println("⚠ Downlink: Empty payload");
    return;
  }
  
  // Check for enhanced protocol (0xAA prefix)
  if (data[0] == 0xAA && len >= 3) {
    uint8_t cmd = data[1];
    uint8_t param = data[2];
    
    Serial.print("📥 Enhanced downlink: AA ");
    Serial.print(cmd, HEX);
    Serial.print(" ");
    Serial.println(param, HEX);
    
    switch (cmd) {
      case 0x01: // WiFi control
        if (param == 0x00) {
          Serial.println("  → WiFi OFF");
          wifiEnabled = false;
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          Serial.println("  ✓ WiFi disabled");
        } else if (param == 0x01) {
          Serial.println("  → WiFi ON");
          wifiEnabled = true;
          WiFi.mode(WIFI_STA);
          WiFi.begin();
          Serial.println("  ✓ WiFi enabled");
        }
        break;
        
      case 0x02: // GPS control
        if (param == 0x00) {
          Serial.println("  → GPS OFF");
          gpsEnabled = false;
          disableGpsPower();  // Actually power down GPS hardware
        } else if (param == 0x01) {
          Serial.println("  → GPS ON");
          gpsEnabled = true;
          enableGpsPower();  // Power up GPS hardware
        }
        break;
        
      case 0x03: // Sleep mode with wake interval
        if (len >= 7) {
          sleepWakeInterval = ((uint32_t)data[3] << 24) |
                             ((uint32_t)data[4] << 16) |
                             ((uint32_t)data[5] << 8) |
                             (uint32_t)data[6];
          sleepModeEnabled = true;
          lastWakeTime = millis();
          Serial.print("  → Sleep mode enabled, wake every ");
          Serial.print(sleepWakeInterval);
          Serial.println(" seconds");
          Serial.print("  ✓ Next wake in ");
          Serial.print(sleepWakeInterval / 3600);
          Serial.println(" hours");
        }
        break;
        
      case 0x04: // LoRa send interval
        if (len >= 7) {
          loraSendIntervalSec = ((uint32_t)data[3] << 24) |
                                ((uint32_t)data[4] << 16) |
                                ((uint32_t)data[5] << 8) |
                                (uint32_t)data[6];
          Serial.print("  → LoRa send interval set to ");
          Serial.print(loraSendIntervalSec);
          Serial.println(" seconds");
          Serial.print("  ✓ Sending every ");
          Serial.print(loraSendIntervalSec / 60);
          Serial.println(" minutes");
        }
        break;
        
      case 0x05: // Sleep mode control
        if (param == 0x00) {
          Serial.println("  → Sleep mode OFF");
          sleepModeEnabled = false;
          Serial.println("  ✓ Sleep mode disabled");
        }
        break;
        
      case 0x06: // Debug control
        if (param == 0x00) {
          Serial.println("  → Debug OFF");
          debugEnabled = false;
          Serial.println("  ✓ Debug disabled");
        } else if (param == 0x01) {
          Serial.println("  → Debug ON");
          debugEnabled = true;
          Serial.println("  ✓ Debug enabled");
        }
        break;
        
      case 0x07: // Display control
        if (param == 0x00) {
          Serial.println("  → Display OFF");
          disableDisplay();
        } else if (param == 0x01) {
          Serial.println("  → Display ON");
          enableDisplay();
        }
        break;
        
      default:
        Serial.print("  ✗ Unknown enhanced command: 0x");
        Serial.println(cmd, HEX);
        break;
    }
    return;
  }
  
  // Legacy simple commands (backward compatible)
  uint8_t cmd = data[0];
  Serial.print("📥 Simple downlink: 0x");
  Serial.println(cmd, HEX);
  
  switch (cmd) {
    case 0x01: // WiFi ON
      Serial.println("  → WiFi ON");
      wifiEnabled = true;
      WiFi.mode(WIFI_STA);
      WiFi.begin();
      Serial.println("  ✓ WiFi enabled");
      break;
      
    case 0x02: // WiFi OFF
      Serial.println("  → WiFi OFF");
      wifiEnabled = false;
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      Serial.println("  ✓ WiFi disabled");
      break;
      
    case 0x03: // WiFi Reset
      Serial.println("  → WiFi Reset");
      wifiManager.resetSettings();
      delay(1000);
      ESP.restart();
      break;
      
    case 0x04: // Device Restart
      Serial.println("  → Device Restart");
      delay(1000);
      ESP.restart();
      break;
      
    case 0x05: // Debug ON
      Serial.println("  → Debug ON");
      debugEnabled = true;
      Serial.println("  ✓ Debug enabled");
      break;
      
    case 0x06: // Debug OFF
      Serial.println("  → Debug OFF");
      debugEnabled = false;
      Serial.println("  ✓ Debug disabled");
      break;
      
    default:
      Serial.print("  ✗ Unknown command: 0x");
      Serial.println(cmd, HEX);
      break;
  }
}

void onEvent(ev_t ev) {
  if (debugEnabled) {
    Serial.print("[LMIC] event=");
    Serial.print((unsigned)ev);
    Serial.print(" at ms=");
    Serial.println(millis());
  }

  switch (ev) {
    case EV_JOINING:
      if (debugEnabled) {
        Serial.println("[LMIC] Joining... OTAA request sent");
      }
      break;
    case EV_JOINED:
      Serial.println("[LMIC] ✓ Joined network");
      logDebug("✓ LoRaWAN network joined successfully");
      loraJoined = true;
      LMIC_setLinkCheckMode(0);
      // Trigger first uplink immediately after successful join.
      lastLoraSendAt = millis() - LORA_SEND_INTERVAL_MS;
      if (debugEnabled) {
        printLoRaStatus();
      }
      break;
    case EV_JOIN_FAILED:
      Serial.println("[LMIC] ✗ Join failed; restarting OTAA join");
      logDebug("✗ LoRaWAN join failed - retrying");
      loraJoined = false;
      LMIC_reset();
    #if defined(CFG_us915)
      LMIC_selectSubBand(1);
    #endif
      LMIC_startJoining();
      break;
    case EV_REJOIN_FAILED:
      Serial.println("[LMIC] ✗ Rejoin failed");
      logDebug("✗ LoRaWAN rejoin failed");
      loraJoined = false;
      break;
    case EV_TXSTART:
      if (debugEnabled) {
        Serial.println("[LMIC] TX start");
      }
      break;
    case EV_TXCOMPLETE:
      if (debugEnabled) {
        Serial.println("[LMIC] TX complete");
      }
      if (LMIC.txrxFlags & TXRX_ACK) {
        if (debugEnabled) {
          Serial.println("[LMIC] ACK received");
        }
      }
      if (LMIC.dataLen > 0) {
        downlinkCounter++;  // Increment downlink counter
        String downlinkMsg = "📥 Downlink received #" + String(downlinkCounter) + ": " + String(LMIC.dataLen) + " bytes - Data: ";
        for (int i = 0; i < LMIC.dataLen; i++) {
          if (LMIC.frame[LMIC.dataBeg + i] < 0x10) downlinkMsg += "0";
          downlinkMsg += String(LMIC.frame[LMIC.dataBeg + i], HEX);
          downlinkMsg += " ";
        }
        Serial.println(downlinkMsg);
        logDebug(downlinkMsg);
        
        // Process downlink commands
        processDownlinkCommand(LMIC.frame + LMIC.dataBeg, LMIC.dataLen);
      }
      if (debugEnabled) {
        printLoRaStatus();
      }
      break;
    default:
      break;
  }
}

// Configure LMIC and start OTAA join sequence or set ABP session.
void setupLoRa() {
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_NSS_PIN);
  os_init();
  LMIC_reset();
  // Wider tolerance helps when RX windows are missed due oscillator drift.
  LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);

#if defined(CFG_us915)
  LMIC_selectSubBand(1);
  Serial.println("LoRa region US915, subband=1");
#endif

  if (useABP) {
    // ABP mode - set session keys directly
    Serial.println("LoRa: using ABP mode");
    
    // Set device address (big-endian)
    uint32_t devAddr = ((uint32_t)storedDEVADDR[0] << 24) |
                       ((uint32_t)storedDEVADDR[1] << 16) |
                       ((uint32_t)storedDEVADDR[2] << 8) |
                       (uint32_t)storedDEVADDR[3];
    
    // Set session keys and device address
    LMIC_setSession(0x1, devAddr, storedNWKSKEY, storedAPPSKEY);
    
    // Disable link check validation
    LMIC_setLinkCheckMode(0);
    
    // Set data rate and transmit power for US915
    LMIC_setDrTxpow(DR_SF7, 14);
    
    loraJoined = true;
    Serial.println("✓ ABP session configured");
    Serial.print("  DevAddr: 0x");
    Serial.println(devAddr, HEX);
  } else {
    // OTAA mode - start join procedure
    Serial.println("LoRa: starting OTAA join");
    LMIC_startJoining();
  }
}

// PAX Counter - Scan for WiFi and BLE devices
void performPaxScan() {
  if (!paxCounterEnabled) {
    return;
  }
  
  unsigned long now = millis();
  if (now - lastPaxScanAt < paxScanInterval) {
    return;
  }
  
  lastPaxScanAt = now;
  wifiDeviceCount = 0;
  bleDeviceCount = 0;
  
  // WiFi scan (only if enabled)
  if (paxWifiScanEnabled) {
    if (debugEnabled) {
      Serial.println("PAX: Starting WiFi scan...");
    }
    
    int n = WiFi.scanNetworks(false, false, false, 300);
    if (n > 0) {
      wifiDeviceCount = n;
      if (debugEnabled) {
        Serial.print("PAX: Found ");
        Serial.print(wifiDeviceCount);
        Serial.println(" WiFi devices");
      }
    }
  } else {
    if (debugEnabled) {
      Serial.println("PAX: WiFi scanning disabled");
    }
  }
  
  // BLE scan (only if enabled)
  if (paxBleScanEnabled) {
    if (debugEnabled) {
      Serial.println("PAX: Starting BLE scan...");
    }
    
    bleDevices.clear(); // Clear previous scan results
    
    if (pBLEScan != nullptr) {
      // Scan for 5 seconds, passive scan (no active connections)
      NimBLEScanResults results = pBLEScan->start(5, false);
      
      // Count unique devices
      for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice device = results.getDevice(i);
        std::string addr = device.getAddress().toString();
        bleDevices.insert(addr);
      }
      
      bleDeviceCount = bleDevices.size();
      pBLEScan->clearResults(); // Clear results to free memory
      
      if (debugEnabled) {
        Serial.print("PAX: Found ");
        Serial.print(bleDeviceCount);
        Serial.println(" BLE devices");
      }
    } else {
      bleDeviceCount = 0;
      if (debugEnabled) {
        Serial.println("PAX: BLE scanner not initialized");
      }
    }
  } else {
    if (debugEnabled) {
      Serial.println("PAX: BLE scanning disabled");
    }
  }
  
  totalPaxCount = wifiDeviceCount + bleDeviceCount;
  
  if (debugEnabled) {
    Serial.print("PAX: Total count = ");
    Serial.println(totalPaxCount);
  }
}


// Build and enqueue CayenneLPP uplink payload.
// Channel map:
// 1: Temperature (BME)
// 2: Relative humidity (BME)
// 3: Barometric pressure (BME)
// 4: Analog altitude from BME
// 5: GPS (lat/lon/alt)
// 6: Analog HDOP
// 7: Analog satellite count
// 8: Analog seconds-of-day (GPS time)
// 9: Analog battery voltage (AXP192)
// 10: Analog PAX counter (WiFi + BLE device count)
void queueCayenneUplink() {
  if (!loraJoined) {
    Serial.println("LoRa: not joined yet, uplink skipped");
    return;
  }

  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println("LoRa: TX/RX pending, uplink deferred");
    return;
  }

  lpp.reset();

  // Channel 1-4: BME280 data
  if (bmeFound) {
    if (channelEnabled[0]) lpp.addTemperature(1, lastTemperature);
    if (channelEnabled[1]) lpp.addRelativeHumidity(2, lastHumidity);
    if (channelEnabled[2]) lpp.addBarometricPressure(3, lastPressureHpa);
    if (channelEnabled[3]) lpp.addAnalogInput(4, lastAltitude);
  }

  // Channel 5: GPS location
  if (channelEnabled[4] && gps.location.isValid()) {
    float gpsAlt = gps.altitude.isValid() ? gps.altitude.meters() : 0.0f;
    lpp.addGPS(5, gps.location.lat(), gps.location.lng(), gpsAlt);
  }

  // Channel 6: GPS HDOP
  if (channelEnabled[5] && gps.hdop.isValid()) {
    lpp.addAnalogInput(6, gps.hdop.hdop());
  }

  // Channel 7: GPS satellite count
  if (channelEnabled[6] && gps.satellites.isValid()) {
    lpp.addAnalogInput(7, static_cast<float>(gps.satellites.value()));
  }

  // Channel 8: GPS time (seconds of day)
  if (channelEnabled[7] && gps.time.isValid()) {
    float secondsOfDay = static_cast<float>(gps.time.hour() * 3600 + gps.time.minute() * 60 + gps.time.second());
    lpp.addAnalogInput(8, secondsOfDay);
  }

  // Channel 9: Battery voltage
  if (channelEnabled[8] && axpFound) {
    float batteryVoltage = readBatteryVoltage();
    if (batteryVoltage > 0) {
      lpp.addAnalogInput(9, batteryVoltage);
    }
  }
  
  // Channel 10: PAX counter (optional)
  if (channelEnabled[9] && paxCounterEnabled) {
    lpp.addAnalogInput(10, static_cast<float>(totalPaxCount));
  }

  // Build log message
  String logMsg = "📡 LoRa uplink #" + String(++uplinkCounter) +
                  " bytes=" + String(lpp.getSize()) + " payload=";
  for (uint8_t i = 0; i < lpp.getSize(); i++) {
    if (lpp.getBuffer()[i] < 0x10) {
      logMsg += '0';
    }
    logMsg += String(lpp.getBuffer()[i], HEX);
  }
  
  // Log to both Serial and web debug buffer
  Serial.println(logMsg);
  logDebug(logMsg);

  LMIC_setTxData2(LORA_PORT, lpp.getBuffer(), lpp.getSize(), 0);
}

int8_t currentShiftX() {
  return SHIFT_X[burnInShiftIndex % (sizeof(SHIFT_X) / sizeof(SHIFT_X[0]))];
}

int8_t currentShiftY() {
  return SHIFT_Y[burnInShiftIndex % (sizeof(SHIFT_Y) / sizeof(SHIFT_Y[0]))];
}

void startGpsSerial(uint8_t profileIndex) {
  gpsProfileIndex = profileIndex % GPS_PROFILE_COUNT;
  GpsSerial.end();
  delay(20);
  GpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_PROFILES[gpsProfileIndex].rx, GPS_PROFILES[gpsProfileIndex].tx);
  gpsStreamSeen = false;

  Serial.print("GPS serial started: ");
  Serial.print(GPS_PROFILES[gpsProfileIndex].name);
  Serial.print(" @");
  Serial.print(GPS_BAUD);
  Serial.print(" RX=");
  Serial.print(GPS_PROFILES[gpsProfileIndex].rx);
  Serial.print(" TX=");
  Serial.println(GPS_PROFILES[gpsProfileIndex].tx);

  reenableUbxNmeaOutput();
}

void printTwoDigits(int value) {
  if (value < 10) {
    Serial.print('0');
  }
  Serial.print(value);
}

void printGpsTimeToSerial() {
  if (gps.time.isValid() && gps.date.isValid()) {
    Serial.print(gps.date.year());
    Serial.print('-');
    printTwoDigits(gps.date.month());
    Serial.print('-');
    printTwoDigits(gps.date.day());
    Serial.print(' ');
    printTwoDigits(gps.time.hour());
    Serial.print(':');
    printTwoDigits(gps.time.minute());
    Serial.print(':');
    printTwoDigits(gps.time.second());
    Serial.print('.');
    Serial.print(gps.time.centisecond());
    Serial.print(" UTC");
  } else {
    Serial.print("invalid");
  }
}

void drawHeader(const char *title) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(currentShiftX(), currentShiftY());
  display.println(title);
}

// Environment page prioritizes large temperature/humidity for quick readability
// on aging OLED panels where fine details may be harder to distinguish.
void drawPageEnvironment() {
  drawHeader("ENV BME280");
  display.setTextSize(2);
  display.print("T: ");
  display.print(lastTemperature, 1);
  display.println(" C");
  display.print("H: ");
  display.print(lastHumidity, 1);
  display.println(" %");
  display.setTextSize(1);
  display.print("P: ");
  display.print(lastPressureHpa, 1);
  display.println(" hPa");
  
  // Battery voltage
  if (axpFound) {
    float batteryVoltage = readBatteryVoltage();
    bool charging = isBatteryCharging();
    display.print("Bat: ");
    display.print(batteryVoltage, 2);
    display.print("V ");
    display.println(charging ? "CHG" : "");
  }
}

// GPS status page: large summary metrics first, coordinates below.
void drawPageGpsFix() {
  drawHeader("GPS FIX");
  display.setTextSize(2);
  display.print("S:");
  display.print(gps.satellites.isValid() ? String(gps.satellites.value()) : "-");
  display.print(" H:");
  display.println(gps.hdop.isValid() ? String(gps.hdop.hdop(), 1) : "-");
  display.setTextSize(1);
  display.print("Lat: ");
  display.println(gps.location.isValid() ? String(gps.location.lat(), 5) : "-");
  display.print("Lon: ");
  display.println(gps.location.isValid() ? String(gps.location.lng(), 5) : "-");
}

// Clock page uses the ESP32 system clock, which is in turn synchronized from
// GPS when plausible date/time and fix quality are available.
void drawPageGpsTime() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(currentShiftX(), currentShiftY());
  display.setTextSize(1);
  display.print("CLOCK UTC ");
  display.println(clockSynced ? "SYNC" : "WAIT");

  if (hasSystemTime()) {
    time_t now = time(nullptr);
    struct tm utc = {};
    gmtime_r(&now, &utc);

    display.setTextSize(2);
    char hhmm[6];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", utc.tm_hour, utc.tm_min);
    display.println(hhmm);

    display.setTextSize(1);
    char dateLine[24];
    snprintf(dateLine, sizeof(dateLine), "%04d-%02d-%02d  %02d",
             utc.tm_year + 1900,
             utc.tm_mon + 1,
             utc.tm_mday,
             utc.tm_sec);
    display.println(dateLine);
  } else {
    display.setTextSize(2);
    display.println("--:--");
    display.setTextSize(1);
    display.println("Waiting GPS time");
  }

  display.print("RTC chip: ");
  display.println(rtcChipFound ? "FOUND" : "NO");
}

// LoRa page shows current join state and lightweight transmission counters.
void drawPageLoRa() {
  drawHeader("LORA US915");
  display.setTextSize(2);
  display.print("J:");
  display.println(loraJoined ? "Y" : "N");
  display.setTextSize(1);
  display.print("Uplink: ");
  display.println(uplinkCounter);
  display.print("TX pend: ");
  display.println((LMIC.opmode & OP_TXRXPEND) ? "YES" : "NO");
  char devTail[5];
  snprintf(devTail, sizeof(devTail), "%02X%02X", DEVEUI[1], DEVEUI[0]);
  display.print("DevEUI: ");
  display.println(devTail);
}

// Detailed GPS diagnostics for field troubleshooting.
void logGpsDetailed() {
  Serial.println("---------------- GPS DETAIL ----------------");
  Serial.print("charsProcessed=");
  Serial.print(gps.charsProcessed());
  Serial.print(" sentencesWithFix=");
  Serial.print(gps.sentencesWithFix());
  Serial.print(" passedChecksum=");
  Serial.print(gps.passedChecksum());
  Serial.print(" failedChecksum=");
  Serial.println(gps.failedChecksum());

  Serial.print("location.valid=");
  Serial.print(gps.location.isValid());
  Serial.print(" ageMs=");
  Serial.print(gps.location.age());
  Serial.print(" lat=");
  Serial.print(gps.location.isValid() ? gps.location.lat() : 0.0, 6);
  Serial.print(" lon=");
  Serial.println(gps.location.isValid() ? gps.location.lng() : 0.0, 6);

  Serial.print("altitude.valid=");
  Serial.print(gps.altitude.isValid());
  Serial.print(" meters=");
  Serial.print(gps.altitude.isValid() ? gps.altitude.meters() : 0.0, 2);
  Serial.print(" course.valid=");
  Serial.print(gps.course.isValid());
  Serial.print(" deg=");
  Serial.print(gps.course.isValid() ? gps.course.deg() : 0.0, 2);
  Serial.print(" speed.valid=");
  Serial.print(gps.speed.isValid());
  Serial.print(" kmph=");
  Serial.println(gps.speed.isValid() ? gps.speed.kmph() : 0.0, 2);

  Serial.print("satellites.valid=");
  Serial.print(gps.satellites.isValid());
  Serial.print(" count=");
  Serial.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
  Serial.print(" hdop.valid=");
  Serial.print(gps.hdop.isValid());
  Serial.print(" hdop=");
  Serial.println(gps.hdop.isValid() ? gps.hdop.hdop() : 0.0, 2);

  Serial.print("time/date=");
  printGpsTimeToSerial();
  Serial.println();

  if (gps.charsProcessed() > 100 && gps.satellites.isValid() && gps.satellites.value() == 0) {
    Serial.println("GPS status: NMEA present but satellites=0 (no sky lock yet)");
    Serial.println("Hint: place outdoors, clear sky, verify antenna connector, wait for cold start");
  }

  if (gps.hdop.isValid() && gps.hdop.hdop() >= 99.0f) {
    Serial.println("GPS status: HDOP is very high (99.99), still searching for a valid fix");
  }

  Serial.println("--------------------------------------------");
}

bool initBme() {
  // Most BME280 boards use either 0x76 or 0x77.
  if (bme.begin(0x76)) {
    Serial.println("BME280 found at 0x76");
    return true;
  }
  if (bme.begin(0x77)) {
    Serial.println("BME280 found at 0x77");
    return true;
  }
  return false;
}

void showBootScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("TTGO T-Beam start");
  display.println("OLED: OK");
  display.print("BME280: ");
  display.println(bmeFound ? "OK" : "NOT FOUND");
  display.print("GPS UART: ");
  display.print(GPS_BAUD);
  display.println(" bps");
  display.display();
}

void updateSensorReadings() {
  if (!bmeFound) {
    return;
  }

  bool valid = false;
  for (uint8_t i = 0; i < BME_READ_RETRIES; i++) {
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0f;
    float a = bme.readAltitude(seaLevelPressureHpa);

    if (isnan(t) || isnan(h) || isnan(p) || isnan(a)) {
      delay(20);
      continue;
    }

    if (t < BME_MIN_TEMP_C || t > BME_MAX_TEMP_C ||
        h < BME_MIN_HUMIDITY || h > BME_MAX_HUMIDITY ||
        p < BME_MIN_PRESSURE_HPA || p > BME_MAX_PRESSURE_HPA) {
      delay(20);
      continue;
    }

    lastTemperature = t;
    lastHumidity = h;
    lastPressureHpa = p;
    lastAltitude = a;
    valid = true;
    break;
  }

  if (!valid) {
    if (debugEnabled) {
      Serial.println("BME read invalid after retries, keeping previous values");
    }
    return;
  }

  if (debugEnabled) {
    Serial.print("ENV tempC=");
    Serial.print(lastTemperature, 2);
    Serial.print(" humidityPct=");
    Serial.print(lastHumidity, 2);
    Serial.print(" pressureHpa=");
    Serial.print(lastPressureHpa, 2);
    Serial.print(" altitudeM=");
    Serial.println(lastAltitude, 2);
  }
}

// Rotating page renderer with burn-in protection handled elsewhere.
void renderDisplayPage() {
  if (displaySleeping) {
    return;
  }

  if (!bmeFound) {
    drawHeader("BME280 missing");
    display.println("Check wiring");
    display.println("Addr 0x76/0x77");
    display.display();
    return;
  }

  if (displayPage == 0) {
    drawPageEnvironment();
  } else if (displayPage == 1) {
    drawPageGpsFix();
  } else if (displayPage == 2) {
    drawPageGpsTime();
  } else {
    drawPageLoRa();
  }

  display.display();
}

// =============================================================================
// LoRaWAN Key Management Functions
// =============================================================================
// Author: Markus van Kempen | markus.van.kempen@gmail.com
//
// Manages LoRaWAN OTAA keys (APPEUI, DEVEUI, APPKEY) with:
// - NVS (Non-Volatile Storage) persistence
// - Serial command configuration
// - Web UI configuration
// - Hex string parsing and validation
// =============================================================================

/**
 * @brief Load LoRaWAN keys from NVS or use defaults
 *
 * Attempts to load keys from ESP32 NVS. If not found, uses defaults
 * from lorawan_secrets.h and marks as unconfigured.
 *
 * @note Keys are stored in NVS namespace "lora"
 * @note APPEUI and DEVEUI are 8 bytes, APPKEY is 16 bytes
 */
void loadLoRaKeysFromNVS() {
  preferences.begin("lora", true); // Read-only
  
  // Load activation mode (default to OTAA)
  useABP = preferences.getBool("useABP", false);
  
  if (useABP) {
    // Load ABP keys
    size_t len = preferences.getBytes("devaddr", storedDEVADDR, 4);
    if (len == 4) {
      preferences.getBytes("nwkskey", storedNWKSKEY, 16);
      preferences.getBytes("appskey", storedAPPSKEY, 16);
      loraKeysConfigured = true;
      Serial.println("LoRa ABP keys loaded from NVS");
      Serial.print("  Mode: ABP | DevAddr: ");
      for (int i = 0; i < 4; i++) {
        if (storedDEVADDR[i] < 0x10) Serial.print("0");
        Serial.print(storedDEVADDR[i], HEX);
      }
      Serial.println();
    } else {
      // No ABP keys stored, use defaults
      loraKeysConfigured = false;
      Serial.println("LoRa ABP keys not found in NVS");
    }
  } else {
    // Load OTAA keys
    size_t len = preferences.getBytes("appeui", storedAPPEUI, 8);
    if (len == 8) {
      preferences.getBytes("deveui", storedDEVEUI, 8);
      preferences.getBytes("appkey", storedAPPKEY, 16);
      loraKeysConfigured = true;
      Serial.println("LoRa OTAA keys loaded from NVS");
    } else {
      // Use defaults from lorawan_secrets.h
      memcpy(storedAPPEUI, APPEUI, 8);
      memcpy(storedDEVEUI, DEVEUI, 8);
      memcpy(storedAPPKEY, APPKEY, 16);
      loraKeysConfigured = false;
      Serial.println("LoRa keys using defaults from lorawan_secrets.h");
    }
  }
  
  preferences.end();
}

/**
 * @brief Save LoRaWAN keys to NVS
 *
 * Stores keys persistently in ESP32 NVS for use across reboots.
 *
 * @return true if save successful, false otherwise
 */
bool saveLoRaKeysToNVS() {
  preferences.begin("lora", false); // Read-write
  
  // Save mode
  preferences.putBool("useABP", useABP);
  
  if (useABP) {
    // Save ABP keys
    preferences.putBytes("devaddr", storedDEVADDR, 4);
    preferences.putBytes("nwkskey", storedNWKSKEY, 16);
    preferences.putBytes("appskey", storedAPPSKEY, 16);
    Serial.println("LoRa ABP keys saved to NVS");
  } else {
    // Save OTAA keys
    preferences.putBytes("appeui", storedAPPEUI, 8);
    preferences.putBytes("deveui", storedDEVEUI, 8);
    preferences.putBytes("appkey", storedAPPKEY, 16);
    Serial.println("LoRa OTAA keys saved to NVS");
  }
  
  preferences.end();
  loraKeysConfigured = true;
  
  return true;
}

/**
 * @brief Clear LoRaWAN keys from NVS
 *
 * Removes stored keys and reverts to defaults from lorawan_secrets.h
 */
void clearLoRaKeysFromNVS() {
  preferences.begin("lora", false);
  preferences.clear();
  preferences.end();
  
  // Reload defaults
  memcpy(storedAPPEUI, APPEUI, 8);
  memcpy(storedDEVEUI, DEVEUI, 8);
  memcpy(storedAPPKEY, APPKEY, 16);
  loraKeysConfigured = false;
  
  Serial.println("LoRa keys cleared from NVS, using defaults");
}

/**
 * @brief Convert hex string to byte array
 *
 * Parses hex string (with or without spaces/colons) into byte array.
 * Supports formats: "0102030405060708" or "01:02:03:04:05:06:07:08"
 *
 * @param hexStr Input hex string
 * @param bytes Output byte array
 * @param expectedLen Expected number of bytes
 * @return true if parsing successful, false otherwise
 */
bool hexStringToBytes(const String& hexStr, uint8_t* bytes, size_t expectedLen) {
  String cleaned = hexStr;
  cleaned.replace(" ", "");
  cleaned.replace(":", "");
  cleaned.replace("-", "");
  cleaned.toUpperCase();
  
  if (cleaned.length() != expectedLen * 2) {
    return false;
  }
  
  for (size_t i = 0; i < expectedLen; i++) {
    String byteStr = cleaned.substring(i * 2, i * 2 + 2);
    char* endPtr;
    long val = strtol(byteStr.c_str(), &endPtr, 16);
    if (*endPtr != '\0') {
      return false;
    }
    bytes[i] = (uint8_t)val;
  }
  
  return true;
}

/**
 * @brief Convert byte array to hex string
 *
 * @param bytes Input byte array
 * @param len Length of array
 * @param separator Optional separator between bytes
 * @return Hex string representation
 */
String bytesToHexString(const uint8_t* bytes, size_t len, const char* separator = "") {
  String result = "";
  for (size_t i = 0; i < len; i++) {
    if (i > 0 && separator[0] != '\0') {
      result += separator;
    }
    if (bytes[i] < 0x10) result += "0";
    result += String(bytes[i], HEX);
  }
  result.toUpperCase();
  return result;
}

/**
 * @brief Print current LoRaWAN keys to serial
 */
void printLoRaKeys() {
  Serial.println("=== LoRaWAN Configuration ===");
  Serial.print("Mode: ");
  Serial.println(useABP ? "ABP" : "OTAA");
  
  if (useABP) {
    // Print ABP keys
    Serial.print("DevAddr: ");
    Serial.println(bytesToHexString(storedDEVADDR, 4, ":"));
    Serial.print("NwkSKey: ");
    Serial.println(bytesToHexString(storedNWKSKEY, 16, ":"));
    Serial.print("AppSKey: ");
    Serial.println(bytesToHexString(storedAPPSKEY, 16, ":"));
  } else {
    // Print OTAA keys
    Serial.print("APPEUI: ");
    Serial.println(bytesToHexString(storedAPPEUI, 8, ":"));
    Serial.print("DEVEUI: ");
    Serial.println(bytesToHexString(storedDEVEUI, 8, ":"));
    Serial.print("APPKEY: ");
    Serial.println(bytesToHexString(storedAPPKEY, 16, ":"));
  }
  
  Serial.print("Configured: ");
  Serial.println(loraKeysConfigured ? "YES (from NVS)" : "NO (using defaults)");
  Serial.println("=============================");
}

// =============================================================================
// System Settings Persistence (NVS)
// =============================================================================
// Author: Markus van Kempen | markus.van.kempen@gmail.com
//
// Manages persistent storage of system settings in NVS:
// - Debug mode enable/disable
// - WiFi enable/disable
// - GPS enable/disable
// - Display enable/disable
// - Sleep mode settings
// - LoRa send interval
// =============================================================================

/**
 * @brief Load system settings from NVS
 *
 * Loads all system configuration from Non-Volatile Storage.
 * If settings don't exist, uses default values.
 */
void loadSystemSettings() {
  preferences.begin("system", true); // Read-only
  
  debugEnabled = preferences.getBool("debugEnabled", false);
  wifiEnabled = preferences.getBool("wifiEnabled", true);
  gpsEnabled = preferences.getBool("gpsEnabled", true);
  displayEnabled = preferences.getBool("displayEnabled", true);
  sleepModeEnabled = preferences.getBool("sleepEnabled", false);
  sleepWakeInterval = preferences.getUInt("sleepInterval", 86400);
  loraSendIntervalSec = preferences.getUInt("loraInterval", 300);
  
  // Load PAX counter settings
  paxCounterEnabled = preferences.getBool("paxEnabled", false);
  paxWifiScanEnabled = preferences.getBool("paxWifiScan", true);
  paxBleScanEnabled = preferences.getBool("paxBleScan", true);
  paxScanInterval = preferences.getUInt("paxInterval", 60) * 1000; // Convert to ms
  
  // Load last known good GPS position
  lastGoodLat = preferences.getDouble("gpsLat", 0.0);
  lastGoodLon = preferences.getDouble("gpsLon", 0.0);
  lastGoodAlt = preferences.getDouble("gpsAlt", 0.0);
  lastGoodSats = preferences.getUInt("gpsSats", 0);
  lastGoodHdop = preferences.getDouble("gpsHdop", 99.99);
  lastGoodTimestamp = preferences.getUInt("gpsTime", 0);
  
  preferences.end();
  
  Serial.println("=== System Settings Loaded ===");
  Serial.print("Debug: "); Serial.println(debugEnabled ? "ON" : "OFF");
  Serial.print("WiFi: "); Serial.println(wifiEnabled ? "ON" : "OFF");
  Serial.print("GPS: "); Serial.println(gpsEnabled ? "ON" : "OFF");
  Serial.print("Display: "); Serial.println(displayEnabled ? "ON" : "OFF");
  Serial.print("Sleep: "); Serial.println(sleepModeEnabled ? "ON" : "OFF");
  if (sleepModeEnabled) {
    Serial.print("  Sleep Interval: "); Serial.print(sleepWakeInterval); Serial.println(" sec");
  }
  Serial.print("LoRa Interval: "); Serial.print(loraSendIntervalSec); Serial.println(" sec");
  Serial.print("PAX Counter: "); Serial.println(paxCounterEnabled ? "ON" : "OFF");
  if (paxCounterEnabled) {
    Serial.print("  WiFi Scan: "); Serial.println(paxWifiScanEnabled ? "ON" : "OFF");
    Serial.print("  BLE Scan: "); Serial.println(paxBleScanEnabled ? "ON" : "OFF");
    Serial.print("  Interval: "); Serial.print(paxScanInterval / 1000); Serial.println(" sec");
  }
  if (lastGoodTimestamp > 0) {
    Serial.print("Last GPS Fix: ");
    Serial.print(lastGoodLat, 6);
    Serial.print(", ");
    Serial.print(lastGoodLon, 6);
    Serial.print(" (");
    Serial.print((millis() - lastGoodTimestamp) / 1000);
    Serial.println(" sec ago)");
  }
  Serial.println("==============================");
}

/**
 * @brief Save system settings to NVS
 *
 * Persists all system configuration to Non-Volatile Storage.
 * Settings survive reboots and power cycles.
 */
void saveSystemSettings() {
  preferences.begin("system", false); // Read-write
  
  preferences.putBool("debugEnabled", debugEnabled);
  preferences.putBool("wifiEnabled", wifiEnabled);
  preferences.putBool("gpsEnabled", gpsEnabled);
  preferences.putBool("displayEnabled", displayEnabled);
  preferences.putBool("sleepEnabled", sleepModeEnabled);
  preferences.putUInt("sleepInterval", sleepWakeInterval);
  preferences.putUInt("loraInterval", loraSendIntervalSec);
  
  // Save PAX counter settings
  preferences.putBool("paxEnabled", paxCounterEnabled);
  preferences.putBool("paxWifiScan", paxWifiScanEnabled);
  preferences.putBool("paxBleScan", paxBleScanEnabled);
  preferences.putUInt("paxInterval", paxScanInterval / 1000); // Convert to seconds
  
  // Save last known good GPS position
  preferences.putDouble("gpsLat", lastGoodLat);
  preferences.putDouble("gpsLon", lastGoodLon);
  preferences.putDouble("gpsAlt", lastGoodAlt);
  preferences.putUInt("gpsSats", lastGoodSats);
  preferences.putDouble("gpsHdop", lastGoodHdop);
  preferences.putUInt("gpsTime", lastGoodTimestamp);
  
  preferences.end();
  
  Serial.println("✓ System settings saved to NVS");
}

// =============================================================================
// WiFi and Web Server Functions
// =============================================================================
// Author: Markus van Kempen | markus.van.kempen@gmail.com
//
// This section implements the WiFi web dashboard functionality including:
// - JSON API endpoint for sensor data
// - HTML web interface with real-time updates
// - Google Maps integration for GPS tracking
// - WiFi Manager for easy configuration
// - Serial command interface
// =============================================================================

/**
 * @brief Generate JSON string with all sensor and system data
 *
 * Creates a JSON document containing:
 * - BME280 environmental data (temperature, humidity, pressure, altitude)
 * - GPS data (position, satellites, HDOP, speed)
 * - System information (uptime, memory, LoRa status)
 *
 * @return String JSON-formatted sensor data
 *
 * @note This function is called by the /api/data endpoint
 * @note Data is updated in real-time from global sensor variables
 *
 * Example output:
 * {
 *   "temperature": 23.5,
 *   "humidity": 45.2,
 *   "pressure": 1013.25,
 *   "altitude": 120.5,
 *   "gps": {
 *     "valid": true,
 *     "latitude": 43.6532,
 *     "longitude": -79.3832,
 *     "altitude": 125.3,
 *     "satellites": 8,
 *     "hdop": 1.2,
 *     "speed": 0.0
 *   },
 *   "uptime": 3600,
 *   "freeHeap": 250000,
 *   "loraJoined": true,
 *   "loraUplinks": 42
 * }
 */
String getJsonSensorData() {
  JsonDocument doc;
  
  // BME280 data
  doc["temperature"] = lastTemperature;
  doc["humidity"] = lastHumidity;
  doc["pressure"] = lastPressureHpa;
  doc["altitude"] = lastAltitude;
  
  // GPS data - use current fix if available, otherwise use last known position
  doc["gps"]["valid"] = gps.location.isValid();
  if (gps.location.isValid()) {
    doc["gps"]["latitude"] = gps.location.lat();
    doc["gps"]["longitude"] = gps.location.lng();
    doc["gps"]["altitude"] = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    doc["gps"]["satellites"] = gps.satellites.isValid() ? gps.satellites.value() : 0;
    doc["gps"]["hdop"] = gps.hdop.isValid() ? gps.hdop.hdop() : 99.99;
    doc["gps"]["lastUpdate"] = "current";
  } else if (lastGoodTimestamp > 0) {
    // Use last known good position
    doc["gps"]["latitude"] = lastGoodLat;
    doc["gps"]["longitude"] = lastGoodLon;
    doc["gps"]["altitude"] = lastGoodAlt;
    doc["gps"]["satellites"] = lastGoodSats;
    doc["gps"]["hdop"] = lastGoodHdop;
    doc["gps"]["lastUpdate"] = String((millis() - lastGoodTimestamp) / 1000) + "s ago";
  } else {
    doc["gps"]["latitude"] = 0.0;
    doc["gps"]["longitude"] = 0.0;
    doc["gps"]["altitude"] = 0.0;
    doc["gps"]["satellites"] = 0;
    doc["gps"]["hdop"] = 99.99;
    doc["gps"]["lastUpdate"] = "never";
  }
  doc["gps"]["speed"] = gps.speed.isValid() ? gps.speed.kmph() : 0.0;
  
  // System info
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["loraJoined"] = loraJoined;
  doc["loraUplinks"] = uplinkCounter;
  
  // Battery info
  if (axpFound) {
    float batteryVoltage = readBatteryVoltage();
    bool charging = isBatteryCharging();
    doc["battery"]["voltage"] = batteryVoltage;
    doc["battery"]["charging"] = charging;
  } else {
    doc["battery"]["voltage"] = -1.0;
    doc["battery"]["charging"] = false;
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}

/**
 * @brief Generate complete HTML page for web dashboard
 *
 * Creates a responsive, modern web interface featuring:
 * - Real-time sensor data cards (temperature, humidity, pressure, altitude)
 * - GPS status and satellite information
 * - Interactive Google Maps with live GPS marker
 * - Auto-refresh every 1 second via JavaScript
 * - Responsive design for mobile/tablet/desktop
 * - Beautiful gradient styling
 *
 * @return String Complete HTML page as string
 *
 * @note Uses raw string literal for multi-line HTML
 * @note Includes embedded CSS and JavaScript
 * @note Google Maps API key should be replaced with actual key
 * @note Map will work without API key but shows watermark
 *
 * Features:
 * - Gradient header with project title
 * - Grid layout for sensor cards
 * - Color-coded status indicators (online/offline)
 * - Hybrid map view (satellite + roads)
 * - Auto-centering on GPS position
 * - Uptime and last update timestamp
 * - Hover effects on cards
 * - Mobile-responsive breakpoints
 */
String getHtmlPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TTGO T-Beam Sensor Dashboard</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: #333;
      padding: 0;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 2px 10px rgba(0,0,0,0.2);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .page-content {
      padding: 20px;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    .header h1 { font-size: 2em; margin-bottom: 10px; }
    .header p { opacity: 0.9; }
    .content { padding: 30px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      margin-bottom: 30px;
    }
    .card {
      background: #f8f9fa;
      border-radius: 10px;
      padding: 20px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
      transition: transform 0.2s;
    }
    .card:hover { transform: translateY(-5px); }
    .card-title {
      font-size: 0.9em;
      color: #666;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 10px;
    }
    .card-value {
      font-size: 2em;
      font-weight: bold;
      color: #667eea;
    }
    .card-unit {
      font-size: 0.8em;
      color: #999;
      margin-left: 5px;
    }
    #map {
      width: 100%;
      height: 400px;
      border-radius: 10px;
      margin-top: 20px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    .status {
      display: inline-block;
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 0.9em;
      font-weight: bold;
    }
    .status.online { background: #4caf50; color: white; }
    .status.offline { background: #f44336; color: white; }
    .footer {
      text-align: center;
      padding: 20px;
      color: #999;
      font-size: 0.9em;
    }
    @media (max-width: 768px) {
      .grid { grid-template-columns: 1fr; }
      .header h1 { font-size: 1.5em; }
    }
  </style>
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v2.6.0</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link active">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>
  
  <div class="page-content">
    <div class="container">
      <div class="header">
        <h1>🛰️ TTGO T-Beam Dashboard</h1>
        <p>Real-time Sensor & GPS Monitoring</p>
      </div>
      
      <div class="content">
      <!-- GPS Location Card (Full Width) -->
      <div style="background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border-radius: 10px; padding: 20px; margin-bottom: 20px; color: white; box-shadow: 0 4px 15px rgba(0,0,0,0.2);">
        <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 15px;">
          <div style="flex: 1; min-width: 200px;">
            <div style="font-size: 0.9em; opacity: 0.9; margin-bottom: 5px;">📍 GPS LOCATION</div>
            <div style="font-size: 1.3em; font-weight: bold;" id="gpsCoords">Waiting for fix...</div>
            <div style="font-size: 0.9em; opacity: 0.8; margin-top: 5px;" id="gpsDetails">--</div>
          </div>
          <div style="text-align: right;">
            <div id="gpsStatusBadge" class="status offline">NO FIX</div>
            <div style="font-size: 0.9em; opacity: 0.9; margin-top: 10px;">
              🛰️ <span id="satellitesCount">0</span> satellites
            </div>
          </div>
        </div>
      </div>
      
      <!-- Sensor Data Grid -->
      <div class="grid">
        <div class="card">
          <div class="card-title">🌡️ Temperature</div>
          <div class="card-value" id="temp">--</div>
          <span class="card-unit">°C</span>
        </div>
        <div class="card">
          <div class="card-title">💧 Humidity</div>
          <div class="card-value" id="humidity">--</div>
          <span class="card-unit">%</span>
        </div>
        <div class="card">
          <div class="card-title">🌪️ Pressure</div>
          <div class="card-value" id="pressure">--</div>
          <span class="card-unit">hPa</span>
        </div>
        <div class="card">
          <div class="card-title">⛰️ Altitude</div>
          <div class="card-value" id="altitude">--</div>
          <span class="card-unit">m</span>
        </div>
        <div class="card">
          <div class="card-title">🚀 Speed</div>
          <div class="card-value" id="speed">--</div>
          <span class="card-unit">km/h</span>
        </div>
        <div class="card">
          <div class="card-title">📡 LoRa Uplinks</div>
          <div class="card-value" id="uplinks">--</div>
        </div>
        <div class="card">
          <div class="card-title">🔋 Battery</div>
          <div class="card-value" id="battery">--</div>
          <span class="card-unit">V</span>
          <div style="font-size: 0.8em; color: #999; margin-top: 5px;" id="batteryStatus">--</div>
        </div>
      </div>
      
      <div id="map"></div>
    </div>
    
    <div class="footer">
      <p>Last Update: <span id="lastUpdate">--</span></p>
      <p>Uptime: <span id="uptime">--</span></p>
    </div>
  </div>

  <script>
    let map, marker;
    
    function initMap() {
      // Initialize Leaflet map with OpenStreetMap tiles
      map = L.map('map').setView([0, 0], 2);
      
      // Add OpenStreetMap tile layer
      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
        maxZoom: 19
      }).addTo(map);
      
      // Create marker (initially hidden)
      marker = L.marker([0, 0]).addTo(map);
      marker.bindPopup('TTGO T-Beam Location');
    }
    
    function updateData() {
      fetch('/api/data')
        .then(response => response.json())
        .then(data => {
          // Update sensor cards
          document.getElementById('temp').textContent = data.temperature.toFixed(1);
          document.getElementById('humidity').textContent = data.humidity.toFixed(1);
          document.getElementById('pressure').textContent = data.pressure.toFixed(1);
          document.getElementById('altitude').textContent = data.altitude.toFixed(1);
          document.getElementById('speed').textContent = data.gps.speed.toFixed(1);
          document.getElementById('uplinks').textContent = data.loraUplinks;
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
          document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
          
          // Update battery information
          if (data.battery && data.battery.voltage > 0) {
            document.getElementById('battery').textContent = data.battery.voltage.toFixed(2);
            document.getElementById('batteryStatus').textContent = data.battery.charging ? '⚡ Charging' : '🔌 Discharging';
            document.getElementById('batteryStatus').style.color = data.battery.charging ? '#4CAF50' : '#FF9800';
          } else {
            document.getElementById('battery').textContent = '--';
            document.getElementById('batteryStatus').textContent = 'No battery';
          }
          
          // Update GPS information
          document.getElementById('satellitesCount').textContent = data.gps.satellites;
          
          if (data.gps.valid) {
            // Update GPS coordinates display
            const lat = data.gps.latitude.toFixed(6);
            const lon = data.gps.longitude.toFixed(6);
            document.getElementById('gpsCoords').textContent = `${lat}, ${lon}`;
            document.getElementById('gpsDetails').textContent =
              `Alt: ${data.gps.altitude.toFixed(1)}m | HDOP: ${data.gps.hdop.toFixed(2)} | Speed: ${data.gps.speed.toFixed(1)} km/h`;
            document.getElementById('gpsStatusBadge').innerHTML = '<span class="status online">GPS LOCKED</span>';
            
            // Update map
            const latLng = [data.gps.latitude, data.gps.longitude];
            marker.setLatLng(latLng);
            map.setView(latLng, 15);
            marker.setPopupContent(`
              <b>TTGO T-Beam Location</b><br>
              Lat: ${lat}<br>
              Lon: ${lon}<br>
              Alt: ${data.gps.altitude.toFixed(1)}m<br>
              Satellites: ${data.gps.satellites}
            `);
            marker.openPopup();
          } else {
            document.getElementById('gpsCoords').textContent = 'Waiting for GPS fix...';
            document.getElementById('gpsDetails').textContent = 'Move to open area with clear sky view';
            document.getElementById('gpsStatusBadge').innerHTML = '<span class="status offline">NO FIX</span>';
          }
        })
        .catch(err => console.error('Error fetching data:', err));
    }
    
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const mins = Math.floor((seconds % 3600) / 60);
      return `${days}d ${hours}h ${mins}m`;
    }
    
    initMap();
    updateData();
    setInterval(updateData, 1000);
  </script>
    </div>
  </div>
</body>
</html>
)rawliteral";
  return html;
}

/**
 * @brief Initialize and configure the async web server
 *
 * Sets up HTTP endpoints for:
 * - Main dashboard page (/)
 * - JSON API endpoint (/api/data)
 * - WiFi reset endpoint (/reset-wifi)
 *
 * @note Uses ESPAsyncWebServer for non-blocking operation
 * @note Server runs on port 80 (standard HTTP)
 * @note Does not interfere with LoRaWAN or GPS timing
 *
 * Endpoints:
 * - GET /           : Serves HTML dashboard
 * - GET /api/data   : Returns JSON sensor data
 * - GET /reset-wifi : Resets WiFi credentials and restarts
 *
 * @see getHtmlPage() for dashboard HTML
 * @see getJsonSensorData() for API response format
 */
void setupWebServer() {
  // Serve main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getHtmlPage());
  });
  
  // API endpoint for JSON data
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", getJsonSensorData());
  });
  
  // WiFi reset endpoint
  server.on("/reset-wifi", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "WiFi settings will be reset. Device will restart.");
    delay(1000);
    wifiManager.resetSettings();
    ESP.restart();
  });
  
  // LoRa configuration page
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LoRaWAN Configuration</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 0;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 2px 10px rgba(0,0,0,0.2);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .page-content {
      padding: 20px;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    .content { padding: 30px; }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      font-weight: bold;
      margin-bottom: 5px;
      color: #333;
    }
    input[type="text"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
      font-family: monospace;
    }
    input[type="text"]:focus {
      outline: none;
      border-color: #667eea;
    }
    .btn {
      padding: 12px 30px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      margin-right: 10px;
      margin-top: 10px;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-danger {
      background: #f44336;
      color: white;
    }
    .btn-danger:hover {
      background: #da190b;
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .btn-secondary:hover {
      background: #545b62;
    }
    .current-keys {
      background: #f8f9fa;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      font-family: monospace;
      font-size: 12px;
    }
    .current-keys div {
      margin-bottom: 8px;
    }
    .status {
      padding: 10px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .status.success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .status.error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .help-text {
      font-size: 12px;
      color: #666;
      margin-top: 5px;
    }
    .back-link {
      display: inline-block;
      margin-top: 20px;
      color: #667eea;
      text-decoration: none;
    }
    .back-link:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v2.6.0</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link active">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>
  
  <div class="page-content">
    <div class="container">
    <div class="header">
      <h1>🔐 LoRaWAN Configuration</h1>
      <p>Configure Activation Mode and Keys</p>
    </div>
    
    <div class="content">
      <div id="status" class="status"></div>
      
      <!-- Activation Mode Selection -->
      <div style="background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 20px; border-left: 4px solid #667eea;">
        <h3 style="margin-top: 0;">📡 Activation Mode</h3>
        <div style="display: flex; gap: 30px; margin-top: 15px;">
          <label style="display: flex; align-items: center; cursor: pointer;">
            <input type="radio" name="activationMode" value="otaa" id="modeOTAA" checked onchange="switchMode()" style="margin-right: 8px; width: 18px; height: 18px;">
            <span style="font-weight: bold;">OTAA (Over-The-Air Activation)</span>
          </label>
          <label style="display: flex; align-items: center; cursor: pointer;">
            <input type="radio" name="activationMode" value="abp" id="modeABP" onchange="switchMode()" style="margin-right: 8px; width: 18px; height: 18px;">
            <span style="font-weight: bold;">ABP (Activation By Personalization)</span>
          </label>
        </div>
        <p style="margin: 10px 0 0 0; font-size: 13px; color: #666;">
          <strong>OTAA:</strong> Device joins network with APPEUI, DEVEUI, APPKEY (recommended)<br>
          <strong>ABP:</strong> Device uses pre-configured session keys (DevAddr, NwkSKey, AppSKey)
        </p>
      </div>
      
      <!-- OTAA Help Section -->
      <div id="otaaHelp" style="background: #e7f3ff; padding: 15px; border-radius: 8px; margin-bottom: 20px; border-left: 4px solid #2196F3;">
        <h3 style="margin-top: 0; color: #1976D2;">📘 OTAA Key Format Guide</h3>
        <p style="margin: 10px 0;"><strong>APPEUI (JoinEUI):</strong> 8 bytes, <span style="color: #d32f2f;">little-endian</span> for LMIC</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          If TTN shows: <code>70B3D57ED0012345</code><br>
          Enter here: <code>45 23 01 D0 7E D5 B3 70</code> (reversed)
        </p>
        
        <p style="margin: 10px 0;"><strong>DEVEUI:</strong> 8 bytes, <span style="color: #d32f2f;">little-endian</span> for LMIC</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          If TTN shows: <code>0004A30B001A2B3C</code><br>
          Enter here: <code>3C 2B 1A 00 0B A3 04 00</code> (reversed)
        </p>
        
        <p style="margin: 10px 0;"><strong>APPKEY (AppKey/NwkKey):</strong> 16 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>00112233445566778899AABBCCDDEEFF</code>
        </p>
        
        <p style="margin: 15px 0 5px 0; font-size: 13px;"><strong>💡 Tip:</strong> Use spaces or colons for readability - they'll be removed automatically</p>
      </div>
      
      <!-- ABP Help Section -->
      <div id="abpHelp" style="background: #fff3e0; padding: 15px; border-radius: 8px; margin-bottom: 20px; border-left: 4px solid #ff9800; display: none;">
        <h3 style="margin-top: 0; color: #f57c00;">📘 ABP Key Format Guide</h3>
        <p style="margin: 10px 0;"><strong>DevAddr (Device Address):</strong> 4 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>260B1234</code> or <code>26 0B 12 34</code>
        </p>
        
        <p style="margin: 10px 0;"><strong>NwkSKey (Network Session Key):</strong> 16 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>00112233445566778899AABBCCDDEEFF</code>
        </p>
        
        <p style="margin: 10px 0;"><strong>AppSKey (Application Session Key):</strong> 16 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>FFEEDDCCBBAA99887766554433221100</code>
        </p>
        
        <p style="margin: 15px 0 5px 0; font-size: 13px;"><strong>⚠️ Note:</strong> ABP devices don't perform join - they start transmitting immediately</p>
      </div>
      
      <div class="current-keys">
        <strong>Current Keys:</strong>
        <div id="currentKeys">Loading...</div>
      </div>
      
      <form id="loraForm">
        <!-- OTAA Fields -->
        <div id="otaaFields">
          <div class="form-group">
            <label for="appeui">APPEUI (JoinEUI) - 8 bytes hex</label>
            <input type="text" id="appeui" name="appeui" placeholder="45 23 01 D0 7E D5 B3 70" maxlength="23">
            <div class="help-text">⚠️ Little-endian (reversed from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="deveui">DEVEUI - 8 bytes hex</label>
            <input type="text" id="deveui" name="deveui" placeholder="3C 2B 1A 00 0B A3 04 00" maxlength="23">
            <div class="help-text">⚠️ Little-endian (reversed from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="appkey">APPKEY - 16 bytes hex</label>
            <input type="text" id="appkey" name="appkey" placeholder="00112233445566778899AABBCCDDEEFF" maxlength="47">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
        </div>
        
        <!-- ABP Fields -->
        <div id="abpFields" style="display: none;">
          <div class="form-group">
            <label for="devaddr">DevAddr (Device Address) - 4 bytes hex</label>
            <input type="text" id="devaddr" name="devaddr" placeholder="260B1234" maxlength="11">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="nwkskey">NwkSKey (Network Session Key) - 16 bytes hex</label>
            <input type="text" id="nwkskey" name="nwkskey" placeholder="00112233445566778899AABBCCDDEEFF" maxlength="47">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="appskey">AppSKey (Application Session Key) - 16 bytes hex</label>
            <input type="text" id="appskey" name="appskey" placeholder="FFEEDDCCBBAA99887766554433221100" maxlength="47">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
        </div>
        
        <button type="submit" class="btn btn-primary">💾 Save Keys</button>
        <button type="button" class="btn btn-danger" onclick="clearKeys()">🗑️ Clear Keys</button>
        <button type="button" class="btn btn-secondary" onclick="window.location.href='/'">🏠 Dashboard</button>
        <button type="button" class="btn btn-secondary" onclick="window.location.href='/settings'">⚙️ Settings</button>
      </form>
      
      <hr style="margin: 30px 0; border: none; border-top: 2px solid #eee;">
      
      <h3 style="margin-bottom: 15px;">📡 WiFi Control</h3>
      <div style="margin-bottom: 20px;">
        <button type="button" class="btn btn-primary" onclick="wifiControl('reset')">🔄 Reset WiFi</button>
        <button type="button" class="btn btn-secondary" onclick="wifiControl('restart')">⚡ Restart Device</button>
      </div>
      <div class="help-text" style="margin-bottom: 20px;">
        Reset WiFi will clear saved credentials and restart the device in AP mode for reconfiguration.
      </div>
      
    </div>
      </div>
    </div>
  </div>

  <script>
    function showStatus(message, isError) {
      const status = document.getElementById('status');
      status.textContent = message;
      status.className = 'status ' + (isError ? 'error' : 'success');
      status.style.display = 'block';
      setTimeout(() => {
        status.style.display = 'none';
      }, 5000);
    }
    
    function switchMode() {
      const isOTAA = document.getElementById('modeOTAA').checked;
      
      // Toggle visibility of help sections
      document.getElementById('otaaHelp').style.display = isOTAA ? 'block' : 'none';
      document.getElementById('abpHelp').style.display = isOTAA ? 'none' : 'block';
      
      // Toggle visibility of form fields
      document.getElementById('otaaFields').style.display = isOTAA ? 'block' : 'none';
      document.getElementById('abpFields').style.display = isOTAA ? 'none' : 'block';
    }
    
    function loadCurrentKeys() {
      fetch('/api/lora-keys')
        .then(response => response.json())
        .then(data => {
          // Set mode radio buttons
          if (data.useABP) {
            document.getElementById('modeABP').checked = true;
            switchMode();
          } else {
            document.getElementById('modeOTAA').checked = true;
            switchMode();
          }
          
          // Display current keys based on mode
          if (data.useABP) {
            document.getElementById('currentKeys').innerHTML = `
              <div><strong>Mode:</strong> ABP (Activation By Personalization)</div>
              <div>DevAddr: ${data.devaddr || 'Not set'}</div>
              <div>NwkSKey: ${data.nwkskey || 'Not set'}</div>
              <div>AppSKey: ${data.appskey || 'Not set'}</div>
              <div>Status: ${data.configured ? 'Configured (from NVS)' : 'Using defaults'}</div>
            `;
            if (data.devaddr) document.getElementById('devaddr').value = data.devaddr.replace(/:/g, '');
            if (data.nwkskey) document.getElementById('nwkskey').value = data.nwkskey.replace(/:/g, '');
            if (data.appskey) document.getElementById('appskey').value = data.appskey.replace(/:/g, '');
          } else {
            document.getElementById('currentKeys').innerHTML = `
              <div><strong>Mode:</strong> OTAA (Over-The-Air Activation)</div>
              <div>APPEUI: ${data.appeui}</div>
              <div>DEVEUI: ${data.deveui}</div>
              <div>APPKEY: ${data.appkey}</div>
              <div>Status: ${data.configured ? 'Configured (from NVS)' : 'Using defaults'}</div>
            `;
            document.getElementById('appeui').value = data.appeui.replace(/:/g, '');
            document.getElementById('deveui').value = data.deveui.replace(/:/g, '');
            document.getElementById('appkey').value = data.appkey.replace(/:/g, '');
          }
        })
        .catch(err => {
          document.getElementById('currentKeys').textContent = 'Error loading keys';
        });
    }
    
    document.getElementById('loraForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      const isOTAA = document.getElementById('modeOTAA').checked;
      let data = { useABP: !isOTAA };
      
      if (isOTAA) {
        // OTAA mode - validate and send OTAA keys
        data.appeui = document.getElementById('appeui').value.replace(/[:\s-]/g, '');
        data.deveui = document.getElementById('deveui').value.replace(/[:\s-]/g, '');
        data.appkey = document.getElementById('appkey').value.replace(/[:\s-]/g, '');
        
        // Validate hex format
        if (data.appeui.length !== 16 || !/^[0-9A-Fa-f]+$/.test(data.appeui)) {
          showStatus('Error: APPEUI must be 16 hex characters (8 bytes)', true);
          return;
        }
        if (data.deveui.length !== 16 || !/^[0-9A-Fa-f]+$/.test(data.deveui)) {
          showStatus('Error: DEVEUI must be 16 hex characters (8 bytes)', true);
          return;
        }
        if (data.appkey.length !== 32 || !/^[0-9A-Fa-f]+$/.test(data.appkey)) {
          showStatus('Error: APPKEY must be 32 hex characters (16 bytes)', true);
          return;
        }
      } else {
        // ABP mode - validate and send ABP keys
        data.devaddr = document.getElementById('devaddr').value.replace(/[:\s-]/g, '');
        data.nwkskey = document.getElementById('nwkskey').value.replace(/[:\s-]/g, '');
        data.appskey = document.getElementById('appskey').value.replace(/[:\s-]/g, '');
        
        // Validate hex format
        if (data.devaddr.length !== 8 || !/^[0-9A-Fa-f]+$/.test(data.devaddr)) {
          showStatus('Error: DevAddr must be 8 hex characters (4 bytes)', true);
          return;
        }
        if (data.nwkskey.length !== 32 || !/^[0-9A-Fa-f]+$/.test(data.nwkskey)) {
          showStatus('Error: NwkSKey must be 32 hex characters (16 bytes)', true);
          return;
        }
        if (data.appskey.length !== 32 || !/^[0-9A-Fa-f]+$/.test(data.appskey)) {
          showStatus('Error: AppSKey must be 32 hex characters (16 bytes)', true);
          return;
        }
      }
      
      fetch('/api/lora-keys', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(response => response.json())
      .then(result => {
        if (result.success) {
          showStatus('Keys saved successfully! Restart device for changes to take effect.', false);
          loadCurrentKeys();
        } else {
          showStatus('Error: ' + result.message, true);
        }
      })
      .catch(err => {
        showStatus('Error saving keys: ' + err, true);
      });
    });
    
    function clearKeys() {
      if (confirm('Clear all LoRa keys and revert to defaults?')) {
        fetch('/api/lora-keys', {method: 'DELETE'})
          .then(response => response.json())
          .then(result => {
            if (result.success) {
              showStatus('Keys cleared. Restart device for changes to take effect.', false);
              loadCurrentKeys();
            } else {
              showStatus('Error clearing keys', true);
            }
          })
          .catch(err => {
            showStatus('Error: ' + err, true);
          });
      }
    }
    
    function wifiControl(action) {
      let confirmMsg = '';
      let endpoint = '';
      
      if (action === 'reset') {
        confirmMsg = 'Reset WiFi settings? Device will restart in AP mode for reconfiguration.';
        endpoint = '/api/wifi-reset';
      } else if (action === 'restart') {
        confirmMsg = 'Restart device now?';
        endpoint = '/api/restart';
      }
      
      if (confirm(confirmMsg)) {
        fetch(endpoint, {method: 'POST'})
          .then(response => response.json())
          .then(result => {
            if (result.success) {
              showStatus(result.message, false);
              setTimeout(() => {
                if (action === 'reset') {
                  alert('Device restarting in AP mode. Connect to "TTGO-T-Beam-Setup" WiFi network.');
                }
              }, 2000);
            } else {
              showStatus('Error: ' + result.message, true);
            }
          })
          .catch(err => {
            showStatus('Command sent. Device restarting...', false);
          });
      }
    }
    
    loadCurrentKeys();
  </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });
  
  // LoRa keys API - GET current keys
  server.on("/api/lora-keys", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["appeui"] = bytesToHexString(storedAPPEUI, 8, ":");
    doc["deveui"] = bytesToHexString(storedDEVEUI, 8, ":");
    doc["appkey"] = bytesToHexString(storedAPPKEY, 16, ":");
    doc["configured"] = loraKeysConfigured;
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
  
  // LoRa keys API - POST to save keys (OTAA or ABP)
  server.on("/api/lora-keys", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      bool success = true;
      String message = "";
      
      // Check if ABP mode
      bool isABP = doc["useABP"].as<bool>();
      useABP = isABP;
      
      if (isABP) {
        // ABP mode - parse DevAddr, NwkSKey, AppSKey
        String devaddr = doc["devaddr"].as<String>();
        String nwkskey = doc["nwkskey"].as<String>();
        String appskey = doc["appskey"].as<String>();
        
        if (!hexStringToBytes(devaddr, storedDEVADDR, 4)) {
          success = false;
          message = "Invalid DevAddr format (must be 8 hex chars)";
        } else if (!hexStringToBytes(nwkskey, storedNWKSKEY, 16)) {
          success = false;
          message = "Invalid NwkSKey format (must be 32 hex chars)";
        } else if (!hexStringToBytes(appskey, storedAPPSKEY, 16)) {
          success = false;
          message = "Invalid AppSKey format (must be 32 hex chars)";
        } else {
          success = saveLoRaKeysToNVS();
          message = success ? "ABP keys saved successfully" : "Failed to save ABP keys";
        }
      } else {
        // OTAA mode - parse APPEUI, DEVEUI, APPKEY
        String appeui = doc["appeui"].as<String>();
        String deveui = doc["deveui"].as<String>();
        String appkey = doc["appkey"].as<String>();
        
        if (!hexStringToBytes(appeui, storedAPPEUI, 8)) {
          success = false;
          message = "Invalid APPEUI format (must be 16 hex chars)";
        } else if (!hexStringToBytes(deveui, storedDEVEUI, 8)) {
          success = false;
          message = "Invalid DEVEUI format (must be 16 hex chars)";
        } else if (!hexStringToBytes(appkey, storedAPPKEY, 16)) {
          success = false;
          message = "Invalid APPKEY format (must be 32 hex chars)";
        } else {
          success = saveLoRaKeysToNVS();
          message = success ? "OTAA keys saved successfully" : "Failed to save OTAA keys";
        }
      }
      
      String response = "{\"success\":" + String(success ? "true" : "false") +
                       ",\"message\":\"" + message + "\"}";
      request->send(success ? 200 : 400, "application/json", response);
    }
  );
  
  // LoRa keys API - DELETE to clear keys
  server.on("/api/lora-keys", HTTP_DELETE, [](AsyncWebServerRequest *request){
    clearLoRaKeysFromNVS();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Keys cleared\"}");
  });
  
  // WiFi control API - Reset WiFi settings
  server.on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi settings reset. Restarting...\"}");
    delay(1000);
    wifiManager.resetSettings();
    ESP.restart();
  });
  
  // System control API - Restart device
  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Device restarting...\"}");
    delay(1000);
    ESP.restart();
  });
  
  // WiFi network selection page
  server.on("/wifi-select", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi Network Selection</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    .content { padding: 30px; }
    .network-list {
      list-style: none;
      margin-bottom: 20px;
    }
    .network-item {
      padding: 15px;
      border: 2px solid #ddd;
      border-radius: 8px;
      margin-bottom: 10px;
      cursor: pointer;
      transition: all 0.2s;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .network-item:hover {
      border-color: #667eea;
      background: #f8f9fa;
    }
    .network-item.selected {
      border-color: #667eea;
      background: #e7f0ff;
    }
    .network-name {
      font-weight: bold;
      font-size: 16px;
    }
    .network-signal {
      color: #666;
      font-size: 14px;
    }
    .network-secure {
      color: #f44336;
      font-size: 12px;
    }
    .password-section {
      display: none;
      margin-top: 20px;
      padding: 20px;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .password-section.show {
      display: block;
    }
    input[type="password"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
      margin-top: 10px;
    }
    .btn {
      padding: 12px 30px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      margin-right: 10px;
      margin-top: 10px;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .status {
      padding: 10px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .status.success {
      background: #d4edda;
      color: #155724;
      display: block;
    }
    .status.error {
      background: #f8d7da;
      color: #721c24;
      display: block;
    }
    .loading {
      text-align: center;
      padding: 20px;
      color: #666;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>📡 WiFi Network Selection</h1>
      <p>Choose a network to connect</p>
    </div>
    
    <div class="content">
      <div id="status" class="status"></div>
      
      <div class="loading" id="loading">
        Scanning for networks...
      </div>
      
      <ul class="network-list" id="networkList" style="display:none;"></ul>
      
      <div class="password-section" id="passwordSection">
        <h3>Enter Password</h3>
        <p>Network: <strong id="selectedNetwork"></strong></p>
        <input type="password" id="password" placeholder="Enter WiFi password">
        <br>
        <button class="btn btn-primary" onclick="connectToNetwork()">Connect</button>
        <button class="btn btn-secondary" onclick="cancelSelection()">Cancel</button>
      </div>
      
      <button class="btn btn-secondary" onclick="scanNetworks()" style="margin-top: 20px;">🔄 Rescan</button>
      <button class="btn btn-secondary" onclick="window.location.href='/config'">← Back</button>
    </div>
  </div>

  <script>
    let selectedSSID = '';
    let selectedSecure = false;
    
    function showStatus(message, isError) {
      const status = document.getElementById('status');
      status.textContent = message;
      status.className = 'status ' + (isError ? 'error' : 'success');
      setTimeout(() => {
        status.style.display = 'none';
      }, 5000);
    }
    
    function scanNetworks() {
      document.getElementById('loading').style.display = 'block';
      document.getElementById('networkList').style.display = 'none';
      
      fetch('/api/wifi-scan')
        .then(response => response.json())
        .then(data => {
          document.getElementById('loading').style.display = 'none';
          displayNetworks(data.networks);
        })
        .catch(err => {
          document.getElementById('loading').textContent = 'Error scanning networks';
          showStatus('Error: ' + err, true);
        });
    }
    
    function displayNetworks(networks) {
      const list = document.getElementById('networkList');
      list.innerHTML = '';
      list.style.display = 'block';
      
      networks.forEach(network => {
        const item = document.createElement('li');
        item.className = 'network-item';
        item.onclick = () => selectNetwork(network.ssid, network.secure);
        
        item.innerHTML = `
          <div>
            <div class="network-name">${network.ssid}</div>
            <div class="network-secure">${network.secure ? '🔒 Secured' : '🔓 Open'}</div>
          </div>
          <div class="network-signal">${network.rssi} dBm</div>
        `;
        
        list.appendChild(item);
      });
    }
    
    function selectNetwork(ssid, secure) {
      selectedSSID = ssid;
      selectedSecure = secure;
      
      // Highlight selected
      document.querySelectorAll('.network-item').forEach(item => {
        item.classList.remove('selected');
      });
      event.currentTarget.classList.add('selected');
      
      // Show password section if secured
      document.getElementById('selectedNetwork').textContent = ssid;
      document.getElementById('passwordSection').className = 'password-section show';
      
      if (!secure) {
        document.getElementById('password').style.display = 'none';
      } else {
        document.getElementById('password').style.display = 'block';
      }
    }
    
    function cancelSelection() {
      document.getElementById('passwordSection').className = 'password-section';
      selectedSSID = '';
    }
    
    function connectToNetwork() {
      if (!selectedSSID) {
        showStatus('Please select a network', true);
        return;
      }
      
      const password = document.getElementById('password').value;
      
      if (selectedSecure && !password) {
        showStatus('Please enter password', true);
        return;
      }
      
      showStatus('Connecting to ' + selectedSSID + '...', false);
      
      fetch('/api/wifi-connect', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          ssid: selectedSSID,
          password: password
        })
      })
      .then(response => response.json())
      .then(result => {
        if (result.success) {
          showStatus('Connected! Redirecting...', false);
          setTimeout(() => {
            window.location.href = '/';
          }, 3000);
        } else {
          showStatus('Connection failed: ' + result.message, true);
        }
      })
      .catch(err => {
        showStatus('Error: ' + err, true);
      });
    }
    
    // Auto-scan on load
    scanNetworks();
  </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });
  
  // WiFi scan API
  server.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    for (int i = 0; i < n && i < 20; i++) {
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
  
  // WiFi connect API
  server.on("/api/wifi-connect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      String ssid = doc["ssid"].as<String>();
      String password = doc["password"].as<String>();
      
      // Save credentials using WiFiManager
      WiFi.begin(ssid.c_str(), password.c_str());
      
      // Wait up to 10 seconds for connection
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Connected successfully\"}");
      } else {
        request->send(200, "application/json", "{\"success\":false,\"message\":\"Connection timeout\"}");
      }
    }
  );
  
  // System settings/configuration page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>System Settings</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 0;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 2px 10px rgba(0,0,0,0.2);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .page-content {
      padding: 20px;
    }
    .container {
      max-width: 1000px;
      margin: 0 auto;
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    .header h1 { font-size: 28px; margin-bottom: 10px; }
    .header p { opacity: 0.9; }
    .content { padding: 30px; }
    .section {
      margin-bottom: 30px;
      padding: 20px;
      background: #f8f9fa;
      border-radius: 10px;
      border-left: 4px solid #667eea;
    }
    .section h2 {
      font-size: 20px;
      margin-bottom: 15px;
      color: #333;
    }
    .control-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 15px;
      background: white;
      border-radius: 8px;
      margin-bottom: 10px;
    }
    .control-label {
      font-weight: bold;
      color: #333;
    }
    .control-value {
      color: #666;
      font-size: 14px;
    }
    .toggle-switch {
      position: relative;
      width: 60px;
      height: 30px;
    }
    .toggle-switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
      border-radius: 30px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 22px;
      width: 22px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: #667eea;
    }
    input:checked + .slider:before {
      transform: translateX(30px);
    }
    .input-group {
      margin-bottom: 15px;
    }
    .input-group label {
      display: block;
      font-weight: bold;
      margin-bottom: 5px;
      color: #333;
    }
    .input-group input {
      width: 100%;
      padding: 10px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
    }
    .input-group input:focus {
      outline: none;
      border-color: #667eea;
    }
    .btn {
      padding: 12px 30px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      margin-right: 10px;
      margin-top: 10px;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .btn-secondary:hover {
      background: #5a6268;
    }
    .status-badge {
      display: inline-block;
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 12px;
      font-weight: bold;
    }
    .status-on {
      background: #28a745;
      color: white;
    }
    .status-off {
      background: #dc3545;
      color: white;
    }
    .nav-buttons {
      text-align: center;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v2.6.0</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link active">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>
  
  <div class="page-content">
    <div class="container">
      <div class="header">
        <h1>⚙️ System Settings</h1>
        <p>Configure device behavior and power management</p>
      </div>
      
      <div class="content">
        <!-- Status & Controls Combined -->
        <div class="section">
          <h2>📊 Status & Controls</h2>
        <div class="control-row">
          <div>
            <div class="control-label">WiFi</div>
            <div class="control-value" id="wifi-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-wifi" onchange="toggleWiFi(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">GPS</div>
            <div class="control-value" id="gps-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-gps" onchange="toggleGPS(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">Debug Mode</div>
            <div class="control-value" id="debug-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-debug" onchange="toggleDebug(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">Display</div>
            <div class="control-value" id="display-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-display" onchange="toggleDisplay(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">Sleep Mode</div>
            <div class="control-value" id="sleep-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-sleep" onchange="toggleSleep(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">LoRa Send Interval</div>
            <div class="control-value" id="lora-interval">Loading...</div>
          </div>
        </div>
      </div>
      
      <!-- Configuration -->
      <div class="section">
        <h2>⏱️ Intervals</h2>
        <div class="input-group">
          <label>LoRa Send Interval (seconds)</label>
          <input type="number" id="lora-send-interval" min="60" value="60">
          <button class="btn btn-primary" onclick="setLoRaInterval()">Update LoRa Interval</button>
        </div>
        <div class="input-group">
          <label>Sleep Wake Interval (seconds)</label>
          <input type="number" id="sleep-wake-interval" min="60" value="86400">
          <button class="btn btn-primary" onclick="setSleepInterval()">Update Sleep Interval</button>
        </div>
      </div>
      
      <!-- Downlink Commands Reference -->
      <div class="section">
        <h2>📡 LoRaWAN Downlink Commands</h2>
        <p style="margin-bottom: 15px;">Send these hex commands from your LoRaWAN network server (TTN/ChirpStack) to control the device remotely:</p>
        
        <div style="background: white; padding: 15px; border-radius: 8px; margin-bottom: 10px;">
          <h4 style="margin-top: 0; color: #667eea;">Simple Commands (1 byte):</h4>
          <table style="width: 100%; font-size: 13px;">
            <tr><td style="padding: 5px;"><code>01</code></td><td>WiFi ON</td></tr>
            <tr><td style="padding: 5px;"><code>02</code></td><td>WiFi OFF</td></tr>
            <tr><td style="padding: 5px;"><code>05</code></td><td>Debug ON</td></tr>
            <tr><td style="padding: 5px;"><code>06</code></td><td>Debug OFF</td></tr>
          </table>
        </div>
        
        <div style="background: white; padding: 15px; border-radius: 8px;">
          <h4 style="margin-top: 0; color: #667eea;">Enhanced Commands (0xAA prefix):</h4>
          <table style="width: 100%; font-size: 13px;">
            <tr><td style="padding: 5px; width: 180px;"><code>AA 01 00</code></td><td>WiFi OFF</td></tr>
            <tr><td style="padding: 5px;"><code>AA 01 01</code></td><td>WiFi ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 02 00</code></td><td>GPS OFF (saves ~40mA)</td></tr>
            <tr><td style="padding: 5px;"><code>AA 02 01</code></td><td>GPS ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 06 00</code></td><td>Debug OFF</td></tr>
            <tr><td style="padding: 5px;"><code>AA 06 01</code></td><td>Debug ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 07 00</code></td><td>Display OFF (saves ~15mA)</td></tr>
            <tr><td style="padding: 5px;"><code>AA 07 01</code></td><td>Display ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 05 00</code></td><td>Sleep Mode OFF</td></tr>
            <tr><td style="padding: 5px;"><code>AA 03 [4 bytes]</code></td><td>Sleep Mode with interval (seconds in hex)</td></tr>
            <tr><td style="padding: 5px;"><code>AA 04 [4 bytes]</code></td><td>LoRa send interval (seconds in hex)</td></tr>
          </table>
          
          <div style="margin-top: 15px; padding: 10px; background: #f8f9fa; border-radius: 5px;">
            <strong>Examples:</strong><br>
            <code style="display: block; margin: 5px 0;">AA 03 00 01 51 80</code> → Sleep 24 hours (86400 sec)<br>
            <code style="display: block; margin: 5px 0;">AA 04 00 00 03 84</code> → Send every 15 min (900 sec)<br>
            <code style="display: block; margin: 5px 0;">AA 02 00</code> → GPS OFF for power saving
          </div>
          
          <p style="margin-top: 10px; font-size: 12px; color: #666;">
            💡 <strong>Tip:</strong> For hex conversion, use: <code>printf '%08X' 86400</code> → <code>00015180</code>
          </p>
        </div>
      </div>
      
      <!-- Refresh Button -->
      <div class="nav-buttons">
        <button class="btn btn-primary" onclick="loadStatus()">🔄 Refresh Status</button>
      </div>
      </div>
    </div>
  </div>
  
  <script>
    function loadStatus() {
      fetch('/api/system-config')
        .then(response => response.json())
        .then(data => {
          // Update status displays
          document.getElementById('wifi-status').innerHTML =
            data.wifiEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('gps-status').innerHTML =
            data.gpsEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('debug-status').innerHTML =
            data.debugEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('display-status').innerHTML =
            data.displayEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('sleep-status').innerHTML =
            data.sleepEnabled ? '<span class="status-badge status-on">ON (' + data.sleepInterval + 's)</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('lora-interval').textContent = data.loraInterval + ' seconds';
          
          // Update toggles
          document.getElementById('toggle-wifi').checked = data.wifiEnabled;
          document.getElementById('toggle-gps').checked = data.gpsEnabled;
          document.getElementById('toggle-debug').checked = data.debugEnabled;
          document.getElementById('toggle-display').checked = data.displayEnabled;
          document.getElementById('toggle-sleep').checked = data.sleepEnabled;
          
          // Update inputs
          document.getElementById('lora-send-interval').value = data.loraInterval;
          document.getElementById('sleep-wake-interval').value = data.sleepInterval;
        })
        .catch(error => console.error('Error loading status:', error));
    }
    
    function toggleWiFi(enabled) {
      sendCommand('wifi', enabled ? 'on' : 'off');
    }
    
    function toggleGPS(enabled) {
      sendCommand('gps', enabled ? 'on' : 'off');
    }
    
    function toggleDebug(enabled) {
      sendCommand('debug', enabled ? 'on' : 'off');
    }
    
    function toggleDisplay(enabled) {
      sendCommand('display', enabled ? 'on' : 'off');
    }
    
    function toggleSleep(enabled) {
      sendCommand('sleep', enabled ? 'on' : 'off');
    }
    
    function setLoRaInterval() {
      const interval = document.getElementById('lora-send-interval').value;
      sendCommand('lora-interval', interval);
    }
    
    function setSleepInterval() {
      const interval = document.getElementById('sleep-wake-interval').value;
      sendCommand('sleep-interval', interval);
    }
    
    function sendCommand(type, value) {
      fetch('/api/system-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type: type, value: value })
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          alert('✓ ' + data.message);
          setTimeout(loadStatus, 500);
        } else {
          alert('✗ ' + data.message);
        }
      })
      .catch(error => {
        console.error('Error:', error);
        alert('Error sending command');
      });
    }
    
    // Load status on page load
    loadStatus();
    
    // Auto-refresh every 5 seconds
    setInterval(loadStatus, 5000);
  </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });
  
  // API endpoint for system configuration (GET)
  server.on("/api/system-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"wifiEnabled\":" + String(wifiEnabled ? "true" : "false") + ",";
    json += "\"gpsEnabled\":" + String(gpsEnabled ? "true" : "false") + ",";
    json += "\"debugEnabled\":" + String(debugEnabled ? "true" : "false") + ",";
    json += "\"displayEnabled\":" + String(displayEnabled ? "true" : "false") + ",";
    json += "\"sleepEnabled\":" + String(sleepModeEnabled ? "true" : "false") + ",";
    json += "\"sleepInterval\":" + String(sleepWakeInterval) + ",";
    json += "\"loraInterval\":" + String(loraSendIntervalSec);
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // API endpoint for system configuration (POST)
  server.on("/api/system-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
      }
      
      // Parse JSON manually (simple parsing)
      String type = "";
      String value = "";
      
      int typeStart = body.indexOf("\"type\":\"") + 8;
      int typeEnd = body.indexOf("\"", typeStart);
      if (typeStart > 7 && typeEnd > typeStart) {
        type = body.substring(typeStart, typeEnd);
      }
      
      int valueStart = body.indexOf("\"value\":\"") + 9;
      int valueEnd = body.indexOf("\"", valueStart);
      if (valueStart > 8 && valueEnd > valueStart) {
        value = body.substring(valueStart, valueEnd);
      }
      
      String response = "{\"success\":false,\"message\":\"Unknown command\"}";
      
      bool settingsChanged = false;
      
      if (type == "wifi") {
        if (value == "on") {
          wifiEnabled = true;
          response = "{\"success\":true,\"message\":\"WiFi enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          wifiEnabled = false;
          response = "{\"success\":true,\"message\":\"WiFi disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "gps") {
        if (value == "on") {
          gpsEnabled = true;
          enableGpsPower();
          response = "{\"success\":true,\"message\":\"GPS enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          gpsEnabled = false;
          disableGpsPower();
          response = "{\"success\":true,\"message\":\"GPS disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "debug") {
        if (value == "on") {
          debugEnabled = true;
          response = "{\"success\":true,\"message\":\"Debug mode enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          debugEnabled = false;
          response = "{\"success\":true,\"message\":\"Debug mode disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "display") {
        if (value == "on") {
          enableDisplay();
          response = "{\"success\":true,\"message\":\"Display enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          disableDisplay();
          response = "{\"success\":true,\"message\":\"Display disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "sleep") {
        if (value == "on") {
          sleepModeEnabled = true;
          lastWakeTime = millis();
          response = "{\"success\":true,\"message\":\"Sleep mode enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          sleepModeEnabled = false;
          response = "{\"success\":true,\"message\":\"Sleep mode disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "lora-interval") {
        int interval = value.toInt();
        if (interval >= 10) {
          loraSendIntervalSec = interval;
          response = "{\"success\":true,\"message\":\"LoRa interval updated to " + String(interval) + " seconds\"}";
          settingsChanged = true;
        } else {
          response = "{\"success\":false,\"message\":\"Interval must be >= 10 seconds\"}";
        }
      } else if (type == "sleep-interval") {
        int interval = value.toInt();
        if (interval >= 60) {
          sleepWakeInterval = interval;
          response = "{\"success\":true,\"message\":\"Sleep interval updated to " + String(interval) + " seconds\"}";
          settingsChanged = true;
        } else {
          response = "{\"success\":false,\"message\":\"Interval must be >= 60 seconds\"}";
        }
      }
      
      // Save settings to NVS if any were changed
      if (settingsChanged) {
        saveSystemSettings();
      }
      
      request->send(200, "application/json", response);
    }
  );
  
  // Diagnostics API endpoint
  server.on("/api/diagnostics", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"uplinkCount\":" + String(uplinkCounter) + ",";
    json += "\"downlinkCount\":" + String(downlinkCounter) + ",";
    json += "\"frameCounterUp\":" + String(LMIC.seqnoUp) + ",";
    json += "\"frameCounterDown\":" + String(LMIC.seqnoDn) + ",";
    json += "\"loraJoined\":" + String(loraJoined ? "true" : "false") + ",";
    json += "\"useABP\":" + String(useABP ? "true" : "false") + ",";
    json += "\"sendInterval\":" + String(loraSendIntervalSec) + ",";
    json += "\"lastUplink\":\"" + String(uplinkCounter > 0 ? "Recently" : "Never") + "\",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"wifiEnabled\":" + String(wifiEnabled ? "true" : "false") + ",";
    json += "\"gpsEnabled\":" + String(gpsEnabled ? "true" : "false") + ",";
    
    // WiFi information
    json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    if (WiFi.status() == WL_CONNECTED) {
      json += "\"wifiSSID\":\"" + WiFi.SSID() + "\",";
      json += "\"wifiIP\":\"" + WiFi.localIP().toString() + "\",";
      json += "\"wifiRSSI\":" + String(WiFi.RSSI()) + ",";
    } else {
      json += "\"wifiSSID\":\"Not connected\",";
      json += "\"wifiIP\":\"0.0.0.0\",";
      json += "\"wifiRSSI\":0,";
    }
    
    // LoRa signal information
    json += "\"loraRSSI\":" + String(LMIC.rssi) + ",";
    json += "\"loraSNR\":" + String(LMIC.snr) + ",";
    json += "\"loraFrequency\":" + String(LMIC.freq) + ",";
    
    // Calculate bandwidth and spreading factor from data rate (US915)
    // DR0: SF10/125kHz, DR1: SF9/125kHz, DR2: SF8/125kHz, DR3: SF7/125kHz
    // DR4: SF8/500kHz, DR8-13: RFU
    int bandwidth = 125; // Default 125 kHz
    int spreadingFactor = 10; // Default SF10
    
    switch(LMIC.datarate) {
      case 0: spreadingFactor = 10; bandwidth = 125; break;
      case 1: spreadingFactor = 9; bandwidth = 125; break;
      case 2: spreadingFactor = 8; bandwidth = 125; break;
      case 3: spreadingFactor = 7; bandwidth = 125; break;
      case 4: spreadingFactor = 8; bandwidth = 500; break;
      default: spreadingFactor = 7; bandwidth = 125; break;
    }
    
    json += "\"loraBandwidth\":" + String(bandwidth) + ",";
    json += "\"loraSpreadingFactor\":" + String(spreadingFactor) + ",";
    json += "\"loraDataRate\":" + String(LMIC.datarate) + ",";
    json += "\"loraTxPower\":" + String(LMIC.txpow) + ",";
    
    // Battery voltage
    float batteryVoltage = 0.0;
    if (axpFound) {
      batteryVoltage = readBatteryVoltage();
    }
    json += "\"batteryVoltage\":" + String(batteryVoltage, 2);
    json += "}";
    
    request->send(200, "application/json", json);
  });
  
  // Reset frame counter API endpoint
  server.on("/api/reset-frame-counter", HTTP_POST, [](AsyncWebServerRequest *request){
    // Reset LMIC frame counter
    LMIC.seqnoUp = 0;
    
    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"Frame counter reset to 0\",";
    json += "\"frameCounterUp\":" + String(LMIC.seqnoUp);
    json += "}";
    
    Serial.println("⚠️ Frame counter reset to 0 via web UI");
    logDebug("⚠️ Frame counter reset to 0 via web UI");
    
    request->send(200, "application/json", json);
  });
  
  // Channel configuration API - GET
  server.on("/api/channel-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"channels\":[";
    for (int i = 0; i < 10; i++) {
      if (i > 0) json += ",";
      json += channelEnabled[i] ? "true" : "false";
    }
    json += "]}";
    request->send(200, "application/json", json);
  });
  
  // Channel configuration API - POST
  server.on("/api/channel-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      JsonArray channels = doc["channels"].as<JsonArray>();
      if (channels.size() != 10) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Must provide 10 channel states\"}");
        return;
      }
      
      for (int i = 0; i < 10; i++) {
        channelEnabled[i] = channels[i].as<bool>();
      }
      
      Serial.println("Channel configuration updated via web UI");
      if (debugEnabled) {
        Serial.print("Enabled channels: ");
        for (int i = 0; i < 10; i++) {
          if (channelEnabled[i]) Serial.print(String(i+1) + " ");
        }
        Serial.println();
      }
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"Channel configuration saved\"}");
    }
  );
  
  // PAX counter configuration API - GET
  server.on("/api/pax-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"enabled\":" + String(paxCounterEnabled ? "true" : "false") + ",";
    json += "\"wifiEnabled\":" + String(paxWifiScanEnabled ? "true" : "false") + ",";
    json += "\"bleEnabled\":" + String(paxBleScanEnabled ? "true" : "false") + ",";
    json += "\"interval\":" + String(paxScanInterval / 1000) + ",";
    json += "\"wifiCount\":" + String(wifiDeviceCount) + ",";
    json += "\"bleCount\":" + String(bleDeviceCount) + ",";
    json += "\"totalCount\":" + String(totalPaxCount);
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // PAX counter configuration API - POST
  server.on("/api/pax-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      bool enabled = doc["enabled"].as<bool>();
      bool wifiEnabled = doc["wifiEnabled"] | true; // Default to true if not provided
      bool bleEnabled = doc["bleEnabled"] | true;   // Default to true if not provided
      uint32_t interval = doc["interval"].as<uint32_t>();
      
      if (interval < 30 || interval > 3600) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Interval must be 30-3600 seconds\"}");
        return;
      }
      
      if (!wifiEnabled && !bleEnabled) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"At least one scanning method must be enabled\"}");
        return;
      }
      
      paxCounterEnabled = enabled;
      paxWifiScanEnabled = wifiEnabled;
      paxBleScanEnabled = bleEnabled;
      paxScanInterval = interval * 1000; // Convert to milliseconds
      channelEnabled[9] = enabled; // Enable/disable channel 10 based on PAX counter state
      
      // Initialize or deinitialize BLE scanner based on enabled state
      if (enabled && bleEnabled && pBLEScan == nullptr) {
        Serial.println("Initializing BLE scanner for PAX counter...");
        NimBLEDevice::init("");
        pBLEScan = NimBLEDevice::getScan();
        pBLEScan->setActiveScan(false); // Passive scan
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        Serial.println("BLE scanner initialized");
      }
      
      // Save settings to NVS
      saveSystemSettings();
      
      Serial.print("PAX counter ");
      Serial.print(enabled ? "enabled" : "disabled");
      Serial.print(" (WiFi: ");
      Serial.print(wifiEnabled ? "ON" : "OFF");
      Serial.print(", BLE: ");
      Serial.print(bleEnabled ? "ON" : "OFF");
      Serial.print(") with interval ");
      Serial.print(interval);
      Serial.println(" seconds");
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"PAX counter settings saved\"}");
    }
  );
  
  // Debug console page
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Debug Console - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Courier New', monospace;
      background: #1e1e1e;
      color: #d4d4d4;
      padding: 0;
      margin: 0;
      height: 100vh;
      display: flex;
      flex-direction: column;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 2px 10px rgba(0,0,0,0.2);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 15px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 2px 10px rgba(0,0,0,0.3);
    }
    .header h1 {
      font-size: 1.2em;
      font-family: Arial, sans-serif;
    }
    .header-controls {
      display: flex;
      gap: 10px;
      align-items: center;
    }
    .btn {
      padding: 8px 15px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 0.9em;
      font-family: Arial, sans-serif;
      transition: all 0.2s;
    }
    .btn-primary {
      background: white;
      color: #667eea;
      font-weight: bold;
    }
    .btn-primary:hover {
      background: #f0f0f0;
    }
    .btn-secondary {
      background: rgba(255,255,255,0.2);
      color: white;
      border: 1px solid rgba(255,255,255,0.3);
    }
    .btn-secondary:hover {
      background: rgba(255,255,255,0.3);
    }
    .console-container {
      flex: 1;
      display: flex;
      flex-direction: column;
      overflow: hidden;
      background: #1e1e1e;
    }
    .console-header {
      padding: 10px 20px;
      background: #2d2d2d;
      border-bottom: 1px solid #3e3e3e;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .console-info {
      font-size: 0.85em;
      color: #858585;
    }
    .status-indicator {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      margin-right: 5px;
      animation: pulse 2s infinite;
    }
    .status-indicator.active {
      background: #4caf50;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
    #console {
      flex: 1;
      overflow-y: auto;
      padding: 15px 20px;
      font-size: 13px;
      line-height: 1.5;
      white-space: pre-wrap;
      word-wrap: break-word;
    }
    #console::-webkit-scrollbar {
      width: 10px;
    }
    #console::-webkit-scrollbar-track {
      background: #2d2d2d;
    }
    #console::-webkit-scrollbar-thumb {
      background: #555;
      border-radius: 5px;
    }
    #console::-webkit-scrollbar-thumb:hover {
      background: #777;
    }
    .log-line {
      margin-bottom: 2px;
    }
    .log-error {
      color: #f48771;
    }
    .log-warning {
      color: #dcdcaa;
    }
    .log-success {
      color: #4ec9b0;
    }
    .log-info {
      color: #9cdcfe;
    }
    .back-link {
      color: white;
      text-decoration: none;
      font-family: Arial, sans-serif;
    }
    .back-link:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v2.6.0</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link active">🖥️ Debug</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>
  
  <div class="header">
    <h1>🖥️ Debug Console</h1>
    <div class="header-controls">
      <button class="btn btn-secondary" id="debugToggleBtn" onclick="toggleDebugMode()">
        <span id="debugStatus">🐛 Debug: Loading...</span>
      </button>
      <button class="btn btn-secondary" onclick="clearConsole()">🗑️ Clear</button>
      <button class="btn btn-secondary" onclick="toggleAutoScroll()">
        <span id="scrollBtn">📌 Auto-scroll: ON</span>
      </button>
    </div>
  </div>
  
  <div class="console-container">
    <div class="console-header">
      <div class="console-info">
        <span class="status-indicator active"></span>
        Live Serial Monitor | Messages: <span id="msgCount">0</span> |
        Last Update: <span id="lastUpdate">--</span>
      </div>
      <div class="console-info">
        Update Rate: 1s | Buffer: 100 messages
      </div>
    </div>
    <div id="console">Connecting to device...</div>
  </div>

  <script>
    let autoScroll = true;
    let messageCount = 0;
    let lastMessageCount = 0;
    
    function toggleAutoScroll() {
      autoScroll = !autoScroll;
      document.getElementById('scrollBtn').textContent =
        autoScroll ? '📌 Auto-scroll: ON' : '📌 Auto-scroll: OFF';
    }
    
    function clearConsole() {
      document.getElementById('console').textContent = 'Console cleared. Waiting for new messages...';
      messageCount = 0;
      document.getElementById('msgCount').textContent = '0';
    }
    
    function toggleDebugMode() {
      // Get current debug status first
      fetch('/api/system-config')
        .then(response => response.json())
        .then(data => {
          const newState = !data.debugEnabled;
          
          // Send toggle command
          fetch('/api/system-config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ type: 'debug', value: newState ? 'on' : 'off' })
          })
          .then(response => response.json())
          .then(result => {
            if (result.success) {
              updateDebugStatus();
            }
          })
          .catch(err => console.error('Error toggling debug:', err));
        });
    }
    
    function updateDebugStatus() {
      fetch('/api/system-config')
        .then(response => response.json())
        .then(data => {
          const debugBtn = document.getElementById('debugToggleBtn');
          const debugStatus = document.getElementById('debugStatus');
          if (data.debugEnabled) {
            debugStatus.textContent = '🐛 Debug: ON';
            debugBtn.style.background = 'rgba(76, 175, 80, 0.3)';
          } else {
            debugStatus.textContent = '🐛 Debug: OFF';
            debugBtn.style.background = 'rgba(255, 255, 255, 0.1)';
          }
        })
        .catch(err => console.error('Error fetching debug status:', err));
    }
    
    function colorizeLog(line) {
      // Simple colorization based on keywords
      if (line.includes('ERROR') || line.includes('FAIL') || line.includes('✗')) {
        return '<span class="log-error">' + line + '</span>';
      } else if (line.includes('WARN') || line.includes('⚠')) {
        return '<span class="log-warning">' + line + '</span>';
      } else if (line.includes('SUCCESS') || line.includes('✓') || line.includes('LOCKED')) {
        return '<span class="log-success">' + line + '</span>';
      } else if (line.includes('INFO') || line.includes('ℹ') || line.includes('GPS') || line.includes('LoRa')) {
        return '<span class="log-info">' + line + '</span>';
      }
      return line;
    }
    
    function updateConsole() {
      fetch('/api/debug-log')
        .then(response => response.json())
        .then(data => {
          const consoleDiv = document.getElementById('console');
          const messages = data.messages || [];
          
          if (messages.length > 0) {
            // Build HTML with colorization
            let html = '';
            messages.forEach(msg => {
              html += '<div class="log-line">' + colorizeLog(msg) + '</div>';
            });
            
            consoleDiv.innerHTML = html;
            messageCount = data.count || messages.length;
            
            // Auto-scroll to bottom if enabled
            if (autoScroll) {
              consoleDiv.scrollTop = consoleDiv.scrollHeight;
            }
          }
          
          document.getElementById('msgCount').textContent = messageCount;
          document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
        })
        .catch(err => {
          console.error('Error fetching debug log:', err);
          document.getElementById('console').textContent =
            'Error connecting to device. Retrying...\n' + err;
        });
    }
    
    // Initial updates
    updateConsole();
    updateDebugStatus();
    
    // Update every second
    setInterval(updateConsole, 1000);
    setInterval(updateDebugStatus, 5000); // Update debug status every 5 seconds
    
    // Pause auto-scroll when user scrolls up
    document.getElementById('console').addEventListener('scroll', function() {
      const elem = this;
      const isAtBottom = elem.scrollHeight - elem.scrollTop <= elem.clientHeight + 50;
      if (!isAtBottom && autoScroll) {
        autoScroll = false;
        document.getElementById('scrollBtn').textContent = '📌 Auto-scroll: OFF';
      }
    });
  </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });
  
  // Debug log API endpoint
  server.on("/api/debug-log", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"messages\":";
    json += getDebugBufferJson();
    json += ",\"count\":" + String(debugMessageCounter);
    json += ",\"bufferSize\":" + String(DEBUG_BUFFER_SIZE);
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // Diagnostics page
  server.on("/diagnostics", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /diagnostics page");
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Diagnostics - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    h2 {
      color: #667eea;
      margin-bottom: 15px;
      font-size: 20px;
      border-bottom: 2px solid #667eea;
      padding-bottom: 10px;
    }
    .stats-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }
    .stat-box {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 20px;
      border-radius: 10px;
      text-align: center;
    }
    .stat-label {
      font-size: 14px;
      opacity: 0.9;
      margin-bottom: 5px;
    }
    .stat-value {
      font-size: 32px;
      font-weight: bold;
    }
    .stat-unit {
      font-size: 14px;
      opacity: 0.8;
    }
    .info-row {
      display: flex;
      justify-content: space-between;
      padding: 12px;
      border-bottom: 1px solid #eee;
    }
    .info-row:last-child {
      border-bottom: none;
    }
    .info-label {
      font-weight: 600;
      color: #555;
    }
    .info-value {
      color: #333;
      font-family: 'Courier New', monospace;
    }
    .btn {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      font-size: 16px;
      font-weight: 600;
      transition: all 0.3s;
      display: inline-flex;
      align-items: center;
      gap: 8px;
    }
    .btn-danger {
      background: #f44336;
      color: white;
    }
    .btn-danger:hover {
      background: #d32f2f;
      transform: translateY(-2px);
      box-shadow: 0 4px 12px rgba(244, 67, 54, 0.4);
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
    }
    .btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .alert {
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .alert-success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .alert-error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .button-group {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-top: 15px;
    }
    @media (max-width: 768px) {
      .stats-grid {
        grid-template-columns: 1fr;
      }
      .info-row {
        flex-direction: column;
        gap: 5px;
      }
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v2.6.0</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link active">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <h1>🔧 System Diagnostics</h1>
      <p style="color: #666; margin-bottom: 20px;">LoRaWAN statistics and system health monitoring</p>
      
      <div id="alertBox" class="alert"></div>
      
      <!-- LoRa Statistics -->
      <h2>📡 LoRaWAN Statistics</h2>
      <div class="stats-grid">
        <div class="stat-box">
          <div class="stat-label">Uplink Messages</div>
          <div class="stat-value" id="uplinkCount">-</div>
          <div class="stat-unit">sent</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Downlink Messages</div>
          <div class="stat-value" id="downlinkCount">-</div>
          <div class="stat-unit">received</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Frame Counter (Up)</div>
          <div class="stat-value" id="frameCounterUp">-</div>
          <div class="stat-unit">LMIC.seqnoUp</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Frame Counter (Down)</div>
          <div class="stat-value" id="frameCounterDown">-</div>
          <div class="stat-unit">LMIC.seqnoDn</div>
        </div>
      </div>
      
      <!-- LoRa Details -->
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px; margin-bottom: 20px;">
        <div class="info-row">
          <span class="info-label">Network Status:</span>
          <span class="info-value" id="loraStatus">-</span>
        </div>
        <div class="info-row">
          <span class="info-label">Activation Mode:</span>
          <span class="info-value" id="activationMode">-</span>
        </div>
        <div class="info-row">
          <span class="info-label">Send Interval:</span>
          <span class="info-value" id="sendInterval">-</span>
        </div>
        <div class="info-row">
          <span class="info-label">Last Uplink:</span>
          <span class="info-value" id="lastUplink">-</span>
        </div>
      </div>
      
      <!-- LoRa Signal Information -->
      <h2>📡 LoRa Signal Information</h2>
      <p style="color: #666; margin-bottom: 15px;">Radio signal quality and transmission parameters</p>
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px; margin-bottom: 20px;">
        <div class="info-row">
          <span class="info-label">RSSI (Signal Strength):</span>
          <span class="info-value" id="loraRSSI">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Received Signal Strength Indicator in dBm. Closer to 0 = stronger signal. Typical range: -120 to -30 dBm
        </p>
        
        <div class="info-row">
          <span class="info-label">SNR (Signal-to-Noise Ratio):</span>
          <span class="info-value" id="loraSNR">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Signal quality in dB. Positive values indicate good signal above noise floor. Negative values can still work with LoRa
        </p>
        
        <div class="info-row">
          <span class="info-label">Frequency:</span>
          <span class="info-value" id="loraFrequency">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Radio frequency in MHz. US915 band uses 902-928 MHz
        </p>
        
        <div class="info-row">
          <span class="info-label">Bandwidth:</span>
          <span class="info-value" id="loraBandwidth">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Channel bandwidth in kHz. Wider bandwidth = faster data rate but shorter range
        </p>
        
        <div class="info-row">
          <span class="info-label">Spreading Factor:</span>
          <span class="info-value" id="loraSpreadingFactor">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          SF7-SF12. Higher SF = longer range but slower data rate and more airtime
        </p>
        
        <div class="info-row">
          <span class="info-label">Data Rate:</span>
          <span class="info-value" id="loraDataRate">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          DR0-DR15. Combination of spreading factor and bandwidth. Higher DR = faster transmission
        </p>
        
        <div class="info-row">
          <span class="info-label">TX Power:</span>
          <span class="info-value" id="loraTxPower">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px;">
          Transmission power in dBm. Higher power = longer range but more battery consumption
        </p>
      </div>
      
      <!-- Actions -->
      <h2>⚙️ Maintenance Actions</h2>
      <div class="button-group">
        <button class="btn btn-danger" onclick="resetFrameCounter()">
          🔄 Reset Frame Counter
        </button>
        <button class="btn btn-primary" onclick="refreshStats()">
          ♻️ Refresh Statistics
        </button>
      </div>
      <p style="color: #999; font-size: 14px; margin-top: 10px;">
        ⚠️ Warning: Resetting the frame counter will set LMIC.seqnoUp to 0. Only do this if instructed by your network administrator or after re-joining the network.
      </p>
    </div>
    
    <!-- System Health -->
    <div class="card">
      <h2>💚 System Health</h2>
      <div class="info-row">
        <span class="info-label">Uptime:</span>
        <span class="info-value" id="uptime">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">Free Heap:</span>
        <span class="info-value" id="freeHeap">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi Status:</span>
        <span class="info-value" id="wifiStatus">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi SSID:</span>
        <span class="info-value" id="wifiSSID">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi IP Address:</span>
        <span class="info-value" id="wifiIP">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi Signal (RSSI):</span>
        <span class="info-value" id="wifiRSSI">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">GPS Status:</span>
        <span class="info-value" id="gpsStatus">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">Battery Voltage:</span>
        <span class="info-value" id="batteryVoltage">-</span>
      </div>
    </div>
  </div>

  <script>
    function showAlert(message, isError = false) {
      const alertBox = document.getElementById('alertBox');
      alertBox.textContent = message;
      alertBox.className = 'alert ' + (isError ? 'alert-error' : 'alert-success');
      alertBox.style.display = 'block';
      setTimeout(() => {
        alertBox.style.display = 'none';
      }, 5000);
    }
    
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const mins = Math.floor((seconds % 3600) / 60);
      const secs = seconds % 60;
      
      if (days > 0) return `${days}d ${hours}h ${mins}m`;
      if (hours > 0) return `${hours}h ${mins}m ${secs}s`;
      if (mins > 0) return `${mins}m ${secs}s`;
      return `${secs}s`;
    }
    
    function refreshStats() {
      // Fetch diagnostics data
      fetch('/api/diagnostics')
        .then(response => response.json())
        .then(data => {
          // LoRa stats
          document.getElementById('uplinkCount').textContent = data.uplinkCount || 0;
          document.getElementById('downlinkCount').textContent = data.downlinkCount || 0;
          document.getElementById('frameCounterUp').textContent = data.frameCounterUp || 0;
          document.getElementById('frameCounterDown').textContent = data.frameCounterDown || 0;
          
          // LoRa details
          document.getElementById('loraStatus').textContent = data.loraJoined ? '✅ Joined' : '❌ Not Joined';
          document.getElementById('activationMode').textContent = data.useABP ? 'ABP' : 'OTAA';
          document.getElementById('sendInterval').textContent = data.sendInterval + ' seconds';
          document.getElementById('lastUplink').textContent = data.lastUplink || 'Never';
          
          // LoRa signal information
          document.getElementById('loraRSSI').textContent = (data.loraRSSI || 0) + ' dBm';
          document.getElementById('loraSNR').textContent = (data.loraSNR || 0) + ' dB';
          document.getElementById('loraFrequency').textContent = ((data.loraFrequency || 0) / 1000000).toFixed(2) + ' MHz';
          document.getElementById('loraBandwidth').textContent = (data.loraBandwidth || 0) + ' kHz';
          document.getElementById('loraSpreadingFactor').textContent = 'SF' + (data.loraSpreadingFactor || 7);
          document.getElementById('loraDataRate').textContent = 'DR' + (data.loraDataRate || 0);
          document.getElementById('loraTxPower').textContent = (data.loraTxPower || 0) + ' dBm';
          
          // System health
          document.getElementById('uptime').textContent = formatUptime(data.uptime || 0);
          document.getElementById('freeHeap').textContent = (data.freeHeap || 0).toLocaleString() + ' bytes';
          
          // WiFi information
          const wifiConnected = data.wifiConnected || false;
          document.getElementById('wifiStatus').textContent = wifiConnected ? '✅ Connected' : (data.wifiEnabled ? '⚠️ Enabled (Not Connected)' : '❌ Disabled');
          document.getElementById('wifiSSID').textContent = data.wifiSSID || 'N/A';
          document.getElementById('wifiIP').textContent = data.wifiIP || '0.0.0.0';
          document.getElementById('wifiRSSI').textContent = wifiConnected ? (data.wifiRSSI || 0) + ' dBm' : 'N/A';
          
          document.getElementById('gpsStatus').textContent = data.gpsEnabled ? '✅ Enabled' : '❌ Disabled';
          document.getElementById('batteryVoltage').textContent = (data.batteryVoltage || 0).toFixed(2) + ' V';
        })
        .catch(err => {
          console.error('Error fetching diagnostics:', err);
          showAlert('Failed to fetch diagnostics data', true);
        });
    }
    
    function resetFrameCounter() {
      if (!confirm('Are you sure you want to reset the frame counter? This should only be done after re-joining the network or if instructed by your network administrator.')) {
        return;
      }
      
      fetch('/api/reset-frame-counter', {
        method: 'POST'
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ Frame counter reset successfully');
            refreshStats();
          } else {
            showAlert('❌ Failed to reset frame counter: ' + data.message, true);
          }
        })
        .catch(err => {
          console.error('Error resetting frame counter:', err);
          showAlert('❌ Network error while resetting frame counter', true);
        });
    }
    
    // Initial load and auto-refresh every 5 seconds
    refreshStats();
    setInterval(refreshStats, 5000);
  </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });
  
  // Payload Info page
  server.on("/payload-info", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /payload-info page");
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Payload Info - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    h2 {
      color: #667eea;
      margin-bottom: 15px;
      font-size: 20px;
      border-bottom: 2px solid #667eea;
      padding-bottom: 10px;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-bottom: 20px;
    }
    th, td {
      padding: 12px;
      text-align: left;
      border-bottom: 1px solid #eee;
    }
    th {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      font-weight: 600;
    }
    tr:hover {
      background: #f8f9fa;
    }
    .code-block {
      background: #2d2d2d;
      color: #d4d4d4;
      padding: 15px;
      border-radius: 8px;
      font-family: 'Courier New', monospace;
      font-size: 14px;
      overflow-x: auto;
      margin-bottom: 15px;
    }
    .info-box {
      background: #e3f2fd;
      border-left: 4px solid #2196f3;
      padding: 15px;
      margin-bottom: 20px;
      border-radius: 4px;
    }
    .warning-box {
      background: #fff3e0;
      border-left: 4px solid #ff9800;
      padding: 15px;
      margin-bottom: 20px;
      border-radius: 4px;
    }
    .channel-badge {
      display: inline-block;
      padding: 4px 8px;
      border-radius: 4px;
      font-size: 12px;
      font-weight: bold;
      color: white;
    }
    .ch-temp { background: #f44336; }
    .ch-humid { background: #2196f3; }
    .ch-press { background: #4caf50; }
    .ch-alt { background: #ff9800; }
    .ch-gps { background: #9c27b0; }
    .ch-analog { background: #607d8b; }
    .ch-battery { background: #ffc107; color: #333; }
    .alert {
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .alert-success {
      background: #d4edda;
      border: 1px solid #c3e6cb;
      color: #155724;
    }
    .alert-error {
      background: #f8d7da;
      border: 1px solid #f5c6cb;
      color: #721c24;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v2.6.0</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link active">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <h1>📦 CayenneLPP Payload Documentation</h1>
      <p style="color: #666; margin-bottom: 20px;">Complete channel mapping and payload structure for LoRaWAN uplinks</p>
      
      <div class="info-box">
        <strong>ℹ️ Uplink Configuration</strong><br>
        Port: <code>1</code> | Format: <code>CayenneLPP</code> | Max Size: <code>51 bytes</code> (US915 DR0)
      </div>
      
      <!-- Combined Channel Map & Configuration -->
      <h2>📋 Channel Map & Configuration</h2>
      <p style="color: #666; margin-bottom: 15px;">Enable or disable individual channels to optimize payload size</p>
      
      <div id="alertBox" class="alert" style="display: none;"></div>
      
      <table>
        <thead>
          <tr>
            <th>Channel</th>
            <th>Type</th>
            <th>Data</th>
            <th>Unit</th>
            <th>Size</th>
            <th>Source</th>
            <th style="text-align: center;">Enabled</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><span class="channel-badge ch-temp">CH 1</span></td>
            <td>Temperature</td>
            <td>Ambient temperature</td>
            <td>°C</td>
            <td>4 bytes</td>
            <td>BME280</td>
            <td style="text-align: center;"><input type="checkbox" id="ch1" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-humid">CH 2</span></td>
            <td>Relative Humidity</td>
            <td>Relative humidity</td>
            <td>%</td>
            <td>4 bytes</td>
            <td>BME280</td>
            <td style="text-align: center;"><input type="checkbox" id="ch2" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-press">CH 3</span></td>
            <td>Barometric Pressure</td>
            <td>Atmospheric pressure</td>
            <td>hPa</td>
            <td>4 bytes</td>
            <td>BME280</td>
            <td style="text-align: center;"><input type="checkbox" id="ch3" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-alt">CH 4</span></td>
            <td>Analog Input</td>
            <td>Calculated altitude</td>
            <td>meters</td>
            <td>4 bytes</td>
            <td>BME280 (calculated)</td>
            <td style="text-align: center;"><input type="checkbox" id="ch4" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-gps">CH 5</span></td>
            <td>GPS</td>
            <td>Latitude, Longitude, Altitude</td>
            <td>degrees, meters</td>
            <td>11 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch5" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 6</span></td>
            <td>Analog Input</td>
            <td>HDOP (GPS accuracy)</td>
            <td>unitless</td>
            <td>4 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch6" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 7</span></td>
            <td>Analog Input</td>
            <td>Satellite count</td>
            <td>count</td>
            <td>4 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch7" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 8</span></td>
            <td>Analog Input</td>
            <td>GPS time (seconds of day)</td>
            <td>seconds</td>
            <td>4 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch8" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-battery">CH 9</span></td>
            <td>Analog Input</td>
            <td>Battery voltage</td>
            <td>volts</td>
            <td>4 bytes</td>
            <td>AXP192 PMU</td>
            <td style="text-align: center;"><input type="checkbox" id="ch9" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 10</span></td>
            <td>Analog Input</td>
            <td>PAX counter (WiFi + BLE devices)</td>
            <td>count</td>
            <td>4 bytes</td>
            <td>WiFi/BLE Scan</td>
            <td style="text-align: center;"><input type="checkbox" id="ch10" onchange="updateChannelConfig()"></td>
          </tr>
        </tbody>
      </table>
      
      <div style="margin-top: 20px;">
        <button onclick="saveChannelConfig()" style="padding: 10px 20px; background: #667eea; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px;">
          💾 Save Channel Configuration
        </button>
        <button onclick="loadChannelConfig()" style="padding: 10px 20px; background: #6c757d; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; margin-left: 10px;">
          🔄 Reload Configuration
        </button>
      </div>
      
      <p style="color: #999; font-size: 14px; margin-top: 15px;">
        💡 Tip: Disable unused channels to reduce payload size and save airtime. Changes take effect on the next uplink.
      </p>
      
      <div class="warning-box">
        <strong>⚠️ Important Limitations</strong><br>
        CayenneLPP Analog Input uses 2-byte signed integer with 0.01 resolution (range: -327.68 to 327.67).<br>
        • Channel 4 (altitude) may exceed range at high elevations<br>
        • Channel 8 (seconds-of-day: 0-86400) always exceeds range<br>
        Use alternative decoding or scaled values if needed.
      </div>
      
      <!-- Payload Example -->
      <h2>📄 Example Payload</h2>
      <div class="code-block">
01 67 00 E1    // CH1: Temperature = 22.5°C
02 68 50       // CH2: Humidity = 80%
03 73 03 F1    // CH3: Pressure = 1009 hPa
04 02 01 2C    // CH4: Altitude = 300m
05 88 06 76 5E FF F2 9E 00 00 64    // CH5: GPS (lat, lon, alt)
06 02 00 0F    // CH6: HDOP = 1.5
07 02 00 08    // CH7: Satellites = 8
08 02 4E 20    // CH8: Time = 20000s (overflow)
09 02 01 04    // CH9: Battery = 2.60V
      </div>
      
      <!-- Decoder -->
      <h2>🔧 JavaScript Decoder (TTN/ChirpStack)</h2>
      <div class="code-block">
function decodeUplink(input) {
  var data = {};
  var bytes = input.bytes;
  var i = 0;
  
  while (i < bytes.length) {
    var channel = bytes[i++];
    var type = bytes[i++];
    
    switch (type) {
      case 0x67: // Temperature
        data['temperature_' + channel] = 
          ((bytes[i] << 8) | bytes[i+1]) / 10.0;
        i += 2;
        break;
      case 0x68: // Humidity
        data['humidity_' + channel] = bytes[i] / 2.0;
        i += 1;
        break;
      case 0x73: // Barometric Pressure
        data['pressure_' + channel] = 
          ((bytes[i] << 8) | bytes[i+1]) / 10.0;
        i += 2;
        break;
      case 0x02: // Analog Input
        data['analog_' + channel] = 
          ((bytes[i] << 8) | bytes[i+1]) / 100.0;
        i += 2;
        break;
      case 0x88: // GPS
        data['latitude_' + channel] = 
          ((bytes[i] << 16) | (bytes[i+1] << 8) | bytes[i+2]) / 10000.0;
        data['longitude_' + channel] = 
          ((bytes[i+3] << 16) | (bytes[i+4] << 8) | bytes[i+5]) / 10000.0;
        data['altitude_' + channel] = 
          ((bytes[i+6] << 16) | (bytes[i+7] << 8) | bytes[i+8]) / 100.0;
        i += 9;
        break;
      default:
        i = bytes.length; // Unknown type, stop parsing
    }
  }
  
  return {
    data: data,
    warnings: [],
    errors: []
  };
}
      </div>
      
      <!-- Integration Guide -->
      <h2>🔗 Network Server Integration</h2>
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px;">
        <h3 style="margin-bottom: 10px;">The Things Network (TTN)</h3>
        <ol style="margin-left: 20px; line-height: 1.8;">
          <li>Go to your application in TTN Console</li>
          <li>Navigate to <strong>Payload Formatters</strong></li>
          <li>Select <strong>Custom JavaScript formatter</strong></li>
          <li>Paste the decoder code above</li>
          <li>Test with example payload</li>
        </ol>
        
        <h3 style="margin: 20px 0 10px 0;">ChirpStack</h3>
        <ol style="margin-left: 20px; line-height: 1.8;">
          <li>Go to <strong>Device Profile</strong></li>
          <li>Select <strong>Codec</strong> tab</li>
          <li>Choose <strong>Custom JavaScript codec functions</strong></li>
          <li>Paste decoder in <strong>Decode function</strong></li>
          <li>Save and test</li>
        </ol>
      </div>
    </div>
    
    <!-- PAX Counter Settings -->
    <div class="card">
      <h2>👥 PAX Counter Settings</h2>
      <p style="color: #666; margin-bottom: 15px;">Configure crowd counting via WiFi and BLE device scanning</p>
      
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px;">
        <div style="margin-bottom: 20px;">
          <label style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
            <input type="checkbox" id="paxEnabled" onchange="updatePaxSettings()" style="width: 20px; height: 20px;">
            <span style="font-weight: 600;">Enable PAX Counter</span>
          </label>
          <p style="color: #666; font-size: 14px; margin-top: 5px; margin-left: 30px;">
            Scan for nearby WiFi access points and BLE devices to estimate crowd size
          </p>
        </div>
        
        <div style="margin-bottom: 20px;">
          <label style="font-weight: 600; display: block; margin-bottom: 10px;">Scanning Methods:</label>
          <div style="margin-left: 20px;">
            <label style="display: flex; align-items: center; gap: 10px; cursor: pointer; margin-bottom: 10px;">
              <input type="checkbox" id="paxWifiEnabled" checked style="width: 18px; height: 18px;">
              <span>📶 WiFi Scanning</span>
            </label>
            <p style="color: #666; font-size: 13px; margin-left: 28px; margin-bottom: 15px;">
              Detect WiFi-enabled devices (smartphones, tablets, laptops)
            </p>
            
            <label style="display: flex; align-items: center; gap: 10px; cursor: pointer; margin-bottom: 10px;">
              <input type="checkbox" id="paxBleEnabled" checked style="width: 18px; height: 18px;">
              <span>📡 BLE Scanning</span>
            </label>
            <p style="color: #666; font-size: 13px; margin-left: 28px;">
              Detect Bluetooth Low Energy devices (wearables, beacons, smartphones)
            </p>
          </div>
        </div>
        
        <div style="margin-bottom: 20px;">
          <label style="font-weight: 600; display: block; margin-bottom: 5px;">Scan Interval (seconds):</label>
          <input type="number" id="paxInterval" value="60" min="30" max="3600" style="padding: 8px; border: 1px solid #ddd; border-radius: 5px; width: 150px;">
          <p style="color: #666; font-size: 14px; margin-top: 5px;">
            How often to perform WiFi/BLE scans (30-3600 seconds)
          </p>
        </div>
        
        <div style="background: #e3f2fd; border-left: 4px solid #2196f3; padding: 15px; margin-bottom: 20px; border-radius: 4px;">
          <strong>ℹ️ Current PAX Count</strong><br>
          <div style="margin-top: 10px; font-size: 18px;">
            WiFi Devices: <span id="wifiCount" style="font-weight: bold;">-</span><br>
            BLE Devices: <span id="bleCount" style="font-weight: bold;">-</span><br>
            Total: <span id="totalPax" style="font-weight: bold; color: #667eea;">-</span>
          </div>
        </div>
        
        <button onclick="savePaxSettings()" style="padding: 10px 20px; background: #667eea; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px;">
          💾 Save PAX Settings
        </button>
        <button onclick="loadPaxSettings()" style="padding: 10px 20px; background: #6c757d; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; margin-left: 10px;">
          🔄 Reload Settings
        </button>
      </div>
      
      <div style="background: #fff3e0; border-left: 4px solid #ff9800; padding: 15px; margin-top: 20px; border-radius: 4px;">
        <strong>⚠️ Privacy Note</strong><br>
        PAX counter only counts the number of devices, not their identities. No personal data is collected or transmitted.
      </div>
    </div>
  </div>
  
  <script>
    function showAlert(message, isError = false) {
      const alertBox = document.getElementById('alertBox');
      alertBox.textContent = message;
      alertBox.className = 'alert ' + (isError ? 'alert-error' : 'alert-success');
      alertBox.style.display = 'block';
      setTimeout(() => {
        alertBox.style.display = 'none';
      }, 5000);
    }
    
    function loadChannelConfig() {
      fetch('/api/channel-config')
        .then(response => response.json())
        .then(data => {
          for (let i = 0; i < 10; i++) {
            const checkbox = document.getElementById('ch' + (i + 1));
            if (checkbox && data.channels && data.channels[i] !== undefined) {
              checkbox.checked = data.channels[i];
            }
          }
        })
        .catch(err => {
          console.error('Error loading channel config:', err);
          showAlert('Failed to load channel configuration', true);
        });
    }
    
    function updateChannelConfig() {
      // Just visual feedback, actual save happens when user clicks Save button
    }
    
    function saveChannelConfig() {
      const channels = [];
      for (let i = 0; i < 10; i++) {
        const checkbox = document.getElementById('ch' + (i + 1));
        channels.push(checkbox ? checkbox.checked : true);
      }
      
      fetch('/api/channel-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({channels: channels})
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ Channel configuration saved successfully');
          } else {
            showAlert('❌ Failed to save: ' + data.message, true);
          }
        })
        .catch(err => {
          console.error('Error saving channel config:', err);
          showAlert('❌ Network error while saving', true);
        });
    }
    
    function loadPaxSettings() {
      fetch('/api/pax-config')
        .then(response => response.json())
        .then(data => {
          document.getElementById('paxEnabled').checked = data.enabled || false;
          document.getElementById('paxWifiEnabled').checked = data.wifiEnabled !== undefined ? data.wifiEnabled : true;
          document.getElementById('paxBleEnabled').checked = data.bleEnabled !== undefined ? data.bleEnabled : true;
          document.getElementById('paxInterval').value = data.interval || 60;
          document.getElementById('wifiCount').textContent = data.wifiCount || 0;
          document.getElementById('bleCount').textContent = data.bleCount || 0;
          document.getElementById('totalPax').textContent = data.totalCount || 0;
        })
        .catch(err => {
          console.error('Error loading PAX config:', err);
          showAlert('Failed to load PAX configuration', true);
        });
    }
    
    function updatePaxSettings() {
      // Just visual feedback
    }
    
    function savePaxSettings() {
      const enabled = document.getElementById('paxEnabled').checked;
      const wifiEnabled = document.getElementById('paxWifiEnabled').checked;
      const bleEnabled = document.getElementById('paxBleEnabled').checked;
      const interval = parseInt(document.getElementById('paxInterval').value);
      
      if (interval < 30 || interval > 3600) {
        showAlert('❌ Scan interval must be between 30 and 3600 seconds', true);
        return;
      }
      
      if (!wifiEnabled && !bleEnabled) {
        showAlert('❌ At least one scanning method (WiFi or BLE) must be enabled', true);
        return;
      }
      
      fetch('/api/pax-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          enabled: enabled,
          wifiEnabled: wifiEnabled,
          bleEnabled: bleEnabled,
          interval: interval
        })
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ PAX counter settings saved successfully');
            loadPaxSettings(); // Reload to show current counts
          } else {
            showAlert('❌ Failed to save: ' + data.message, true);
          }
        })
        .catch(err => {
          console.error('Error saving PAX config:', err);
          showAlert('❌ Network error while saving', true);
        });
    }
    
    // Load configurations on page load
    loadChannelConfig();
    loadPaxSettings();
    
    // Auto-refresh PAX counts every 10 seconds
    setInterval(loadPaxSettings, 10000);
  </script>
)rawliteral";
    request->send(200, "text/html", html);
  });
  
  // About page - Developer information
  server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /about page");
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>About - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .container {
      max-width: 900px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 20px;
      font-size: 32px;
    }
    h2 {
      color: #667eea;
      margin: 25px 0 15px 0;
      font-size: 22px;
    }
    p {
      color: #555;
      line-height: 1.8;
      margin-bottom: 15px;
    }
    .profile {
      display: flex;
      align-items: center;
      gap: 20px;
      margin-bottom: 30px;
      padding: 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      border-radius: 10px;
      color: white;
    }
    .profile-icon {
      font-size: 60px;
    }
    .profile-info h2 {
      color: white;
      margin: 0 0 10px 0;
    }
    .profile-info p {
      color: rgba(255,255,255,0.9);
      margin: 0;
    }
    .links {
      display: flex;
      gap: 15px;
      flex-wrap: wrap;
      margin-top: 20px;
    }
    .link-btn {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 10px 20px;
      background: #667eea;
      color: white;
      text-decoration: none;
      border-radius: 8px;
      transition: all 0.3s;
      font-weight: 600;
    }
    .link-btn:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
    }
    .project-list {
      list-style: none;
      padding: 0;
    }
    .project-list li {
      padding: 15px;
      margin-bottom: 10px;
      background: #f8f9fa;
      border-left: 4px solid #667eea;
      border-radius: 5px;
    }
    .project-list li strong {
      color: #667eea;
    }
    .motto {
      font-style: italic;
      color: #764ba2;
      font-size: 18px;
      text-align: center;
      padding: 20px;
      background: #f8f9fa;
      border-radius: 10px;
      margin: 20px 0;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v2.6.0</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/about" class="nav-link active">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <div class="profile">
        <div class="profile-icon">👨‍💻</div>
        <div class="profile-info">
          <h2>Markus van Kempen</h2>
          <p>Developer & Open Source Contributor</p>
          <p>📍 Toronto, Canada</p>
        </div>
      </div>
      
      <div class="motto">
        "No bug too small, no syntax too weird."
      </div>
      
      <h2>About the Developer</h2>
      <p>
        I have a keen interest in debating and predicting The Next Big Thing in technology.
        Based in Toronto, I build open-source developer tools that bridge enterprise platforms
        like IBM Watsonx Orchestrate and Maximo with modern AI-assisted development workflows.
      </p>
      
      <p>
        These tools are part of an open-source effort to make enterprise API development faster,
        more accessible, and AI-ready. Whether you use VS Code, Cursor, Claude, or ChatGPT,
        there's a tool here for you.
      </p>
      
      <p>
        All projects are released under the <strong>Apache 2.0 license</strong>. Contributions,
        issues, and feedback are welcome.
      </p>
      
      <h2>Key Projects</h2>
      <ul class="project-list">
        <li><strong>TTGO T-Beam Sensor Node</strong> - IoT sensor platform with LoRaWAN, GPS, and web dashboard</li>
        <li><strong>Enterprise API Tools</strong> - Developer tools for IBM Watsonx and Maximo integration</li>
        <li><strong>AI-Assisted Development</strong> - Tools for modern AI-powered coding workflows</li>
      </ul>
      
      <h2>Connect</h2>
      <div class="links">
        <a href="https://markusvankempen.github.io/" class="link-btn" target="_blank">
          🌐 Website
        </a>
        <a href="https://github.com/markusvankempen" class="link-btn" target="_blank">
          💻 GitHub
        </a>
        <a href="https://github.com/markusvankempen" class="link-btn" target="_blank">
          📦 Main Project Repo
        </a>
        <a href="https://markusvankempen.github.io/" class="link-btn" target="_blank">
          📝 Blog
        </a>
      </div>
      
      <h2>About This Project</h2>
      <p>
        This TTGO T-Beam firmware is a comprehensive IoT sensor node featuring:
      </p>
      <ul style="margin-left: 20px; color: #555; line-height: 1.8;">
        <li>LoRaWAN connectivity (OTAA & ABP)</li>
        <li>GPS tracking with real-time mapping</li>
        <li>BME280 environmental sensors</li>
        <li>WiFi web dashboard with full configuration</li>
        <li>PAX counter for crowd monitoring</li>
        <li>Configurable payload channels</li>
        <li>Power management and sleep modes</li>
        <li>Comprehensive diagnostics and debugging</li>
      </ul>
      
      <p style="margin-top: 20px;">
        <strong>License:</strong> Apache License 2.0<br>
        <strong>Version:</strong> 2.6.0<br>
        <strong>Organization:</strong> Research | Floor 7½ 🏢🤏
      </p>
    </div>
  </div>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });
  
  server.begin();
  Serial.println("Web server started on port 80");
}

/**
 * @brief Initialize WiFi connection with WiFiManager
 *
 * Handles WiFi connectivity with automatic fallback to AP mode:
 * 1. Checks if boot button (GPIO 0) is pressed for manual reset
 * 2. Attempts to connect using saved credentials
 * 3. If no credentials or connection fails, starts AP mode
 *
 * WiFi Manager Features:
 * - Auto-connect to saved network
 * - Captive portal for configuration
 * - Network scanning and selection
 * - Password entry
 * - Persistent credential storage
 *
 * Callbacks:
 * - setAPCallback: Called when entering config mode
 * - setSaveConfigCallback: Called when credentials saved
 *
 * @see setupWebServer() for web server initialization
 * @see WIFI_AP_NAME and WIFI_AP_PASSWORD constants
 */
void setupWiFi() {
  Serial.println("=== WiFi Setup ===");
  
  // Check if boot button is pressed for WiFi reset
  pinMode(WIFI_RESET_BUTTON_PIN, INPUT);
  if (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
    Serial.println("Boot button pressed - resetting WiFi settings");
    wifiManager.resetSettings();
    delay(1000);
  }
  
  // Set WiFi mode
  WiFi.mode(WIFI_STA);
  
  // Configure WiFiManager
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT_MS / 1000);
  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.print("AP Name: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    wifiConfigMode = true;
  });
  
  wifiManager.setSaveConfigCallback([]() {
    Serial.println("WiFi credentials saved");
    wifiConfigMode = false;
  });
  
  // Try to connect or start config portal
  if (!wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD)) {
    Serial.println("Failed to connect and timeout reached");
    wifiConnected = false;
  } else {
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    setupWebServer();
  }
}

/**
 * @brief Process serial commands for WiFi management
 *
 * Monitors serial input for user commands to control WiFi settings.
 * Commands are case-sensitive and should be terminated with newline.
 *
 * Available Commands:
 * - wifi-reset  : Resets WiFi credentials and restarts device
 * - wifi-status : Displays current WiFi connection status and IP
 * - help        : Shows list of available commands
 *
 * @note Commands are processed in main loop()
 * @note Serial baud rate: 115200
 * @note Commands must be followed by newline character
 *
 * Usage Examples:
 *   wifi-reset    -> Clears saved WiFi and reboots
 *   wifi-status   -> Shows: Connected: YES, IP: 192.168.1.100, SSID: MyNetwork
 *   help          -> Displays command reference
 *
 * @see setupWiFi() for WiFi initialization
 */
void printSerialMenu() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║          TTGO T-Beam Configuration Menu                   ║");
  Serial.println("║  Author: Markus van Kempen | Floor 7½ 🏢🤏                ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println("\n📡 WiFi Commands:");
  Serial.println("  wifi-status    - Show WiFi connection status");
  Serial.println("  wifi-on        - Enable WiFi and web server");
  Serial.println("  wifi-off       - Disable WiFi (saves power)");
  Serial.println("  wifi-reset     - Reset WiFi credentials and restart");
  Serial.println("\n🔐 LoRaWAN Configuration:");
  Serial.println("  lora-keys      - Display current LoRa keys");
  Serial.println("  lora-appeui <hex> - Set APPEUI (8 bytes, e.g., 0011223344556677)");
  Serial.println("  lora-deveui <hex> - Set DEVEUI (8 bytes)");
  Serial.println("  lora-appkey <hex> - Set APPKEY (16 bytes)");
  Serial.println("  lora-save      - Save keys to NVS memory");
  Serial.println("  lora-clear     - Clear keys from NVS (use defaults)");
  Serial.println("  lora-interval <sec> - Set LoRa send interval (seconds)");
  Serial.println("\n🛰️  GPS & Power Management:");
  Serial.println("  gps-on         - Enable GPS");
  Serial.println("  gps-off        - Disable GPS (power saving)");
  Serial.println("  gps-status     - Show GPS enable status");
  Serial.println("  sleep-on       - Enable sleep mode");
  Serial.println("  sleep-off      - Disable sleep mode");
  Serial.println("  sleep-interval <sec> - Set sleep wake interval (seconds)");
  Serial.println("  sleep-status   - Show sleep mode status");
  Serial.println("\n📺 Display Commands:");
  Serial.println("  display-on     - Enable OLED display");
  Serial.println("  display-off    - Disable OLED display (power saving)");
  Serial.println("  display-status - Show display enable status");
  Serial.println("\n📊 PAX Counter Commands:");
  Serial.println("  pax-on         - Enable PAX counter");
  Serial.println("  pax-off        - Disable PAX counter");
  Serial.println("  pax-wifi-on    - Enable WiFi scanning");
  Serial.println("  pax-wifi-off   - Disable WiFi scanning");
  Serial.println("  pax-ble-on     - Enable BLE scanning");
  Serial.println("  pax-ble-off    - Disable BLE scanning");
  Serial.println("  pax-interval <sec> - Set scan interval (30-3600 seconds)");
  Serial.println("  pax-status     - Show PAX counter status");
  Serial.println("\n📡 Payload Channel Commands:");
  Serial.println("  channel-enable <1-10>  - Enable payload channel");
  Serial.println("  channel-disable <1-10> - Disable payload channel");
  Serial.println("  channel-status - Show all channel states");
  Serial.println("\n🔧 System Commands:");
  Serial.println("  status         - Show complete system status");
  Serial.println("  debug-on       - Enable verbose debugging");
  Serial.println("  debug-off      - Disable verbose debugging");
  Serial.println("  restart        - Restart device");
  Serial.println("  menu           - Show this menu");
  Serial.println("  help           - Show quick help");
  Serial.println("\n💡 Quick Start:");
  Serial.println("  1. Type 'status' to see current system state");
  Serial.println("  2. Type 'wifi-status' to check WiFi connection");
  Serial.println("  3. Type 'lora-keys' to view LoRaWAN configuration");
  Serial.println("════════════════════════════════════════════════════════════\n");
}

void printSystemStatus() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║                    System Status                           ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  
  // WiFi Status
  Serial.println("\n📡 WiFi:");
  Serial.print("  Status: ");
  Serial.println(wifiConnected ? "✓ Connected" : "✗ Disconnected");
  if (wifiConnected) {
    Serial.print("  SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("  IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  }
  
  // GPS Status
  Serial.println("\n🛰️  GPS:");
  Serial.print("  Enabled: ");
  Serial.println(gpsEnabled ? "✓ Yes" : "✗ No (Power Saving)");
  Serial.print("  Fix: ");
  Serial.println(gps.location.isValid() ? "✓ Valid" : "✗ No Fix");
  if (gps.location.isValid()) {
    Serial.print("  Satellites: ");
    Serial.println(gps.satellites.value());
    Serial.print("  HDOP: ");
    Serial.println(gps.hdop.hdop());
    Serial.print("  Location: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", ");
    Serial.println(gps.location.lng(), 6);
  }
  
  // LoRaWAN Status
  Serial.println("\n📶 LoRaWAN:");
  Serial.print("  Joined: ");
  Serial.println(loraJoined ? "✓ Yes" : "✗ No");
  Serial.print("  Uplinks: ");
  Serial.println(uplinkCounter);
  Serial.print("  Send Interval: ");
  Serial.print(loraSendIntervalSec);
  Serial.print(" sec (");
  Serial.print(loraSendIntervalSec / 60);
  Serial.println(" min)");
  Serial.print("  Keys Configured: ");
  Serial.println(loraKeysConfigured ? "✓ Yes (NVS)" : "✗ No (Defaults)");
  
  // Power Management
  Serial.println("\n💤 Power Management:");
  Serial.print("  Sleep Mode: ");
  Serial.println(sleepModeEnabled ? "✓ Enabled" : "✗ Disabled");
  if (sleepModeEnabled) {
    Serial.print("  Wake Interval: ");
    Serial.print(sleepWakeInterval);
    Serial.print(" sec (");
    Serial.print(sleepWakeInterval / 3600);
    Serial.println(" hours)");
    unsigned long timeUntilSleep = (sleepWakeInterval * 1000) - (millis() - lastWakeTime);
    Serial.print("  Next Sleep: ");
    Serial.print(timeUntilSleep / 1000);
    Serial.println(" seconds");
  }
  
  // Sensors
  Serial.println("\n🌡️  Sensors:");
  Serial.print("  BME280: ");
  Serial.println(bmeFound ? "✓ Found" : "✗ Not Found");
  if (bmeFound) {
    Serial.print("  Temperature: ");
    Serial.print(lastTemperature, 1);
    Serial.println(" °C");
    Serial.print("  Humidity: ");
    Serial.print(lastHumidity, 1);
    Serial.println(" %");
    Serial.print("  Pressure: ");
    Serial.print(lastPressureHpa, 1);
    Serial.println(" hPa");
  }
  
  // System
  Serial.println("\n💾 System:");
  Serial.print("  Firmware: v");
  Serial.print(FIRMWARE_VERSION);
  Serial.print(" (");
  Serial.print(FIRMWARE_DATE);
  Serial.println(")");
  Serial.print("  Uptime: ");
  unsigned long uptimeSec = millis() / 1000;
  Serial.print(uptimeSec / 86400);
  Serial.print("d ");
  Serial.print((uptimeSec % 86400) / 3600);
  Serial.print("h ");
  Serial.print((uptimeSec % 3600) / 60);
  Serial.println("m");
  Serial.print("  Free Heap: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.print("  Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("  CPU Freq: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  
  Serial.println("════════════════════════════════════════════════════════════\n");
}

void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "menu") {
      printSerialMenu();
      
    } else if (cmd == "status") {
      printSystemStatus();
      
    } else if (cmd == "wifi-on") {
      if (!wifiEnabled) {
        wifiEnabled = true;
        Serial.println("✓ WiFi enabled. Reconnecting...");
        WiFi.mode(WIFI_STA);
        WiFi.begin();
      } else {
        Serial.println("ℹ WiFi already enabled");
      }
      
    } else if (cmd == "wifi-off") {
      if (wifiEnabled) {
        wifiEnabled = false;
        Serial.println("✓ WiFi disabled (power saving mode)");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
      } else {
        Serial.println("ℹ WiFi already disabled");
      }
      
    } else if (cmd == "wifi-reset") {
      Serial.println("⚠ Resetting WiFi settings...");
      wifiManager.resetSettings();
      delay(1000);
      ESP.restart();
      
    } else if (cmd == "wifi-status") {
      Serial.println("\n📡 WiFi Status:");
      Serial.print("  Enabled: ");
      Serial.println(wifiEnabled ? "Yes" : "No");
      Serial.print("  Connected: ");
      Serial.println(wifiConnected ? "Yes" : "No");
      if (wifiConnected) {
        Serial.print("  SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("  IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("  Signal: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        Serial.print("  Dashboard: http://");
        Serial.println(WiFi.localIP());
      }
      Serial.println();
      
    } else if (cmd == "debug-on") {
      debugEnabled = true;
      Serial.println("✓ Verbose debugging enabled");
      
    } else if (cmd == "debug-off") {
      debugEnabled = false;
      Serial.println("✓ Verbose debugging disabled");
      
    } else if (cmd == "lora-keys") {
      printLoRaKeys();
      
    } else if (cmd.startsWith("lora-appeui ")) {
      String hexStr = cmd.substring(12);
      if (hexStringToBytes(hexStr, storedAPPEUI, 8)) {
        Serial.println("✓ APPEUI updated (not saved yet)");
        Serial.print("  New APPEUI: ");
        Serial.println(bytesToHexString(storedAPPEUI, 8, ":"));
        Serial.println("  Use 'lora-save' to persist to NVS");
      } else {
        Serial.println("✗ ERROR: Invalid APPEUI format");
        Serial.println("  Usage: lora-appeui 0102030405060708");
      }
      
    } else if (cmd.startsWith("lora-deveui ")) {
      String hexStr = cmd.substring(12);
      if (hexStringToBytes(hexStr, storedDEVEUI, 8)) {
        Serial.println("✓ DEVEUI updated (not saved yet)");
        Serial.print("  New DEVEUI: ");
        Serial.println(bytesToHexString(storedDEVEUI, 8, ":"));
        Serial.println("  Use 'lora-save' to persist to NVS");
      } else {
        Serial.println("✗ ERROR: Invalid DEVEUI format");
        Serial.println("  Usage: lora-deveui 0102030405060708");
      }
      
    } else if (cmd.startsWith("lora-appkey ")) {
      String hexStr = cmd.substring(12);
      if (hexStringToBytes(hexStr, storedAPPKEY, 16)) {
        Serial.println("✓ APPKEY updated (not saved yet)");
        Serial.print("  New APPKEY: ");
        Serial.println(bytesToHexString(storedAPPKEY, 16, ":"));
        Serial.println("  Use 'lora-save' to persist to NVS");
      } else {
        Serial.println("✗ ERROR: Invalid APPKEY format");
        Serial.println("  Usage: lora-appkey 0102030405060708090A0B0C0D0E0F10");
      }
      
    } else if (cmd == "lora-save") {
      if (saveLoRaKeysToNVS()) {
        Serial.println("✓ LoRa keys saved to NVS");
        Serial.println("⚠ Restart required for changes to take effect");
        Serial.println("  Use 'restart' command to reboot");
      } else {
        Serial.println("✗ ERROR: Failed to save LoRa keys");
      }
      
    } else if (cmd == "lora-clear") {
      clearLoRaKeysFromNVS();
      Serial.println("✓ LoRa keys cleared from NVS");
      Serial.println("⚠ Restart required to load defaults");
      
    } else if (cmd.startsWith("gps-")) {
      if (cmd == "gps-on") {
        gpsEnabled = true;
        enableGpsPower();  // Power up GPS hardware
      } else if (cmd == "gps-off") {
        gpsEnabled = false;
        disableGpsPower();  // Power down GPS hardware
      } else if (cmd == "gps-status") {
        Serial.print("GPS: ");
        Serial.println(gpsEnabled ? "Enabled" : "Disabled");
        if (axpFound) {
          uint8_t powerReg = 0;
          if (axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, powerReg)) {
            Serial.print("  AXP192 LDO3 (GPS Power): ");
            Serial.println((powerReg & 0x08) ? "ON" : "OFF");
          }
        }
      }
      
    } else if (cmd.startsWith("display-")) {
      if (cmd == "display-on") {
        enableDisplay();
      } else if (cmd == "display-off") {
        disableDisplay();
      } else if (cmd == "display-status") {
        Serial.print("Display: ");
        Serial.println(displayEnabled ? "Enabled" : "Disabled");
      }
      
    } else if (cmd.startsWith("sleep-")) {
      if (cmd == "sleep-on") {
        sleepModeEnabled = true;
        lastWakeTime = millis();
        Serial.print("✓ Sleep mode enabled, wake every ");
        Serial.print(sleepWakeInterval);
        Serial.println(" seconds");
      } else if (cmd == "sleep-off") {
        sleepModeEnabled = false;
        Serial.println("✓ Sleep mode disabled");
      } else if (cmd.startsWith("sleep-interval ")) {
        String intervalStr = cmd.substring(15);
        uint32_t interval = intervalStr.toInt();
        if (interval > 0) {
          sleepWakeInterval = interval;
          Serial.print("✓ Sleep interval set to ");
          Serial.print(sleepWakeInterval);
          Serial.print(" seconds (");
          Serial.print(sleepWakeInterval / 3600);
          Serial.println(" hours)");
        } else {
          Serial.println("✗ Invalid interval");
        }
      } else if (cmd == "sleep-status") {
        Serial.print("Sleep Mode: ");
        Serial.println(sleepModeEnabled ? "Enabled" : "Disabled");
        if (sleepModeEnabled) {
          Serial.print("  Wake Interval: ");
          Serial.print(sleepWakeInterval);
          Serial.print(" seconds (");
          Serial.print(sleepWakeInterval / 3600);
          Serial.println(" hours)");
        }
      }
      
    } else if (cmd.startsWith("lora-interval ")) {
      String intervalStr = cmd.substring(14);
      uint32_t interval = intervalStr.toInt();
      if (interval >= 10) {
        loraSendIntervalSec = interval;
        Serial.print("✓ LoRa send interval set to ");
        Serial.print(loraSendIntervalSec);
        Serial.print(" seconds (");
        Serial.print(loraSendIntervalSec / 60);
        Serial.println(" minutes)");
      } else {
        Serial.println("✗ Interval must be >= 10 seconds");
      }
      
    } else if (cmd.startsWith("pax-")) {
      if (cmd == "pax-on") {
        paxCounterEnabled = true;
        channelEnabled[9] = true; // Enable channel 10
        // Initialize BLE scanner if BLE scanning is enabled
        if (paxBleScanEnabled && pBLEScan == nullptr) {
          Serial.println("Initializing BLE scanner...");
          NimBLEDevice::init("");
          pBLEScan = NimBLEDevice::getScan();
          pBLEScan->setActiveScan(false);
          pBLEScan->setInterval(100);
          pBLEScan->setWindow(99);
          Serial.println("BLE scanner initialized");
        }
        saveSystemSettings();
        Serial.println("✓ PAX counter enabled");
      } else if (cmd == "pax-off") {
        paxCounterEnabled = false;
        channelEnabled[9] = false; // Disable channel 10
        saveSystemSettings();
        Serial.println("✓ PAX counter disabled");
      } else if (cmd == "pax-wifi-on") {
        paxWifiScanEnabled = true;
        saveSystemSettings();
        Serial.println("✓ PAX WiFi scanning enabled");
      } else if (cmd == "pax-wifi-off") {
        paxWifiScanEnabled = false;
        if (!paxBleScanEnabled) {
          Serial.println("⚠ Warning: Both WiFi and BLE scanning disabled!");
        }
        saveSystemSettings();
        Serial.println("✓ PAX WiFi scanning disabled");
      } else if (cmd == "pax-ble-on") {
        paxBleScanEnabled = true;
        // Initialize BLE scanner if PAX counter is enabled
        if (paxCounterEnabled && pBLEScan == nullptr) {
          Serial.println("Initializing BLE scanner...");
          NimBLEDevice::init("");
          pBLEScan = NimBLEDevice::getScan();
          pBLEScan->setActiveScan(false);
          pBLEScan->setInterval(100);
          pBLEScan->setWindow(99);
          Serial.println("BLE scanner initialized");
        }
        saveSystemSettings();
        Serial.println("✓ PAX BLE scanning enabled");
      } else if (cmd == "pax-ble-off") {
        paxBleScanEnabled = false;
        if (!paxWifiScanEnabled) {
          Serial.println("⚠ Warning: Both WiFi and BLE scanning disabled!");
        }
        saveSystemSettings();
        Serial.println("✓ PAX BLE scanning disabled");
      } else if (cmd.startsWith("pax-interval ")) {
        String intervalStr = cmd.substring(13);
        uint32_t interval = intervalStr.toInt();
        if (interval >= 30 && interval <= 3600) {
          paxScanInterval = interval * 1000; // Convert to ms
          saveSystemSettings();
          Serial.print("✓ PAX scan interval set to ");
          Serial.print(interval);
          Serial.println(" seconds");
        } else {
          Serial.println("✗ Interval must be 30-3600 seconds");
        }
      } else if (cmd == "pax-status") {
        Serial.println("\n📊 PAX Counter Status:");
        Serial.print("  Enabled: ");
        Serial.println(paxCounterEnabled ? "✓ Yes" : "✗ No");
        Serial.print("  WiFi Scanning: ");
        Serial.println(paxWifiScanEnabled ? "✓ Enabled" : "✗ Disabled");
        Serial.print("  BLE Scanning: ");
        Serial.println(paxBleScanEnabled ? "✓ Enabled" : "✗ Disabled");
        Serial.print("  Scan Interval: ");
        Serial.print(paxScanInterval / 1000);
        Serial.println(" seconds");
        Serial.print("  WiFi Devices: ");
        Serial.println(wifiDeviceCount);
        Serial.print("  BLE Devices: ");
        Serial.println(bleDeviceCount);
        Serial.print("  Total Count: ");
        Serial.println(totalPaxCount);
        Serial.println();
      }
      
    } else if (cmd.startsWith("channel-")) {
      if (cmd.startsWith("channel-enable ")) {
        String channelStr = cmd.substring(15);
        int channel = channelStr.toInt();
        if (channel >= 1 && channel <= 10) {
          channelEnabled[channel - 1] = true;
          Serial.print("✓ Channel ");
          Serial.print(channel);
          Serial.println(" enabled");
        } else {
          Serial.println("✗ Channel must be 1-10");
        }
      } else if (cmd.startsWith("channel-disable ")) {
        String channelStr = cmd.substring(16);
        int channel = channelStr.toInt();
        if (channel >= 1 && channel <= 10) {
          channelEnabled[channel - 1] = false;
          Serial.print("✓ Channel ");
          Serial.print(channel);
          Serial.println(" disabled");
        } else {
          Serial.println("✗ Channel must be 1-10");
        }
      } else if (cmd == "channel-status") {
        Serial.println("\n📡 Payload Channel Status:");
        const char* channelNames[] = {
          "Temperature", "Humidity", "Pressure", "Altitude",
          "GPS", "HDOP", "Satellites", "GPS Time",
          "Battery", "PAX Counter"
        };
        for (int i = 0; i < 10; i++) {
          Serial.print("  Ch");
          Serial.print(i + 1);
          Serial.print(" (");
          Serial.print(channelNames[i]);
          Serial.print("): ");
          Serial.println(channelEnabled[i] ? "✓ Enabled" : "✗ Disabled");
        }
        Serial.println();
      }
      
    } else if (cmd == "restart") {
      Serial.println("⚠ Restarting device in 2 seconds...");
      delay(2000);
      ESP.restart();
      
    } else if (cmd == "help") {
      Serial.println("\n📖 Quick Help:");
      Serial.println("  menu         - Show full interactive menu");
      Serial.println("  status       - Show system status");
      Serial.println("  wifi-status  - Check WiFi connection");
      Serial.println("  lora-keys    - View LoRa configuration");
      Serial.println("\nType 'menu' for complete command list\n");
      
    } else if (cmd.length() > 0) {
      Serial.print("✗ Unknown command: '");
      Serial.print(cmd);
      Serial.println("'");
      Serial.println("Type 'menu' for available commands or 'help' for quick help\n");
    }
  }
}

// One-time platform initialization sequence.
// Order matters: power rails -> display -> GPS -> sensors -> LoRa join.
void setup() {
  Serial.begin(115200);
  delay(300);
  
  // Print welcome banner
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════════╗");
  Serial.println("║          TTGO T-Beam T22 V1.1 Sensor Node                 ║");
  Serial.println("║  Author: Markus van Kempen | Floor 7½ 🏢🤏                ║");
  Serial.println("║  Email: markus.van.kempen@gmail.com                       ║");
  Serial.print("║  Version: ");
  Serial.print(FIRMWARE_VERSION);
  Serial.print(" | ");
  Serial.print(FIRMWARE_DATE);
  Serial.println("                          ║");
  Serial.println("║  License: Apache 2.0                                       ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println("\n💡 Type 'menu' for interactive configuration menu");
  Serial.println("💡 Type 'help' for quick command reference");
  Serial.println("💡 Type 'status' to see system status\n");
  
  // Initialize debug buffer with startup message
  logDebug("🚀 TTGO T-Beam T22 V1.1 starting...");
  logDebug("Version: " + String(FIRMWARE_VERSION) + " (" + String(FIRMWARE_DATE) + ")");
  
  setenv("TZ", "UTC0", 1);
  tzset();
  
  // Load system settings from NVS
  loadSystemSettings();
  
  // Load LoRaWAN keys from NVS
  loadLoRaKeysFromNVS();
  printLoRaKeys();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  rtcChipFound = i2cDevicePresent(RTC_I2C_ADDR);
  Serial.print("RTC I2C 0x51: ");
  Serial.println(rtcChipFound ? "FOUND" : "NOT FOUND");
  Serial.print("I2C clock Hz: ");
  Serial.println(I2C_CLOCK_HZ);
  setupAxp192PowerForGps();
  logAxp192Detailed();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("SSD1306 init failed");
    while (true) {
      delay(1000);
    }
  }

  // T22 V1.1 20191212 GPS initialization - GPIO reset disabled to test LED issue
  Serial.println("=== T22 V1.1 20191212 GPS INITIALIZATION ===");
  delay(500);  // Allow AXP192 power to stabilize
  
  // Ensure GPS power is enabled (in case NVS had it disabled)
  if (gpsEnabled && axpFound) {
    enableGpsPower();
  }
  
  // resetGpsModuleViaGpio();  // DISABLED - was preventing GPS LED from turning on
  // delay(1000);
  
  startGpsSerial(0);
  gpsRawDebugStartAt = millis();
  if (GPS_RAW_NMEA_DEBUG_ENABLED) {
    Serial.print("GPS raw NMEA debug enabled for ");
    Serial.print(GPS_RAW_NMEA_DEBUG_MS / 1000);
    Serial.println("s");
  }
  logSecretsAndConfig();

  bmeFound = initBme();
  showBootScreen();

  if (!bmeFound) {
    Serial.println("BME280 not detected on 0x76 or 0x77");
  }

  setupLoRa();
  
  // Setup WiFi and Web Server
  setupWiFi();
  
  // Initialize BLE for PAX counter if enabled and BLE scanning is enabled
  if (paxCounterEnabled && paxBleScanEnabled) {
    Serial.println("Initializing BLE scanner for PAX counter...");
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setActiveScan(false); // Passive scan is less power-hungry
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    Serial.println("BLE scanner initialized");
  }

  Serial.println("Display pages: ENV -> GPS FIX -> GPS TIME -> LORA");
  Serial.println("Display anti-burn-in: page rotate + pixel shift + periodic display off");
  Serial.println("\nSerial Commands: wifi-reset, wifi-status, help");
}

// Main cooperative scheduler loop.
// Keep this loop responsive so LMIC timing windows are not missed.
void loop() {
  // Process GPS data only if GPS is enabled
  if (gpsEnabled) {
    while (GpsSerial.available() > 0) {
      char c = static_cast<char>(GpsSerial.read());
      logRawNmeaChar(c, millis());
      gps.encode(c);
      if (!gpsStreamSeen) {
        Serial.print("GPS NMEA stream detected on ");
        Serial.println(GPS_PROFILES[gpsProfileIndex].name);
        gpsFirstStreamAt = millis();
      }
      gpsStreamSeen = true;
    }
  }

  unsigned long now = millis();
  
  // Check sleep mode
  if (sleepModeEnabled && (now - lastWakeTime) >= (sleepWakeInterval * 1000)) {
    Serial.println("💤 Sleep mode: Sending uplink before sleep");
    queueCayenneUplink();
    delay(5000); // Wait for uplink to complete
    
    Serial.print("💤 Entering deep sleep for ");
    Serial.print(sleepWakeInterval);
    Serial.println(" seconds");
    Serial.println("💤 Powering down peripherals...");
    
    // Power down GPS
    if (gpsEnabled) {
      disableGpsPower();
      Serial.println("  ✓ GPS powered off");
    }
    
    // Power down WiFi
    if (wifiEnabled) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      Serial.println("  ✓ WiFi powered off");
    }
    
    // Clear and power down OLED
    display.clearDisplay();
    display.display();
    display.ssd1306_command(0xAE); // Display OFF
    Serial.println("  ✓ OLED powered off");
    
    Serial.println("💤 Good night! 😴");
    delay(100); // Let serial finish
    
    // Enter deep sleep
    esp_sleep_enable_timer_wakeup(sleepWakeInterval * 1000000ULL); // Convert to microseconds
    esp_deep_sleep_start();
  }
  
  // Perform PAX counter scan if enabled
  performPaxScan();
  
  
  syncSystemClockFromGpsIfNeeded(now);

  if (GPS_RAW_NMEA_DEBUG_ENABLED && gpsRawDebugStartAt > 0 && (now - gpsRawDebugStartAt) > GPS_RAW_NMEA_DEBUG_MS) {
    gpsRawDebugStartAt = 0;
    gpsRawLine = "";
    Serial.println("GPS raw NMEA debug window ended");
  }

  if (debugEnabled && gpsStreamSeen && (now - lastGpsLog >= SERIAL_GPS_LOG_MS)) {
    lastGpsLog = now;
    logGpsDetailed();
  }

  if (now - lastUpdate >= SENSOR_UPDATE_MS) {
    lastUpdate = now;
    updateSensorReadings();
  }

  // Use configurable LoRa send interval
  uint32_t loraSendIntervalMs = loraSendIntervalSec * 1000;
  if (now - lastLoraSendAt >= loraSendIntervalMs) {
    lastLoraSendAt = now;
    queueCayenneUplink();
  }

  if (now - lastPageRotate >= DISPLAY_PAGE_MS) {
    lastPageRotate = now;
    displayPage = (displayPage + 1) % 4;
    if (debugEnabled) {
      Serial.print("Display page -> ");
      Serial.println(displayPage);
    }
  }

  if (now - lastShiftRotate >= DISPLAY_SHIFT_MS) {
    lastShiftRotate = now;
    burnInShiftIndex++;
    if (debugEnabled) {
      Serial.print("Display shift x=");
      Serial.print(currentShiftX());
      Serial.print(" y=");
      Serial.println(currentShiftY());
    }
  }

  if (!displaySleeping && (now - lastDisplaySleep >= DISPLAY_SLEEP_EVERY_MS)) {
    displaySleeping = true;
    displayWakeAt = now + DISPLAY_SLEEP_MS;
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    if (debugEnabled) {
      Serial.println("Display OFF for burn-in protection");
    }
  }

  if (displaySleeping && now >= displayWakeAt) {
    displaySleeping = false;
    lastDisplaySleep = now;
    display.ssd1306_command(SSD1306_DISPLAYON);
    if (debugEnabled) {
      Serial.println("Display ON");
    }
  }

  if (displayEnabled) {
    renderDisplayPage();
  }

  // Only show GPS debug messages when GPS is enabled AND debug mode is on
  if (gpsEnabled && debugEnabled) {
    if (!gpsStreamSeen && (now - lastGpsNoStreamLog >= GPS_NO_STREAM_LOG_MS)) {
      lastGpsNoStreamLog = now;
      Serial.print("GPS warning: no NMEA stream yet on ");
      Serial.print(GPS_PROFILES[gpsProfileIndex].name);
      Serial.print(". AXP192=");
      Serial.print(axpFound ? "FOUND" : "MISSING");
      Serial.println(" Check power, antenna view, and GPS warm-up time.");
    }

    // Save last known good GPS position when we have a valid fix
    if (gpsStreamSeen && gps.location.isValid()) {
      // Only save if position has changed significantly (>10m) or it's been >5 minutes
      double latDiff = abs(gps.location.lat() - lastGoodLat);
      double lonDiff = abs(gps.location.lng() - lastGoodLon);
      bool positionChanged = (latDiff > 0.0001 || lonDiff > 0.0001); // ~10m
      bool timeToUpdate = (now - lastGoodTimestamp) > 300000; // 5 minutes
      
      if (positionChanged || timeToUpdate || lastGoodTimestamp == 0) {
        lastGoodLat = gps.location.lat();
        lastGoodLon = gps.location.lng();
        lastGoodAlt = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
        lastGoodSats = gps.satellites.isValid() ? gps.satellites.value() : 0;
        lastGoodHdop = gps.hdop.isValid() ? gps.hdop.hdop() : 99.99;
        lastGoodTimestamp = now;
        
        // Save to NVS periodically (not every update to reduce flash wear)
        static unsigned long lastNvsSave = 0;
        if ((now - lastNvsSave) > 600000) { // Save every 10 minutes
          saveSystemSettings();
          lastNvsSave = now;
          if (debugEnabled) {
            Serial.println("GPS: Last known position saved to NVS");
          }
        }
      }
    }
    
    if (gpsStreamSeen && !gps.location.isValid() && (now - lastGpsNoFixLog >= GPS_NO_FIX_LOG_MS)) {
      lastGpsNoFixLog = now;
      Serial.print("GPS no-fix: sats=");
      Serial.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
      Serial.print(" hdop=");
      Serial.print(gps.hdop.isValid() ? gps.hdop.hdop() : 0.0f, 2);
      Serial.print(" streamAgeMs=");
      Serial.print(gpsFirstStreamAt > 0 ? now - gpsFirstStreamAt : 0);
      Serial.println();

      if (gpsFirstStreamAt > 0 && (now - gpsFirstStreamAt) < GPS_WARMUP_MS) {
        Serial.println("GPS warm-up window active (up to ~3 min cold start)");
      } else {
        Serial.println("GPS fix delayed >3 min: check antenna, outdoor sky view, and PMU rails");
      }
    }
  }

  if (debugEnabled && now - lastSystemDebugAt >= SERIAL_SYSTEM_DEBUG_MS) {
    lastSystemDebugAt = now;
    logSystemDetailed(now);
  }

  if (debugEnabled && now - lastAxpDebugAt >= SERIAL_AXP_DEBUG_MS) {
    lastAxpDebugAt = now;
    logAxp192Detailed();
  }

  os_runloop_once();
  
  // Handle serial commands for WiFi management
  handleSerialCommands();

  delay(5);
}