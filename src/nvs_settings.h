#pragma once
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
  time24h = preferences.getBool("time24h", true);
  ntpTzOffsetSec = preferences.getInt("ntpTzOff", 0);
  strlcpy(ntpTzAbbr, preferences.getString("ntpTzAbbr", "UTC").c_str(), sizeof(ntpTzAbbr));
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
  
  // Load GPS path size (5-100, default 20)
  gpsPathSize = preferences.getUInt("gpsPathSize", 20);
  if (gpsPathSize < 5) gpsPathSize = 5;
  if (gpsPathSize > GPS_PATH_MAX_SIZE) gpsPathSize = GPS_PATH_MAX_SIZE;
  
  // Load MQTT settings
  mqttEnabled = preferences.getBool("mqttEnabled", false);
  mqttServer = preferences.getString("mqttServer", "");
  mqttPort = preferences.getUShort("mqttPort", 1883);
  mqttUsername = preferences.getString("mqttUser", "");
  mqttPassword = preferences.getString("mqttPass", "");
  mqttTopic = preferences.getString("mqttTopic", "ttgo/sensor");
  mqttDeviceName = preferences.getString("mqttDevice", "TTGO-T-Beam");
  mqttPublishInterval = preferences.getULong("mqttInterval", 60000); // Default 60 seconds
  
  // Load MQTT payload configuration
  mqttIncludeTimestamp = preferences.getBool("mqttTime", true);
  mqttIncludeBME280 = preferences.getBool("mqttBME", true);
  mqttIncludeGPS = preferences.getBool("mqttGPS", true);
  mqttIncludeBattery = preferences.getBool("mqttBatt", true);
  mqttIncludePAX = preferences.getBool("mqttPAX", true);
  mqttIncludeSystem = preferences.getBool("mqttSys", true);
  
  // Load last known good GPS position
  lastGoodLat = preferences.getDouble("gpsLat", 0.0);
  lastGoodLon = preferences.getDouble("gpsLon", 0.0);
  lastGoodAlt = preferences.getDouble("gpsAlt", 0.0);
  lastGoodSats = preferences.getUInt("gpsSats", 0);
  lastGoodHdop = preferences.getDouble("gpsHdop", 99.99);
  lastGoodTimestamp = preferences.getUInt("gpsTime", 0);

  // Load hardware I2C pin configuration
  i2cSdaPin    = preferences.getUChar("sdaPin",  21);
  i2cSclPin    = preferences.getUChar("sclPin",  22);
  oledI2cAddr  = preferences.getUChar("oledAddr", 0x3C);
  bme280I2cAddr = preferences.getUChar("bmeAddr", 0x76);
  
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
  preferences.putBool("time24h", time24h);
  preferences.putInt("ntpTzOff", ntpTzOffsetSec);
  preferences.putString("ntpTzAbbr", String(ntpTzAbbr));
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
  
  // Save GPS path size
  preferences.putUInt("gpsPathSize", gpsPathSize);
  
  // Save MQTT settings
  preferences.putBool("mqttEnabled", mqttEnabled);
  preferences.putString("mqttServer", mqttServer);
  preferences.putUShort("mqttPort", mqttPort);
  preferences.putString("mqttUser", mqttUsername);
  preferences.putString("mqttPass", mqttPassword);
  preferences.putString("mqttTopic", mqttTopic);
  preferences.putString("mqttDevice", mqttDeviceName);
  preferences.putULong("mqttInterval", mqttPublishInterval);
  
  // Save MQTT payload configuration
  preferences.putBool("mqttTime", mqttIncludeTimestamp);
  preferences.putBool("mqttBME", mqttIncludeBME280);
  preferences.putBool("mqttGPS", mqttIncludeGPS);
  preferences.putBool("mqttBatt", mqttIncludeBattery);
  preferences.putBool("mqttPAX", mqttIncludePAX);
  preferences.putBool("mqttSys", mqttIncludeSystem);
  
  // Save last known good GPS position
  preferences.putDouble("gpsLat", lastGoodLat);
  preferences.putDouble("gpsLon", lastGoodLon);

  // Save hardware I2C pin configuration
  preferences.putUChar("sdaPin",  i2cSdaPin);
  preferences.putUChar("sclPin",  i2cSclPin);
  preferences.putUChar("oledAddr", oledI2cAddr);
  preferences.putUChar("bmeAddr", bme280I2cAddr);
  
  preferences.end();
}

// =============================================================================
// MQTT Functions
// =============================================================================
// Author: Markus van Kempen | markus.van.kempen@gmail.com
//
// MQTT client for publishing sensor data to MQTT broker
// =============================================================================

/**
 * @brief Connect to MQTT broker
 * 
 * Attempts to connect to the configured MQTT broker with credentials
 */
