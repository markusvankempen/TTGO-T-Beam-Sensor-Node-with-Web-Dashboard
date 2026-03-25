#pragma once
void showWiFiStatusOnDisplay(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("WiFi Setup");
  display.println("----------");
  display.println(line1);
  if (line2) display.println(line2);
  if (line3) display.println(line3);
  display.display();
}

void configureWiFiRadio() {
  WiFi.persistent(false);
  // Disable built-in auto-reconnect: it races with our manual reconnect
  // logic in loop() and produces duplicate WiFi.begin() calls that stall
  // the driver.  We manage reconnects explicitly.
  WiFi.setAutoReconnect(false);

  // Maximum TX power improves reliability at weak signal levels (-80 dBm+)
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // ESP32 coexistence rule: WiFi modem sleep MUST be enabled when BLE is
  // also active. Using WIFI_PS_NONE with BLE causes an abort().
  bool bleActive = paxCounterEnabled && paxBleScanEnabled;
  if (bleActive) {
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    WiFi.setSleep(true);
  } else {
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setSleep(false);
  }

  wifi_country_t country = {"EU", 1, 13, WIFI_COUNTRY_POLICY_MANUAL};
  esp_wifi_set_country(&country);
}

void logWiFiScanForTarget(const String& targetSsid) {
  int networkCount = WiFi.scanNetworks(false, true, false, 300);
  if (networkCount < 0) {
    Serial.println("⚠️  WiFi scan failed before connect attempt");
    return;
  }

  Serial.print("📶 WiFi scan: ");
  Serial.print(networkCount);
  Serial.println(" network(s) visible:");

  bool found = false;
  for (int i = 0; i < networkCount; i++) {
    bool isTarget = (WiFi.SSID(i) == targetSsid);
    if (isTarget) found = true;

    Serial.print(isTarget ? "  ► " : "    ");
    Serial.print(WiFi.SSID(i).isEmpty() ? "(hidden)" : WiFi.SSID(i));
    Serial.print("  ch=");
    Serial.print(WiFi.channel(i));
    Serial.print("  rssi=");
    Serial.print(WiFi.RSSI(i));
    Serial.print("  enc=");
    Serial.println(static_cast<int>(WiFi.encryptionType(i)));
  }

  if (!found) {
    Serial.print("⚠️  Target SSID \"");
    Serial.print(targetSsid);
    Serial.println("\" is not visible to the ESP32");
    Serial.println("   Check that the network is 2.4 GHz, not WPA3-only, and on channels 1-13");
  }

  WiFi.scanDelete();
}

// ---------------------------------------------------------------------------
// Minimal NTP sync via raw POSIX sockets (no WiFiUDP — avoids IRAM callbacks).
// Uses time.google.com's stable anycast IP to skip DNS entirely.
// ---------------------------------------------------------------------------
bool fetchNtpTime() {
  if (!wifiConnected) return false;

  // time.google.com anycast IPs — hardcoded to avoid DNS IRAM overhead
  const char* ntpIPs[] = {"216.239.35.0", "216.239.35.4", "129.6.15.28"};

  for (const char* ntpIP : ntpIPs) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(123);
    addr.sin_addr.s_addr = inet_addr(ntpIP);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) continue;

    struct timeval tv = {2, 0};  // 2-second receive timeout
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf[48] = {};
    buf[0] = 0x1B;  // LI=0, VN=3, Mode=3 (client)

    if (sendto(sock, buf, 48, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      close(sock); continue;
    }

    socklen_t addrLen = sizeof(addr);
    int n = recvfrom(sock, buf, 48, 0, (struct sockaddr*)&addr, &addrLen);
    close(sock);

    if (n < 48) continue;

    unsigned long epoch =
        ((unsigned long)buf[40] << 24 | (unsigned long)buf[41] << 16 |
         (unsigned long)buf[42] <<  8 |  (unsigned long)buf[43])
        - 2208988800UL;

    if (epoch < 1700000000UL) continue;  // sanity: must be after Nov 2023

    struct timeval tvset = {(time_t)epoch, 0};
    settimeofday(&tvset, nullptr);
    return true;
  }
  return false;
}

void setupWiFi() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║                    WiFi Setup                              ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  
  // Set WiFi hostname to T-Beam-XXXXXX (using last 6 chars of MAC)
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  String hostname = "T-Beam-" + macAddress.substring(6); // Last 6 chars
  WiFi.setHostname(hostname.c_str());
  Serial.print("📛 WiFi Hostname: ");
  Serial.println(hostname);
  
  // Show WiFi setup on display
  showWiFiStatusOnDisplay("Initializing...");
  
  // Check if boot button is pressed for WiFi reset
  pinMode(WIFI_RESET_BUTTON_PIN, INPUT);
  if (digitalRead(WIFI_RESET_BUTTON_PIN) == LOW) {
    Serial.println("🔘 Boot button pressed - clearing WiFi credentials");
    showWiFiStatusOnDisplay("Clearing WiFi", "credentials...");
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    delay(1000);
  }
  
  // Load saved WiFi credentials from preferences
  preferences.begin("wifi", true);
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();

  configureWiFiRadio();

  bool connectedToSavedNetwork = false;
  
  // Try to connect to saved WiFi if credentials exist
  if (savedSSID.length() > 0) {
    Serial.println("📡 Attempting to connect to saved network...");
    Serial.print("   SSID: ");
    Serial.println(savedSSID);
    
    showWiFiStatusOnDisplay("Connecting to", savedSSID.c_str(), "Please wait...");
    
    // Set STA mode and clear any stale connection state BEFORE scanning
    // so the radio is in the correct mode when we call WiFi.begin().
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);   // release any lingering association
    delay(100);
    logWiFiScanForTarget(savedSSID);
    wifiConnecting = true;
    wifiConnectStartedAt = millis();
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    
    // Wait up to 40 seconds — weak signals (-80 dBm+) need more handshake time
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 80) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      connectedToSavedNetwork = true;
      wifiConnecting = false;

      // WL_CONNECTED fires before DHCP completes; wait up to 5 s for a real IP
      Serial.print("⏳ Waiting for DHCP...");
      for (int d = 0; d < 10; d++) {
        IPAddress chk = WiFi.localIP();
        if (chk != IPAddress(0,0,0,0) && chk != IPAddress(255,255,255,255)) break;
        delay(500);
        Serial.print(".");
      }
      Serial.println();

      Serial.println("\n╔════════════════════════════════════════════════════════════╗");
      Serial.println("║              ✓ WiFi Connected Successfully!               ║");
      Serial.println("╚════════════════════════════════════════════════════════════╝");
      Serial.print("🌐 IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("📶 SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("📡 RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      Serial.println("\n🌐 Web Dashboard: http://" + WiFi.localIP().toString());
      Serial.println("════════════════════════════════════════════════════════════\n");
      
      // Show success on display
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("WiFi Connected!");
      display.println("------------");
      display.print("SSID: ");
      display.println(WiFi.SSID());
      display.print("IP: ");
      display.println(WiFi.localIP());
      display.print("RSSI: ");
      display.print(WiFi.RSSI());
      display.println(" dBm");
      display.display();
      delay(2000);
      
      wifiConnected = true;

      // Sync time via raw UDP NTP (no lwip SNTP module → no IRAM impact)
      Serial.printf("⏰ Syncing time via NTP (%s UTC%+d)...\n",
                    ntpTzAbbr, ntpTzOffsetSec / 3600);
      if (fetchNtpTime()) {
        ntpSynced = true;
        clockSynced = true;
        lastClockSyncAt = millis();
        time_t now_t = time(nullptr);
        time_t adj = now_t + (time_t)ntpTzOffsetSec;
        struct tm lt = {}; gmtime_r(&adj, &lt);
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d %02d:%02d:%02d %s",
                 lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday,
                 lt.tm_hour, lt.tm_min, lt.tm_sec, ntpTzAbbr);
        Serial.println("✓ NTP synced: " + String(tbuf));
      } else {
        Serial.println("⚠️  NTP fetch failed, will retry in loop()");
      }
    } else {
      wifiConnecting = false;
      Serial.println("\n⚠️  Failed to connect to saved network");
    }
  }
  
  // If not connected, start AP mode
  if (!connectedToSavedNetwork) {
    Serial.println("\n🔄 Starting AP mode...");
    wifiConnected = false;
    wifiConfigMode = true;
    
    showWiFiStatusOnDisplay("Starting AP", "mode...");
    delay(1000);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("📡 AP Mode - SSID: ");
    Serial.println(WIFI_AP_NAME);
    Serial.print("🔑 Password: ");
    Serial.println(WIFI_AP_PASSWORD);
    Serial.print("🌐 AP IP address: ");
    Serial.println(IP);
    Serial.println("\n🌐 Web Dashboard: http://" + IP.toString());
    Serial.println("   Configure WiFi via the WiFi page");
    Serial.println("════════════════════════════════════════════════════════════\n");
    
    // Show AP info on display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WiFi AP Mode");
    display.println("------------");
    display.print("SSID: ");
    display.println(WIFI_AP_NAME);
    display.print("Pass: ");
    display.println(WIFI_AP_PASSWORD);
    display.print("IP: ");
    display.println(IP);
    display.display();
  }
  
  // Always start web server with application pages
  setupWebServer();
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
