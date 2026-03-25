#pragma once
void printSerialMenu() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║          TTGO T-Beam Configuration Menu                   ║");
  Serial.println("║  Author: Markus van Kempen | Floor 7½ 🏢🤏                ║");
  Serial.println("║  Email: markus.van.kempen@gmail.com                       ║");
  Serial.println("║  Motto: \"No bug too small, no syntax too weird.\"          ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println("\n📡 WiFi Commands:");
  Serial.println("  wifi-status    - Show WiFi connection status");
  Serial.println("  wifi-info      - Show saved WiFi credentials and config");
  Serial.println("  wifi-set <ssid> <password> - Set WiFi credentials and connect");
  Serial.println("  wifi-on        - Enable WiFi and web server");
  Serial.println("  wifi-off       - Disable WiFi (saves power)");
  Serial.println("  wifi-ap        - Force AP mode (SSID: " + String(WIFI_AP_NAME) + ")");
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
  Serial.println("\n📨 MQTT Commands:");
  Serial.println("  mqtt-status    - Show MQTT configuration and status");
  Serial.println("  mqtt-on        - Enable MQTT publishing");
  Serial.println("  mqtt-off       - Disable MQTT publishing");
  Serial.println("  mqtt-server <server> - Set MQTT broker server");
  Serial.println("  mqtt-port <port> - Set MQTT broker port");
  Serial.println("  mqtt-user <username> - Set MQTT username");
  Serial.println("  mqtt-pass <password> - Set MQTT password");
  Serial.println("  mqtt-topic <topic> - Set MQTT topic");
  Serial.println("  mqtt-device <name> - Set device name");
  Serial.println("  mqtt-interval <sec> - Set publish interval (5-3600 sec)");
  Serial.println("  mqtt-test      - Test MQTT connection");
  Serial.println("  mqtt-save      - Save MQTT settings to NVS");
  Serial.println("\n🔧 System Commands:");
  Serial.println("  status         - Show complete system status");
  Serial.println("  debug-on       - Enable verbose debugging");
  Serial.println("  debug-off      - Disable verbose debugging");
  Serial.println("  serial-on      - Enable serial output");
  Serial.println("  serial-off     - Disable serial output (auto-off after 10 min inactivity)");
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
  
  // MQTT Status
  Serial.println("\n📨 MQTT:");
  Serial.print("  Enabled: ");
  Serial.println(mqttEnabled ? "✓ Yes" : "✗ No");
  if (mqttEnabled) {
    Serial.print("  Connected: ");
    Serial.println(mqttConnected ? "✓ Yes" : "✗ No");
    Serial.print("  Server: ");
    Serial.println(mqttServer.length() > 0 ? mqttServer : "(not set)");
    Serial.print("  Port: ");
    Serial.println(mqttPort);
    Serial.print("  Topic: ");
    Serial.println(mqttTopic);
    Serial.print("  Device Name: ");
    Serial.println(mqttDeviceName);
    Serial.print("  Publish Interval: ");
    Serial.print(mqttPublishInterval / 1000);
    Serial.println(" sec");
    Serial.print("  Published: ");
    Serial.println(mqttPublishCounter);
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
  String rawCmd = "";

  if (pendingWebCommand.length() > 0) {
    rawCmd = pendingWebCommand;
    pendingWebCommand = "";
    rawCmd.trim();
    serialOutputEnabled = true;
    serialLastActivityAt = millis();
    Serial.println(">> [WEB] " + rawCmd);
  } else if (Serial.available()) {
    // Reset inactivity timer on any serial input; wake output if it was auto-muted
    bool wasMuted = !serialOutputEnabled;
    serialOutputEnabled = true;
    serialLastActivityAt = millis();
    if (wasMuted) {
      Serial.println("✓ Serial output re-enabled");
    }
    rawCmd = Serial.readStringUntil('\n');
    rawCmd.trim();
  }

  if (rawCmd.length() > 0) {
    String cmd = rawCmd;
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
      
    } else if (cmd.startsWith("wifi-set ")) {
      // Parse: wifi-set <ssid> <password>
      // Use rawCmd to preserve case of SSID and password
      String params = rawCmd.substring(9); // Remove "wifi-set "
      int spaceIndex = params.indexOf(' ');
      
      if (spaceIndex == -1) {
        Serial.println("❌ Usage: wifi-set <ssid> <password>");
        Serial.println("   Example: wifi-set MyNetwork MyPassword123");
        Serial.println("   Note: For open networks, use: wifi-set <ssid> \"\"");
      } else {
        String ssid = params.substring(0, spaceIndex);
        String password = params.substring(spaceIndex + 1);
        
        // Remove quotes if present
        ssid.trim();
        password.trim();
        if (password.startsWith("\"") && password.endsWith("\"")) {
          password = password.substring(1, password.length() - 1);
        }
        
        Serial.println("\n╔════════════════════════════════════════════════════════════╗");
        Serial.println("║              📡 Setting WiFi Credentials                  ║");
        Serial.println("╚════════════════════════════════════════════════════════════╝");
        Serial.print("📝 SSID: ");
        Serial.println(ssid);
        Serial.print("🔐 Password: ");
        Serial.println(password.length() > 0 ? "****** (set)" : "(empty - open network)");
        
        // Properly tear down AP mode if it was running, then switch to STA
        wifi_mode_t currentMode = WiFi.getMode();
        if (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA) {
          Serial.println("📡 Stopping AP mode...");
          WiFi.softAPdisconnect(true);
          delay(300);
        }
        configureWiFiRadio();
        WiFi.mode(WIFI_OFF);
        delay(500);                   // Allow radio to fully shut down
        WiFi.mode(WIFI_STA);
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // Re-enable DHCP
        delay(500);                   // Allow STA radio to initialise
        
        wifiConfigMode = false;
        logWiFiScanForTarget(ssid);
        
        // Try to connect
        Serial.println("📡 Attempting to connect...");
        Serial.print("   Debug - status before begin: ");
        Serial.println(WiFi.status());
        wifiConnecting = true;
        wifiConnectStartedAt = millis();
        WiFi.begin(ssid.c_str(), password.c_str());
        
        // Wait up to 20 seconds for connection
        int attempts = 0;
        int lastStatus = -1;
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
          delay(500);
          int s = WiFi.status();
          if (s != lastStatus) {
            Serial.print("[status:");
            Serial.print(s);
            Serial.print("]");
            lastStatus = s;
          } else {
            Serial.print(".");
          }
          attempts++;
        }
        Serial.println();
        
        // Save credentials to NVS regardless of connection result
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("password", password);
        prefs.end();
        
        if (WiFi.status() == WL_CONNECTED) {
          wifiConnected = true;
          wifiConnecting = false;
          wifiConfigMode = false;
          Serial.println("\n✓ WiFi Connected Successfully!");
          Serial.print("   IP Address: ");
          Serial.println(WiFi.localIP().toString());
          Serial.print("   SSID: ");
          Serial.println(WiFi.SSID());
          Serial.print("   RSSI: ");
          Serial.print(WiFi.RSSI());
          Serial.println(" dBm");
          Serial.println("\n💾 Credentials saved to NVS");
          Serial.println("   Device will use these credentials on next boot");
        } else {
          wifiConnected = false;
          wifiConnecting = false;
          Serial.println("\n✗ WiFi Connection Failed!");
          Serial.print("   Status code: ");
          Serial.println(WiFi.status());
          Serial.println("   Possible reasons:");
          Serial.println("   - Wrong password");
          Serial.println("   - Network out of range");
          Serial.println("   - Router not responding");
          Serial.println("   - WPA3-only network (ESP32 needs WPA2)");
          Serial.println("\n💾 Credentials saved to NVS anyway");
          Serial.println("   Device will retry these credentials on next boot");
          Serial.println("\n💡 Tip: Use 'wifi-info' to check saved credentials");
        }
      }
      
    } else if (cmd == "wifi-ap") {
      Serial.println("\n╔════════════════════════════════════════════════════════════╗");
      Serial.println("║              📡  Starting WiFi AP Mode                    ║");
      Serial.println("╚════════════════════════════════════════════════════════════╝");
      // Tear down any existing STA or AP connection cleanly
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
        delay(300);
      }
      WiFi.mode(WIFI_AP);
      WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);
      IPAddress apIP = WiFi.softAPIP();
      wifiConnected = false;
      wifiConfigMode = true;
      Serial.print("📡 AP SSID:     "); Serial.println(WIFI_AP_NAME);
      Serial.print("🔑 AP Password: "); Serial.println(WIFI_AP_PASSWORD);
      Serial.print("🌐 AP IP:       "); Serial.println(apIP);
      Serial.println("\n🌐 Web Dashboard: http://" + apIP.toString());
      Serial.println("   Connect your phone/laptop to '" + String(WIFI_AP_NAME) + "' and open the URL above.");
      showWiFiStatusOnDisplay("AP Mode Active", apIP.toString().c_str());
      Serial.println("════════════════════════════════════════════════════════════\n");

    } else if (cmd == "wifi-reset") {
      Serial.println("\n╔════════════════════════════════════════════════════════════╗");
      Serial.println("║              ⚠️  Resetting WiFi Settings                  ║");
      Serial.println("╚════════════════════════════════════════════════════════════╝");
      Serial.println("📝 Clearing saved WiFi credentials...");
      preferences.begin("wifi", false);
      preferences.clear();
      preferences.end();
      Serial.println("✓ WiFi settings cleared");
      Serial.println("🔄 Restarting device in 2 seconds...");
      Serial.println("   After restart, device will enter AP mode");
      Serial.println("   Connect to: TTGO-T-Beam-Setup");
      Serial.println("   Password: 12345678");
      delay(2000);
      ESP.restart();
      
    } else if (cmd == "wifi-info") {
      Serial.println("\n╔════════════════════════════════════════════════════════════╗");
      Serial.println("║                WiFi Configuration Info                     ║");
      Serial.println("╚════════════════════════════════════════════════════════════╝");
      
      // Read saved WiFi credentials from preferences
      preferences.begin("wifi", true);
      String savedSSID = preferences.getString("ssid", "");
      String savedPassword = preferences.getString("password", "");
      preferences.end();
      
      Serial.print("📡 Saved SSID: ");
      if (savedSSID.length() > 0) {
        Serial.println(savedSSID);
        Serial.print("   SSID Length: ");
        Serial.println(savedSSID.length());
        Serial.print("🔐 Password: ");
        if (savedPassword.length() > 0) {
          Serial.println("****** (set)");
          Serial.print("   Password Length: ");
          Serial.println(savedPassword.length());
        } else {
          Serial.println("(not set)");
        }
      } else {
        Serial.println("(none - will enter AP mode)");
      }
      
      Serial.print("📶 Current Status: ");
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected");
        Serial.print("   IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("   RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        Serial.print("   MAC: ");
        Serial.println(WiFi.macAddress());
      } else {
        Serial.print("Not connected (Status: ");
        Serial.print(WiFi.status());
        Serial.println(")");
        Serial.println("   Status codes:");
        Serial.println("   0 = WL_IDLE_STATUS");
        Serial.println("   1 = WL_NO_SSID_AVAIL");
        Serial.println("   3 = WL_CONNECTED");
        Serial.println("   4 = WL_CONNECT_FAILED");
        Serial.println("   6 = WL_DISCONNECTED");
      }
      
      Serial.print("📡 WiFi Mode: ");
      Serial.println(WiFi.getMode());
      Serial.print("🔋 TX Power: ");
      Serial.println(WiFi.getTxPower());
      
    } else if (cmd == "wifi-status") {
      Serial.println("\n📡 WiFi Status:");
      Serial.print("  Mode: ");
      
      wifi_mode_t mode = WiFi.getMode();
      if (mode == WIFI_MODE_AP) {
        Serial.println("AP (Access Point)");
        Serial.print("  AP SSID: ");
        Serial.println(WiFi.softAPSSID());
        Serial.print("  AP IP: ");
        Serial.println(WiFi.softAPIP());
        Serial.print("  Clients: ");
        Serial.println(WiFi.softAPgetStationNum());
        Serial.print("  Dashboard: http://");
        Serial.println(WiFi.softAPIP());
      } else if (mode == WIFI_MODE_STA) {
        Serial.println("STA (Station)");
        Serial.print("  Connected: ");
        Serial.println(WiFi.status() == WL_CONNECTED ? "Yes" : "No");
        if (WiFi.status() == WL_CONNECTED) {
          IPAddress staIP = WiFi.localIP();
          bool dhcpReady = (staIP != IPAddress(0,0,0,0)) && (staIP != IPAddress(255,255,255,255));
          Serial.print("  SSID: ");
          Serial.println(WiFi.SSID());
          Serial.print("  IP: ");
          if (dhcpReady) {
            Serial.println(staIP);
          } else {
            Serial.println("(DHCP pending...)");
          }
          Serial.print("  Signal: ");
          Serial.print(WiFi.RSSI());
          Serial.println(" dBm");
          if (dhcpReady) {
            Serial.print("  Dashboard: http://");
            Serial.println(staIP);
          }
        }
      } else if (mode == WIFI_MODE_APSTA) {
        Serial.println("AP+STA (Both)");
        Serial.print("  AP IP: ");
        Serial.println(WiFi.softAPIP());
        IPAddress staIP2 = WiFi.localIP();
        bool dhcpReady2 = (staIP2 != IPAddress(0,0,0,0)) && (staIP2 != IPAddress(255,255,255,255));
        Serial.print("  STA Connected: ");
        Serial.println(WiFi.status() == WL_CONNECTED ? "Yes" : "No");
        Serial.print("  STA IP: ");
        Serial.println(dhcpReady2 ? staIP2.toString() : String("(DHCP pending...)"));
      } else {
        Serial.println("OFF");
      }
      Serial.println();
      
    } else if (cmd == "debug-on") {
      debugEnabled = true;
      Serial.println("✓ Verbose debugging enabled");
      
    } else if (cmd == "debug-off") {
      debugEnabled = false;
      Serial.println("✓ Verbose debugging disabled");
      
    } else if (cmd == "serial-on") {
      serialOutputEnabled = true;
      serialLastActivityAt = millis();
      Serial.println("✓ Serial output enabled");
      
    } else if (cmd == "serial-off") {
      Serial.println("✓ Serial output disabled (type any command to re-enable)");
      serialOutputEnabled = false;
      
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
      
    } else if (cmd.startsWith("mqtt-")) {
      if (cmd == "mqtt-status") {
        Serial.println("\n📨 MQTT Configuration:");
        Serial.print("  Enabled: ");
        Serial.println(mqttEnabled ? "✓ Yes" : "✗ No");
        Serial.print("  Connected: ");
        Serial.println(mqttConnected ? "✓ Yes" : "✗ No");
        Serial.print("  Server: ");
        Serial.println(mqttServer.length() > 0 ? mqttServer : "(not set)");
        Serial.print("  Port: ");
        Serial.println(mqttPort);
        Serial.print("  Username: ");
        Serial.println(mqttUsername.length() > 0 ? mqttUsername : "(not set)");
        Serial.print("  Password: ");
        Serial.println(mqttPassword.length() > 0 ? "********" : "(not set)");
        Serial.print("  Topic: ");
        Serial.println(mqttTopic);
        Serial.print("  Device Name: ");
        Serial.println(mqttDeviceName);
        Serial.print("  Publish Interval: ");
        Serial.print(mqttPublishInterval / 1000);
        Serial.println(" seconds");
        Serial.print("  Messages Published: ");
        Serial.println(mqttPublishCounter);
        Serial.println("\n  Payload Content:");
        Serial.print("    Timestamp: ");
        Serial.println(mqttIncludeTimestamp ? "✓" : "✗");
        Serial.print("    BME280: ");
        Serial.println(mqttIncludeBME280 ? "✓" : "✗");
        Serial.print("    GPS: ");
        Serial.println(mqttIncludeGPS ? "✓" : "✗");
        Serial.print("    Battery: ");
        Serial.println(mqttIncludeBattery ? "✓" : "✗");
        Serial.print("    PAX Counter: ");
        Serial.println(mqttIncludePAX ? "✓" : "✗");
        Serial.print("    System Info: ");
        Serial.println(mqttIncludeSystem ? "✓" : "✗");
        Serial.println();
        
      } else if (cmd == "mqtt-on") {
        mqttEnabled = true;
        Serial.println("✓ MQTT enabled");
        Serial.println("  Use 'mqtt-save' to persist to NVS");
        
      } else if (cmd == "mqtt-off") {
        mqttEnabled = false;
        if (mqttClient.connected()) {
          mqttClient.disconnect();
        }
        Serial.println("✓ MQTT disabled");
        Serial.println("  Use 'mqtt-save' to persist to NVS");
        
      } else if (cmd.startsWith("mqtt-server ")) {
        mqttServer = rawCmd.substring(12);
        mqttServer.trim();
        Serial.print("✓ MQTT server set to: ");
        Serial.println(mqttServer);
        Serial.println("  Use 'mqtt-save' to persist to NVS");
        
      } else if (cmd.startsWith("mqtt-port ")) {
        String portStr = cmd.substring(10);
        uint16_t port = portStr.toInt();
        if (port > 0 && port <= 65535) {
          mqttPort = port;
          Serial.print("✓ MQTT port set to: ");
          Serial.println(mqttPort);
          Serial.println("  Use 'mqtt-save' to persist to NVS");
        } else {
          Serial.println("✗ Invalid port (must be 1-65535)");
        }
        
      } else if (cmd.startsWith("mqtt-user ")) {
        mqttUsername = rawCmd.substring(10);
        mqttUsername.trim();
        Serial.print("✓ MQTT username set to: ");
        Serial.println(mqttUsername);
        Serial.println("  Use 'mqtt-save' to persist to NVS");
        
      } else if (cmd.startsWith("mqtt-pass ")) {
        mqttPassword = rawCmd.substring(10);
        mqttPassword.trim();
        Serial.println("✓ MQTT password set");
        Serial.println("  Use 'mqtt-save' to persist to NVS");
        
      } else if (cmd.startsWith("mqtt-topic ")) {
        mqttTopic = rawCmd.substring(11);
        mqttTopic.trim();
        Serial.print("✓ MQTT topic set to: ");
        Serial.println(mqttTopic);
        Serial.println("  Use 'mqtt-save' to persist to NVS");
        
      } else if (cmd.startsWith("mqtt-device ")) {
        mqttDeviceName = rawCmd.substring(12);
        mqttDeviceName.trim();
        Serial.print("✓ MQTT device name set to: ");
        Serial.println(mqttDeviceName);
        Serial.println("  Use 'mqtt-save' to persist to NVS");
        
      } else if (cmd.startsWith("mqtt-interval ")) {
        String intervalStr = cmd.substring(14);
        uint32_t interval = intervalStr.toInt();
        if (interval >= 5 && interval <= 3600) {
          mqttPublishInterval = interval * 1000;
          Serial.print("✓ MQTT publish interval set to ");
          Serial.print(interval);
          Serial.println(" seconds");
          Serial.println("  Use 'mqtt-save' to persist to NVS");
        } else {
          Serial.println("✗ Interval must be 5-3600 seconds");
        }
        
      } else if (cmd == "mqtt-test") {
        if (mqttServer.length() == 0) {
          Serial.println("✗ MQTT server not configured");
          Serial.println("  Use 'mqtt-server <server>' to set server");
        } else {
          Serial.print("Testing connection to ");
          Serial.print(mqttServer);
          Serial.print(":");
          Serial.println(mqttPort);
          
          WiFiClient testWifiClient;
          PubSubClient testClient(testWifiClient);
          testClient.setServer(mqttServer.c_str(), mqttPort);
          
          String clientId = "TTGO-Test-" + String((uint32_t)ESP.getEfuseMac(), HEX);
          bool connected = false;
          
          if (mqttUsername.length() > 0) {
            connected = testClient.connect(clientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str());
          } else {
            connected = testClient.connect(clientId.c_str());
          }
          
          if (connected) {
            Serial.println("✓ Connection successful!");
            testClient.disconnect();
          } else {
            Serial.print("✗ Connection failed (code: ");
            Serial.print(testClient.state());
            Serial.println(")");
            Serial.println("  Error codes:");
            Serial.println("    -4: Connection timeout");
            Serial.println("    -3: Connection lost");
            Serial.println("    -2: Connect failed");
            Serial.println("    -1: Disconnected");
            Serial.println("     5: Connection refused (bad credentials)");
          }
        }
        
      } else if (cmd == "mqtt-save") {
        preferences.begin("mqtt", false);
        preferences.putBool("enabled", mqttEnabled);
        preferences.putString("server", mqttServer);
        preferences.putUShort("port", mqttPort);
        preferences.putString("username", mqttUsername);
        preferences.putString("password", mqttPassword);
        preferences.putString("topic", mqttTopic);
        preferences.putString("device", mqttDeviceName);
        preferences.putULong("interval", mqttPublishInterval);
        preferences.putBool("incTime", mqttIncludeTimestamp);
        preferences.putBool("incBME", mqttIncludeBME280);
        preferences.putBool("incGPS", mqttIncludeGPS);
        preferences.putBool("incBatt", mqttIncludeBattery);
        preferences.putBool("incPAX", mqttIncludePAX);
        preferences.putBool("incSys", mqttIncludeSystem);
        preferences.end();
        Serial.println("✓ MQTT settings saved to NVS");
        
        // Reconnect if enabled
        if (mqttEnabled && wifiConnected) {
          if (mqttClient.connected()) {
            mqttClient.disconnect();
          }
          mqttClient.setServer(mqttServer.c_str(), mqttPort);
          Serial.println("  MQTT client reconfigured");
        }
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
      Serial.println("  mqtt-status  - View MQTT configuration");
      Serial.println("\nType 'menu' for complete command list\n");
      
    } else if (cmd.length() > 0) {
      Serial.print("✗ Unknown command: '");
      Serial.print(cmd);
      Serial.println("'");
      Serial.println("Type 'menu' for available commands or 'help' for quick help\n");
    }
  } // end if (rawCmd.length() > 0)
}

/**
 * @brief OLED boot splash — 2.5 s title + MvK credit screen
 *
 * Call immediately after display.begin() succeeds.
 * Layout (128×64 SSD1306):
 *   Border rect → "T-BEAM" large → separator → "MvK / Floor 7.5"
 *   → "Markus van Kempen" → separator → version + date
 */
