#pragma once
PubSubClient mqttClient(mqttWifiClient);

// MQTT Configuration
bool mqttEnabled = false;
String mqttServer = "";
uint16_t mqttPort = 1883;
String mqttUsername = "";
String mqttPassword = "";
String mqttTopic = "ttgo/sensor";
String mqttDeviceName = "TTGO-T-Beam";
unsigned long lastMqttAttempt = 0;
unsigned long mqttReconnectInterval = 5000; // 5 seconds
bool mqttConnected = false;
unsigned long mqttPublishCounter = 0;
unsigned long mqttPublishInterval = 60000; // 60 seconds default
unsigned long lastMqttPublish = 0;

// MQTT Payload Configuration (which fields to include)
bool mqttIncludeTimestamp = true;
bool mqttIncludeBME280 = true;
bool mqttIncludeGPS = true;
bool mqttIncludeBattery = true;
bool mqttIncludePAX = true;
bool mqttIncludeSystem = true;

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
bool time24h = true;                          // true = 24h display, false = 12h AM/PM
int ntpTzOffsetSec = 0;                       // UTC offset in seconds (e.g. -18000 = UTC-5)
char ntpTzAbbr[8] = "UTC";                   // Display abbreviation (e.g. "EST", "CET")
bool ntpSynced = false;                       // true once NTP has set the clock
bool fetchNtpTime();                          // forward declaration (defined near setupWiFi)
bool clockSynced = false;
bool wifiConnected = false;
bool wifiConfigMode = false;
bool wifiEnabled = true;
bool wifiConnecting = false;
bool debugEnabled = false;
bool serialOutputEnabled = true;             // Serial output on/off (auto-mutes after SERIAL_OUTPUT_TIMEOUT_MS)
unsigned long serialLastActivityAt = 0;      // Timestamp of last serial command (for auto-mute timer)
#define SERIAL_OUTPUT_TIMEOUT_MS (10UL * 60UL * 1000UL)  // 10 minutes
String pendingWebCommand = "";               // Command queued from web UI (/debug page)
bool displayEnabled = true;
bool otaInProgress = false;  // Flag to stop main loop during OTA
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
unsigned long wifiConnectStartedAt = 0;
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
// GPS path tracking - circular buffer for last 20 positions
// GPS path tracking - configurable size (5-100 positions)
#define GPS_PATH_MAX_SIZE 100
int gpsPathSize = 20; // Default: 20 positions, configurable via web UI
struct GpsPosition {
  double lat;
  double lon;
  uint32_t timestamp;
  bool valid;
};
GpsPosition gpsPath[GPS_PATH_MAX_SIZE];
int gpsPathIndex = 0;
int gpsPathCount = 0;


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

// Hardware I2C pin configuration (runtime; loaded from NVS on boot, defaults match config.h)
uint8_t i2cSdaPin = 21;
uint8_t i2cSclPin = 22;
uint8_t oledI2cAddr = 0x3C;
uint8_t bme280I2cAddr = 0x76;

