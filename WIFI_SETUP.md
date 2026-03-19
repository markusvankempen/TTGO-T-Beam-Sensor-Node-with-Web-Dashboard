# WiFi Web Dashboard Setup Guide

**Author:** Markus van Kempen | markus.van.kempen@gmail.com
**Organization:** Research | Floor 7½ 🏢🤏
**Motto:** "No bug too small, no syntax too weird."

---

## Overview

Your TTGO T-Beam now includes a comprehensive web dashboard with:
- Real-time sensor data display (BME280: temperature, humidity, pressure, altitude)
- Live GPS tracking with Google Maps integration
- LoRaWAN status monitoring
- WiFi Manager for easy configuration
- Serial command interface

> **📸 Screenshots:** See [README.md](README.md#web-dashboard-screenshots) for web dashboard screenshots, or use the automated [screenshot capture tool](SCREENSHOTS.md) to generate your own.


## First Time Setup

### Method 1: Automatic WiFi Configuration Portal

1. **Upload the code** to your TTGO T-Beam
2. **Power on** the device
3. **Wait 10 seconds** - if no saved WiFi credentials exist, it will create an Access Point
4. **Connect to WiFi AP**:
   - SSID: `TTGO-T-Beam-Setup`
   - Password: `12345678`
5. **Configure WiFi**:
   - A captive portal should open automatically
   - If not, browse to `http://192.168.4.1`
   - Select your WiFi network and enter password
   - Click "Save"
6. **Device will restart** and connect to your WiFi
7. **Find the IP address** in serial monitor (115200 baud)
8. **Open web browser** and navigate to the IP address

### Method 2: Serial Command Configuration

1. **Connect via serial** (115200 baud)
2. **Type commands**:
   ```
   help           - Show available commands
   wifi-status    - Check current WiFi status
   wifi-reset     - Reset WiFi settings (will restart device)
   ```

### Method 3: Boot Button WiFi Reset

1. **Hold the BOOT button** (GPIO 0) while powering on
2. Device will **reset WiFi settings** and enter config mode
3. Follow Method 1 steps above

## Accessing the Dashboard

Once connected to WiFi:

1. **Check serial monitor** for IP address:
   ```
   WiFi connected!
   IP address: 192.168.1.100
   Web server started on port 80
   ```

2. **Open web browser** and navigate to the IP address:
   ```
   http://192.168.1.100
   ```

3. **Dashboard displays**:
   - Temperature, Humidity, Pressure, Altitude
   - GPS Status (LOCKED/NO FIX)
   - Satellite count
   - Speed (km/h)
   - LoRa uplink counter
   - Google Maps with live GPS marker
   - System uptime and last update time

## Google Maps API Key Setup

The dashboard includes Google Maps integration. To enable it:

1. **Get a Google Maps API key**:
   - Go to [Google Cloud Console](https://console.cloud.google.com/)
   - Create a new project or select existing
   - Enable "Maps JavaScript API"
   - Create credentials (API Key)
   - Restrict the key to your domain/IP if desired

2. **Update the code**:
   - Open `src/main.cpp`
   - Find line with: `<script src="https://maps.googleapis.com/maps/api/js?key=YOUR_API_KEY"></script>`
   - Replace `YOUR_API_KEY` with your actual API key

3. **Re-upload** the code

**Note**: Without an API key, the map will show a "For development purposes only" watermark but will still work.

## Dashboard Features

### Real-Time Data Updates
- Data refreshes every 1 second
- No page reload required
- Smooth GPS marker movement on map

### Responsive Design
- Works on desktop, tablet, and mobile
- Adaptive layout for different screen sizes
- Modern gradient design

### API Endpoint

Access raw JSON data programmatically:
```
http://YOUR_IP/api/data
```

Example response:
```json
{
  "temperature": 23.5,
  "humidity": 45.2,
  "pressure": 1013.25,
  "altitude": 120.5,
  "gps": {
    "valid": true,
    "latitude": 43.6532,
    "longitude": -79.3832,
    "altitude": 125.3,
    "satellites": 8,
    "hdop": 1.2,
    "speed": 0.0
  },
  "uptime": 3600,
  "freeHeap": 250000,
  "loraJoined": true,
  "loraUplinks": 42
}
```

## Troubleshooting

### WiFi Won't Connect
1. Check serial monitor for error messages
2. Verify WiFi credentials are correct
3. Try `wifi-reset` command via serial
4. Hold BOOT button during power-on to force config mode

### Can't Access Dashboard
1. Verify device is connected to WiFi (check serial monitor)
2. Ping the IP address from your computer
3. Ensure you're on the same network
4. Check firewall settings
5. Try accessing from different browser

### GPS Not Showing on Map
1. Ensure GPS has a fix (check "GPS Status" card)
2. Wait for satellite lock (can take 3-10 minutes outdoors)
3. Verify Google Maps API key is configured
4. Check browser console for JavaScript errors

### Map Shows "For development purposes only"
- This is normal without a Google Maps API key
- Map will still function, just with watermark
- Add API key to remove watermark (see above)

## Serial Commands Reference

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `wifi-status` | Display WiFi connection status and IP |
| `wifi-reset` | Reset WiFi credentials and restart device |

## Network Configuration

### Default AP Settings
- **SSID**: `TTGO-T-Beam-Setup`
- **Password**: `12345678`
- **IP Address**: `192.168.4.1`
- **Timeout**: 3 minutes (then continues without WiFi)

### Customization

To change AP name/password, edit in `src/main.cpp`:
```cpp
static const char* WIFI_AP_NAME = "Your-Custom-Name";
static const char* WIFI_AP_PASSWORD = "YourPassword";
```

## Security Notes

1. **Change default AP password** in production
2. **Use strong WiFi passwords**
3. **Consider adding authentication** to web dashboard for public deployments
4. **Restrict Google Maps API key** to your domain/IP
5. **Use HTTPS** if exposing to internet (requires additional setup)

## Advanced Features

### Remote WiFi Reset
Access from any browser on the network:
```
http://YOUR_IP/reset-wifi
```
Device will reset WiFi settings and restart.

### Integration with Home Automation
Use the `/api/data` endpoint to integrate with:
- Home Assistant
- Node-RED
- MQTT bridges
- Custom applications

Example curl command:
```bash
curl http://192.168.1.100/api/data
```

## Performance Notes

- Web server runs asynchronously (non-blocking)
- LoRaWAN timing is not affected
- GPS processing continues normally
- Display updates continue as before
- Minimal memory overhead (~50KB)

## Support

For issues or questions:
1. Check serial monitor output (115200 baud)
2. Review this documentation
3. Check GitHub issues
4. Verify all libraries are installed correctly

## Library Dependencies

The following libraries are automatically installed by PlatformIO:
- `tzapu/WiFiManager @ ^2.0.17`
- `bblanchon/ArduinoJson @ ^7.2.1`
- `me-no-dev/ESPAsyncWebServer @ ^3.3.18`
- `me-no-dev/AsyncTCP @ ^1.1.4`

All existing libraries (LMIC, BME280, GPS, etc.) remain unchanged.