# TTGO T-Beam Web UI Screenshots Guide

This guide explains how to capture and add screenshots of the TTGO T-Beam web dashboard to the project documentation.

## Automated Screenshot Capture (Recommended)

We've created a Python script that automatically captures all web UI pages using Selenium WebDriver.

### Prerequisites

1. **Python 3.7+** installed
2. **Chrome or Chromium** browser installed
3. **Python packages** (install with pip)

### Installation

```bash
# Install required Python packages
pip install -r requirements-screenshots.txt

# Or install manually
pip install selenium webdriver-manager
```

### Usage

1. **Ensure your TTGO T-Beam device is powered on and connected to WiFi**
2. **Note your device's IP address** (shown on OLED display or in Serial Monitor)
3. **Run the screenshot script:**

```bash
# Using default IP (10.0.4.80)
python3 capture_screenshots.py

# Or specify custom IP
python3 capture_screenshots.py --ip 192.168.1.100

# Specify custom output directory
python3 capture_screenshots.py --output my-screenshots
```

### What It Does

The script will:
1. Initialize Chrome WebDriver in headless mode
2. Navigate to each of the 7 web pages
3. Wait for content to load (including dynamic elements like maps)
4. Capture high-quality screenshots (1280x800)
5. Save screenshots to the `screenshots/` directory

### Output

Screenshots will be saved with these filenames:
- `dashboard.png` - Main dashboard with sensor data and GPS map
- `settings.png` - System settings page
- `config.png` - LoRaWAN configuration page
- `payload-info.png` - Payload channel configuration
- `diagnostics.png` - System diagnostics and statistics
- `debug.png` - Live debug console
- `about.png` - About page with developer info

## Manual Screenshot Capture (Alternative)

If you prefer to capture screenshots manually:

### Method 1: Browser Screenshots

1. Open your device's web dashboard in a browser (e.g., http://10.0.4.80/)
2. Navigate to each page using the navigation menu
3. Capture screenshots using:
   - **macOS:** `Cmd + Shift + 4` (select area) or `Cmd + Shift + 3` (full screen)
   - **Windows:** `Win + Shift + S` (Snipping Tool)
   - **Linux:** `PrtScn` or use Screenshot tool

### Method 2: Browser DevTools

1. Open browser DevTools (F12)
2. Click the Device Toolbar icon (or press `Cmd+Shift+M` / `Ctrl+Shift+M`)
3. Set viewport to 1280x800
4. Navigate to each page
5. Right-click → "Capture screenshot" or use DevTools screenshot feature

## Pages to Capture

Capture screenshots of these 7 pages:

1. **Dashboard** (`/`)
   - Shows real-time sensor data
   - OpenStreetMap with GPS marker
   - System status indicators

2. **Settings** (`/settings`)
   - WiFi configuration
   - GPS enable/disable
   - Display settings
   - Sleep mode configuration

3. **LoRa Configuration** (`/config`)
   - OTAA/ABP mode selection
   - LoRaWAN keys configuration
   - Join status

4. **Payload Info** (`/payload-info`)
   - CayenneLPP channel documentation
   - Channel enable/disable controls
   - PAX counter settings

5. **Diagnostics** (`/diagnostics`)
   - Uplink/downlink counters
   - Frame counter display
   - WiFi information
   - LoRa signal strength
   - System health metrics

6. **Debug Console** (`/debug`)
   - Live serial output
   - Debug toggle control
   - Real-time system messages

7. **About** (`/about`)
   - Developer information
   - Project details
   - License information

## Adding Screenshots to README

Once you have the screenshots, add them to README.md:

```markdown
## Web Dashboard Screenshots

### Dashboard
![Dashboard](screenshots/dashboard.png)
*Real-time sensor data with GPS tracking on OpenStreetMap*

### Settings
![Settings](screenshots/settings.png)
*System configuration and power management*

### LoRa Configuration
![LoRa Config](screenshots/config.png)
*LoRaWAN OTAA/ABP key management*

### Payload Configuration
![Payload Info](screenshots/payload-info.png)
*CayenneLPP channel configuration and PAX counter*

### Diagnostics
![Diagnostics](screenshots/diagnostics.png)
*System diagnostics and statistics*

### Debug Console
![Debug Console](screenshots/debug.png)
*Live debug output and system messages*

### About
![About](screenshots/about.png)
*Developer information and project details*
```

## Troubleshooting

### Script Issues

**"ChromeDriver not found"**
```bash
# Install ChromeDriver
# macOS:
brew install chromedriver

# Or use webdriver-manager (automatic):
pip install webdriver-manager
```

**"Connection refused" or "Cannot connect to device"**
- Verify device is powered on
- Check device IP address (shown on OLED or Serial Monitor)
- Ensure you're on the same network as the device
- Try pinging the device: `ping 10.0.4.80`

**"Selenium not installed"**
```bash
pip install selenium webdriver-manager
```

### Manual Capture Issues

**Map not loading**
- Wait 3-5 seconds for OpenStreetMap tiles to load
- Ensure device has internet connection (for map tiles)
- Check browser console for errors

**Page not responding**
- Refresh the page
- Check Serial Monitor for errors
- Verify device is not in sleep mode

## Tips for Best Screenshots

1. **Use consistent viewport size** (1280x800 recommended)
2. **Wait for dynamic content** to load (especially maps)
3. **Capture during active operation** (with GPS fix, sensor data updating)
4. **Use good lighting** if capturing OLED display photos
5. **Crop unnecessary browser chrome** (address bar, bookmarks, etc.)
6. **Save in PNG format** for best quality

## Next Steps

After capturing screenshots:

1. Review all screenshots for quality and completeness
2. Add screenshots to README.md with descriptive captions
3. Commit screenshots to the repository:
   ```bash
   git add screenshots/
   git commit -m "Add web UI screenshots"
   git push
   ```
4. Update GitHub repository description and topics
5. Create a release with screenshots in the release notes

---

**Author:** Markus van Kempen  
**Email:** markus.van.kempen@gmail.com  
**Organization:** Research | Floor 7½ 🏢🤏