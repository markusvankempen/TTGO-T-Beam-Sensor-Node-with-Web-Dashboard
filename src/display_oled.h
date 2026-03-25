#pragma once
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
  
  // Show current GPS if valid, otherwise show last known position
  if (gps.location.isValid()) {
    display.print("Lat: ");
    display.println(String(gps.location.lat(), 5));
    display.print("Lon: ");
    display.println(String(gps.location.lng(), 5));
  } else if (lastGoodLat != 0.0 || lastGoodLon != 0.0) {
    display.print("Last: ");
    display.println(String(lastGoodLat, 5));
    display.print("      ");
    display.println(String(lastGoodLon, 5));
  } else {
    display.print("Lat: -");
    display.println();
    display.print("Lon: -");
  }
}

// Clock page uses the ESP32 system clock, which is in turn synchronized from
// GPS when plausible date/time and fix quality are available.
void drawPageGpsTime() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(currentShiftX(), currentShiftY());
  display.setTextSize(1);
  display.print("CLOCK ");
  display.println(clockSynced ? (ntpSynced ? "NTP" : "GPS") : "WAIT");

  if (hasSystemTime()) {
    time_t now = time(nullptr);
    struct tm utc = {};
    gmtime_r(&now, &utc);

    // Apply UTC offset manually — avoids IRAM-resident localtime_r/strftime/tzset chain
    time_t adj = now + (time_t)ntpTzOffsetSec;
    struct tm local = {}; gmtime_r(&adj, &local);

    display.setTextSize(2);
    char hhmm[9];
    if (time24h) {
      snprintf(hhmm, sizeof(hhmm), "%02d:%02d", local.tm_hour, local.tm_min);
    } else {
      int h12 = local.tm_hour % 12; if (h12 == 0) h12 = 12;
      snprintf(hhmm, sizeof(hhmm), "%2d:%02d%s", h12, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
    }

    display.setTextSize(1);
    char dateLine[22];
    snprintf(dateLine, sizeof(dateLine), "%04d-%02d-%02d %s",
             local.tm_year + 1900,
             local.tm_mon + 1,
             local.tm_mday,
             ntpTzAbbr);
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
void showBootSplash() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  // "T-BEAM" centred at textSize=2 (12 px/char × 6 = 72 px; (128-72)/2 = 28)
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(28, 4);
  display.print("T-BEAM");

  display.drawLine(1, 22, 126, 22, SSD1306_WHITE);

  display.setTextSize(1);
  // "MvK / Floor 7.5" — 15 chars × 6 = 90 px; (128-90)/2 = 19
  display.setCursor(19, 26);
  display.print("MvK / Floor 7.5");

  // "Markus van Kempen" — 18 chars × 6 = 108 px; (128-108)/2 = 10
  display.setCursor(10, 36);
  display.print("Markus van Kempen");

  display.drawLine(1, 46, 126, 46, SSD1306_WHITE);

  // "v2.7.x" left | "YYYY-MM-DD" right
  display.setCursor(4, 50);
  display.print("v");
  display.print(FIRMWARE_VERSION);
  display.setCursor(64, 50);   // "2026-03-23" = 10 × 6 = 60 px; 64+60 = 124 < 127 ✓
  display.print(FIRMWARE_DATE);

  display.display();
  delay(2500);
}

// One-time platform initialization sequence.
// Order matters: power rails -> display -> GPS -> sensors -> LoRa join.
