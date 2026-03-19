# LoRaWAN Downlink Commands - Enhanced Protocol

**Author:** Markus van Kempen | markus.van.kempen@gmail.com  
**Organization:** Research | Floor 7½ 🏢🤏

## Overview

The TTGO T-Beam supports comprehensive remote control via LoRaWAN downlink messages. This document describes both the **simple** (backward compatible) and **enhanced** (0xAA prefix) command protocols.

---

## Protocol Formats

### Simple Commands (Backward Compatible)
Single-byte commands for basic operations.

**Format:** `[CMD]`

### Enhanced Commands (Recommended)
Multi-byte structured commands with parameters.

**Format:** `[0xAA] [CMD] [PARAMS...]`

---

## Simple Commands

### 0x01 - WiFi ON
**Payload:** `01`  
**Description:** Enable WiFi and reconnect to saved network

### 0x02 - WiFi OFF  
**Payload:** `02`  
**Description:** Disable WiFi to save power

### 0x03 - WiFi Reset
**Payload:** `03`  
**Description:** Clear saved WiFi credentials and restart in AP mode

### 0x04 - Device Restart
**Payload:** `04`  
**Description:** Restart the entire device

### 0x05 - Debug ON
**Payload:** `05`  
**Description:** Enable verbose debugging output

### 0x06 - Debug OFF
**Payload:** `06`  
**Description:** Disable verbose debugging output

---

## Enhanced Commands (0xAA Protocol)

### AA 01 - WiFi Control

#### AA 01 00 - WiFi OFF
**Payload:** `AA 01 00`  
**Description:** Disable WiFi (power saving mode)  
**Example (TTN):** `AA0100`

```
Response: ✓ WiFi disabled
Power Savings: ~80mA reduction
```

#### AA 01 01 - WiFi ON
**Payload:** `AA 01 01`  
**Description:** Enable WiFi and web server  
**Example (TTN):** `AA0101`

```
Response: ✓ WiFi enabled
```

---

### AA 02 - GPS Control

#### AA 02 00 - GPS OFF
**Payload:** `AA 02 00`  
**Description:** Disable GPS module (power saving)  
**Example (TTN):** `AA0200`

```
Response: ✓ GPS disabled (power saving)
Power Savings: ~40mA reduction
Use Case: Indoor deployment, stationary nodes
```

#### AA 02 01 - GPS ON
**Payload:** `AA 02 01`  
**Description:** Enable GPS module  
**Example (TTN):** `AA0201`

```
Response: ✓ GPS enabled
```

---

### AA 03 - Sleep Mode with Wake Interval

**Payload:** `AA 03 [HH] [HH] [HH] [HH]`  
**Description:** Enable deep sleep mode with configurable wake interval  
**Parameters:** 4 bytes (32-bit unsigned integer) - wake interval in seconds

#### Examples:

**Wake every 1 hour (3600 seconds):**
```
Payload: AA 03 00 00 0E 10
Calculation: 3600 = 0x00000E10
```

**Wake every 24 hours (86400 seconds):**
```
Payload: AA 03 00 01 51 80
Calculation: 86400 = 0x00015180
TTN Console: AA0300015180
```

**Wake every 12 hours (43200 seconds):**
```
Payload: AA 03 00 00 A8 C0
Calculation: 43200 = 0x0000A8C0
```

```
Response: ✓ Sleep mode enabled, wake every 86400 seconds
          ✓ Next wake in 24 hours
          
Behavior:
- Device sends one uplink before sleeping
- Enters ESP32 deep sleep
- Wakes after specified interval
- Sends uplink immediately after wake
- Returns to sleep cycle

Power Savings: ~1mA during sleep (vs ~200mA active)
Battery Life: Months to years depending on interval
```

---

### AA 04 - LoRa Send Interval

**Payload:** `AA 04 [HH] [HH] [HH] [HH]`  
**Description:** Set LoRaWAN uplink transmission interval  
**Parameters:** 4 bytes (32-bit unsigned integer) - interval in seconds  
**Minimum:** 10 seconds

#### Examples:

**Send every 1 minute (60 seconds):**
```
Payload: AA 04 00 00 00 3C
Calculation: 60 = 0x0000003C
TTN Console: AA040000003C
```

**Send every 5 minutes (300 seconds - default):**
```
Payload: AA 04 00 00 01 2C
Calculation: 300 = 0x0000012C
```

**Send every 15 minutes (900 seconds):**
```
Payload: AA 04 00 00 03 84
Calculation: 900 = 0x00000384
```

**Send every 1 hour (3600 seconds):**
```
Payload: AA 04 00 00 0E 10
Calculation: 3600 = 0x00000E10
```

```
Response: ✓ LoRa send interval set to 300 seconds
          ✓ Sending every 5 minutes
          
Note: Interval persists until changed or device restart
Fair Use: Respect TTN fair use policy (30 seconds airtime/day)
```

---

### AA 05 - Sleep Mode Control

#### AA 05 00 - Sleep Mode OFF
**Payload:** `AA 05 00`  
**Description:** Disable sleep mode, return to continuous operation  
**Example (TTN):** `AA0500`

```
Response: ✓ Sleep mode disabled
```

---

### AA 06 - Debug Control

#### AA 06 00 - Debug OFF
**Payload:** `AA 06 00`  
**Description:** Disable verbose debugging (production mode)  
**Example (TTN):** `AA0600`

```
Response: ✓ Debug disabled
Effect: Minimal serial output, only important events shown
```

#### AA 06 01 - Debug ON
**Payload:** `AA 06 01`  
**Description:** Enable verbose debugging (troubleshooting mode)  
**Example (TTN):** `AA0601`

```
Response: ✓ Debug enabled
Effect: Detailed logging of all operations
```

---

## Serial Commands

All downlink functions are also available via serial commands:

### WiFi Commands
```
wifi-on          - Enable WiFi
wifi-off         - Disable WiFi
wifi-status      - Show WiFi status
wifi-reset       - Reset WiFi credentials
```

### GPS Commands
```
gps-on           - Enable GPS
gps-off          - Disable GPS
gps-status       - Show GPS status
```

### Sleep Mode Commands
```
sleep-on         - Enable sleep mode
sleep-off        - Disable sleep mode
sleep-interval <seconds> - Set wake interval
sleep-status     - Show sleep mode status
```

### LoRa Commands
```
lora-interval <seconds> - Set send interval
lora-keys        - Show LoRa keys
```

### System Commands
```
status           - Complete system status
debug-on         - Enable debugging
debug-off        - Disable debugging
restart          - Restart device
menu             - Show command menu
```

---

## Usage Examples

### The Things Network (TTN)

1. Go to your device in TTN Console
2. Navigate to "Messaging" → "Downlink"
3. Enter hex payload
4. Select FPort (any port 1-223)
5. Click "Schedule downlink"

**Examples:**
```
WiFi OFF:           AA0100
GPS OFF:            AA0200
Sleep 24h:          AA0300015180
Send every 15min:   AA0400000384
Debug OFF:          AA0600
```

### ChirpStack

```bash
# WiFi OFF
mosquitto_pub -h localhost -t "application/1/device/DEVICE_EUI/command/down" \
  -m '{"devEUI":"DEVICE_EUI","fPort":1,"data":"qAEA"}'

# Sleep mode 24 hours (base64 encoded)
mosquitto_pub -h localhost -t "application/1/device/DEVICE_EUI/command/down" \
  -m '{"devEUI":"DEVICE_EUI","fPort":1,"data":"qAMAAVGA"}'
```

### TTN CLI

```bash
# WiFi OFF
ttn-lw-cli end-devices downlink push myapp mydevice --frm-payload AA0100

# GPS OFF + WiFi OFF (power saving)
ttn-lw-cli end-devices downlink push myapp mydevice --frm-payload AA0200
ttn-lw-cli end-devices downlink push myapp mydevice --frm-payload AA0100

# Sleep mode 24 hours
ttn-lw-cli end-devices downlink push myapp mydevice --frm-payload AA0300015180
```

---

## Power Management Scenarios

### Maximum Power Saving
```
1. AA 02 00  - GPS OFF (~40mA saved)
2. AA 01 00  - WiFi OFF (~80mA saved)
3. AA 03 00 01 51 80 - Sleep 24h (~199mA saved during sleep)

Total savings: ~200mA → ~1mA
Battery life: Months to years
```

### Balanced Mode
```
1. AA 02 00  - GPS OFF (stationary node)
2. AA 04 00 00 03 84 - Send every 15 minutes
3. WiFi ON for remote access

Power: ~120mA average
Battery life: Days to weeks
```

### High Frequency Monitoring
```
1. GPS ON
2. WiFi ON
3. AA 04 00 00 00 3C - Send every 1 minute

Power: ~200mA average
Battery life: Hours to days
Use: Active tracking, testing
```

---

## Hex Calculation Helper

### Convert Seconds to Hex

**Python:**
```python
seconds = 86400  # 24 hours
hex_value = hex(seconds)[2:].upper().zfill(8)
payload = f"AA03{hex_value}"
print(payload)  # AA0300015180
```

**JavaScript:**
```javascript
const seconds = 86400;  // 24 hours
const hex = seconds.toString(16).toUpperCase().padStart(8, '0');
const payload = `AA03${hex}`;
console.log(payload);  // AA0300015180
```

**Online Calculator:**
```
1. Go to: https://www.rapidtables.com/convert/number/decimal-to-hex.html
2. Enter seconds (e.g., 86400)
3. Result: 15180
4. Pad to 8 digits: 00015180
5. Payload: AA0300015180
```

---

## Common Intervals

| Interval | Seconds | Hex (4 bytes) | Payload |
|----------|---------|---------------|---------|
| 1 minute | 60 | 0000003C | AA040000003C |
| 5 minutes | 300 | 0000012C | AA040000012C |
| 15 minutes | 900 | 00000384 | AA0400000384 |
| 30 minutes | 1800 | 00000708 | AA0400000708 |
| 1 hour | 3600 | 00000E10 | AA0400000E10 |
| 6 hours | 21600 | 00005460 | AA0400005460 |
| 12 hours | 43200 | 0000A8C0 | AA040000A8C0 |
| 24 hours | 86400 | 00015180 | AA0300015180 |
| 1 week | 604800 | 00093A80 | AA030009 3A80 |

---

## Troubleshooting

**Downlink not received:**
- Device must be joined to network
- Check downlink scheduling in network server
- Device must be in RX window (after uplink)
- Verify hex payload format

**Command not executed:**
- Check serial monitor for error messages
- Verify command byte is correct
- Ensure parameter length matches requirement
- Check for 0xAA prefix on enhanced commands

**Sleep mode issues:**
- Device sends uplink before sleeping
- Wake interval must be > 0
- Deep sleep resets device state
- LoRa session persists across sleep

**Power consumption still high:**
- Verify GPS is disabled (AA 02 00)
- Verify WiFi is disabled (AA 01 00)
- Check for other peripherals drawing power
- Measure current during sleep cycle

---

## Security Considerations

- Commands processed immediately upon receipt
- No additional authentication beyond LoRaWAN
- Sleep mode can be disabled remotely
- WiFi reset exposes AP mode
- Consider command rate limiting for production

---

## Integration Examples

### Node-RED Flow
```json
{
  "id": "power_save",
  "type": "function",
  "func": "if (msg.payload.battery < 3.3) {\n  return {payload: 'AA0100'};\n}",
  "outputs": 1
}
```

### Python Script
```python
import requests

def set_sleep_mode(hours):
    seconds = hours * 3600
    hex_val = format(seconds, '08X')
    payload = f"AA03{hex_val}"
    # Send to TTN/Chirpstack API
    return payload

# Sleep for 24 hours
payload = set_sleep_mode(24)
print(payload)  # AA0300015180
```

---

## Best Practices

1. **Test commands** in development before production
2. **Document** your downlink strategy
3. **Monitor** power consumption after changes
4. **Respect** TTN fair use policy
5. **Plan** for device wake windows
6. **Consider** battery capacity vs. interval
7. **Use** sleep mode for long-term deployments
8. **Keep** WiFi off unless needed
9. **Disable** GPS for stationary nodes
10. **Set** appropriate send intervals

---

## Support

For issues or questions:
- Email: markus.van.kempen@gmail.com
- Check serial monitor for detailed logs
- Use `status` command to verify settings
- Enable debug mode for troubleshooting

**Version:** 3.0  
**Last Updated:** 2026-03-19