#pragma once
/**
 * @brief Read battery voltage from AXP192
 *
 * Reads the battery voltage ADC registers and converts to voltage.
 * Battery voltage is stored in registers 0x78 (MSB) and 0x79 (LSB)
 * Resolution: 1.1mV per LSB
 *
 * @return float Battery voltage in volts, or -1.0 if read fails
 */
float readBatteryVoltage() {
  if (!axpFound) return -1.0;
  
  uint8_t msb = 0, lsb = 0;
  if (!axpRead(0x78, msb) || !axpRead(0x79, lsb)) {
    return -1.0;
  }
  
  // Combine MSB and LSB (12-bit value)
  uint16_t raw = (msb << 4) | (lsb & 0x0F);
  
  // Convert to voltage: 1.1mV per LSB
  float voltage = raw * 1.1 / 1000.0;
  
  return voltage;
}

/**
 * @brief Read battery charge/discharge current from AXP192
 *
 * Positive = charging, Negative = discharging
 *
 * @return float Current in mA, or 0.0 if read fails
 */
float readBatteryCurrent() {
  if (!axpFound) return 0.0;
  
  // Read charge current (0x7A, 0x7B)
  uint8_t chargeMsb = 0, chargeLsb = 0;
  if (axpRead(0x7A, chargeMsb) && axpRead(0x7B, chargeLsb)) {
    uint16_t chargeRaw = (chargeMsb << 5) | (chargeLsb & 0x1F);
    float chargeCurrent = chargeRaw * 0.5;  // 0.5mA per LSB
    
    // Read discharge current (0x7C, 0x7D)
    uint8_t dischargeMsb = 0, dischargeLsb = 0;
    if (axpRead(0x7C, dischargeMsb) && axpRead(0x7D, dischargeLsb)) {
      uint16_t dischargeRaw = (dischargeMsb << 5) | (dischargeLsb & 0x1F);
      float dischargeCurrent = dischargeRaw * 0.5;  // 0.5mA per LSB
      
      // Return net current (positive = charging, negative = discharging)
      return chargeCurrent - dischargeCurrent;
    }
  }
  
  return 0.0;
}

/**
 * @brief Check if battery is currently charging
 *
 * @return bool True if charging, false otherwise
 */
bool isBatteryCharging() {
  if (!axpFound) return false;
  
  uint8_t status = 0;
  if (axpRead(0x01, status)) {
    return (status & 0x40) != 0;  // Bit 6 = charging indicator
  }
  
  return false;
}

/**
 * @brief Enable GPS power via AXP192
 *
 * Turns on LDO3 which powers the GPS module
 */
void enableGpsPower() {
  if (!axpFound) return;
  
  uint8_t current = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, current)) {
    return;
  }
  
  // Set LDO3 bit (bit 3) to enable GPS power
  uint8_t updated = current | 0x08;  // LDO3 enable
  
  if (axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, updated)) {
    Serial.println("✓ GPS power enabled (AXP192 LDO3 ON)");
    delay(100); // Allow GPS to power up
  }
}

/**
 * @brief Disable GPS power via AXP192
 *
 * Turns off LDO3 which powers the GPS module
 * This actually powers down the GPS hardware, stopping the LED
 */
void disableGpsPower() {
  if (!axpFound) return;
  
  uint8_t current = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, current)) {
    return;
  }
  
  // Clear LDO3 bit (bit 3) to disable GPS power
  uint8_t updated = current & ~0x08;  // LDO3 disable
  
  if (axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, updated)) {
    Serial.println("✓ GPS power disabled (AXP192 LDO3 OFF) - LED should stop blinking");
  }
}

/**
 * @brief Enable OLED display
 */
void enableDisplay() {
  displayEnabled = true;
  display.ssd1306_command(0xAF); // Display ON
  display.clearDisplay();
  display.display();
  Serial.println("✓ Display enabled");
}

/**
 * @brief Disable OLED display
 */
void disableDisplay() {
  displayEnabled = false;
  display.clearDisplay();
  display.display();
  display.ssd1306_command(0xAE); // Display OFF
  Serial.println("✓ Display disabled (power saving)");
}

bool initBme() {
  // Try configured address first, then fall back to the other standard BME280 address.
  if (bme.begin(bme280I2cAddr)) {
    Serial.printf("BME280 found at 0x%02X\n", bme280I2cAddr);
    return true;
  }
  uint8_t altAddr = (bme280I2cAddr == 0x76) ? 0x77 : 0x76;
  if (bme.begin(altAddr)) {
    Serial.printf("BME280 found at 0x%02X (fallback)\n", altAddr);
    return true;
  }
  return false;
}

void showBootScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("TTGO T-Beam start");
  display.println("OLED: OK");
  display.print("BME280: ");
  display.println(bmeFound ? "OK" : "NOT FOUND");
  display.print("GPS UART: ");
  display.print(GPS_BAUD);
  display.println(" bps");
  display.display();
}

void updateSensorReadings() {
  if (!bmeFound) {
    return;
  }

  bool valid = false;
  for (uint8_t i = 0; i < BME_READ_RETRIES; i++) {
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0f;
    float a = bme.readAltitude(seaLevelPressureHpa);

    if (isnan(t) || isnan(h) || isnan(p) || isnan(a)) {
      delay(20);
      continue;
    }

    if (t < BME_MIN_TEMP_C || t > BME_MAX_TEMP_C ||
        h < BME_MIN_HUMIDITY || h > BME_MAX_HUMIDITY ||
        p < BME_MIN_PRESSURE_HPA || p > BME_MAX_PRESSURE_HPA) {
      delay(20);
      continue;
    }

    lastTemperature = t;
    lastHumidity = h;
    lastPressureHpa = p;
    lastAltitude = a;
    valid = true;
    break;
  }

  if (!valid) {
    if (debugEnabled) {
      Serial.println("BME read invalid after retries, keeping previous values");
    }
    return;
  }

  if (debugEnabled) {
    Serial.print("ENV tempC=");
    Serial.print(lastTemperature, 2);
    Serial.print(" humidityPct=");
    Serial.print(lastHumidity, 2);
    Serial.print(" pressureHpa=");
    Serial.print(lastPressureHpa, 2);
    Serial.print(" altitudeM=");
    Serial.println(lastAltitude, 2);
  }
}

// Rotating page renderer with burn-in protection handled elsewhere.
