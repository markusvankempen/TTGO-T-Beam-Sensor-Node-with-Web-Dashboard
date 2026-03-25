#pragma once
String getJsonSensorData() {
  JsonDocument doc;
  
  // BME280 data
  doc["temperature"] = lastTemperature;
  doc["humidity"] = lastHumidity;
  doc["pressure"] = lastPressureHpa;
  doc["altitude"] = lastAltitude;
  
  // GPS data - use current fix if available, otherwise use last known position
  doc["gps"]["valid"] = gps.location.isValid();
  if (gps.location.isValid()) {
    doc["gps"]["latitude"] = gps.location.lat();
    doc["gps"]["longitude"] = gps.location.lng();
    doc["gps"]["altitude"] = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    doc["gps"]["satellites"] = gps.satellites.isValid() ? gps.satellites.value() : 0;
    doc["gps"]["hdop"] = gps.hdop.isValid() ? gps.hdop.hdop() : 99.99;
    doc["gps"]["lastUpdate"] = "current";
  } else if (lastGoodTimestamp > 0) {
    // Use last known good position
    doc["gps"]["latitude"] = lastGoodLat;
    doc["gps"]["longitude"] = lastGoodLon;
    doc["gps"]["altitude"] = lastGoodAlt;
    doc["gps"]["satellites"] = lastGoodSats;
    doc["gps"]["hdop"] = lastGoodHdop;
    doc["gps"]["lastUpdate"] = String((millis() - lastGoodTimestamp) / 1000) + "s ago";
  } else {
    doc["gps"]["latitude"] = 0.0;
    doc["gps"]["longitude"] = 0.0;
    doc["gps"]["altitude"] = 0.0;
    doc["gps"]["satellites"] = 0;
    doc["gps"]["hdop"] = 99.99;
    doc["gps"]["lastUpdate"] = "never";
  }
  doc["gps"]["speed"] = gps.speed.isValid() ? gps.speed.kmph() : 0.0;
  
  // System info
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["loraJoined"] = loraJoined;
  doc["loraUplinks"] = uplinkCounter;
  
  // Battery info
  if (axpFound) {
    float batteryVoltage = readBatteryVoltage();
    bool charging = isBatteryCharging();
    doc["battery"]["voltage"] = batteryVoltage;
    doc["battery"]["charging"] = charging;
  } else {
    doc["battery"]["voltage"] = -1.0;
    doc["battery"]["charging"] = false;
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}

/**
 * @brief Generate complete HTML page for web dashboard
 *
 * Creates a responsive, modern web interface featuring:
 * - Real-time sensor data cards (temperature, humidity, pressure, altitude)
 * - GPS status and satellite information
 * - Interactive Google Maps with live GPS marker
 * - Auto-refresh every 1 second via JavaScript
 * - Responsive design for mobile/tablet/desktop
 * - Beautiful gradient styling
 *
 * @return String Complete HTML page as string
 *
 * @note Uses raw string literal for multi-line HTML
 * @note Includes embedded CSS and JavaScript
 * @note Google Maps API key should be replaced with actual key
 * @note Map will work without API key but shows watermark
 *
 * Features:
 * - Gradient header with project title
 * - Grid layout for sensor cards
 * - Color-coded status indicators (online/offline)
 * - Hybrid map view (satellite + roads)
 * - Auto-centering on GPS position
 * - Uptime and last update timestamp
 * - Hover effects on cards
 * - Mobile-responsive breakpoints
 */
String getHtmlPage() {
  static String html;
  if (html.length() > 0) return html;  // Return cached copy after first build
  html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TTGO T-Beam Sensor Dashboard</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: #333;
      padding: 0;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 2px 10px rgba(0,0,0,0.2);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .navbar-menu {
      display: flex;
      gap: 5px;
      flex-wrap: wrap;
    }
    .nav-link {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 8px;
      transition: all 0.3s;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 5px;
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .page-content {
      padding: 20px;
    }
    .container {
      max-width: 1200px;
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
    .header h1 { font-size: 2em; margin-bottom: 10px; }
    .header p { opacity: 0.9; }
    .content { padding: 30px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      margin-bottom: 30px;
    }
    .card {
      background: #f8f9fa;
      border-radius: 10px;
      padding: 20px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
      transition: transform 0.2s;
    }
    .card:hover { transform: translateY(-5px); }
    .card-title {
      font-size: 0.9em;
      color: #666;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 10px;
    }
    .card-value {
      font-size: 2em;
      font-weight: bold;
      color: #667eea;
    }
    .card-unit {
      font-size: 0.8em;
      color: #999;
      margin-left: 5px;
    }
    #map {
      width: 100%;
      height: 400px;
      border-radius: 10px;
      margin-top: 20px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    .status {
      display: inline-block;
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 0.9em;
      font-weight: bold;
    }
    .status.online { background: #4caf50; color: white; }
    .status.offline { background: #f44336; color: white; }
    .footer {
      text-align: center;
      padding: 20px;
      color: #999;
      font-size: 0.9em;
    }
    @media (max-width: 768px) {
      .grid { grid-template-columns: 1fr; }
      .header h1 { font-size: 1.5em; }
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral( | )rawliteral" + String(BUILD_TIMESTAMP) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link active">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/ota" class="nav-link">🔄 OTA</a>
      <a href="/mqtt" class="nav-link">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link">📡 WiFi</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>
  
  <div class="page-content">
    <div class="container">
      <div class="header">
        <h1>🛰️ TTGO T-Beam Dashboard</h1>
        <p>Real-time Sensor & GPS Monitoring</p>
      </div>
      
      <div class="content">
      <!-- GPS Location Card (Full Width) -->
      <div style="background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); border-radius: 10px; padding: 20px; margin-bottom: 20px; color: white; box-shadow: 0 4px 15px rgba(0,0,0,0.2);">
        <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 15px;">
          <div style="flex: 1; min-width: 200px;">
            <div style="font-size: 0.9em; opacity: 0.9; margin-bottom: 5px;">📍 GPS LOCATION</div>
            <div style="font-size: 1.3em; font-weight: bold;" id="gpsCoords">Waiting for fix...</div>
            <div style="font-size: 0.9em; opacity: 0.8; margin-top: 5px;" id="gpsDetails">--</div>
          </div>
          <div style="text-align: right;">
            <div id="gpsStatusBadge" class="status offline">NO FIX</div>
            <div style="font-size: 0.9em; opacity: 0.9; margin-top: 10px;">
              🛰️ <span id="satellitesCount">0</span> satellites
            </div>
          </div>
        </div>
      </div>
      
      <!-- Sensor Data Grid -->
      <div class="grid">
        <div class="card">
          <div class="card-title">🌡️ Temperature</div>
          <div class="card-value" id="temp">--</div>
          <span class="card-unit">°C</span>
        </div>
        <div class="card">
          <div class="card-title">💧 Humidity</div>
          <div class="card-value" id="humidity">--</div>
          <span class="card-unit">%</span>
        </div>
        <div class="card">
          <div class="card-title">🌪️ Pressure</div>
          <div class="card-value" id="pressure">--</div>
          <span class="card-unit">hPa</span>
        </div>
        <div class="card">
          <div class="card-title">⛰️ Altitude</div>
          <div class="card-value" id="altitude">--</div>
          <span class="card-unit">m</span>
        </div>
        <div class="card">
          <div class="card-title">🚀 Speed</div>
          <div class="card-value" id="speed">--</div>
          <span class="card-unit">km/h</span>
        </div>
        <div class="card">
          <div class="card-title">📡 LoRa Uplinks</div>
          <div class="card-value" id="uplinks">--</div>
        </div>
        <div class="card">
          <div class="card-title">🔋 Battery</div>
          <div class="card-value" id="battery">--</div>
          <span class="card-unit">V</span>
          <div style="font-size: 0.8em; color: #999; margin-top: 5px;" id="batteryStatus">--</div>
        </div>
      </div>
      
      <div id="map"></div>
    </div>
    
    <div class="footer">
      <p>Last Update: <span id="lastUpdate">--</span></p>
      <p>Uptime: <span id="uptime">--</span></p>
    </div>
  </div>

  <script>
    let map, marker, pathPolyline;
    
    function initMap() {
      // Check if Leaflet is available (requires internet connection)
      if (typeof L === 'undefined') {
        console.log('Leaflet not available - map disabled (no internet connection)');
        const mapDiv = document.getElementById('map');
        if (mapDiv) {
          mapDiv.innerHTML = '<div style="display:flex;align-items:center;justify-content:center;height:100%;color:#666;text-align:center;padding:20px;"><div>📡 Map requires internet connection<br><small>Connect to WiFi to view GPS location on map</small></div></div>';
        }
        return;
      }
      
      try {
        // Initialize Leaflet map with OpenStreetMap tiles
        map = L.map('map').setView([0, 0], 2);
        
        // Add OpenStreetMap tile layer
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
          attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
          maxZoom: 19
        }).addTo(map);
        
        // Create marker (initially hidden)
        marker = L.marker([0, 0]).addTo(map);
        marker.bindPopup('TTGO T-Beam Location');
        
        // Create polyline for GPS path (blue line with 50% opacity)
        pathPolyline = L.polyline([], {
          color: '#2196F3',
          weight: 3,
          opacity: 0.7,
          smoothFactor: 1
        }).addTo(map);
      } catch (error) {
        console.error('Error initializing map:', error);
      }
    }
    
    function updateData() {
      fetch('/api/data')
        .then(response => response.json())
        .then(data => {
          // Update sensor cards
          document.getElementById('temp').textContent = data.temperature.toFixed(1);
          document.getElementById('humidity').textContent = data.humidity.toFixed(1);
          document.getElementById('pressure').textContent = data.pressure.toFixed(1);
          document.getElementById('altitude').textContent = data.altitude.toFixed(1);
          document.getElementById('speed').textContent = data.gps.speed.toFixed(1);
          document.getElementById('uplinks').textContent = data.loraUplinks;
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
          document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
          
          // Update battery information
          if (data.battery && data.battery.voltage > 0) {
            document.getElementById('battery').textContent = data.battery.voltage.toFixed(2);
            document.getElementById('batteryStatus').textContent = data.battery.charging ? '⚡ Charging' : '🔌 Discharging';
            document.getElementById('batteryStatus').style.color = data.battery.charging ? '#4CAF50' : '#FF9800';
          } else {
            document.getElementById('battery').textContent = '--';
            document.getElementById('batteryStatus').textContent = 'No battery';
          }
          
          // Update GPS information
          document.getElementById('satellitesCount').textContent = data.gps.satellites;
          
          if (data.gps.valid) {
            // Update GPS coordinates display
            const lat = data.gps.latitude.toFixed(6);
            const lon = data.gps.longitude.toFixed(6);
            document.getElementById('gpsCoords').textContent = `${lat}, ${lon}`;
            document.getElementById('gpsDetails').textContent =
              `Alt: ${data.gps.altitude.toFixed(1)}m | HDOP: ${data.gps.hdop.toFixed(2)} | Speed: ${data.gps.speed.toFixed(1)} km/h`;
            document.getElementById('gpsStatusBadge').innerHTML = '<span class="status online">GPS LOCKED</span>';
            
            // Update map (only if Leaflet is available)
            if (map && marker) {
              const latLng = [data.gps.latitude, data.gps.longitude];
              marker.setLatLng(latLng);
              map.setView(latLng, 15);
              marker.setPopupContent(`
                <b>TTGO T-Beam Location</b><br>
                Lat: ${lat}<br>
                Lon: ${lon}<br>
                Alt: ${data.gps.altitude.toFixed(1)}m<br>
                Satellites: ${data.gps.satellites}
              `);
              marker.openPopup();
              
              // GPS path is fetched separately every 30s to reduce payload size
            }
          } else {
            // No current fix - check if we have last known position
            if (data.gps.latitude !== 0 && data.gps.longitude !== 0) {
              // Show last known position
              const lat = data.gps.latitude.toFixed(6);
              const lon = data.gps.longitude.toFixed(6);
              document.getElementById('gpsCoords').textContent = `${lat}, ${lon} (Last Known)`;
              document.getElementById('gpsDetails').textContent = `Last update: ${data.gps.lastUpdate || 'unknown'}`;
              document.getElementById('gpsStatusBadge').innerHTML = '<span class="status offline">LAST KNOWN</span>';
              
              // Show last known position on map with different marker style
              const latLng = [data.gps.latitude, data.gps.longitude];
              marker.setLatLng(latLng);
              map.setView(latLng, 15);
              marker.setPopupContent(`
                <b>Last Known Position</b><br>
                Lat: ${lat}<br>
                Lon: ${lon}<br>
                Last update: ${data.gps.lastUpdate || 'unknown'}<br>
                <i>Waiting for GPS fix...</i>
              `);
              marker.openPopup();
            } else {
              document.getElementById('gpsCoords').textContent = 'Waiting for GPS fix...';
              document.getElementById('gpsDetails').textContent = 'Move to open area with clear sky view';
              document.getElementById('gpsStatusBadge').innerHTML = '<span class="status offline">NO FIX</span>';
            }
          }
        })
        .catch(err => console.error('Error fetching data:', err));
    }
    
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const mins = Math.floor((seconds % 3600) / 60);
      return `${days}d ${hours}h ${mins}m`;
    }
    
    // Load Leaflet CSS and JS asynchronously with timeout
    let leafletLoaded = false;
    
    // Load CSS first (non-blocking)
    const leafletCSS = document.createElement('link');
    leafletCSS.rel = 'stylesheet';
    leafletCSS.href = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';
    leafletCSS.onerror = function() {
      console.log('Failed to load Leaflet CSS - continuing without it');
    };
    document.head.appendChild(leafletCSS);
    
    // Load JS
    const leafletScript = document.createElement('script');
    leafletScript.src = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
    leafletScript.async = true;
    leafletScript.onload = function() {
      leafletLoaded = true;
      initMap();
    };
    leafletScript.onerror = function() {
      console.log('Failed to load Leaflet JS - map will be disabled');
      initMap(); // Call anyway to show message
    };
    document.head.appendChild(leafletScript);
    
    // Timeout after 2 seconds if Leaflet doesn't load
    setTimeout(function() {
      if (!leafletLoaded) {
        console.log('Leaflet load timeout - continuing without map');
        initMap(); // Call anyway to show message
      }
    }, 2000);
    
    // Fetch GPS path separately - it changes slowly, no need to send with every poll
    function updateGpsPath() {
      fetch('/api/gps-path')
        .then(response => response.json())
        .then(data => {
          if (pathPolyline && data.path && data.path.length > 0) {
            const pathCoords = data.path.map(point => [point.lat, point.lon]);
            pathPolyline.setLatLngs(pathCoords);
          }
        })
        .catch(err => console.error('GPS path fetch error:', err));
    }

    updateData();
    setInterval(updateData, 3000);   // Poll sensor data every 3s (was 1s)
    setInterval(updateGpsPath, 30000); // Poll GPS path every 30s
    setTimeout(updateGpsPath, 3500);   // First path fetch shortly after load
  </script>
    </div>
  </div>
</body>
</html>
)rawliteral";
  return html;
}

/**
 * @brief Initialize and configure the async web server
 *
 * Sets up HTTP endpoints for:
 * - Main dashboard page (/)
 * - JSON API endpoint (/api/data)
 * - WiFi reset endpoint (/reset-wifi)
 *
 * @note Uses ESPAsyncWebServer for non-blocking operation
 * @note Server runs on port 80 (standard HTTP)
 * @note Does not interfere with LoRaWAN or GPS timing
 *
 * Endpoints:
 * - GET /           : Serves HTML dashboard
 * - GET /api/data   : Returns JSON sensor data
 * - GET /reset-wifi : Resets WiFi credentials and restarts
 *
 * @see getHtmlPage() for dashboard HTML
 * @see getJsonSensorData() for API response format
 */
