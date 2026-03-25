#pragma once
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
  
  // WiFi scan (only if enabled and not connected to a network)
  if (paxWifiScanEnabled && WiFi.status() != WL_CONNECTED && !wifiConnecting) {
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
  
  // Channel 11: WiFi IP Address (if connected)
  // Encode IP as 4 bytes: each octet as analog input scaled 0-255
  if (wifiConnected) {
    IPAddress ip = WiFi.localIP();
    // Encode as single 32-bit value divided by 1000000 to fit in analog range
    float ipEncoded = (ip[0] * 1000000.0f + ip[1] * 10000.0f + ip[2] * 100.0f + ip[3]) / 1000000.0f;
    lpp.addAnalogInput(11, ipEncoded);
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

