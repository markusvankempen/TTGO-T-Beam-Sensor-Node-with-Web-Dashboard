#pragma once
void os_getArtEui(u1_t *buf) {
  memcpy(buf, storedAPPEUI, 8);
}

void os_getDevEui(u1_t *buf) {
  memcpy(buf, storedDEVEUI, 8);
}

void os_getDevKey(u1_t *buf) {
  memcpy(buf, storedAPPKEY, 16);
}

void printLoRaStatus() {
  Serial.print("LMIC opmode=0x");
  Serial.print(LMIC.opmode, HEX);
  Serial.print(" joined=");
  Serial.print(loraJoined ? 1 : 0);
  Serial.print(" txrxFlags=0x");
  Serial.print(LMIC.txrxFlags, HEX);
  Serial.print(" seqnoUp=");
  Serial.print(LMIC.seqnoUp);
  Serial.print(" seqnoDn=");
  Serial.println(LMIC.seqnoDn);
}

// Sleep mode variables
bool sleepModeEnabled = false;
uint32_t sleepWakeInterval = 86400; // Default: 24 hours in seconds
unsigned long lastWakeTime = 0;
bool gpsEnabled = true;
uint32_t loraSendIntervalSec = 300; // Default: 5 minutes

/**
 * @brief Process LoRaWAN downlink commands
 *
 * Enhanced protocol with 0xAA prefix for structured commands.
 *
 * Protocol Format: [PREFIX] [CMD] [PARAMS...]
 *
 * Simple Commands (backward compatible):
 * - 0x01: WiFi ON
 * - 0x02: WiFi OFF
 * - 0x03: WiFi Reset
 * - 0x04: Device Restart
 * - 0x05: Debug ON
 * - 0x06: Debug OFF
 *
 * Enhanced Commands (with 0xAA prefix):
 * - AA 01 00: WiFi OFF
 * - AA 01 01: WiFi ON
 * - AA 02 00: GPS OFF
 * - AA 02 01: GPS ON
 * - AA 03 [HH] [HH] [HH] [HH]: Sleep mode with wake interval (4 bytes, seconds in hex)
 *   Example: AA 03 00 01 51 80 = wake every 24 hours (86400 seconds)
 * - AA 04 [HH] [HH] [HH] [HH]: Set LoRa send interval (4 bytes, seconds in hex)
 *   Example: AA 04 00 00 00 3C = send every 60 seconds
 * - AA 05 00: Sleep mode OFF
 * - AA 06 00: Debug OFF
 * - AA 06 01: Debug ON
 *
 * @param data Pointer to downlink payload
 * @param len Length of downlink payload
 */
void processDownlinkCommand(uint8_t* data, uint8_t len) {
  if (len < 1) {
    Serial.println("⚠ Downlink: Empty payload");
    return;
  }
  
  // Check for enhanced protocol (0xAA prefix)
  if (data[0] == 0xAA && len >= 3) {
    uint8_t cmd = data[1];
    uint8_t param = data[2];
    
    Serial.print("📥 Enhanced downlink: AA ");
    Serial.print(cmd, HEX);
    Serial.print(" ");
    Serial.println(param, HEX);
    
    switch (cmd) {
      case 0x01: // WiFi control
        if (param == 0x00) {
          Serial.println("  → WiFi OFF");
          wifiEnabled = false;
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          Serial.println("  ✓ WiFi disabled");
        } else if (param == 0x01) {
          Serial.println("  → WiFi ON");
          wifiEnabled = true;
          WiFi.mode(WIFI_STA);
          WiFi.begin();
          Serial.println("  ✓ WiFi enabled");
        }
        break;
        
      case 0x02: // GPS control
        if (param == 0x00) {
          Serial.println("  → GPS OFF");
          gpsEnabled = false;
          disableGpsPower();  // Actually power down GPS hardware
        } else if (param == 0x01) {
          Serial.println("  → GPS ON");
          gpsEnabled = true;
          enableGpsPower();  // Power up GPS hardware
        }
        break;
        
      case 0x03: // Sleep mode with wake interval
        if (len >= 7) {
          sleepWakeInterval = ((uint32_t)data[3] << 24) |
                             ((uint32_t)data[4] << 16) |
                             ((uint32_t)data[5] << 8) |
                             (uint32_t)data[6];
          sleepModeEnabled = true;
          lastWakeTime = millis();
          Serial.print("  → Sleep mode enabled, wake every ");
          Serial.print(sleepWakeInterval);
          Serial.println(" seconds");
          Serial.print("  ✓ Next wake in ");
          Serial.print(sleepWakeInterval / 3600);
          Serial.println(" hours");
        }
        break;
        
      case 0x04: // LoRa send interval
        if (len >= 7) {
          loraSendIntervalSec = ((uint32_t)data[3] << 24) |
                                ((uint32_t)data[4] << 16) |
                                ((uint32_t)data[5] << 8) |
                                (uint32_t)data[6];
          Serial.print("  → LoRa send interval set to ");
          Serial.print(loraSendIntervalSec);
          Serial.println(" seconds");
          Serial.print("  ✓ Sending every ");
          Serial.print(loraSendIntervalSec / 60);
          Serial.println(" minutes");
        }
        break;
        
      case 0x05: // Sleep mode control
        if (param == 0x00) {
          Serial.println("  → Sleep mode OFF");
          sleepModeEnabled = false;
          Serial.println("  ✓ Sleep mode disabled");
        }
        break;
        
      case 0x06: // Debug control
        if (param == 0x00) {
          Serial.println("  → Debug OFF");
          debugEnabled = false;
          Serial.println("  ✓ Debug disabled");
        } else if (param == 0x01) {
          Serial.println("  → Debug ON");
          debugEnabled = true;
          Serial.println("  ✓ Debug enabled");
        }
        break;
        
      case 0x07: // Display control
        if (param == 0x00) {
          Serial.println("  → Display OFF");
          disableDisplay();
        } else if (param == 0x01) {
          Serial.println("  → Display ON");
          enableDisplay();
        }
        break;

      default:
        Serial.print("  ✗ Unknown enhanced command: 0x");
        Serial.println(cmd, HEX);
        break;
    }
    return;
  }
  
  // Legacy simple commands (backward compatible)
  uint8_t cmd = data[0];
  Serial.print("📥 Simple downlink: 0x");
  Serial.println(cmd, HEX);
  
  switch (cmd) {
    case 0x01: // WiFi ON
      Serial.println("  → WiFi ON");
      wifiEnabled = true;
      WiFi.mode(WIFI_STA);
      WiFi.begin();
      Serial.println("  ✓ WiFi enabled");
      break;
      
    case 0x02: // WiFi OFF
      Serial.println("  → WiFi OFF");
      wifiEnabled = false;
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      Serial.println("  ✓ WiFi disabled");
      break;
      
    case 0x03: // WiFi Reset
      Serial.println("  → WiFi Reset");
      preferences.begin("wifi", false);
      preferences.clear();
      preferences.end();
      delay(1000);
      ESP.restart();
      break;
      
    case 0x04: // Device Restart
      Serial.println("  → Device Restart");
      delay(1000);
      ESP.restart();
      break;
      
    case 0x05: // Debug ON
      Serial.println("  → Debug ON");
      debugEnabled = true;
      Serial.println("  ✓ Debug enabled");
      break;
      
    case 0x06: // Debug OFF
      Serial.println("  → Debug OFF");
      debugEnabled = false;
      Serial.println("  ✓ Debug disabled");
      break;
      
    default:
      Serial.print("  ✗ Unknown command: 0x");
      Serial.println(cmd, HEX);
      break;
  }
}

void onEvent(ev_t ev) {
  if (debugEnabled) {
    Serial.print("[LMIC] event=");
    Serial.print((unsigned)ev);
    Serial.print(" at ms=");
    Serial.println(millis());
  }

  switch (ev) {
    case EV_JOINING:
      if (debugEnabled) {
        Serial.println("[LMIC] Joining... OTAA request sent");
      }
      break;
    case EV_JOINED:
      Serial.println("[LMIC] ✓ Joined network");
      logDebug("✓ LoRaWAN network joined successfully");
      loraJoined = true;
      LMIC_setLinkCheckMode(0);
      // Trigger first uplink immediately after successful join.
      lastLoraSendAt = millis() - LORA_SEND_INTERVAL_MS;
      if (debugEnabled) {
        printLoRaStatus();
      }
      break;
    case EV_JOIN_FAILED:
      Serial.println("[LMIC] ✗ Join failed; restarting OTAA join");
      logDebug("✗ LoRaWAN join failed - retrying");
      loraJoined = false;
      LMIC_reset();
    #if defined(CFG_us915)
      LMIC_selectSubBand(1);
    #endif
      LMIC_startJoining();
      break;
    case EV_REJOIN_FAILED:
      Serial.println("[LMIC] ✗ Rejoin failed");
      logDebug("✗ LoRaWAN rejoin failed");
      loraJoined = false;
      break;
    case EV_TXSTART:
      if (debugEnabled) {
        Serial.println("[LMIC] TX start");
      }
      break;
    case EV_TXCOMPLETE:
      if (debugEnabled) {
        Serial.println("[LMIC] TX complete");
      }
      if (LMIC.txrxFlags & TXRX_ACK) {
        if (debugEnabled) {
          Serial.println("[LMIC] ACK received");
        }
      }
      if (LMIC.dataLen > 0) {
        downlinkCounter++;  // Increment downlink counter
        String downlinkMsg = "📥 Downlink received #" + String(downlinkCounter) + ": " + String(LMIC.dataLen) + " bytes - Data: ";
        for (int i = 0; i < LMIC.dataLen; i++) {
          if (LMIC.frame[LMIC.dataBeg + i] < 0x10) downlinkMsg += "0";
          downlinkMsg += String(LMIC.frame[LMIC.dataBeg + i], HEX);
          downlinkMsg += " ";
        }
        Serial.println(downlinkMsg);
        logDebug(downlinkMsg);
        
        // Process downlink commands
        processDownlinkCommand(LMIC.frame + LMIC.dataBeg, LMIC.dataLen);
      }
      if (debugEnabled) {
        printLoRaStatus();
      }
      break;
    default:
      break;
  }
}

// Configure LMIC and start OTAA join sequence or set ABP session.
void setupLoRa() {
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_NSS_PIN);
  os_init();
  LMIC_reset();
  // Wider tolerance helps when RX windows are missed due oscillator drift.
  LMIC_setClockError(MAX_CLOCK_ERROR * 10 / 100);

#if defined(CFG_us915)
  LMIC_selectSubBand(1);
  Serial.println("LoRa region US915, subband=1");
#endif

  if (useABP) {
    // ABP mode - set session keys directly
    Serial.println("LoRa: using ABP mode");
    
    // Set device address (big-endian)
    uint32_t devAddr = ((uint32_t)storedDEVADDR[0] << 24) |
                       ((uint32_t)storedDEVADDR[1] << 16) |
                       ((uint32_t)storedDEVADDR[2] << 8) |
                       (uint32_t)storedDEVADDR[3];
    
    // Set session keys and device address
    LMIC_setSession(0x1, devAddr, storedNWKSKEY, storedAPPSKEY);
    
    // Disable link check validation
    LMIC_setLinkCheckMode(0);
    
    // Set data rate and transmit power for US915
    LMIC_setDrTxpow(DR_SF7, 14);
    
    loraJoined = true;
    Serial.println("✓ ABP session configured");
    Serial.print("  DevAddr: 0x");
    Serial.println(devAddr, HEX);
  } else {
    // OTAA mode - start join procedure
    Serial.println("LoRa: starting OTAA join");
    LMIC_startJoining();
  }
}

// PAX Counter - Scan for WiFi and BLE devices
