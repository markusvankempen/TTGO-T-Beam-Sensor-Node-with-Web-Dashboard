#pragma once

// -----------------------------------------------------------------------------
// Firmware Version
// -----------------------------------------------------------------------------
const char* FIRMWARE_VERSION = "2.7.4";
const char* FIRMWARE_DATE = "2026-03-23";
const char* BUILD_TIMESTAMP = __DATE__ " " __TIME__;

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
Preferences preferences;

// MQTT Client
WiFiClient mqttWifiClient;
