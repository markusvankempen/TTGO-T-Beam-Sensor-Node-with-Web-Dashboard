# TTGO T-Beam (T22 V1.1) Sensor Node

**Version:** 2.6.0
**Author:** Markus van Kempen
**Email:** Markus.van.Kempen@gmail.com
**Organization:** Research | Floor 7½ 🏢🤏
**Motto:** "No bug too small, no syntax too weird."
**License:** Apache License 2.0 (see [LICENSE](LICENSE))

---

This project runs on a TTGO T-Beam T22 V1.1 and integrates:
- BME280 (temperature, humidity, pressure)
- GPS (UART, TinyGPS++) with AXP192 power management
- OLED (SSD1306) with anti-burn-in
- LoRaWAN OTAA & ABP (LMIC, US915)
- CayenneLPP payload formatting with configurable channels
- **WiFi Web Dashboard** with real-time sensor data and GPS tracking
- **WiFi Manager** for easy configuration
- **OpenStreetMap integration** with live GPS marker (no API key required!)
- **PAX Counter** for crowd monitoring (WiFi + BLE scanning)
- **Diagnostics Page** with frame counter reset
- **Payload Configuration** page with channel enable/disable

## Hardware Notes

### Board/pin assumptions
- I2C SDA/SCL: `21/22`
- GPS UART (T22 V1.1): `RX=34`, `TX=12`
- LoRa SX1276 pins:
  - NSS: `18`
  - RST: `23`
  - DIO0/DIO1/DIO2: `26/33/32`

### Power management (AXP192)
On T22 boards, GPS depends on PMU rails being enabled. The firmware enables the required AXP192 outputs at boot.

## LoRaWAN Configuration

Region is configured in [platformio.ini](platformio.ini) (currently US915).

OTAA keys are configured in [include/lorawan_secrets.h](include/lorawan_secrets.h):
- `APPEUI` (JoinEUI, little-endian for LMIC)
- `DEVEUI` (little-endian for LMIC)
- `APPKEY` (big-endian)

## CayenneLPP Uplink Structure

Uplink port: `1`

Current channel map (all configurable via web UI):
1. Channel `1` -> Temperature (`addTemperature`) from BME280
2. Channel `2` -> Relative Humidity (`addRelativeHumidity`) from BME280
3. Channel `3` -> Barometric Pressure (`addBarometricPressure`) from BME280
4. Channel `4` -> Analog Input (`addAnalogInput`) for BME altitude
5. Channel `5` -> GPS (`addGPS`) lat/lon/alt
6. Channel `6` -> Analog Input for HDOP (Horizontal Dilution of Precision - GPS accuracy metric, lower is better: <1=ideal, 1-2=excellent, 2-5=good, 5-10=moderate, 10-20=fair, >20=poor)
7. Channel `7` -> Analog Input for satellite count
8. Channel `8` -> Analog Input for GPS seconds-of-day
9. Channel `9` -> Analog Input for battery voltage (volts)
10. Channel `10` -> Analog Input for PAX counter (WiFi + BLE device count) - **NEW**

**Note:** Individual channels can be enabled/disabled via the `/payload-info` web page to reduce payload size and save airtime.

Source: [src/main.cpp](src/main.cpp)

## Important CayenneLPP Limits

Cayenne Analog Input is 2-byte signed with 0.01 resolution (about -327.68 to 327.67).

This affects:
- Channel `4` altitude (can exceed range)
- Channel `8` seconds-of-day (0..86400 always exceeds range)

If decoded values look wrong, this is expected with current mapping. Use scaled/alternate fields if needed.

## Serial Debugging

The firmware has two modes:

**Normal Mode (debug-off):**
- Shows only important events (joins, errors, downlinks)
- Minimal output for production use
- User commands always work
- Clean, quiet operation

**Debug Mode (debug-on):**
- Detailed GPS parser state
- LoRa event flow (`EV_JOINING`, `EV_TXSTART`, `EV_TXCOMPLETE`, etc.)
- Periodic system health block (heap, sensor status, LoRa counters)
- Display page rotations and burn-in shifts
- Optional raw NMEA sentence dump for first 30s after boot

**Toggle Debug Mode:**
- Serial: `debug-on` / `debug-off`
- Downlink: `0x05` (ON) / `0x06` (OFF)

**Always Shown (regardless of debug mode):**
- ✓ Network join success
- ✗ Join/rejoin failures
- 📥 Downlink messages
- User command responses
- Boot banner and menu

## WiFi Web Dashboard

This firmware includes a comprehensive web dashboard with multiple pages! See [WIFI_SETUP.md](WIFI_SETUP.md) for complete setup instructions.

**Web Pages:**
- `/` - Dashboard with real-time sensor data and OpenStreetMap GPS tracking
- `/settings` - System settings (WiFi, GPS, Display, Sleep mode)
- `/config` - LoRaWAN configuration (OTAA/ABP keys)
- `/payload-info` - CayenneLPP documentation and channel configuration
- `/diagnostics` - System diagnostics with frame counter reset
- `/debug` - Live debug console with Serial output
- `/about` - Developer information and project details

**Quick Start:**
1. Upload firmware
2. Device creates WiFi AP: `TTGO-T-Beam-Setup` (password: `12345678`)
3. Connect and configure your WiFi
4. Access dashboard at device IP address
5. View real-time sensor data and GPS location on OpenStreetMap

**Serial Commands:**
- `menu` - Interactive configuration menu
- `status` - Complete system status
- `wifi-on/off` - Enable/disable WiFi
- `gps-on/off` - Enable/disable GPS
- `display-on/off` - Enable/disable OLED display
- `sleep-on/off` - Enable/disable sleep mode
- `sleep-interval <sec>` - Set sleep wake interval
- `lora-interval <sec>` - Set LoRa send interval
- `lora-keys` - View/configure LoRa keys
- `pax-on/off` - Enable/disable PAX counter
- `pax-wifi-on/off` - Enable/disable WiFi scanning
- `pax-ble-on/off` - Enable/disable BLE scanning
- `pax-interval <sec>` - Set PAX scan interval (30-3600)
- `pax-status` - Show PAX counter status
- `channel-enable <1-10>` - Enable payload channel
- `channel-disable <1-10>` - Disable payload channel
- `channel-status` - Show all channel states
- `debug-on/off` - Toggle verbose debugging
- `help` - Show all commands

**LoRaWAN Downlink Control (Enhanced Protocol):**

*Simple Commands:*
- `01` - WiFi ON
- `02` - WiFi OFF
- `05` - Debug ON
- `06` - Debug OFF

*Enhanced Commands (0xAA prefix):*
- `AA 01 00/01` - WiFi OFF/ON
- `AA 02 00/01` - GPS OFF/ON
- `AA 03 [4 bytes]` - Sleep mode with wake interval (seconds in hex)
- `AA 04 [4 bytes]` - LoRa send interval (seconds in hex)
- `AA 05 00` - Sleep mode OFF
- `AA 06 00/01` - Debug OFF/ON
- `AA 07 00/01` - Display OFF/ON

**Examples:**
```
AA 01 00        - WiFi OFF
AA 02 00        - GPS OFF (power saving)
AA 07 00        - Display OFF (power saving)
AA 03 00 01 51 80 - Sleep 24 hours (86400 sec)
AA 04 00 00 03 84 - Send every 15 minutes (900 sec)
```

See [DOWNLINK_COMMANDS.md](DOWNLINK_COMMANDS.md) for complete documentation with hex calculations and power management scenarios.

## Version History

See [CHANGELOG.md](CHANGELOG.md) for detailed version history and release notes.

**Current Version:** 2.6.0 (2026-03-19)
- **NEW:** PAX Counter mode (WiFi + BLE device scanning for crowd monitoring)
- **NEW:** Payload configuration page with channel enable/disable
- **NEW:** Diagnostics page with frame counter reset and downlink counter
- **NEW:** About page with developer information
- **NEW:** ABP mode support in web UI
- **NEW:** NVS persistence for system settings
- **NEW:** Battery voltage display on OLED and web dashboard
- GPS hardware power control via AXP192
- GPS power fix for NVS persistence
- Debug logging for web page handlers
- Apache License 2.0

## Build and Upload

From project folder:

```bash
platformio run
platformio run --target upload --upload-port /dev/cu.usbserial-01E5CD55
platformio device monitor --baud 115200 --port /dev/cu.usbserial-01E5CD55
```

## Troubleshooting Quick Guide

### Joins but no application data
- Confirm you are checking **uplink events**, not only join events.
- Confirm payload arrives on `FPort 1`.
- Check serial for `LoRa uplink #... payload=...`.

### GPS has NMEA but no fix (`sats=0`, `hdop=99.99`)
- UART is working; this is usually no sky lock yet.
- Move outdoors with clear sky and wait for cold-start lock.
- Verify GPS antenna connector and placement.

### Wrong system time
- Clock sync now only accepts plausible GPS date/time + fix quality.
- If no fix, system clock remains unsynced (expected).

### GPS LED not blinking
- Check Serial Monitor for "✓ GPS power enabled (AXP192 LDO3 ON)"
- Try serial command: `gps-on`
- Check `/diagnostics` page for GPS status
- Verify `gpsEnabled` setting in `/settings` page

### Web pages showing 500 errors
- Check Serial Monitor for page serving messages
- Look for syntax errors or missing closing braces
- Verify HTML string literals are properly closed
- Check free heap memory (should be >50KB)

## New Features

### PAX Counter
The PAX counter scans for nearby WiFi access points and BLE devices to estimate crowd size. Configure via `/payload-info` page:
- Enable/disable PAX counting
- Set scan interval (30-3600 seconds)
- View real-time device counts
- Data sent on Channel 10 (optional)

### Channel Configuration
Individual payload channels can be enabled/disabled via `/payload-info` page:
- Reduces payload size
- Saves airtime
- Optimizes battery life
- Changes take effect on next uplink

### Diagnostics
The `/diagnostics` page provides:
- Uplink/downlink message counters
- Frame counter display (LMIC.seqnoUp/seqnoDn)
- Frame counter reset button
- System health metrics
- Auto-refresh every 5 seconds

## Developer

**Markus van Kempen**
- Email: Markus.van.Kempen@gmail.com
- Organization: Research | Floor 7½ 🏢🤏
- GitHub: [@markusvankempen](https://github.com/markusvankempen)
- Website: [markusvankempen.github.io](https://markusvankempen.github.io/)

*"No bug too small, no syntax too weird."*
