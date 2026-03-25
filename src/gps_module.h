#pragma once
void sendUbxFrame(uint8_t msgClass, uint8_t msgId, const uint8_t *payload, uint16_t payloadLen) {
  uint8_t ckA = 0;
  uint8_t ckB = 0;

  auto addChecksum = [&](uint8_t b) {
    ckA = static_cast<uint8_t>(ckA + b);
    ckB = static_cast<uint8_t>(ckB + ckA);
  };

  GpsSerial.write(0xB5);
  GpsSerial.write(0x62);

  GpsSerial.write(msgClass);
  addChecksum(msgClass);
  GpsSerial.write(msgId);
  addChecksum(msgId);

  uint8_t lenL = static_cast<uint8_t>(payloadLen & 0xFF);
  uint8_t lenH = static_cast<uint8_t>((payloadLen >> 8) & 0xFF);
  GpsSerial.write(lenL);
  addChecksum(lenL);
  GpsSerial.write(lenH);
  addChecksum(lenH);

  for (uint16_t i = 0; i < payloadLen; i++) {
    GpsSerial.write(payload[i]);
    addChecksum(payload[i]);
  }

  GpsSerial.write(ckA);
  GpsSerial.write(ckB);
  GpsSerial.flush();
}

void reenableUbxNmeaOutput() {
  if (!GPS_UBX_REENABLE_NMEA_AT_BOOT) {
    return;
  }

  Serial.println("GPS UBX recovery: forcing UART1 NMEA output");

  // CFG-PRT (0x06 0x00): set UART1 to 9600, 8N1, in=UBX+NMEA, out=NMEA.
  const uint8_t cfgPrtUart1[] = {
    0x01, 0x00,             // portID=UART1, reserved
    0x00, 0x00,             // txReady
    0xD0, 0x08, 0x00, 0x00, // mode: 8N1
    0x80, 0x25, 0x00, 0x00, // baud: 9600
    0x03, 0x00,             // inProtoMask: UBX+NMEA
    0x02, 0x00,             // outProtoMask: NMEA
    0x00, 0x00,             // flags
    0x00, 0x00              // reserved
  };
  sendUbxFrame(0x06, 0x00, cfgPrtUart1, sizeof(cfgPrtUart1));
  delay(120);

  // CFG-MSG (0x06 0x01): ensure GGA and RMC are enabled on output.
  const uint8_t cfgMsgGga[] = {0xF0, 0x00, 0x01};
  const uint8_t cfgMsgRmc[] = {0xF0, 0x04, 0x01};
  sendUbxFrame(0x06, 0x01, cfgMsgGga, sizeof(cfgMsgGga));
  delay(80);
  sendUbxFrame(0x06, 0x01, cfgMsgRmc, sizeof(cfgMsgRmc));
  delay(80);

  Serial.println("GPS UBX recovery: commands sent");
}

void logRawNmeaChar(char c, unsigned long now) {
  if (!GPS_RAW_NMEA_DEBUG_ENABLED) {
    return;
  }
  if (gpsRawDebugStartAt == 0 || (now - gpsRawDebugStartAt) > GPS_RAW_NMEA_DEBUG_MS) {
    return;
  }

  if (c == '\r') {
    return;
  }

  if (c == '\n') {
    if (gpsRawLine.length() > 0) {
      Serial.print("NMEA RAW: ");
      Serial.println(gpsRawLine);
      gpsRawLine = "";
    }
    return;
  }

  if (gpsRawLine.length() < 120) {
    gpsRawLine += c;
  }
}

bool i2cDevicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool hasSystemTime() {
  return time(nullptr) > 100000;
}

// Reject placeholder/no-fix date-time so the internal clock is not poisoned by
// invalid GPS frames (for example 2000-00-00 00:00:00).
bool isPlausibleGpsDateTime() {
  if (!gps.date.isValid() || !gps.time.isValid()) {
    return false;
  }

  int year = gps.date.year();
  int month = gps.date.month();
  int day = gps.date.day();
  int hour = gps.time.hour();
  int minute = gps.time.minute();
  int second = gps.time.second();

  if (year < 2024 || year > 2099) {
    return false;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }
  if (hour > 23 || minute > 59 || second > 59) {
    return false;
  }

  // Avoid syncing from placeholder/no-fix streams.
  if (!gps.location.isValid()) {
    return false;
  }
  if (gps.satellites.isValid() && gps.satellites.value() < 1) {
    return false;
  }
  if (gps.hdop.isValid() && gps.hdop.hdop() >= 50.0f) {
    return false;
  }

  return true;
}

// Synchronize ESP32 system UTC clock from GPS once quality is acceptable.
// The clock is refreshed occasionally to limit drift while avoiding spam writes.
void syncSystemClockFromGpsIfNeeded(unsigned long nowMs) {
  if (!isPlausibleGpsDateTime()) {
    return;
  }
  if (gps.date.age() > 2000 || gps.time.age() > 2000) {
    return;
  }
  if (clockSynced && (nowMs - lastClockSyncAt < CLOCK_RESYNC_MS)) {
    return;
  }

  struct tm t = {};
  t.tm_year = gps.date.year() - 1900;
  t.tm_mon = gps.date.month() - 1;
  t.tm_mday = gps.date.day();
  t.tm_hour = gps.time.hour();
  t.tm_min = gps.time.minute();
  t.tm_sec = gps.time.second();

  time_t epoch = mktime(&t);
  struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  settimeofday(&tv, nullptr);

  clockSynced = true;
  lastClockSyncAt = nowMs;
  Serial.print("Clock synced from GPS epoch=");
  Serial.println(static_cast<unsigned long>(epoch));
}

void printSystemUtcToSerial() {
  if (!hasSystemTime()) {
    Serial.print("invalid");
    return;
  }

  time_t now = time(nullptr);
  struct tm utc = {};
  gmtime_r(&now, &utc);
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d UTC",
           utc.tm_year + 1900,
           utc.tm_mon + 1,
           utc.tm_mday,
           utc.tm_hour,
           utc.tm_min,
           utc.tm_sec);
  Serial.print(buffer);
}

int8_t currentShiftX() {
  return SHIFT_X[burnInShiftIndex % (sizeof(SHIFT_X) / sizeof(SHIFT_X[0]))];
}

int8_t currentShiftY() {
  return SHIFT_Y[burnInShiftIndex % (sizeof(SHIFT_Y) / sizeof(SHIFT_Y[0]))];
}

void startGpsSerial(uint8_t profileIndex) {
  gpsProfileIndex = profileIndex % GPS_PROFILE_COUNT;
  GpsSerial.end();
  delay(20);
  GpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_PROFILES[gpsProfileIndex].rx, GPS_PROFILES[gpsProfileIndex].tx);
  gpsStreamSeen = false;

  Serial.print("GPS serial started: ");
  Serial.print(GPS_PROFILES[gpsProfileIndex].name);
  Serial.print(" @");
  Serial.print(GPS_BAUD);
  Serial.print(" RX=");
  Serial.print(GPS_PROFILES[gpsProfileIndex].rx);
  Serial.print(" TX=");
  Serial.println(GPS_PROFILES[gpsProfileIndex].tx);

  reenableUbxNmeaOutput();
}

void printTwoDigits(int value) {
  if (value < 10) {
    Serial.print('0');
  }
  Serial.print(value);
}

void printGpsTimeToSerial() {
  if (gps.time.isValid() && gps.date.isValid()) {
    Serial.print(gps.date.year());
    Serial.print('-');
    printTwoDigits(gps.date.month());
    Serial.print('-');
    printTwoDigits(gps.date.day());
    Serial.print(' ');
    printTwoDigits(gps.time.hour());
    Serial.print(':');
    printTwoDigits(gps.time.minute());
    Serial.print(':');
    printTwoDigits(gps.time.second());
    Serial.print('.');
    Serial.print(gps.time.centisecond());
    Serial.print(" UTC");
  } else {
    Serial.print("invalid");
  }
}

