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
 * Version: 2.7.4
 * Last Updated: 2026-03-24
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
#include <sys/socket.h>   // raw POSIX sockets for NTP (no WiFiUDP IRAM overhead)
#include <arpa/inet.h>    // inet_addr(), htons()
#include <WiFi.h>
#include <esp_wifi.h>
#include <driver/gpio.h>  // gpio_intr_disable() for OTA flash-write safety
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <Update.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "lorawan_secrets.h"

// ========== Module Headers (single-translation-unit pattern) ==========
#include "config.h"
#include "globals.h"
#include "debug_serial.h"
#include "axp_power.h"
#include "sensors.h"
#include "gps_module.h"
#include "display_oled.h"
#include "lorawan_module.h"
#include "pax_counter.h"
#include "nvs_settings.h"
#include "mqtt_manager.h"
#include "web_pages.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "serial_commands.h"

void setup() {
  Serial.begin(115200);
  serialLastActivityAt = millis();  // Start 10-min serial auto-mute timer from boot
  delay(300);
  
  // Print boot logo (ASCII art) then the info banner
  Serial.println();
  Serial.println("     _____      ___  ___    _    __  __  ");
  Serial.println("    |_   _|    | _ )| __| /_\\  |  \\/  | ");
  Serial.println("      | |      | _ \\| _| / _ \\ | |\\/| | ");
  Serial.println("      |_|      |___/|___|/_/ \\_||_|  |_| ");
  Serial.println("              Sensor Node  \xC2\xB7  MvK  \xC2\xB7  Floor 7\xC2\xBD");
  Serial.println();
  Serial.println("╔════════════════════════════════════════════════════════════╗");
  Serial.println("║          TTGO T-Beam T22 V1.1 Sensor Node                 ║");
  Serial.println("║  Author: Markus van Kempen | Floor 7½ 🏢🤏                ║");
  Serial.println("║  Email: markus.van.kempen@gmail.com                       ║");
  Serial.println("║  Motto: \"No bug too small, no syntax too weird.\"          ║");
  { String s = "  Version: " + String(FIRMWARE_VERSION) + " | " + String(FIRMWARE_DATE);
    while (s.length() < 60) s += ' ';
    Serial.println("║" + s + "║"); }
  { String s = "  Build: " + String(BUILD_TIMESTAMP);
    while (s.length() < 60) s += ' ';
    Serial.println("║" + s + "║"); }
  Serial.println("║  License: Apache 2.0                                       ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println("\n💡 Type 'menu' for interactive configuration menu");
  Serial.println("💡 Type 'help' for quick command reference");
  Serial.println("💡 Type 'status' to see system status\n");
  
  // Initialize debug buffer with startup message
  logDebug("🚀 TTGO T-Beam T22 V1.1 starting...");
  logDebug("Version: " + String(FIRMWARE_VERSION) + " (" + String(FIRMWARE_DATE) + ")");

  // Load system settings from NVS
  loadSystemSettings();
  
  // Load LoRaWAN keys from NVS
  loadLoRaKeysFromNVS();
  printLoRaKeys();

  Wire.begin(i2cSdaPin, i2cSclPin);
  Wire.setClock(I2C_CLOCK_HZ);
  rtcChipFound = i2cDevicePresent(RTC_I2C_ADDR);
  Serial.print("RTC I2C 0x51: ");
  Serial.println(rtcChipFound ? "FOUND" : "NOT FOUND");
  Serial.print("I2C clock Hz: ");
  Serial.println(I2C_CLOCK_HZ);
  setupAxp192PowerForGps();
  logAxp192Detailed();

  if (!display.begin(SSD1306_SWITCHCAPVCC, oledI2cAddr)) {
    Serial.println("SSD1306 init failed");
    while (true) {
      delay(1000);
    }
  }

  showBootSplash();  // 2.5 s title + MvK credit screen

  // T22 V1.1 20191212 GPS initialization - GPIO reset disabled to test LED issue
  Serial.println("=== T22 V1.1 20191212 GPS INITIALIZATION ===");
  delay(500);  // Allow AXP192 power to stabilize
  
  // FORCE GPS ENABLE: Override NVS if GPS was disabled
  // This ensures GPS always starts enabled for troubleshooting
  if (!gpsEnabled) {
    Serial.println("⚠️  GPS was disabled in NVS - forcing GPS ON");
    gpsEnabled = true;
    saveSystemSettings(); // Persist the change
  }
  
  // Ensure GPS power is enabled
  if (gpsEnabled && axpFound) {
    enableGpsPower();
  } else if (!axpFound) {
    Serial.println("❌ Cannot enable GPS power - AXP192 not found!");
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

  // Initialize BLE before WiFi so the BT controller can claim its memory block
  // first. Initialising BLE after WiFi scan + AP setup exhausts heap on ESP32.
  if (paxCounterEnabled && paxBleScanEnabled) {
    Serial.println("Initializing BLE scanner for PAX counter...");
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setActiveScan(false); // Passive scan is less power-hungry
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    Serial.println("BLE scanner initialized");
  }

  setupLoRa();
  
  // Setup WiFi and Web Server
  setupWiFi();

  Serial.println("Display pages: ENV -> GPS FIX -> GPS TIME -> LORA");
  Serial.println("Display anti-burn-in: page rotate + pixel shift + periodic display off");
  Serial.println("\nSerial Commands: wifi-reset, wifi-status, help");
}

// Main cooperative scheduler loop.
// Keep this loop responsive so LMIC timing windows are not missed.
void loop() {
  // CRITICAL: Do nothing if OTA is in progress
  if (otaInProgress) {
    delay(100);
    return;
  }
  
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

  // WiFi connection state tracking and reconnection
  if (wifiEnabled && !wifiConfigMode) {
    static unsigned long lastWifiReconnectAttempt = 0;
    if (WiFi.status() == WL_CONNECTED) {
      // Only set wifiConnected once DHCP has assigned a real IP.
      // WL_CONNECTED is raised before DHCP completes; the IP is 255.255.255.255
      // (or 0.0.0.0) until the lease arrives.
      IPAddress ip = WiFi.localIP();
      bool dhcpDone = (ip != IPAddress(0,0,0,0)) && (ip != IPAddress(255,255,255,255));
      if (dhcpDone) {
        if (!wifiConnected) {
          // First time we see a real IP after connect/reconnect — log it
          Serial.println("✓ WiFi DHCP ready — IP: " + ip.toString() +
                         "  RSSI: " + String(WiFi.RSSI()) + " dBm");
          Serial.println("🌐 Dashboard: http://" + ip.toString());
        }
        wifiConnected = true;
        wifiConnecting = false;
      }
    } else {
      if (wifiConnecting && (now - wifiConnectStartedAt) >= 25000) {
        // Give up on current attempt after 25 s so we can retry
        WiFi.disconnect(false);
        wifiConnecting = false;
      }
      if (wifiConnected) {
        Serial.println("⚠️  WiFi connection lost, will attempt reconnection...");
        wifiConnected = false;
      }
      // Try to reconnect every 15 seconds for faster recovery
      if (!wifiConnecting && now - lastWifiReconnectAttempt >= 15000) {
        lastWifiReconnectAttempt = now;
        preferences.begin("wifi", true);
        String savedSSID = preferences.getString("ssid", "");
        String savedPassword = preferences.getString("password", "");
        preferences.end();
        if (savedSSID.length() > 0) {
          if (debugEnabled) {
            Serial.println("📡 WiFi reconnecting to: " + savedSSID);
          }
          configureWiFiRadio();
          WiFi.mode(WIFI_STA);
          WiFi.disconnect(false);   // clear stale association before re-init
          delay(50);
          wifiConnecting = true;
          wifiConnectStartedAt = now;
          WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        }
      }
    }
  }

  // MQTT connection management — only after DHCP has given us a real IP
  if (mqttEnabled && wifiEnabled && wifiConnected) {
    if (!mqttClient.connected()) {
      connectMQTT();
    } else {
      mqttClient.loop(); // Process MQTT messages
    }
  }
  
  // Use configurable LoRa send interval
  uint32_t loraSendIntervalMs = loraSendIntervalSec * 1000;
  if (now - lastLoraSendAt >= loraSendIntervalMs) {
    lastLoraSendAt = now;
    queueCayenneUplink();
  }
  
  // MQTT publish with independent interval
  if (mqttEnabled && mqttConnected && (now - lastMqttPublish >= mqttPublishInterval)) {
    lastMqttPublish = now;
    publishMQTT();
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
        
        // Add position to GPS path history (circular buffer)
        gpsPath[gpsPathIndex].lat = gps.location.lat();
        gpsPath[gpsPathIndex].lon = gps.location.lng();
        gpsPath[gpsPathIndex].timestamp = now;
        gpsPath[gpsPathIndex].valid = true;
        gpsPathIndex = (gpsPathIndex + 1) % gpsPathSize;
        if (gpsPathCount < gpsPathSize) {
          gpsPathCount++;
        }
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
  
  // Auto-mute serial output after 10 minutes of inactivity
  if (serialOutputEnabled && serialLastActivityAt > 0 &&
      (millis() - serialLastActivityAt > SERIAL_OUTPUT_TIMEOUT_MS)) {
    Serial.println("\n⏱ Serial output auto-muted (10 min inactivity). Type any command to re-enable.");
    serialOutputEnabled = false;
  }

  // Retry NTP if it failed at connect time — check every 60 seconds
  static unsigned long lastNtpRetry = 0;
  if (!ntpSynced && wifiConnected && (millis() - lastNtpRetry > 60000)) {
    lastNtpRetry = millis();
    if (fetchNtpTime()) {
      ntpSynced = true;
      clockSynced = true;
      lastClockSyncAt = millis();
      time_t now_t = time(nullptr);
      time_t adj = now_t + (time_t)ntpTzOffsetSec;
      struct tm lt = {}; gmtime_r(&adj, &lt);
      char buf[32];
      snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d %s",
               lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday,
               lt.tm_hour, lt.tm_min, lt.tm_sec, ntpTzAbbr);
      Serial.println("✓ NTP synced (retry): " + String(buf));
    }
  }

  // Handle serial commands for WiFi management
  handleSerialCommands();

  delay(5);
}