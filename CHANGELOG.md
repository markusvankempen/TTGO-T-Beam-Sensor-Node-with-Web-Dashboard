# Changelog

All notable changes to the TTGO T-Beam Sensor Node project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.6.0] - 2026-03-19

### Added
- **PAX Counter Enhancements**
  - Individual WiFi and BLE scanning enable/disable controls
  - Separate checkboxes for WiFi and BLE scanning methods in web UI
  - Validation to ensure at least one scanning method is enabled
  - Enhanced PAX counter API with `wifiEnabled` and `bleEnabled` parameters
  - NVS persistence for PAX counter settings (enabled state, WiFi/BLE flags, scan interval)
  - Dynamic BLE scanner initialization when PAX counter is enabled via web UI
  - Serial commands for PAX counter control:
    - `pax-on/off` - Enable/disable PAX counter
    - `pax-wifi-on/off` - Enable/disable WiFi scanning
    - `pax-ble-on/off` - Enable/disable BLE scanning
    - `pax-interval <sec>` - Set scan interval (30-3600 seconds)
    - `pax-status` - Show PAX counter status and device counts

- **Payload Channel Serial Commands**
  - `channel-enable <1-10>` - Enable specific payload channel
  - `channel-disable <1-10>` - Disable specific payload channel
  - `channel-status` - Display all channel enable/disable states
  - Channels: Temperature, Humidity, Pressure, Altitude, GPS, HDOP, Satellites, GPS Time, Battery, PAX Counter

- **GPS Last Known Position**
  - Automatically saves last valid GPS fix to NVS
  - Stores: latitude, longitude, altitude, satellites, HDOP, timestamp
  - Updates when position changes >10m or every 5 minutes
  - Saves to NVS every 10 minutes to reduce flash wear
  - Web dashboard shows last known position when GPS has no current fix
  - Displays time since last fix (e.g., "120s ago")
  - Persists across reboots and power cycles
  
- **LoRa Signal Information Enhancements**
  - Bandwidth display (125 kHz or 500 kHz) calculated from data rate
  - Spreading Factor display (SF7-SF12) calculated from data rate
  - Comprehensive explanations for all LoRa signal metrics:
    - RSSI: Signal strength interpretation and typical ranges
    - SNR: Signal-to-noise ratio explanation
    - Frequency: US915 band information
    - Bandwidth: Impact on data rate and range
    - Spreading Factor: Trade-offs between range and speed
    - Data Rate: DR0-DR15 explanation
    - TX Power: Power consumption vs range trade-off
    
- **User Interface Improvements**
  - Version number (v2.6.0) displayed in all page navigation headers
  - "About" link added to all navigation bars for consistency
  - Enhanced diagnostics page with detailed LoRa signal information

### Changed
- PAX counter now respects individual WiFi and BLE enable flags
- Diagnostics API now calculates and returns bandwidth and spreading factor
- All web UI pages now display firmware version in navbar

### Fixed
- PAX counter settings now persist across reboots (saved to NVS)
- BLE scanner now initializes dynamically when PAX counter is enabled
- BLE scanning now works correctly when enabled via web UI

### Technical Details
- NimBLE-Arduino library v1.4.2 integrated for BLE scanning
- US915 data rate to bandwidth/SF mapping implemented
- Partition scheme changed to `huge_app.csv` to accommodate BLE library (Flash: 45.9%)
- PAX settings stored in NVS: `paxEnabled`, `paxWifiScan`, `paxBleScan`, `paxInterval`

## [2.5.0] - 2026-03-19

### Added
- GPS hardware power control via AXP192 LDO3 rail
- `enableGpsPower()` and `disableGpsPower()` functions for true GPS power management
- GPS power state reporting in `gps-status` command showing actual AXP192 LDO3 state
- GPS debug message suppression when GPS is disabled
- Proper hardware power-down when GPS is turned off (LED stops, no more debug messages)
- Apache License 2.0
- Comprehensive CHANGELOG.md with version history

### Changed
- GPS OFF command now actually powers down GPS hardware via AXP192
- GPS debug messages only appear when GPS is enabled
- Enhanced `gps-status` command to show both software flag and hardware power state

### Fixed
- GPS OFF command now properly disables GPS hardware (was only setting flag before)
- GPS LED continues blinking after GPS OFF command (now properly stops)
- GPS debug messages appearing after GPS disabled (now properly suppressed)

## [2.4.0] - 2026-03-18

### Added
- Enhanced downlink command protocol with 0xAA prefix for multi-byte commands
- Sleep mode control via downlink (AA 03 [4 bytes] for wake interval)
- LoRa send interval control via downlink (AA 04 [4 bytes])
- Sleep mode disable command (AA 05 00)
- Comprehensive downlink documentation in DOWNLINK_COMMANDS.md
- Power management scenarios and examples
- Hex calculation guide for interval commands

### Changed
- Downlink protocol now supports both simple (1-byte) and enhanced (multi-byte) commands
- Improved command parsing with proper validation
- Enhanced serial output for downlink command acknowledgment

## [2.3.0] - 2026-03-17

### Added
- Interactive serial menu system with `menu` command
- Comprehensive serial command interface:
  - `status` - Complete system status
  - `wifi-on/off` - WiFi control
  - `gps-on/off` - GPS control
  - `sleep-on/off` - Sleep mode control
  - `sleep-interval <sec>` - Configure sleep wake interval
  - `lora-interval <sec>` - Configure LoRa send interval
  - `lora-keys` - View/configure LoRa keys
  - `debug-on/off` - Toggle debug mode
  - `help` - Show all commands
- LoRaWAN key configuration via serial interface
- NVS persistent storage for LoRa keys (APPEUI, DEVEUI, APPKEY)
- Web UI configuration page documentation (WEB_UI_CONFIGURATION.md)
- API endpoints for system configuration
- LoRa payload viewer documentation

### Changed
- Serial interface now has structured menu system
- Commands provide detailed feedback and status
- LoRa keys can be configured without recompiling

## [2.2.0] - 2026-03-16

### Added
- Debug mode control (verbose vs quiet operation)
- Serial commands: `debug-on` / `debug-off`
- Downlink commands: 0x05 (debug ON) / 0x06 (debug OFF)
- Conditional debug output for GPS, LoRa events, system health
- Always-shown events: joins, errors, downlinks, user commands

### Changed
- Normal mode now shows minimal output (production-ready)
- Debug mode shows detailed GPS parser state, LoRa events, system health
- Improved serial output organization and clarity

## [2.1.0] - 2026-03-15

### Added
- LoRaWAN downlink command support
- WiFi control via downlink (0x01 ON, 0x02 OFF)
- Device restart via downlink (0x03)
- Downlink command acknowledgment in serial output

### Fixed
- GPS initialization for T22 V1.1 20191212 variant
- Removed GPIO reset that was interfering with GPS operation
- GPS now properly initializes and acquires satellite lock

## [2.0.0] - 2026-03-14

### Added
- WiFi web dashboard with real-time sensor data
- OpenStreetMap integration with live GPS marker (no API key required!)
- WiFi Manager for easy network configuration
- WiFi AP mode: SSID="TTGO-T-Beam-Setup", Password="12345678"
- JSON API endpoint at `/api/data`
- Web interface with auto-refresh every 5 seconds
- WiFi network selection UI
- Serial commands: `wifi-status`, `wifi-reset`
- Comprehensive WiFi setup documentation (WIFI_SETUP.md)

### Changed
- Replaced Google Maps with OpenStreetMap (no API key needed)
- Enhanced web UI with better styling and responsiveness
- Improved error handling for WiFi operations

## [1.0.0] - 2026-03-10

### Added
- Initial release
- LoRaWAN OTAA connectivity (US915)
- BME280 environmental sensor integration (temperature, humidity, pressure)
- GPS tracking with NEO-6M/8M module
- SSD1306 OLED display with anti-burn-in protection
- CayenneLPP payload formatting (8 channels)
- AXP192 power management support
- Serial debugging output
- Basic system monitoring

### Hardware Support
- TTGO T-Beam T22 V1.1 (20191212 variant)
- ESP32 microcontroller
- SX1276 LoRa radio
- BME280 I2C sensor
- GPS UART module
- SSD1306 OLED I2C display
- AXP192 PMU

## Version Numbering

This project uses [Semantic Versioning](https://semver.org/):
- **MAJOR** version for incompatible API changes
- **MINOR** version for new functionality in a backwards compatible manner
- **PATCH** version for backwards compatible bug fixes

## Categories

- **Added** - New features
- **Changed** - Changes in existing functionality
- **Deprecated** - Soon-to-be removed features
- **Removed** - Removed features
- **Fixed** - Bug fixes
- **Security** - Vulnerability fixes

---

**Author:** Markus van Kempen | markus.van.kempen@gmail.com  
**Organization:** Research | Floor 7½ 🏢🤏  
**Motto:** "No bug too small, no syntax too weird."