# TTGO T-Beam Sensor Node — End-to-End Guide

**Version:** 2.7.4  
**Author:** Markus van Kempen  
**Email:** markus.van.kempen@gmail.com  
**Organization:** Research | Floor 7½ 🏢🤏  
**Motto:** "No bug too small, no syntax too weird."  
**Last Updated:** 2026-03-24

---

## Table of Contents

1. [Hardware Overview](#1-hardware-overview)
2. [Building & Flashing Firmware](#2-building--flashing-firmware)
3. [First Boot & Serial Monitor](#3-first-boot--serial-monitor)
4. [WiFi Configuration](#4-wifi-configuration)
5. [Web Dashboard Walkthrough](#5-web-dashboard-walkthrough)
6. [Serial Command Reference](#6-serial-command-reference)
7. [LoRaWAN Configuration](#7-lorawan-configuration)
8. [OTA Firmware Update](#8-ota-firmware-update)
9. [PAX Counter (Crowd Monitoring)](#9-pax-counter-crowd-monitoring)
10. [MQTT Integration](#10-mqtt-integration)
11. [LoRaWAN Downlink Commands](#11-lorawan-downlink-commands)
12. [CayenneLPP Payload Reference](#12-cayennelpp-payload-reference)
13. [Troubleshooting](#13-troubleshooting)

---

## 1. Hardware Overview

### Board
**TTGO T-Beam T22 V1.1** (20191212)

| Component | Details |
|-----------|---------|
| MCU | ESP32 dual-core 240 MHz |
| LoRa radio | SX1276 (US915 / 868 MHz configurable) |
| GNSS | NEO-6M / NEO-M8N (UART) |
| Display | SSD1306 128×64 OLED (I²C) |
| Power IC | AXP192 (battery + GPS rail management) |
| Environment sensor | BME280 (temperature, humidity, pressure) |
| Connectivity | WiFi 2.4 GHz + BLE 4.2 |

### Pin Map

| Signal | GPIO |
|--------|------|
| I²C SDA | 21 |
| I²C SCL | 22 |
| GPS RX (board input) | 34 |
| GPS TX (board output) | 12 |
| LoRa NSS | 18 |
| LoRa RST | 23 |
| LoRa DIO0 | 26 |
| LoRa DIO1 | 33 |
| LoRa DIO2 | 32 |

---

## 2. Building & Flashing Firmware

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- USB-to-serial driver for CP210x / CH340 (board-dependent)

### Clone and Build

```bash
git clone https://github.com/markusvankempen/TTGO-T-Beam-Sensor-Node-with-Web-Dashboard.git
cd TTGO-T-Beam-Sensor-Node-with-Web-Dashboard

# Copy and fill in your LoRaWAN credentials
cp include/lorawan_secrets.h.example include/lorawan_secrets.h
# Edit include/lorawan_secrets.h with your APPEUI, DEVEUI, APPKEY

# Build
platformio run

# Flash (replace port with your device port)
platformio run --target upload --upload-port /dev/cu.usbserial-XXXXXXXX
```

> **Partition note:** The firmware uses `min_spiffs.csv` which provides two OTA slots (each 1.875 MB). This is required for web-based OTA updates to work correctly. Do **not** change to `huge_app.csv` — that has only one slot and OTA will crash at the flash-erase step.

### Monitor Serial Output

```bash
platformio device monitor --baud 115200 --port /dev/cu.usbserial-XXXXXXXX
```

---

## 3. First Boot & Serial Monitor

### Boot Banner (Serial, 115200 baud)

```
============================================================
  TTGO T-Beam Sensor Node v2.7.3 (2026-03-23)
  LoRaWAN + WiFi + GPS + BME280 + PAX Counter
============================================================
Initializing BLE scanner for PAX counter...
✓ BLE scanner ready
Setup LoRa... OK
WiFi AP started: TTGO-T-Beam-Setup
Web server started
IP: 192.168.4.1
Joining LoRaWAN network (OTAA)...
```

> **Init order matters:** BLE is initialised **before** WiFi — this is required by the ESP32 coexistence layer to avoid heap exhaustion and `abort()` at boot.

### OLED Display on Boot

The OLED cycles through information screens (anti-burn-in pixel shift active):

1. **Splash** — firmware version, date
2. **GPS status** — satellites, fix quality, lat/lon
3. **LoRa stats** — uplink counter, last TX time
4. **Sensor data** — temperature, humidity, pressure
5. **System** — uptime, battery voltage, WiFi status

---

## 4. WiFi Configuration

### First-Time Setup (AP Mode)

If no WiFi credentials are saved the device starts an Access Point:

| Setting | Value |
|---------|-------|
| SSID | `TTGO-T-Beam-Setup` |
| Password | `12345678` |
| Config URL | `http://192.168.4.1` |

1. Connect your phone or laptop to `TTGO-T-Beam-Setup`
2. Open `http://192.168.4.1` in a browser
3. Navigate to **Settings → WiFi**
4. Enter your network SSID and password, click **Save**
5. The device restarts and connects — the serial monitor prints:
   ```
   WiFi connected!  SSID: YourNetwork  IP: 192.168.1.105  RSSI: -62 dBm
   ```

### Changing WiFi via Serial

```
wifi-set <SSID> <password>   # Save new credentials
wifi-on                       # Connect with saved credentials
wifi-off                      # Disconnect and disable WiFi
wifi-info                     # Show saved SSID and current status
wifi-reset                    # Erase credentials, restart AP mode
```

> **WiFi + BLE coexistence:** When both WiFi and BLE (PAX counter) are active simultaneously, the firmware automatically enables WiFi modem sleep (`WIFI_PS_MIN_MODEM`) as required by the ESP32 coexistence driver.

---

## 5. Web Dashboard Walkthrough

Once connected to your network, open `http://<device-ip>/` in any browser.

---

### Dashboard `/`

![Dashboard](screenshots/dashboard.png)

Live sensor readings updated every 5 seconds:

| Metric | Source |
|--------|--------|
| Temperature (°C) | BME280 |
| Humidity (%) | BME280 |
| Pressure (hPa) | BME280 |
| Altitude (m, baro) | BME280 |
| GPS status, sats, HDOP | NEO GPS |
| Latitude / Longitude | NEO GPS |
| LoRa uplink counter | LMIC |
| Battery voltage (V) | AXP192 |
| PAX count | WiFi + BLE scan |

An **OpenStreetMap** panel (no API key) shows a live GPS marker with a breadcrumb trail of up to 100 past positions.

---

### Settings `/settings`

![Settings](screenshots/settings.png)

| Section | Controls |
|---------|---------|
| **WiFi** | SSID / password, enable / disable |
| **GPS** | Enable / disable, force-on override |
| **Display** | Enable / disable OLED |
| **Sleep mode** | Enable / disable, wake interval (s) |
| **LoRa interval** | TX interval 30–3600 s |
| **Debug mode** | Toggle verbose serial logging |
| **GPS path size** | 5–100 positions (default 20) |

All changes persist to NVS flash and survive power cycles.

---

### LoRa Configuration `/config`

![LoRa Config](screenshots/config.png)

Configure LoRaWAN keys and activation mode:

- **Mode** — OTAA (recommended) or ABP
- **OTAA** — AppEUI (JoinEUI), DevEUI, AppKey
- **ABP** — DevAddr, NwkSKey, AppSKey
- **Region** — US915 set in `platformio.ini`; EU868 available by recompile

> Keys are persisted in NVS. Sensitive values are masked after saving.

---

### Payload Configuration `/payload-info`

![Payload Info](screenshots/payload-info.png)

Enable or disable individual CayenneLPP channels to control payload size and airtime:

| Channel | LPP Type | Sensor |
|---------|----------|--------|
| 1 | Temperature | BME280 |
| 2 | Humidity | BME280 |
| 3 | Pressure | BME280 |
| 4 | Analog | BME altitude |
| 5 | GPS | lat/lon/alt |
| 6 | Analog | HDOP |
| 7 | Analog | Satellite count |
| 8 | Analog | GPS seconds-of-day |
| 9 | Analog | Battery voltage |
| 10 | Analog | PAX counter |

Toggle switches take effect on the next uplink. Disabled channels produce zero bytes — no stubs.

---

### Diagnostics `/diagnostics`

![Diagnostics](screenshots/diagnostics.png)

| Item | Description |
|------|-------------|
| Uplink counter | Total frames sent since boot |
| Downlink counter | Total frames received |
| Frame counter | LMIC `seqnoUp` (NVS-persisted) |
| Heap free | Current free heap bytes |
| Uptime | Days, hours, minutes, seconds |
| WiFi RSSI | Signal strength to AP |
| Last downlink | Raw hex of last received frame |
| **Reset frame counter** | Clears `seqnoUp` in NVS — use when rejoining TTN after device swap |

---

### Debug Console `/debug`

![Debug Console](screenshots/debug.png)

Real-time WebSocket stream of all serial log output:

- Autoscrolls, configurable line limit (default 200)
- Colour-coded by log level
- **Clear** button to empty buffer
- Toggle `debug-on` / `debug-off` directly from the page

The console mirrors exactly what the serial monitor shows.

---

### About `/about`

![About](screenshots/about.png)

Firmware version, build date, author details, license (Apache 2.0), and GitHub link.

---

### MQTT Configuration `/mqtt`

Configure MQTT broker and JSON publishing:

| Field | Example |
|-------|---------|
| Broker server | `mqtt.example.com` |
| Port | `1883` |
| Username | `device1` |
| Password | (masked) |
| Topic | `ttgo/node01/telemetry` |
| Device name | `Node-01` |
| Publish interval | `60` s |

The **Test** button sends a single JSON publish immediately.

---

### OTA Update `/ota`

- **File upload** — browse for a `.bin` file and upload directly
- **URL download** — enter a firmware URL, device fetches and flashes it

During upload the **OLED display** shows live progress:

```
┌──────────────────┐        ┌──────────────────┐        ┌──────────────────┐
│  Firmware Update │  →     │  Firmware Update │  →     │  Upload complete!│
│  Uploading...    │        │  512 KB written  │        │  Restarting...   │
│  Do NOT power off│        │  Do NOT power off│        │                  │
└──────────────────┘        └──────────────────┘        └──────────────────┘
```

> **Safety:** OTA writes to the *inactive* partition slot first. The running firmware is never touched until the new image passes CRC. If the update fails, the device stays on the previous firmware.

---

## 6. Serial Command Reference

Connect at **115200 baud**. Type `help` to print the full command list.

### System & Status

| Command | Description |
|---------|-------------|
| `help` | Print all available commands |
| `menu` | Interactive numbered configuration menu |
| `status` | Complete system snapshot — WiFi, GPS, LoRa, sensors, heap |
| `version` | Print firmware version and build date |
| `debug-on` | Enable verbose logging |
| `debug-off` | Disable verbose logging |

### WiFi

| Command | Description |
|---------|-------------|
| `wifi-on` | Connect using saved credentials |
| `wifi-off` | Disconnect and stop WiFi |
| `wifi-info` | Show saved SSID and connection status |
| `wifi-set <SSID> <pass>` | Save new credentials |
| `wifi-reset` | Erase credentials, restart in AP mode |

### GPS

| Command | Description |
|---------|-------------|
| `gps-on` | Enable GPS (powers AXP192 LDO3 rail) |
| `gps-off` | Disable GPS (~30 mA saving) |

### Display

| Command | Description |
|---------|-------------|
| `display-on` | Enable OLED |
| `display-off` | Disable OLED (~4 mA saving) |

### Sleep / Power

| Command | Description |
|---------|-------------|
| `sleep-on` | Enable deep-sleep cycling |
| `sleep-off` | Disable deep-sleep, run continuously |
| `sleep-interval <sec>` | Set wake interval, e.g. `sleep-interval 300` |

### LoRa

| Command | Description |
|---------|-------------|
| `lora-interval <sec>` | Set uplink TX interval, e.g. `lora-interval 60` |
| `lora-keys` | View/configure OTAA or ABP keys interactively |

### PAX Counter

| Command | Description |
|---------|-------------|
| `pax-on` | Enable PAX counter (WiFi + BLE scan) |
| `pax-off` | Disable PAX counter |
| `pax-wifi-on` / `pax-wifi-off` | Enable/disable WiFi scan component |
| `pax-ble-on` / `pax-ble-off` | Enable/disable BLE scan component |
| `pax-interval <sec>` | Scan interval 30–3600 s |
| `pax-status` | Show current PAX config and device count |

### Payload Channels

| Command | Description |
|---------|-------------|
| `channel-enable <1-10>` | Enable a CayenneLPP channel |
| `channel-disable <1-10>` | Disable a CayenneLPP channel |
| `channel-status` | Show all channel states |

### MQTT

| Command | Description |
|---------|-------------|
| `mqtt-on` / `mqtt-off` | Enable/disable MQTT publishing |
| `mqtt-status` | Show broker config and connection state |
| `mqtt-server <host>` | Set broker hostname or IP |
| `mqtt-port <port>` | Set broker port (1–65535) |
| `mqtt-user <user>` | Set username |
| `mqtt-pass <pass>` | Set password |
| `mqtt-topic <topic>` | Set publish topic |
| `mqtt-device <name>` | Set device name in JSON payload |
| `mqtt-interval <sec>` | Set publish interval (5–3600) |
| `mqtt-test` | Send a single test publish now |
| `mqtt-save` | Persist MQTT settings to NVS |

### Example Serial Session

```
> status
--- System Status ---
Version : 2.7.3 (2026-03-23)
Uptime  : 0d 00:14:32
Heap    : 186240 bytes free
WiFi    : CONNECTED  SSID: HomeNetwork  IP: 192.168.1.105  RSSI: -58 dBm
GPS     : LOCKED  Sats: 9  HDOP: 0.9  Lat: 37.7749  Lon: -122.4194
BME280  : 22.4 °C  58.1 %RH  1013.2 hPa
Battery : 3.87 V
LoRa    : uplinks=142  seqnoUp=142
PAX     : 7 devices  (WiFi: 5  BLE: 2)
Debug   : OFF

> pax-status
PAX counter: ENABLED
  WiFi scan: ENABLED
  BLE scan : ENABLED
  Interval : 60 s
  Current  : 7 devices (WiFi: 5, BLE: 2)

> lora-interval 120
LoRa send interval set to 120 seconds.

> channel-status
Channel  1 [Temperature    ] ENABLED
Channel  2 [Humidity       ] ENABLED
Channel  3 [Pressure       ] ENABLED
Channel  4 [Altitude       ] ENABLED
Channel  5 [GPS            ] ENABLED
Channel  6 [HDOP           ] ENABLED
Channel  7 [Satellites     ] ENABLED
Channel  8 [GPS seconds    ] DISABLED
Channel  9 [Battery        ] ENABLED
Channel 10 [PAX counter    ] ENABLED

> debug-on
Debug logging enabled.
[DEBUG] GPS: lat=37.7749 lon=-122.4194 sats=9 hdop=0.90 fix=3
[DEBUG] LoRa: EV_TXSTART seqnoUp=143
[DEBUG] LoRa: EV_TXCOMPLETE uplinks=143
```

---

## 7. LoRaWAN Configuration

### Keys File

Edit `include/lorawan_secrets.h`:

```cpp
// AppEUI / JoinEUI — LITTLE-ENDIAN for LMIC (reverse byte order from TTN)
static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// DevEUI — LITTLE-ENDIAN for LMIC (reverse byte order from TTN)
static const u1_t PROGMEM DEVEUI[8] = { 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX };

// AppKey — BIG-ENDIAN (copy exactly as shown in TTN console)
static const u1_t PROGMEM APPKEY[16] = { 0xXX, 0xXX, ... };
```

> **Key byte order — the most common mistake:** APPEUI and DEVEUI are stored **LSB first** in LMIC. If TTN shows `70 B3 D5 7E D0 04 00 00`, store it as `{ 0x00, 0x00, 0x04, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 }`. APPKEY stays big-endian exactly as shown.

### Region

Set in `platformio.ini`:

```ini
build_flags =
    -D CFG_us915=1    ; US915 (default)
    ; -D CFG_eu868=1  ; EU868 — uncomment to switch
```

### OTAA Join Flow (Serial)

```
Joining LoRaWAN network (OTAA)...
EV_JOINING
EV_TXSTART
EV_JOINED  ✓ Network joined!  DevAddr: 26 01 2A 4F
First uplink scheduled in 5 seconds.
EV_TXCOMPLETE  uplinks=1
```

---

## 8. OTA Firmware Update

### Requirements

- `min_spiffs.csv` partition table (two OTA slots, 1.875 MB each) — already set
- Device connected to WiFi (STA or AP mode both work)
- `.bin` file: `.pio/build/ttgo-t-beam/firmware.bin`

### File Upload

1. Open `http://<device-ip>/ota`
2. Click **Choose File**, select `firmware.bin`
3. Click **Upload**
4. OLED shows progress — wait for auto-reboot

### URL Download

1. Host `firmware.bin` on any HTTP server reachable from the device
2. Paste the URL into **Firmware URL** and click **Download & Flash**

### What Happens Internally

```
POST /api/ota-upload  →  GPIO interrupts disabled (DIO0/1/2)
                      →  NimBLE host task torn down (deinit)
                      →  delay(300) — FreeRTOS settles
                      →  Update.begin(UPDATE_SIZE_UNKNOWN)
                      →  chunks written to inactive OTA slot
                      →  Update.end(true)  — CRC verified
                      →  ESP.restart()
```

---

## 9. PAX Counter (Crowd Monitoring)

Scans for nearby WiFi and BLE devices; reports unique device count on CayenneLPP channel 10.

### How It Works

Each scan interval the firmware:
1. Passive WiFi scan — records unique probe MAC addresses
2. BLE active scan — counts advertisement packets
3. Reports combined unique count on next LoRaWAN uplink

### ESP32 Coexistence Notes

- BLE initialised **before** WiFi (required by ESP-IDF coexistence layer)
- `WIFI_PS_MIN_MODEM` activated automatically when both stacks run
- LMIC DIO interrupts disabled via `gpio_intr_disable()` (not `detachInterrupt()`, which does not work with LMIC's `esp_intr_alloc()` ISRs) before OTA flash operations

### Serial Configuration

```
pax-on              # Enable PAX counter
pax-interval 120    # Scan every 2 minutes
pax-ble-off         # WiFi only (saves ~30 KB heap)
channel-enable 10   # Include count in LoRa uplink
```

---

## 10. MQTT Integration

### Quick Setup (Serial)

```
mqtt-server mqtt.example.com
mqtt-port 1883
mqtt-user myuser
mqtt-pass mypassword
mqtt-topic sensors/node01
mqtt-device Node-01
mqtt-interval 60
mqtt-save
mqtt-on
mqtt-test   # Verify connectivity
```

### JSON Payload

```json
{
  "device": "Node-01",
  "version": "2.7.3",
  "ts": 1742745123,
  "temperature": 22.4,
  "humidity": 58.1,
  "pressure": 1013.2,
  "altitude_baro": 45.0,
  "battery": 3.87,
  "lat": 37.7749,
  "lon": -122.4194,
  "gps_alt": 12.0,
  "satellites": 9,
  "hdop": 0.9,
  "pax": 7,
  "lora_uplinks": 142,
  "uptime_s": 3612
}
```

---

## 11. LoRaWAN Downlink Commands

Send downlink frames from TTN / ChirpStack Console on **FPort 1**.

### Simple Commands (single byte)

| Byte | Action |
|------|--------|
| `0x01` | WiFi ON |
| `0x02` | WiFi OFF |
| `0x05` | Debug logging ON |
| `0x06` | Debug logging OFF |

### Enhanced Protocol (0xAA prefix)

| Bytes | Action |
|-------|--------|
| `AA 01 00` | WiFi OFF |
| `AA 01 01` | WiFi ON |
| `AA 02 00` | GPS OFF |
| `AA 02 01` | GPS ON |
| `AA 03 XX XX XX XX` | Set sleep wake interval (4-byte big-endian seconds) |
| `AA 04 XX XX XX XX` | Set LoRa TX interval (4-byte big-endian seconds) |
| `AA 05 00` | Sleep mode OFF |
| `AA 06 00` | Debug OFF |
| `AA 06 01` | Debug ON |
| `AA 07 00` | Display OFF |
| `AA 07 01` | Display ON |

### Hex Calculation Examples

| Desired action | Downlink bytes |
|----------------|---------------|
| WiFi OFF | `AA 01 00` |
| GPS OFF (power saving) | `AA 02 00` |
| Display OFF (power saving) | `AA 07 00` |
| Sleep every 24 h (86400 s = 0x00015180) | `AA 03 00 01 51 80` |
| Send every 15 min (900 s = 0x00000384) | `AA 04 00 00 03 84` |
| Send every 30 s (burst mode) | `AA 04 00 00 00 1E` |

---

## 12. CayenneLPP Payload Reference

Uplink **FPort 1**, encoding: CayenneLPP.

| Ch | LPP Type | Data | Notes |
|----|----------|------|-------|
| 1 | Temperature | BME280 °C | –327.67 to +327.67 |
| 2 | Relative Humidity | BME280 % | 0–100 |
| 3 | Barometric Pressure | BME280 hPa | 260–1260 |
| 4 | Analog Input | Altitude m | Saturates above 327 m |
| 5 | GPS | lat, lon, alt | Full precision |
| 6 | Analog Input | HDOP | <1=ideal, >20=poor |
| 7 | Analog Input | Satellite count | 0–32 |
| 8 | Analog Input | GPS seconds-of-day | Always saturates (>327.67) |
| 9 | Analog Input | Battery V | 0.00–327.67 |
| 10 | Analog Input | PAX count | Unique devices seen |

> **Analog Input limits:** Signed 16-bit × 0.01 → ~–327.68 to +327.67. Channels 4 and 8 can exceed this range. Disable unused channels via `/payload-info` to reduce airtime.

### TTN Payload Formatter

```javascript
function decodeUplink(input) {
  // Use the standard CayenneLPP community decoder from TTN marketplace
  // or: npm install cayenne-lpp
  return Decoder(input.bytes, input.fPort);
}
```

---

## 13. Troubleshooting

### Joins but no application data
- Confirm you are viewing **uplink** events, not only join events
- Confirm payload arrives on FPort `1`
- Check serial for `LoRa uplink #... payload=...`

### GPS has NMEA output but no fix (`sats=0`, `hdop=99.99`)
- UART is working — this is usually no sky visibility yet
- Move outdoors with clear sky and wait for cold-start lock (up to 60 s with good antenna placement)
- Verify GPS antenna U.FL connector is seated

### GPS LED not blinking (no power)

The most common GPS issue — blue LED not blinking means the AXP192 LDO3 rail is off.

**Immediate fix via serial:**
```
gps-on
```
You should see: `✓ GPS power enabled (AXP192 LDO3 ON)` and the LED starts blinking within 2 seconds.

**Automatic fix (v2.6.1+):** The firmware detects GPS disabled in NVS and forces it on at boot:
```
⚠️  GPS was disabled in NVS - forcing GPS ON
✓ GPS power enabled (AXP192 LDO3 ON)
```

### OTA upload fails / device crashes during OTA
- Confirm `platformio.ini` uses `board_build.partitions = min_spiffs.csv`
- `huge_app.csv` has only one OTA slot — OTA **will** crash with it
- Check free heap before OTA (`status` command — should be >100 KB)

### WiFi fails to connect after credentials saved
- Use `wifi-info` to confirm the correct SSID is stored
- Use `wifi-reset` to clear and reconfigure
- Check signal strength — device needs RSSI above approximately –85 dBm

### LoRa DIO interrupt errors at boot
```
GPIO isr service is not installed
```
This is a known ESP-IDF message that appears only when `detachInterrupt()` is called for a pin managed by LMIC. It is harmless — the firmware uses `gpio_intr_disable()` (hardware-level) for OTA preparation instead.

### Web pages returning 500 / blank
- Check free heap (`status` command — needs >50 KB for page generation)
- Restart the device; heap may have fragmented after extended uptime

---

*Generated for firmware v2.7.4 — 2026-03-24*  
*Repository: [TTGO-T-Beam-Sensor-Node-with-Web-Dashboard](https://github.com/markusvankempen/TTGO-T-Beam-Sensor-Node-with-Web-Dashboard)*  
*License: Apache License 2.0*
