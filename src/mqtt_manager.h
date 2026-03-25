#pragma once
void connectMQTT() {
  if (!mqttEnabled || mqttServer.length() == 0) {
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  // Don't attempt reconnection too frequently
  if (millis() - lastMqttAttempt < mqttReconnectInterval) {
    return;
  }
  
  lastMqttAttempt = millis();
  
  if (mqttClient.connected()) {
    mqttConnected = true;
    return;
  }
  
  Serial.println("🔌 Connecting to MQTT broker: " + mqttServer + ":" + String(mqttPort));
  
  mqttClient.setServer(mqttServer.c_str(), mqttPort);
  
  String clientId = "TTGO-T-Beam-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  
  bool connected = false;
  if (mqttUsername.length() > 0) {
    connected = mqttClient.connect(clientId.c_str(), mqttUsername.c_str(), mqttPassword.c_str());
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }
  
  if (connected) {
    mqttConnected = true;
    Serial.println("✓ MQTT connected");
  } else {
    mqttConnected = false;
    Serial.print("✗ MQTT connection failed, rc=");
    Serial.println(mqttClient.state());
  }
}

/**
 * @brief Publish sensor data to MQTT broker in JSON format
 * 
 * Publishes all sensor readings, GPS data, and system status to MQTT topic
 */
void publishMQTT() {
  if (!mqttEnabled || !mqttConnected || !mqttClient.connected()) {
    return;
  }
  
  // Create JSON document
  JsonDocument doc;
  
  // Device info (always include device name)
  doc["device"] = mqttDeviceName;
  
  // Timestamp (optional)
  if (mqttIncludeTimestamp) {
    doc["timestamp"] = millis() / 1000;
  }
  
  // BME280 data (optional)
  if (mqttIncludeBME280 && bmeFound) {
    JsonObject bme = doc["bme280"].to<JsonObject>();
    bme["temperature"] = lastTemperature;
    bme["humidity"] = lastHumidity;
    bme["pressure"] = lastPressureHpa;
    bme["altitude"] = lastAltitude;
  }
  
  // GPS data (optional)
  if (mqttIncludeGPS && gps.location.isValid()) {
    JsonObject gpsData = doc["gps"].to<JsonObject>();
    gpsData["latitude"] = gps.location.lat();
    gpsData["longitude"] = gps.location.lng();
    gpsData["altitude"] = gps.altitude.meters();
    gpsData["satellites"] = gps.satellites.value();
    gpsData["hdop"] = gps.hdop.hdop();
    gpsData["speed"] = gps.speed.kmph();
    gpsData["course"] = gps.course.deg();
  }
  
  // Battery (optional)
  if (mqttIncludeBattery && axpFound) {
    doc["battery_voltage"] = readBatteryVoltage();
    doc["battery_charging"] = isBatteryCharging();
  }
  
  // PAX counter (optional)
  if (mqttIncludePAX && paxCounterEnabled) {
    JsonObject pax = doc["pax"].to<JsonObject>();
    pax["wifi"] = wifiDeviceCount;
    pax["ble"] = bleDeviceCount;
    pax["total"] = wifiDeviceCount + bleDeviceCount;
  }
  
  // System status (optional)
  if (mqttIncludeSystem) {
    JsonObject system = doc["system"].to<JsonObject>();
    system["uptime"] = millis() / 1000;
    system["free_heap"] = ESP.getFreeHeap();
    system["wifi_rssi"] = WiFi.RSSI();
    system["wifi_ip"] = WiFi.localIP().toString();
    system["lora_joined"] = loraJoined;
  }
  
  // Serialize to string
  String payload;
  serializeJson(doc, payload);
  
  // Publish
  Serial.println("\n📨 MQTT Publishing...");
  Serial.println("  Topic: " + mqttTopic);
  Serial.println("  Topic length: " + String(mqttTopic.length()) + " bytes");
  Serial.println("  Payload size: " + String(payload.length()) + " bytes");
  Serial.println("  MQTT buffer size: " + String(MQTT_MAX_PACKET_SIZE) + " bytes");
  Serial.println("  Total packet size: ~" + String(mqttTopic.length() + payload.length() + 10) + " bytes");
  
  // Check if payload fits in buffer
  if ((mqttTopic.length() + payload.length() + 10) > MQTT_MAX_PACKET_SIZE) {
    Serial.println("⚠️  WARNING: Packet size exceeds MQTT buffer!");
    Serial.println("  Increase MQTT_MAX_PACKET_SIZE in platformio.ini");
    Serial.println("  Add: -D MQTT_MAX_PACKET_SIZE=512");
  }
  
  if (debugEnabled || payload.length() > 200) {
    Serial.println("  Full payload: " + payload);
  }
  
  bool published = mqttClient.publish(mqttTopic.c_str(), payload.c_str());
  
  if (published) {
    mqttPublishCounter++;
    Serial.println("✓ MQTT published successfully (#" + String(mqttPublishCounter) + ")");
  } else {
    Serial.println("✗ MQTT publish failed!");
    Serial.println("  Error code: " + String(mqttClient.state()));
    Serial.println("  Connected: " + String(mqttClient.connected() ? "Yes" : "No"));
    Serial.println("  Broker: " + mqttServer + ":" + String(mqttPort));
    
    // Provide helpful troubleshooting
    if (payload.length() > 128) {
      Serial.println("\n💡 LIKELY CAUSE: Payload too large for default MQTT buffer (128 bytes)");
      Serial.println("   SOLUTION: Add to platformio.ini build_flags:");
      Serial.println("   -D MQTT_MAX_PACKET_SIZE=512");
    }
  }
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
