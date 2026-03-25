#pragma once

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

// Forward declaration: printSystemUtcToSerial is defined in gps_module.h
void printSystemUtcToSerial();

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
  addToDebugBuffer(message);
  if (serialOutputEnabled) {
    Serial.println(message);
  }
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

