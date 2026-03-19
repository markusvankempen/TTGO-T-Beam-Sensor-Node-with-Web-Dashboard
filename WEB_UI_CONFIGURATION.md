# Web UI Configuration Page - Implementation Guide

**Author:** Markus van Kempen | markus.van.kempen@gmail.com

## Overview

This document describes the web UI configuration page that needs to be added to provide a user-friendly interface for:
1. Controlling WiFi, GPS, and debug modes
2. Configuring LoRa send intervals
3. Setting up sleep mode
4. Generating downlink command payloads
5. Viewing current system configuration

## Page Location

**URL:** `http://[device-ip]/settings`

## Features Required

### 1. Current Status Display
Show real-time status of:
- WiFi: ON/OFF
- GPS: ON/OFF  
- Debug Mode: ON/OFF
- Sleep Mode: ON/OFF with interval
- LoRa Send Interval: Current value
- LoRa Uplink Payload: What's being sent

### 2. Control Toggles
Interactive switches for:
- WiFi Enable/Disable
- GPS Enable/Disable
- Debug Mode Enable/Disable
- Sleep Mode Enable/Disable

### 3. Configuration Inputs
- LoRa Send Interval (seconds)
- Sleep Wake Interval (seconds)

### 4. Downlink Command Generator
Automatically generates hex payloads for TTN/ChirpStack:
- Shows command for each action
- Copy-to-clipboard functionality
- Explains what each command does

### 5. LoRa Payload Viewer
Shows what data is being sent in uplinks:
- CayenneLPP format breakdown
- Channel assignments
- Current sensor values
- Payload size

## API Endpoints Needed

```
GET  /api/system-config    - Get current configuration
POST /api/system-config    - Update configuration
GET  /api/lora-payload     - Get current LoRa payload info
```

## Implementation Code

Add to `setupWebServer()` function in main.cpp:

```cpp
// System configuration page
server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>System Configuration</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
    }
    .container {
      max-width: 1000px;
      margin: 0 auto;
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    .content { padding: 30px; }
    .section {
      margin-bottom: 30px;
      padding: 20px;
      background: #f8f9fa;
      border-radius: 10px;
    }
    .section h3 {
      margin-bottom: 15px;
      color: #667eea;
    }
    .control-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 15px;
      background: white;
      border-radius: 8px;
      margin-bottom: 10px;
    }
    .switch {
      position: relative;
      display: inline-block;
      width: 60px;
      height: 34px;
    }
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
      border-radius: 34px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 26px;
      width: 26px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: #667eea;
    }
    input:checked + .slider:before {
      transform: translateX(26px);
    }
    .input-group {
      margin-bottom: 15px;
    }
    .input-group label {
      display: block;
      margin-bottom: 5px;
      font-weight: bold;
    }
    .input-group input {
      width: 100%;
      padding: 10px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
    }
    .btn {
      padding: 12px 30px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
      margin-right: 10px;
      margin-top: 10px;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .command-box {
      background: #2d3748;
      color: #48bb78;
      padding: 15px;
      border-radius: 8px;
      font-family: monospace;
      margin-top: 10px;
      position: relative;
    }
    .copy-btn {
      position: absolute;
      top: 10px;
      right: 10px;
      padding: 5px 10px;
      background: #667eea;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 12px;
    }
    .status-badge {
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 14px;
      font-weight: bold;
    }
    .status-on {
      background: #d4edda;
      color: #155724;
    }
    .status-off {
      background: #f8d7da;
      color: #721c24;
    }
    .payload-item {
      padding: 10px;
      background: white;
      border-left: 4px solid #667eea;
      margin-bottom: 10px;
      border-radius: 5px;
    }
    .help-text {
      font-size: 12px;
      color: #666;
      margin-top: 5px;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>⚙️ System Configuration</h1>
      <p>Control and monitor your TTGO T-Beam</p>
    </div>
    
    <div class="content">
      <!-- Current Status Section -->
      <div class="section">
        <h3>📊 Current Status</h3>
        <div class="control-row">
          <span>WiFi</span>
          <span id="wifi-status" class="status-badge">Loading...</span>
        </div>
        <div class="control-row">
          <span>GPS</span>
          <span id="gps-status" class="status-badge">Loading...</span>
        </div>
        <div class="control-row">
          <span>Debug Mode</span>
          <span id="debug-status" class="status-badge">Loading...</span>
        </div>
        <div class="control-row">
          <span>Sleep Mode</span>
          <span id="sleep-status" class="status-badge">Loading...</span>
        </div>
        <div class="control-row">
          <span>LoRa Send Interval</span>
          <span id="lora-interval-status">Loading...</span>
        </div>
      </div>

      <!-- Control Toggles Section -->
      <div class="section">
        <h3>🎛️ Quick Controls</h3>
        <div class="control-row">
          <span>WiFi</span>
          <label class="switch">
            <input type="checkbox" id="wifi-toggle" onchange="toggleWiFi()">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <span>GPS</span>
          <label class="switch">
            <input type="checkbox" id="gps-toggle" onchange="toggleGPS()">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <span>Debug Mode</span>
          <label class="switch">
            <input type="checkbox" id="debug-toggle" onchange="toggleDebug()">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <span>Sleep Mode</span>
          <label class="switch">
            <input type="checkbox" id="sleep-toggle" onchange="toggleSleep()">
            <span class="slider"></span>
          </label>
        </div>
      </div>

      <!-- Configuration Section -->
      <div class="section">
        <h3>⚙️ Configuration</h3>
        <div class="input-group">
          <label>LoRa Send Interval (seconds)</label>
          <input type="number" id="lora-interval" min="10" value="300">
          <div class="help-text">Minimum: 10 seconds. Default: 300 (5 minutes)</div>
        </div>
        <div class="input-group">
          <label>Sleep Wake Interval (seconds)</label>
          <input type="number" id="sleep-interval" min="60" value="86400">
          <div class="help-text">Time between wake cycles. Default: 86400 (24 hours)</div>
        </div>
        <button class="btn btn-primary" onclick="saveConfig()">💾 Save Configuration</button>
      </div>

      <!-- Downlink Command Generator -->
      <div class="section">
        <h3>📥 Downlink Command Generator</h3>
        <p>Copy these hex payloads to send via TTN/ChirpStack:</p>
        
        <div style="margin-top: 15px;">
          <strong>WiFi Control:</strong>
          <div class="command-box">
            AA 01 00 - WiFi OFF
            <button class="copy-btn" onclick="copyToClipboard('AA0100')">Copy</button>
          </div>
          <div class="command-box">
            AA 01 01 - WiFi ON
            <button class="copy-btn" onclick="copyToClipboard('AA0101')">Copy</button>
          </div>
        </div>

        <div style="margin-top: 15px;">
          <strong>GPS Control:</strong>
          <div class="command-box">
            AA 02 00 - GPS OFF (Power Saving)
            <button class="copy-btn" onclick="copyToClipboard('AA0200')">Copy</button>
          </div>
          <div class="command-box">
            AA 02 01 - GPS ON
            <button class="copy-btn" onclick="copyToClipboard('AA0201')">Copy</button>
          </div>
        </div>

        <div style="margin-top: 15px;">
          <strong>Sleep Mode (24 hours):</strong>
          <div class="command-box">
            AA 03 00 01 51 80 - Sleep 24h
            <button class="copy-btn" onclick="copyToClipboard('AA0300015180')">Copy</button>
          </div>
        </div>

        <div style="margin-top: 15px;">
          <strong>LoRa Interval (15 minutes):</strong>
          <div class="command-box">
            AA 04 00 00 03 84 - Send every 15 min
            <button class="copy-btn" onclick="copyToClipboard('AA0400000384')">Copy</button>
          </div>
        </div>

        <div style="margin-top: 15px;">
          <strong>Debug Control:</strong>
          <div class="command-box">
            AA 06 00 - Debug OFF
            <button class="copy-btn" onclick="copyToClipboard('AA0600')">Copy</button>
          </div>
          <div class="command-box">
            AA 06 01 - Debug ON
            <button class="copy-btn" onclick="copyToClipboard('AA0601')">Copy</button>
          </div>
        </div>
      </div>

      <!-- LoRa Payload Info -->
      <div class="section">
        <h3>📦 Current LoRa Uplink Payload</h3>
        <div id="payload-info">Loading...</div>
      </div>

      <div style="margin-top: 20px;">
        <button class="btn btn-secondary" onclick="window.location.href='/'">🏠 Dashboard</button>
        <button class="btn btn-secondary" onclick="window.location.href='/config'">🔐 LoRa Keys</button>
      </div>
    </div>
  </div>

  <script>
    function loadStatus() {
      fetch('/api/system-config')
        .then(response => response.json())
        .then(data => {
          // Update status badges
          document.getElementById('wifi-status').textContent = data.wifi ? 'ON' : 'OFF';
          document.getElementById('wifi-status').className = 'status-badge ' + (data.wifi ? 'status-on' : 'status-off');
          
          document.getElementById('gps-status').textContent = data.gps ? 'ON' : 'OFF';
          document.getElementById('gps-status').className = 'status-badge ' + (data.gps ? 'status-on' : 'status-off');
          
          document.getElementById('debug-status').textContent = data.debug ? 'ON' : 'OFF';
          document.getElementById('debug-status').className = 'status-badge ' + (data.debug ? 'status-on' : 'status-off');
          
          document.getElementById('sleep-status').textContent = data.sleep ? 'ON' : 'OFF';
          document.getElementById('sleep-status').className = 'status-badge ' + (data.sleep ? 'status-on' : 'status-off');
          
          document.getElementById('lora-interval-status').textContent = data.loraInterval + ' sec (' + Math.floor(data.loraInterval / 60) + ' min)';
          
          // Update toggles
          document.getElementById('wifi-toggle').checked = data.wifi;
          document.getElementById('gps-toggle').checked = data.gps;
          document.getElementById('debug-toggle').checked = data.debug;
          document.getElementById('sleep-toggle').checked = data.sleep;
          
          // Update inputs
          document.getElementById('lora-interval').value = data.loraInterval;
          document.getElementById('sleep-interval').value = data.sleepInterval;
        });
      
      // Load payload info
      fetch('/api/lora-payload')
        .then(response => response.json())
        .then(data => {
          let html = '';
          data.channels.forEach(ch => {
            html += `<div class="payload-item">
              <strong>Channel ${ch.channel}:</strong> ${ch.name} = ${ch.value} ${ch.unit}
            </div>`;
          });
          html += `<div style="margin-top: 10px; font-size: 12px; color: #666;">
            Total Payload Size: ${data.size} bytes
          </div>`;
          document.getElementById('payload-info').innerHTML = html;
        });
    }

    function toggleWiFi() {
      const enabled = document.getElementById('wifi-toggle').checked;
      fetch('/api/system-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({wifi: enabled})
      }).then(() => loadStatus());
    }

    function toggleGPS() {
      const enabled = document.getElementById('gps-toggle').checked;
      fetch('/api/system-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({gps: enabled})
      }).then(() => loadStatus());
    }

    function toggleDebug() {
      const enabled = document.getElementById('debug-toggle').checked;
      fetch('/api/system-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({debug: enabled})
      }).then(() => loadStatus());
    }

    function toggleSleep() {
      const enabled = document.getElementById('sleep-toggle').checked;
      fetch('/api/system-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({sleep: enabled})
      }).then(() => loadStatus());
    }

    function saveConfig() {
      const loraInterval = parseInt(document.getElementById('lora-interval').value);
      const sleepInterval = parseInt(document.getElementById('sleep-interval').value);
      
      fetch('/api/system-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          loraInterval: loraInterval,
          sleepInterval: sleepInterval
        })
      }).then(() => {
        alert('Configuration saved!');
        loadStatus();
      });
    }

    function copyToClipboard(text) {
      navigator.clipboard.writeText(text).then(() => {
        alert('Copied: ' + text);
      });
    }

    // Load status on page load
    loadStatus();
    setInterval(loadStatus, 5000); // Refresh every 5 seconds
  </script>
</body>
</html>
)rawliteral";
  request->send(200, "text/html", html);
});

// API endpoint for system configuration
server.on("/api/system-config", HTTP_GET, [](AsyncWebServerRequest *request){
  JsonDocument doc;
  doc["wifi"] = wifiEnabled;
  doc["gps"] = gpsEnabled;
  doc["debug"] = debugEnabled;
  doc["sleep"] = sleepModeEnabled;
  doc["loraInterval"] = loraSendIntervalSec;
  doc["sleepInterval"] = sleepWakeInterval;
  
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
});

server.on("/api/system-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    JsonDocument doc;
    deserializeJson(doc, data, len);
    
    if (doc.containsKey("wifi")) {
      wifiEnabled = doc["wifi"].as<bool>();
      if (wifiEnabled) {
        WiFi.mode(WIFI_STA);
        WiFi.begin();
      } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
      }
    }
    
    if (doc.containsKey("gps")) {
      gpsEnabled = doc["gps"].as<bool>();
    }
    
    if (doc.containsKey("debug")) {
      debugEnabled = doc["debug"].as<bool>();
    }
    
    if (doc.containsKey("sleep")) {
      sleepModeEnabled = doc["sleep"].as<bool>();
      if (sleepModeEnabled) {
        lastWakeTime = millis();
      }
    }
    
    if (doc.containsKey("loraInterval")) {
      loraSendIntervalSec = doc["loraInterval"].as<uint32_t>();
    }
    
    if (doc.containsKey("sleepInterval")) {
      sleepWakeInterval = doc["sleepInterval"].as<uint32_t>();
    }
    
    request->send(200, "application/json", "{\"success\":true}");
  }
);

// API endpoint for LoRa payload information
server.on("/api/lora-payload", HTTP_GET, [](AsyncWebServerRequest *request){
  JsonDocument doc;
  JsonArray channels = doc["channels"].to<JsonArray>();
  
  JsonObject ch1 = channels.add<JsonObject>();
  ch1["channel"] = 1;
  ch1["name"] = "Temperature";
  ch1["value"] = String(lastTemperature, 1);
  ch1["unit"] = "°C";
  
  JsonObject ch2 = channels.add<JsonObject>();
  ch2["channel"] = 2;
  ch2["name"] = "Humidity";
  ch2["value"] = String(lastHumidity, 1);
  ch2["unit"] = "%";
  
  JsonObject ch3 = channels.add<JsonObject>();
  ch3["channel"] = 3;
  ch3["name"] = "Pressure";
  ch3["value"] = String(lastPressureHpa, 1);
  ch3["unit"] = "hPa";
  
  JsonObject ch4 = channels.add<JsonObject>();
  ch4["channel"] = 4;
  ch4["name"] = "Altitude";
  ch4["value"] = String(lastAltitude, 1);
  ch4["unit"] = "m";
  
  if (gps.location.isValid()) {
    JsonObject ch5 = channels.add<JsonObject>();
    ch5["channel"] = 5;
    ch5["name"] = "GPS Location";
    ch5["value"] = String(gps.location.lat(), 6) + ", " + String(gps.location.lng(), 6);
    ch5["unit"] = "lat/lon";
  }
  
  doc["size"] = "~50";
  
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
});
```

## Adding Link to Main Dashboard

Update the main dashboard HTML to include a link to settings:

```html
<p style="margin-top: 10px;">
  <a href="/config" style="color: #667eea; text-decoration: none;">⚙️ LoRaWAN Configuration</a>
  <span style="margin: 0 10px;">|</span>
  <a href="/settings" style="color: #667eea; text-decoration: none;">🎛️ System Settings</a>
</p>
```

## Testing

1. Upload firmware
2. Connect to device WiFi or access via IP
3. Navigate to `http://[device-ip]/settings`
4. Test toggles and configuration
5. Copy downlink commands to TTN/ChirpStack
6. Verify changes take effect

## Benefits

- **User-Friendly:** No need to remember hex commands
- **Visual Feedback:** See current status at a glance
- **Copy-Paste:** Easy downlink command generation
- **Real-Time:** Updates every 5 seconds
- **Comprehensive:** All settings in one place
- **Educational:** Shows what's in LoRa payload

This provides a complete web-based configuration interface that makes the device accessible to non-technical users while still providing power users with the hex commands they need.