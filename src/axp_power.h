#pragma once
bool axpWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(AXP192_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool axpRead(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(AXP192_I2C_ADDR);
  Wire.write(reg);
  // Use STOP here instead of repeated-start to avoid intermittent
  // i2cWriteReadNonStop errors on some ESP32 Wire implementations.
  if (Wire.endTransmission(true) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(AXP192_I2C_ADDR), 1) != 1) {
    return false;
  }
  value = Wire.read();
  return true;
}

bool axpSetPowerBits(uint8_t setMask, uint8_t clearMask) {
  uint8_t value = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, value)) {
    return false;
  }
  value |= setMask;
  value &= static_cast<uint8_t>(~clearMask);
  return axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, value);
}

void gpsPowerPulseResetByAxp() {
  if (!axpFound) {
    return;
  }

  Serial.println("GPS reset: pulsing AXP192 LDO3 OFF/ON");
  if (!axpSetPowerBits(0x00, AXP192_BIT_LDO3)) {
    Serial.println("GPS reset: failed to disable LDO3");
    return;
  }
  delay(GPS_LDO3_RESET_OFF_MS);

  if (!axpSetPowerBits(AXP192_BIT_LDO3, 0x00)) {
    Serial.println("GPS reset: failed to enable LDO3");
    return;
  }
  delay(GPS_LDO3_RESET_ON_SETTLE_MS);
  Serial.println("GPS reset: LDO3 restored");
}

void resetGpsModuleViaGpio() {
  Serial.print("GPS reset: toggling GPIO ");
  Serial.print(GPS_RESET_PIN);
  Serial.println(" (T22 V1.1 20191212 fix)");
  
  pinMode(GPS_RESET_PIN, OUTPUT);
  
  // Hold GPS module in reset state
  digitalWrite(GPS_RESET_PIN, LOW);
  delay(200);
  
  // Release reset and allow GPS to boot
  digitalWrite(GPS_RESET_PIN, HIGH);
  delay(500);
  
  Serial.println("GPS reset: GPIO reset complete");
}

void axpPrintReg(uint8_t reg, const char *name) {
  uint8_t v = 0;
  if (axpRead(reg, v)) {
    Serial.print("AXP ");
    Serial.print(name);
    Serial.print(" (0x");
    Serial.print(reg, HEX);
    Serial.print(")=0x");
    if (v < 0x10) {
      Serial.print('0');
    }
    Serial.println(v, HEX);
  } else {
    Serial.print("AXP read fail reg 0x");
    Serial.println(reg, HEX);
  }
}

void logAxp192Detailed() {
  if (!axpFound) {
    Serial.println("AXP192 DETAIL: skipped (AXP not detected)");
    return;
  }

  Serial.println("================ AXP192 DETAIL ====================");
  axpPrintReg(0x00, "PWR_STATUS");
  axpPrintReg(0x01, "CHG_STATUS");
  axpPrintReg(0x02, "MODE_CHG");
  axpPrintReg(0x12, "PWR_OUT_CTRL");
  axpPrintReg(0x28, "LDO23_VOLT");
  axpPrintReg(0x82, "ADC_ENABLE_1");
  axpPrintReg(0x83, "ADC_ENABLE_2");
  axpPrintReg(0x84, "ADC_TS");

  uint8_t pwrOut = 0;
  if (axpRead(0x12, pwrOut)) {
    Serial.print("AXP rails DCDC1=");
    Serial.print((pwrOut & 0x01) ? "ON" : "OFF");
    Serial.print(" DCDC2=");
    Serial.print((pwrOut & 0x10) ? "ON" : "OFF");
    Serial.print(" DCDC3=");
    Serial.print((pwrOut & 0x02) ? "ON" : "OFF");
    Serial.print(" LDO2=");
    Serial.print((pwrOut & 0x04) ? "ON" : "OFF");
    Serial.print(" LDO3=");
    Serial.print((pwrOut & 0x08) ? "ON" : "OFF");
    Serial.print(" EXTEN=");
    Serial.println((pwrOut & 0x40) ? "ON" : "OFF");
  }

  Serial.println("===================================================");
}

void setupAxp192PowerForGps() {
  uint8_t current = 0;
  if (!axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, current)) {
    Serial.println("AXP192 not found on I2C 0x34");
    axpFound = false;
    return;
  }

  axpFound = true;
  Serial.println("================ AXP192 BATTERY SETUP =============");
  Serial.print("AXP192 power reg 0x12 before=0x");
  Serial.println(current, HEX);

  // Match LilyGO T22 GPS sketch: DCDC1, DCDC2, LDO2, LDO3, EXTEN.
  // These rails are required on many T22 boards for GPS to operate reliably.
  const uint8_t requiredBits = 0x5D;
  uint8_t updated = current | requiredBits;

  if (!axpWrite(AXP192_REG_POWER_OUTPUT_CONTROL, updated)) {
    Serial.println("AXP192 write failed");
    return;
  }

  uint8_t verify = 0;
  if (axpRead(AXP192_REG_POWER_OUTPUT_CONTROL, verify)) {
    Serial.print("AXP192 power reg 0x12 after =0x");
    Serial.println(verify, HEX);
  }

  // Configure battery charging (Register 0x33)
  // Bit 7: Charging enable (1=enable)
  // Bit 6-5: Target voltage (00=4.1V, 01=4.15V, 10=4.2V, 11=4.36V)
  // Bit 4: Charging end current (0=10%, 1=15%)
  // Bit 3-0: Charging current (0000=100mA to 1101=1320mA, steps of ~100mA)
  // Default: 0xC0 = 11000000 = Charging enabled, 4.2V, 10% end current, 100mA
  // Recommended for 18650: 0xC2 = 11000010 = Charging enabled, 4.2V, 10% end current, 300mA
  uint8_t chargingConfig = 0xC2;  // Enable charging, 4.2V target, 300mA current
  if (axpWrite(0x33, chargingConfig)) {
    Serial.print("AXP192 charging enabled: 0x");
    Serial.println(chargingConfig, HEX);
    Serial.println("  Target: 4.2V, Current: 300mA");
  } else {
    Serial.println("AXP192 charging config write failed");
  }

  // Enable ADC for battery monitoring (Register 0x82)
  // Bit 7: Battery voltage ADC enable
  // Bit 6: Battery current ADC enable
  // Bit 5: ACIN voltage ADC enable
  // Bit 4: ACIN current ADC enable
  // Bit 3: VBUS voltage ADC enable
  // Bit 2: VBUS current ADC enable
  // Bit 1: APS voltage ADC enable
  // Bit 0: TS pin ADC enable
  uint8_t adcEnable1 = 0x83;  // Enable battery voltage, current, and ACIN voltage ADC
  if (axpWrite(0x82, adcEnable1)) {
    Serial.println("AXP192 ADC enabled for battery monitoring");
  }

  // Read and display charging status
  uint8_t chargingStatus = 0;
  if (axpRead(0x01, chargingStatus)) {
    Serial.print("Charging status: 0x");
    Serial.println(chargingStatus, HEX);
    bool isCharging = (chargingStatus & 0x40) != 0;
    bool batteryConnected = (chargingStatus & 0x20) != 0;
    Serial.print("  Battery connected: ");
    Serial.println(batteryConnected ? "YES" : "NO");
    Serial.print("  Charging: ");
    Serial.println(isCharging ? "YES" : "NO");
  }

  Serial.println("===================================================");

  gpsPowerPulseResetByAxp();
}

