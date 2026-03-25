#pragma once
void setupWebServer() {
  // Serve main page - with long cache since the HTML is static
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", getHtmlPage());
    response->addHeader("Cache-Control", "public, max-age=3600");
    request->send(response);
  });
  
  // Alias for main page - /ui redirects to dashboard
  server.on("/ui", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/");
  });
  
  // API endpoint for JSON data - no-cache so browser always fetches fresh values
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", getJsonSensorData());
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  // GPS path endpoint - separate from /api/data to keep the main poll lightweight
  server.on("/api/gps-path", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    JsonArray pathArray = doc["path"].to<JsonArray>();
    for (int i = 0; i < gpsPathCount; i++) {
      int idx = (gpsPathIndex - gpsPathCount + i + gpsPathSize) % gpsPathSize;
      if (gpsPath[idx].valid) {
        JsonObject point = pathArray.add<JsonObject>();
        point["lat"] = gpsPath[idx].lat;
        point["lon"] = gpsPath[idx].lon;
      }
    }
    String output;
    serializeJson(doc, output);
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  
  // WiFi reset endpoint
  server.on("/reset-wifi", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "WiFi settings will be reset. Device will restart.");
    delay(1000);
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    ESP.restart();
  });
  
  // LoRa configuration page
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LoRaWAN Configuration</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
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
      max-width: 800px;
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
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      font-weight: bold;
      margin-bottom: 5px;
      color: #333;
    }
    input[type="text"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
      font-family: monospace;
    }
    input[type="text"]:focus {
      outline: none;
      border-color: #667eea;
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
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-danger {
      background: #f44336;
      color: white;
    }
    .btn-danger:hover {
      background: #da190b;
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .btn-secondary:hover {
      background: #545b62;
    }
    .current-keys {
      background: #f8f9fa;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      font-family: monospace;
      font-size: 12px;
    }
    .current-keys div {
      margin-bottom: 8px;
    }
    .status {
      padding: 10px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .status.success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .status.error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .help-text {
      font-size: 12px;
      color: #666;
      margin-top: 5px;
    }
    .back-link {
      display: inline-block;
      margin-top: 20px;
      color: #667eea;
      text-decoration: none;
    }
    .back-link:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link active">🔐 LoRa Config</a>
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
      <h1>🔐 LoRaWAN Configuration</h1>
      <p>Configure Activation Mode and Keys</p>
    </div>
    
    <div class="content">
      <div id="status" class="status"></div>
      
      <!-- Activation Mode Selection -->
      <div style="background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 20px; border-left: 4px solid #667eea;">
        <h3 style="margin-top: 0;">📡 Activation Mode</h3>
        <div style="display: flex; gap: 30px; margin-top: 15px;">
          <label style="display: flex; align-items: center; cursor: pointer;">
            <input type="radio" name="activationMode" value="otaa" id="modeOTAA" checked onchange="switchMode()" style="margin-right: 8px; width: 18px; height: 18px;">
            <span style="font-weight: bold;">OTAA (Over-The-Air Activation)</span>
          </label>
          <label style="display: flex; align-items: center; cursor: pointer;">
            <input type="radio" name="activationMode" value="abp" id="modeABP" onchange="switchMode()" style="margin-right: 8px; width: 18px; height: 18px;">
            <span style="font-weight: bold;">ABP (Activation By Personalization)</span>
          </label>
        </div>
        <p style="margin: 10px 0 0 0; font-size: 13px; color: #666;">
          <strong>OTAA:</strong> Device joins network with APPEUI, DEVEUI, APPKEY (recommended)<br>
          <strong>ABP:</strong> Device uses pre-configured session keys (DevAddr, NwkSKey, AppSKey)
        </p>
      </div>
      
      <!-- OTAA Help Section -->
      <div id="otaaHelp" style="background: #e7f3ff; padding: 15px; border-radius: 8px; margin-bottom: 20px; border-left: 4px solid #2196F3;">
        <h3 style="margin-top: 0; color: #1976D2;">📘 OTAA Key Format Guide</h3>
        <p style="margin: 10px 0;"><strong>APPEUI (JoinEUI):</strong> 8 bytes, <span style="color: #d32f2f;">little-endian</span> for LMIC</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          If TTN shows: <code>70B3D57ED0012345</code><br>
          Enter here: <code>45 23 01 D0 7E D5 B3 70</code> (reversed)
        </p>
        
        <p style="margin: 10px 0;"><strong>DEVEUI:</strong> 8 bytes, <span style="color: #d32f2f;">little-endian</span> for LMIC</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          If TTN shows: <code>0004A30B001A2B3C</code><br>
          Enter here: <code>3C 2B 1A 00 0B A3 04 00</code> (reversed)
        </p>
        
        <p style="margin: 10px 0;"><strong>APPKEY (AppKey/NwkKey):</strong> 16 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>00112233445566778899AABBCCDDEEFF</code>
        </p>
        
        <p style="margin: 15px 0 5px 0; font-size: 13px;"><strong>💡 Tip:</strong> Use spaces or colons for readability - they'll be removed automatically</p>
      </div>
      
      <!-- ABP Help Section -->
      <div id="abpHelp" style="background: #fff3e0; padding: 15px; border-radius: 8px; margin-bottom: 20px; border-left: 4px solid #ff9800; display: none;">
        <h3 style="margin-top: 0; color: #f57c00;">📘 ABP Key Format Guide</h3>
        <p style="margin: 10px 0;"><strong>DevAddr (Device Address):</strong> 4 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>260B1234</code> or <code>26 0B 12 34</code>
        </p>
        
        <p style="margin: 10px 0;"><strong>NwkSKey (Network Session Key):</strong> 16 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>00112233445566778899AABBCCDDEEFF</code>
        </p>
        
        <p style="margin: 10px 0;"><strong>AppSKey (Application Session Key):</strong> 16 bytes, <span style="color: #388e3c;">big-endian</span> (normal order)</p>
        <p style="margin: 5px 0 5px 20px; font-size: 12px;">
          Copy directly from TTN/ChirpStack as-is<br>
          Example: <code>FFEEDDCCBBAA99887766554433221100</code>
        </p>
        
        <p style="margin: 15px 0 5px 0; font-size: 13px;"><strong>⚠️ Note:</strong> ABP devices don't perform join - they start transmitting immediately</p>
      </div>
      
      <div class="current-keys">
        <strong>Current Keys:</strong>
        <div id="currentKeys">Loading...</div>
      </div>
      
      <form id="loraForm">
        <!-- OTAA Fields -->
        <div id="otaaFields">
          <div class="form-group">
            <label for="appeui">APPEUI (JoinEUI) - 8 bytes hex</label>
            <input type="text" id="appeui" name="appeui" placeholder="45 23 01 D0 7E D5 B3 70" maxlength="23">
            <div class="help-text">⚠️ Little-endian (reversed from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="deveui">DEVEUI - 8 bytes hex</label>
            <input type="text" id="deveui" name="deveui" placeholder="3C 2B 1A 00 0B A3 04 00" maxlength="23">
            <div class="help-text">⚠️ Little-endian (reversed from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="appkey">APPKEY - 16 bytes hex</label>
            <input type="text" id="appkey" name="appkey" placeholder="00112233445566778899AABBCCDDEEFF" maxlength="47">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
        </div>
        
        <!-- ABP Fields -->
        <div id="abpFields" style="display: none;">
          <div class="form-group">
            <label for="devaddr">DevAddr (Device Address) - 4 bytes hex</label>
            <input type="text" id="devaddr" name="devaddr" placeholder="260B1234" maxlength="11">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="nwkskey">NwkSKey (Network Session Key) - 16 bytes hex</label>
            <input type="text" id="nwkskey" name="nwkskey" placeholder="00112233445566778899AABBCCDDEEFF" maxlength="47">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
          
          <div class="form-group">
            <label for="appskey">AppSKey (Application Session Key) - 16 bytes hex</label>
            <input type="text" id="appskey" name="appskey" placeholder="FFEEDDCCBBAA99887766554433221100" maxlength="47">
            <div class="help-text">✓ Big-endian (copy as-is from TTN/ChirpStack)</div>
          </div>
        </div>
        
        <button type="submit" class="btn btn-primary">💾 Save Keys</button>
        <button type="button" class="btn btn-danger" onclick="clearKeys()">🗑️ Clear Keys</button>
        <button type="button" class="btn btn-secondary" onclick="window.location.href='/'">🏠 Dashboard</button>
        <button type="button" class="btn btn-secondary" onclick="window.location.href='/settings'">⚙️ Settings</button>
      </form>
      
      <hr style="margin: 30px 0; border: none; border-top: 2px solid #eee;">
      
      <h3 style="margin-bottom: 15px;">📡 WiFi Control</h3>
      <div style="margin-bottom: 20px;">
        <button type="button" class="btn btn-primary" onclick="wifiControl('reset')">🔄 Reset WiFi</button>
        <button type="button" class="btn btn-secondary" onclick="wifiControl('restart')">⚡ Restart Device</button>
      </div>
      <div class="help-text" style="margin-bottom: 20px;">
        Reset WiFi will clear saved credentials and restart the device in AP mode for reconfiguration.
      </div>
      
    </div>
      </div>
    </div>
  </div>

  <script>
    function showStatus(message, isError) {
      const status = document.getElementById('status');
      status.textContent = message;
      status.className = 'status ' + (isError ? 'error' : 'success');
      status.style.display = 'block';
      setTimeout(() => {
        status.style.display = 'none';
      }, 5000);
    }
    
    function switchMode() {
      const isOTAA = document.getElementById('modeOTAA').checked;
      
      // Toggle visibility of help sections
      document.getElementById('otaaHelp').style.display = isOTAA ? 'block' : 'none';
      document.getElementById('abpHelp').style.display = isOTAA ? 'none' : 'block';
      
      // Toggle visibility of form fields
      document.getElementById('otaaFields').style.display = isOTAA ? 'block' : 'none';
      document.getElementById('abpFields').style.display = isOTAA ? 'none' : 'block';
    }
    
    function loadCurrentKeys() {
      fetch('/api/lora-keys')
        .then(response => response.json())
        .then(data => {
          // Set mode radio buttons
          if (data.useABP) {
            document.getElementById('modeABP').checked = true;
            switchMode();
          } else {
            document.getElementById('modeOTAA').checked = true;
            switchMode();
          }
          
          // Display current keys based on mode
          if (data.useABP) {
            document.getElementById('currentKeys').innerHTML = `
              <div><strong>Mode:</strong> ABP (Activation By Personalization)</div>
              <div>DevAddr: ${data.devaddr || 'Not set'}</div>
              <div>NwkSKey: ${data.nwkskey || 'Not set'}</div>
              <div>AppSKey: ${data.appskey || 'Not set'}</div>
              <div>Status: ${data.configured ? 'Configured (from NVS)' : 'Using defaults'}</div>
            `;
            if (data.devaddr) document.getElementById('devaddr').value = data.devaddr.replace(/:/g, '');
            if (data.nwkskey) document.getElementById('nwkskey').value = data.nwkskey.replace(/:/g, '');
            if (data.appskey) document.getElementById('appskey').value = data.appskey.replace(/:/g, '');
          } else {
            document.getElementById('currentKeys').innerHTML = `
              <div><strong>Mode:</strong> OTAA (Over-The-Air Activation)</div>
              <div>APPEUI: ${data.appeui}</div>
              <div>DEVEUI: ${data.deveui}</div>
              <div>APPKEY: ${data.appkey}</div>
              <div>Status: ${data.configured ? 'Configured (from NVS)' : 'Using defaults'}</div>
            `;
            document.getElementById('appeui').value = data.appeui.replace(/:/g, '');
            document.getElementById('deveui').value = data.deveui.replace(/:/g, '');
            document.getElementById('appkey').value = data.appkey.replace(/:/g, '');
          }
        })
        .catch(err => {
          document.getElementById('currentKeys').textContent = 'Error loading keys';
        });
    }
    
    document.getElementById('loraForm').addEventListener('submit', function(e) {
      e.preventDefault();
      
      const isOTAA = document.getElementById('modeOTAA').checked;
      let data = { useABP: !isOTAA };
      
      if (isOTAA) {
        // OTAA mode - validate and send OTAA keys
        data.appeui = document.getElementById('appeui').value.replace(/[:\s-]/g, '');
        data.deveui = document.getElementById('deveui').value.replace(/[:\s-]/g, '');
        data.appkey = document.getElementById('appkey').value.replace(/[:\s-]/g, '');
        
        // Validate hex format
        if (data.appeui.length !== 16 || !/^[0-9A-Fa-f]+$/.test(data.appeui)) {
          showStatus('Error: APPEUI must be 16 hex characters (8 bytes)', true);
          return;
        }
        if (data.deveui.length !== 16 || !/^[0-9A-Fa-f]+$/.test(data.deveui)) {
          showStatus('Error: DEVEUI must be 16 hex characters (8 bytes)', true);
          return;
        }
        if (data.appkey.length !== 32 || !/^[0-9A-Fa-f]+$/.test(data.appkey)) {
          showStatus('Error: APPKEY must be 32 hex characters (16 bytes)', true);
          return;
        }
      } else {
        // ABP mode - validate and send ABP keys
        data.devaddr = document.getElementById('devaddr').value.replace(/[:\s-]/g, '');
        data.nwkskey = document.getElementById('nwkskey').value.replace(/[:\s-]/g, '');
        data.appskey = document.getElementById('appskey').value.replace(/[:\s-]/g, '');
        
        // Validate hex format
        if (data.devaddr.length !== 8 || !/^[0-9A-Fa-f]+$/.test(data.devaddr)) {
          showStatus('Error: DevAddr must be 8 hex characters (4 bytes)', true);
          return;
        }
        if (data.nwkskey.length !== 32 || !/^[0-9A-Fa-f]+$/.test(data.nwkskey)) {
          showStatus('Error: NwkSKey must be 32 hex characters (16 bytes)', true);
          return;
        }
        if (data.appskey.length !== 32 || !/^[0-9A-Fa-f]+$/.test(data.appskey)) {
          showStatus('Error: AppSKey must be 32 hex characters (16 bytes)', true);
          return;
        }
      }
      
      fetch('/api/lora-keys', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(response => response.json())
      .then(result => {
        if (result.success) {
          showStatus('Keys saved successfully! Restart device for changes to take effect.', false);
          loadCurrentKeys();
        } else {
          showStatus('Error: ' + result.message, true);
        }
      })
      .catch(err => {
        showStatus('Error saving keys: ' + err, true);
      });
    });
    
    function clearKeys() {
      if (confirm('Clear all LoRa keys and revert to defaults?')) {
        fetch('/api/lora-keys', {method: 'DELETE'})
          .then(response => response.json())
          .then(result => {
            if (result.success) {
              showStatus('Keys cleared. Restart device for changes to take effect.', false);
              loadCurrentKeys();
            } else {
              showStatus('Error clearing keys', true);
            }
          })
          .catch(err => {
            showStatus('Error: ' + err, true);
          });
      }
    }
    
    function wifiControl(action) {
      let confirmMsg = '';
      let endpoint = '';
      
      if (action === 'reset') {
        confirmMsg = 'Reset WiFi settings? Device will restart in AP mode for reconfiguration.';
        endpoint = '/api/wifi-reset';
      } else if (action === 'restart') {
        confirmMsg = 'Restart device now?';
        endpoint = '/api/restart';
      }
      
      if (confirm(confirmMsg)) {
        fetch(endpoint, {method: 'POST'})
          .then(response => response.json())
          .then(result => {
            if (result.success) {
              showStatus(result.message, false);
              setTimeout(() => {
                if (action === 'reset') {
                  alert('Device restarting in AP mode. Connect to "TTGO-T-Beam-Setup" WiFi network.');
                }
              }, 2000);
            } else {
              showStatus('Error: ' + result.message, true);
            }
          })
          .catch(err => {
            showStatus('Command sent. Device restarting...', false);
          });
      }
    }
    
    loadCurrentKeys();
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });
  
  // LoRa keys API - GET current keys
  server.on("/api/lora-keys", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["appeui"] = bytesToHexString(storedAPPEUI, 8, ":");
    doc["deveui"] = bytesToHexString(storedDEVEUI, 8, ":");
    doc["appkey"] = bytesToHexString(storedAPPKEY, 16, ":");
    doc["configured"] = loraKeysConfigured;
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
  
  // LoRa keys API - POST to save keys (OTAA or ABP)
  server.on("/api/lora-keys", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      bool success = true;
      String message = "";
      
      // Check if ABP mode
      bool isABP = doc["useABP"].as<bool>();
      useABP = isABP;
      
      if (isABP) {
        // ABP mode - parse DevAddr, NwkSKey, AppSKey
        String devaddr = doc["devaddr"].as<String>();
        String nwkskey = doc["nwkskey"].as<String>();
        String appskey = doc["appskey"].as<String>();
        
        if (!hexStringToBytes(devaddr, storedDEVADDR, 4)) {
          success = false;
          message = "Invalid DevAddr format (must be 8 hex chars)";
        } else if (!hexStringToBytes(nwkskey, storedNWKSKEY, 16)) {
          success = false;
          message = "Invalid NwkSKey format (must be 32 hex chars)";
        } else if (!hexStringToBytes(appskey, storedAPPSKEY, 16)) {
          success = false;
          message = "Invalid AppSKey format (must be 32 hex chars)";
        } else {
          success = saveLoRaKeysToNVS();
          message = success ? "ABP keys saved successfully" : "Failed to save ABP keys";
        }
      } else {
        // OTAA mode - parse APPEUI, DEVEUI, APPKEY
        String appeui = doc["appeui"].as<String>();
        String deveui = doc["deveui"].as<String>();
        String appkey = doc["appkey"].as<String>();
        
        if (!hexStringToBytes(appeui, storedAPPEUI, 8)) {
          success = false;
          message = "Invalid APPEUI format (must be 16 hex chars)";
        } else if (!hexStringToBytes(deveui, storedDEVEUI, 8)) {
          success = false;
          message = "Invalid DEVEUI format (must be 16 hex chars)";
        } else if (!hexStringToBytes(appkey, storedAPPKEY, 16)) {
          success = false;
          message = "Invalid APPKEY format (must be 32 hex chars)";
        } else {
          success = saveLoRaKeysToNVS();
          message = success ? "OTAA keys saved successfully" : "Failed to save OTAA keys";
        }
      }
      
      String response = "{\"success\":" + String(success ? "true" : "false") +
                       ",\"message\":\"" + message + "\"}";
      request->send(success ? 200 : 400, "application/json", response);
    }
  );
  
  // LoRa keys API - DELETE to clear keys
  server.on("/api/lora-keys", HTTP_DELETE, [](AsyncWebServerRequest *request){
    clearLoRaKeysFromNVS();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Keys cleared\"}");
  });
  
  // WiFi control API - Reset WiFi settings
  server.on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi settings reset. Restarting...\"}");
    delay(1000);
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    ESP.restart();
  });
  
  // System control API - Restart device
  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Device restarting...\"}");
    delay(1000);
    ESP.restart();
  });
  
  // WiFi network selection page
  server.on("/wifi-select", HTTP_GET, [](AsyncWebServerRequest *request){
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi Network Selection</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
    }
    .container {
      max-width: 600px;
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
    .network-list {
      list-style: none;
      margin-bottom: 20px;
    }
    .network-item {
      padding: 15px;
      border: 2px solid #ddd;
      border-radius: 8px;
      margin-bottom: 10px;
      cursor: pointer;
      transition: all 0.2s;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .network-item:hover {
      border-color: #667eea;
      background: #f8f9fa;
    }
    .network-item.selected {
      border-color: #667eea;
      background: #e7f0ff;
    }
    .network-name {
      font-weight: bold;
      font-size: 16px;
    }
    .network-signal {
      color: #666;
      font-size: 14px;
    }
    .network-secure {
      color: #f44336;
      font-size: 12px;
    }
    .password-section {
      display: none;
      margin-top: 20px;
      padding: 20px;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .password-section.show {
      display: block;
    }
    input[type="password"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
      margin-top: 10px;
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
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .status {
      padding: 10px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .status.success {
      background: #d4edda;
      color: #155724;
      display: block;
    }
    .status.error {
      background: #f8d7da;
      color: #721c24;
      display: block;
    }
    .loading {
      text-align: center;
      padding: 20px;
      color: #666;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>📡 WiFi Network Selection</h1>
      <p>Choose a network to connect</p>
    </div>
    
    <div class="content">
      <div id="status" class="status"></div>
      
      <div class="loading" id="loading">
        Scanning for networks...
      </div>
      
      <ul class="network-list" id="networkList" style="display:none;"></ul>
      
      <div class="password-section" id="passwordSection">
        <h3>Enter Password</h3>
        <p>Network: <strong id="selectedNetwork"></strong></p>
        <input type="password" id="password" placeholder="Enter WiFi password">
        <br>
        <button class="btn btn-primary" onclick="connectToNetwork()">Connect</button>
        <button class="btn btn-secondary" onclick="cancelSelection()">Cancel</button>
      </div>
      
      <button class="btn btn-secondary" onclick="scanNetworks()" style="margin-top: 20px;">🔄 Rescan</button>
      <button class="btn btn-secondary" onclick="window.location.href='/config'">← Back</button>
    </div>
  </div>

  <script>
    let selectedSSID = '';
    let selectedSecure = false;
    
    function showStatus(message, isError) {
      const status = document.getElementById('status');
      status.textContent = message;
      status.className = 'status ' + (isError ? 'error' : 'success');
      setTimeout(() => {
        status.style.display = 'none';
      }, 5000);
    }
    
    function scanNetworks() {
      document.getElementById('loading').style.display = 'block';
      document.getElementById('networkList').style.display = 'none';
      
      fetch('/api/wifi-scan')
        .then(response => response.json())
        .then(data => {
          document.getElementById('loading').style.display = 'none';
          displayNetworks(data.networks);
        })
        .catch(err => {
          document.getElementById('loading').textContent = 'Error scanning networks';
          showStatus('Error: ' + err, true);
        });
    }
    
    function displayNetworks(networks) {
      const list = document.getElementById('networkList');
      list.innerHTML = '';
      list.style.display = 'block';
      
      networks.forEach(network => {
        const item = document.createElement('li');
        item.className = 'network-item';
        item.onclick = () => selectNetwork(network.ssid, network.secure);
        
        item.innerHTML = `
          <div>
            <div class="network-name">${network.ssid}</div>
            <div class="network-secure">${network.secure ? '🔒 Secured' : '🔓 Open'}</div>
          </div>
          <div class="network-signal">${network.rssi} dBm</div>
        `;
        
        list.appendChild(item);
      });
    }
    
    function selectNetwork(ssid, secure) {
      selectedSSID = ssid;
      selectedSecure = secure;
      
      // Highlight selected
      document.querySelectorAll('.network-item').forEach(item => {
        item.classList.remove('selected');
      });
      event.currentTarget.classList.add('selected');
      
      // Show password section if secured
      document.getElementById('selectedNetwork').textContent = ssid;
      document.getElementById('passwordSection').className = 'password-section show';
      
      if (!secure) {
        document.getElementById('password').style.display = 'none';
      } else {
        document.getElementById('password').style.display = 'block';
      }
    }
    
    function cancelSelection() {
      document.getElementById('passwordSection').className = 'password-section';
      selectedSSID = '';
    }
    
    function connectToNetwork() {
      if (!selectedSSID) {
        showStatus('Please select a network', true);
        return;
      }
      
      const password = document.getElementById('password').value;
      
      if (selectedSecure && !password) {
        showStatus('Please enter password', true);
        return;
      }
      
      showStatus('Connecting to ' + selectedSSID + '...', false);
      
      fetch('/api/wifi-connect', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          ssid: selectedSSID,
          password: password
        })
      })
      .then(response => response.json())
      .then(result => {
        if (result.success) {
          showStatus('Connected! Redirecting...', false);
          setTimeout(() => {
            window.location.href = '/';
          }, 3000);
        } else {
          showStatus('Connection failed: ' + result.message, true);
        }
      })
      .catch(err => {
        showStatus('Error: ' + err, true);
      });
    }
    
    // Auto-scan on load
    scanNetworks();
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });
  
  // WiFi scan API
  server.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    for (int i = 0; i < n && i < 20; i++) {
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
  
  // WiFi connect API (duplicate - will be handled by the non-blocking version below)
  // This endpoint is kept for compatibility but redirects to the main handler
  
  // System settings/configuration page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>System Settings</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 0;
      min-height: 100vh;
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
    .header h1 { font-size: 28px; margin-bottom: 10px; }
    .header p { opacity: 0.9; }
    .content { padding: 30px; }
    .section {
      margin-bottom: 30px;
      padding: 20px;
      background: #f8f9fa;
      border-radius: 10px;
      border-left: 4px solid #667eea;
    }
    .section h2 {
      font-size: 20px;
      margin-bottom: 15px;
      color: #333;
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
    .control-label {
      font-weight: bold;
      color: #333;
    }
    .control-value {
      color: #666;
      font-size: 14px;
    }
    .toggle-switch {
      position: relative;
      width: 60px;
      height: 30px;
    }
    .toggle-switch input {
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
      border-radius: 30px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 22px;
      width: 22px;
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
      transform: translateX(30px);
    }
    .input-group {
      margin-bottom: 15px;
    }
    .input-group label {
      display: block;
      font-weight: bold;
      margin-bottom: 5px;
      color: #333;
    }
    .input-group input {
      width: 100%;
      padding: 10px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
    }
    .input-group input:focus {
      outline: none;
      border-color: #667eea;
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
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .btn-secondary:hover {
      background: #5a6268;
    }
    .status-badge {
      display: inline-block;
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 12px;
      font-weight: bold;
    }
    .status-on {
      background: #28a745;
      color: white;
    }
    .status-off {
      background: #dc3545;
      color: white;
    }
    .nav-buttons {
      text-align: center;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link active">⚙️ Settings</a>
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
        <h1>⚙️ System Settings</h1>
        <p>Configure device behavior and power management</p>
      </div>
      
      <div class="content">
        <!-- Status & Controls Combined -->
        <div class="section">
          <h2>📊 Status & Controls</h2>
        <div class="control-row">
          <div>
            <div class="control-label">WiFi</div>
            <div class="control-value" id="wifi-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-wifi" onchange="toggleWiFi(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">GPS</div>
            <div class="control-value" id="gps-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-gps" onchange="toggleGPS(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">Debug Mode</div>
            <div class="control-value" id="debug-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-debug" onchange="toggleDebug(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">Serial Output</div>
            <div class="control-value" id="serial-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-serial" onchange="toggleSerial(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">Display</div>
            <div class="control-value" id="display-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-display" onchange="toggleDisplay(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">Sleep Mode</div>
            <div class="control-value" id="sleep-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-sleep" onchange="toggleSleep(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="control-row">
          <div>
            <div class="control-label">LoRa Send Interval</div>
            <div class="control-value" id="lora-interval">Loading...</div>
          </div>
        </div>
      </div>

      <!-- Clock & Time -->
      <div class="section">
        <h2>🕐 Clock &amp; Time</h2>
        <div class="control-row">
          <div>
            <div class="control-label">Time Format</div>
            <div class="control-value" id="time-format-status">Loading...</div>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="toggle-24h" onchange="setTimeFormat(this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <p style="font-size:12px;color:#666;margin:4px 0 12px 4px;">Toggle ON = 24h, OFF = 12h AM/PM</p>
        <div class="input-group">
          <label>Timezone (UTC offset, no auto-DST)</label>
          <select id="tz-select" onchange="setTimezone(this.value)" style="width:100%;padding:8px;border-radius:6px;border:1px solid #ddd;">
            <option value="-43200:IDLW">UTC-12 (IDLW, Int'l Date Line West)</option>
            <option value="-39600:SST">UTC-11 (SST, Samoa)</option>
            <option value="-36000:HST">UTC-10 (HST, Hawaii)</option>
            <option value="-34200:MART">UTC-9:30 (MART, Marquesas)</option>
            <option value="-32400:AKST">UTC-9 (AKST, Alaska Standard)</option>
            <option value="-28800:PST">UTC-8 (PST, US Pacific Standard)</option>
            <option value="-25200:MST">UTC-7 (MST, US Mountain Standard / Arizona)</option>
            <option value="-21600:CST">UTC-6 (CST, US Central Standard)</option>
            <option value="-18000:EST">UTC-5 (EST, US Eastern Standard)</option>
            <option value="-14400:AST">UTC-4 (AST, US Atlantic Standard / EDT)</option>
            <option value="-12600:NST">UTC-3:30 (NST, Newfoundland Standard)</option>
            <option value="-10800:BRT">UTC-3 (BRT, Brazil / Argentina)</option>
            <option value="-7200:GST">UTC-2 (GST, South Georgia)</option>
            <option value="-3600:CVT">UTC-1 (CVT, Cape Verde)</option>
            <option value="0:UTC">UTC+0 (UTC / GMT)</option>
            <option value="3600:CET">UTC+1 (CET, Central European Standard / BST)</option>
            <option value="7200:EET">UTC+2 (EET, Eastern European Standard / CEST)</option>
            <option value="10800:MSK">UTC+3 (MSK, Moscow / EEST)</option>
            <option value="12600:IRST">UTC+3:30 (IRST, Iran Standard)</option>
            <option value="14400:GST4">UTC+4 (Gulf Standard)</option>
            <option value="16200:AFT">UTC+4:30 (AFT, Afghanistan)</option>
            <option value="18000:PKT">UTC+5 (PKT, Pakistan)</option>
            <option value="19800:IST">UTC+5:30 (IST, India)</option>
            <option value="20700:NPT">UTC+5:45 (NPT, Nepal)</option>
            <option value="21600:BST6">UTC+6 (BST, Bangladesh)</option>
            <option value="23400:MMT">UTC+6:30 (MMT, Myanmar)</option>
            <option value="25200:ICT">UTC+7 (ICT, Indochina)</option>
            <option value="28800:CST8">UTC+8 (CST, China / Singapore / perth)</option>
            <option value="32400:JST">UTC+9 (JST, Japan / KST Korea)</option>
            <option value="34200:ACST">UTC+9:30 (ACST, Australia Central Standard)</option>
            <option value="36000:AEST">UTC+10 (AEST, Australia Eastern Standard)</option>
            <option value="37800:LHST">UTC+10:30 (LHST, Lord Howe Island)</option>
            <option value="39600:AEDT">UTC+11 (AEDT, Australia Eastern Daylight)</option>
            <option value="43200:NZST">UTC+12 (NZST, New Zealand Standard)</option>
            <option value="45900:CHAST">UTC+12:45 (CHAST, Chatham Standard)</option>
            <option value="46800:NZDT">UTC+13 (NZDT, New Zealand Daylight)</option>
          </select>
          <p style="font-size:12px;color:#666;margin-top:5px;" id="ntp-status-msg">Loading NTP status...</p>
        </div>
      </div>
      
      <!-- Configuration -->
      <div class="section">
        <h2>⏱️ Intervals</h2>
        <div class="input-group">
          <label>LoRa Send Interval (seconds)</label>
          <input type="number" id="lora-send-interval" min="60" value="60">
          <button class="btn btn-primary" onclick="setLoRaInterval()">Update LoRa Interval</button>
        </div>
        <div class="input-group">
          <label>Sleep Wake Interval (seconds)</label>
          <input type="number" id="sleep-wake-interval" min="60" value="86400">
          <button class="btn btn-primary" onclick="setSleepInterval()">Update Sleep Interval</button>
        </div>
        <div class="input-group">
          <label>GPS Path Size (5-100 positions)</label>
          <input type="number" id="gps-path-size" min="5" max="100" value="20">
          <button class="btn btn-primary" onclick="setGpsPathSize()">Update GPS Path Size</button>
          <p style="font-size: 12px; color: #666; margin-top: 5px;">
            Number of GPS positions to track and display on map (default: 20)
          </p>
        </div>
      </div>

      <!-- Hardware I2C Pin Configuration -->
      <div class="section">
        <h2>🔌 Hardware I2C Configuration</h2>
        <p style="margin-bottom:15px;font-size:13px;color:#666;">⚠️ Changes apply on next reboot. The I2C bus is shared by OLED, BME280, AXP192 and RTC — incorrect pin values may prevent boot.</p>
        <div class="input-group">
          <label>I2C SDA Pin (shared bus)</label>
          <input type="number" id="hw-sda-pin" min="0" max="39" value="21">
        </div>
        <div class="input-group">
          <label>I2C SCL Pin (shared bus)</label>
          <input type="number" id="hw-scl-pin" min="0" max="39" value="22">
        </div>
        <div class="input-group">
          <label>OLED I2C Address</label>
          <select id="hw-oled-addr" style="width:100%;padding:8px;border-radius:6px;border:1px solid #ddd;">
            <option value="60">0x3C (60) — Standard SSD1306</option>
            <option value="61">0x3D (61) — Alternate SSD1306</option>
          </select>
        </div>
        <div class="input-group">
          <label>BME280 I2C Address</label>
          <select id="hw-bme-addr" style="width:100%;padding:8px;border-radius:6px;border:1px solid #ddd;">
            <option value="118">0x76 (118) — SDO to GND (default)</option>
            <option value="119">0x77 (119) — SDO to VCC</option>
          </select>
        </div>
        <button class="btn btn-primary" onclick="saveHwPins()">💾 Save &amp; Reboot to Apply</button>
        <p style="font-size:12px;color:#888;margin-top:8px;" id="hw-save-msg"></p>
      </div>

      <!-- Downlink Commands Reference -->
      <div class="section">
        <h2>📡 LoRaWAN Downlink Commands</h2>
        <p style="margin-bottom: 15px;">Send these hex commands from your LoRaWAN network server (TTN/ChirpStack) to control the device remotely:</p>
        
        <div style="background: white; padding: 15px; border-radius: 8px; margin-bottom: 10px;">
          <h4 style="margin-top: 0; color: #667eea;">Simple Commands (1 byte):</h4>
          <table style="width: 100%; font-size: 13px;">
            <tr><td style="padding: 5px;"><code>01</code></td><td>WiFi ON</td></tr>
            <tr><td style="padding: 5px;"><code>02</code></td><td>WiFi OFF</td></tr>
            <tr><td style="padding: 5px;"><code>05</code></td><td>Debug ON</td></tr>
            <tr><td style="padding: 5px;"><code>06</code></td><td>Debug OFF</td></tr>
          </table>
        </div>
        
        <div style="background: white; padding: 15px; border-radius: 8px;">
          <h4 style="margin-top: 0; color: #667eea;">Enhanced Commands (0xAA prefix):</h4>
          <table style="width: 100%; font-size: 13px;">
            <tr><td style="padding: 5px; width: 180px;"><code>AA 01 00</code></td><td>WiFi OFF</td></tr>
            <tr><td style="padding: 5px;"><code>AA 01 01</code></td><td>WiFi ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 02 00</code></td><td>GPS OFF (saves ~40mA)</td></tr>
            <tr><td style="padding: 5px;"><code>AA 02 01</code></td><td>GPS ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 06 00</code></td><td>Debug OFF</td></tr>
            <tr><td style="padding: 5px;"><code>AA 06 01</code></td><td>Debug ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 07 00</code></td><td>Display OFF (saves ~15mA)</td></tr>
            <tr><td style="padding: 5px;"><code>AA 07 01</code></td><td>Display ON</td></tr>
            <tr><td style="padding: 5px;"><code>AA 05 00</code></td><td>Sleep Mode OFF</td></tr>
            <tr><td style="padding: 5px;"><code>AA 03 [4 bytes]</code></td><td>Sleep Mode with interval (seconds in hex)</td></tr>
            <tr><td style="padding: 5px;"><code>AA 04 [4 bytes]</code></td><td>LoRa send interval (seconds in hex)</td></tr>
          </table>
          
          <div style="margin-top: 15px; padding: 10px; background: #f8f9fa; border-radius: 5px;">
            <strong>Examples:</strong><br>
            <code style="display: block; margin: 5px 0;">AA 03 00 01 51 80</code> → Sleep 24 hours (86400 sec)<br>
            <code style="display: block; margin: 5px 0;">AA 04 00 00 03 84</code> → Send every 15 min (900 sec)<br>
            <code style="display: block; margin: 5px 0;">AA 02 00</code> → GPS OFF for power saving
          </div>
          
          <p style="margin-top: 10px; font-size: 12px; color: #666;">
            💡 <strong>Tip:</strong> For hex conversion, use: <code>printf '%08X' 86400</code> → <code>00015180</code>
          </p>
        </div>
      </div>
      
      <!-- Refresh Button -->
      <div class="nav-buttons">
        <button class="btn btn-primary" onclick="loadStatus()">🔄 Refresh Status</button>
      </div>
      </div>
    </div>
  </div>
  
  <script>
    function loadStatus() {
      fetch('/api/system-config')
        .then(response => response.json())
        .then(data => {
          // Update status displays
          document.getElementById('wifi-status').innerHTML =
            data.wifiEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('gps-status').innerHTML =
            data.gpsEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('debug-status').innerHTML =
            data.debugEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('serial-status').innerHTML =
            data.serialEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('time-format-status').innerHTML =
            data.time24h ? '<span class="status-badge status-on">24h</span>' : '<span class="status-badge status-off">12h</span>';
          document.getElementById('toggle-24h').checked = data.time24h;
          // Select matching timezone option
          const tzSel = document.getElementById('tz-select');
          for (let i = 0; i < tzSel.options.length; i++) {
            if (tzSel.options[i].value === data.ntpTzOffsetStr) { tzSel.selectedIndex = i; break; }
          }
          document.getElementById('ntp-status-msg').textContent =
            data.ntpSynced ? '✓ NTP synced from internet' :
            (data.wifiEnabled ? '⚠ NTP not yet synced (waiting for WiFi/GPS)' : '⚠ NTP requires WiFi connection');
          document.getElementById('display-status').innerHTML =
            data.displayEnabled ? '<span class="status-badge status-on">ON</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('sleep-status').innerHTML =
            data.sleepEnabled ? '<span class="status-badge status-on">ON (' + data.sleepInterval + 's)</span>' : '<span class="status-badge status-off">OFF</span>';
          document.getElementById('lora-interval').textContent = data.loraInterval + ' seconds';
          
          // Update toggles
          document.getElementById('toggle-wifi').checked = data.wifiEnabled;
          document.getElementById('toggle-gps').checked = data.gpsEnabled;
          document.getElementById('toggle-debug').checked = data.debugEnabled;
          document.getElementById('toggle-serial').checked = data.serialEnabled;
          document.getElementById('toggle-display').checked = data.displayEnabled;
          document.getElementById('toggle-sleep').checked = data.sleepEnabled;
          
          // Update inputs
          document.getElementById('lora-send-interval').value = data.loraInterval;
          document.getElementById('sleep-wake-interval').value = data.sleepInterval;
          document.getElementById('gps-path-size').value = data.gpsPathSize || 20;
        })
        .catch(error => console.error('Error loading status:', error));
    }
    
    function toggleWiFi(enabled) {
      sendCommand('wifi', enabled ? 'on' : 'off');
    }
    
    function toggleGPS(enabled) {
      sendCommand('gps', enabled ? 'on' : 'off');
    }
    
    function toggleDebug(enabled) {
      sendCommand('debug', enabled ? 'on' : 'off');
    }
    
    function toggleSerial(enabled) {
      sendCommand('serial', enabled ? 'on' : 'off');
    }

    function setTimeFormat(is24h) {
      sendCommand('time-format', is24h ? '24h' : '12h');
    }

    function setTimezone(tz) {
      sendCommand('timezone', tz);
    }
    
    function toggleDisplay(enabled) {
      sendCommand('display', enabled ? 'on' : 'off');
    }
    
    function toggleSleep(enabled) {
      sendCommand('sleep', enabled ? 'on' : 'off');
    }
    
    function setLoRaInterval() {
      const interval = document.getElementById('lora-send-interval').value;
      sendCommand('lora-interval', interval);
    }
    
    function setSleepInterval() {
      const interval = document.getElementById('sleep-wake-interval').value;
      sendCommand('sleep-interval', interval);
    }
    
    function setGpsPathSize() {
      const size = document.getElementById('gps-path-size').value;
      if (size < 5 || size > 100) {
        alert('GPS path size must be between 5 and 100');
        return;
      }
      sendCommand('gps-path-size', size);
    }
    
    function sendCommand(type, value) {
      fetch('/api/system-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type: type, value: value })
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          alert('✓ ' + data.message);
          setTimeout(loadStatus, 500);
        } else {
          alert('✗ ' + data.message);
        }
      })
      .catch(error => {
        console.error('Error:', error);
        alert('Error sending command');
      });
    }
    
    function loadHwPins() {
      fetch('/api/hw-config')
        .then(r => r.json())
        .then(data => {
          document.getElementById('hw-sda-pin').value = data.sdaPin;
          document.getElementById('hw-scl-pin').value = data.sclPin;
          document.getElementById('hw-oled-addr').value = data.oledAddr;
          document.getElementById('hw-bme-addr').value = data.bmeAddr;
        })
        .catch(e => console.error('hw-config load error:', e));
    }
    
    function saveHwPins() {
      const sda  = parseInt(document.getElementById('hw-sda-pin').value);
      const scl  = parseInt(document.getElementById('hw-scl-pin').value);
      const oled = parseInt(document.getElementById('hw-oled-addr').value);
      const bme  = parseInt(document.getElementById('hw-bme-addr').value);
      if (isNaN(sda) || isNaN(scl) || sda < 0 || sda > 39 || scl < 0 || scl > 39) {
        alert('Pin numbers must be between 0 and 39.');
        return;
      }
      document.getElementById('hw-save-msg').textContent = 'Saving...';
      fetch('/api/hw-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ sdaPin: sda, sclPin: scl, oledAddr: oled, bmeAddr: bme })
      })
      .then(r => r.json())
      .then(data => {
        document.getElementById('hw-save-msg').textContent = data.message;
        if (data.success) {
          setTimeout(() => fetch('/api/reboot', { method: 'POST' }), 1500);
        }
      })
      .catch(e => { document.getElementById('hw-save-msg').textContent = 'Error: ' + e; });
    }
    
    // Load status on page load
    loadStatus();
    loadHwPins();
    
    // Auto-refresh every 5 seconds
    setInterval(loadStatus, 30000);
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });
  
  // API endpoint for system configuration (GET)
  server.on("/api/system-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"wifiEnabled\":" + String(wifiEnabled ? "true" : "false") + ",";
    json += "\"gpsEnabled\":" + String(gpsEnabled ? "true" : "false") + ",";
    json += "\"debugEnabled\":" + String(debugEnabled ? "true" : "false") + ",";
    json += "\"displayEnabled\":" + String(displayEnabled ? "true" : "false") + ",";
    json += "\"sleepEnabled\":" + String(sleepModeEnabled ? "true" : "false") + ",";
    json += "\"sleepInterval\":" + String(sleepWakeInterval) + ",";
    json += "\"loraInterval\":" + String(loraSendIntervalSec) + ",";
    json += "\"gpsPathSize\":" + String(gpsPathSize) + ",";
    json += "\"serialEnabled\":" + String(serialOutputEnabled ? "true" : "false") + ",";
    json += "\"time24h\":" + String(time24h ? "true" : "false") + ",";
    json += "\"ntpTzOffsetSec\":" + String(ntpTzOffsetSec) + ",\"ntpTzAbbr\":\"" + String(ntpTzAbbr) + "\",";
    json += "\"ntpTzOffsetStr\":\"" + String(ntpTzOffsetSec) + ":" + String(ntpTzAbbr) + "\",";
    json += "\"ntpSynced\":" + String(ntpSynced ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // API endpoint for system configuration (POST)
  server.on("/api/system-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
      }
      
      // Parse JSON manually (simple parsing)
      String type = "";
      String value = "";
      
      int typeStart = body.indexOf("\"type\":\"") + 8;
      int typeEnd = body.indexOf("\"", typeStart);
      if (typeStart > 7 && typeEnd > typeStart) {
        type = body.substring(typeStart, typeEnd);
      }
      
      int valueStart = body.indexOf("\"value\":\"") + 9;
      int valueEnd = body.indexOf("\"", valueStart);
      if (valueStart > 8 && valueEnd > valueStart) {
        value = body.substring(valueStart, valueEnd);
      }
      
      String response = "{\"success\":false,\"message\":\"Unknown command\"}";
      
      bool settingsChanged = false;
      
      if (type == "wifi") {
        if (value == "on") {
          wifiEnabled = true;
          response = "{\"success\":true,\"message\":\"WiFi enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          wifiEnabled = false;
          response = "{\"success\":true,\"message\":\"WiFi disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "gps") {
        if (value == "on") {
          gpsEnabled = true;
          enableGpsPower();
          response = "{\"success\":true,\"message\":\"GPS enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          gpsEnabled = false;
          disableGpsPower();
          response = "{\"success\":true,\"message\":\"GPS disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "debug") {
        if (value == "on") {
          debugEnabled = true;
          response = "{\"success\":true,\"message\":\"Debug mode enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          debugEnabled = false;
          response = "{\"success\":true,\"message\":\"Debug mode disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "display") {
        if (value == "on") {
          enableDisplay();
          response = "{\"success\":true,\"message\":\"Display enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          disableDisplay();
          response = "{\"success\":true,\"message\":\"Display disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "sleep") {
        if (value == "on") {
          sleepModeEnabled = true;
          lastWakeTime = millis();
          response = "{\"success\":true,\"message\":\"Sleep mode enabled\"}";
          settingsChanged = true;
        } else if (value == "off") {
          sleepModeEnabled = false;
          response = "{\"success\":true,\"message\":\"Sleep mode disabled\"}";
          settingsChanged = true;
        }
      } else if (type == "lora-interval") {
        int interval = value.toInt();
        if (interval >= 10) {
          loraSendIntervalSec = interval;
          response = "{\"success\":true,\"message\":\"LoRa interval updated to " + String(interval) + " seconds\"}";
          settingsChanged = true;
        } else {
          response = "{\"success\":false,\"message\":\"Interval must be >= 10 seconds\"}";
        }
      } else if (type == "sleep-interval") {
        int interval = value.toInt();
        if (interval >= 60) {
          sleepWakeInterval = interval;
          response = "{\"success\":true,\"message\":\"Sleep interval updated to " + String(interval) + " seconds\"}";
          settingsChanged = true;
        } else {
          response = "{\"success\":false,\"message\":\"Interval must be >= 60 seconds\"}";
        }
      } else if (type == "gps-path-size") {
        int size = value.toInt();
        if (size >= 5 && size <= GPS_PATH_MAX_SIZE) {
          gpsPathSize = size;
          // Reset path tracking with new size
          gpsPathIndex = 0;
          gpsPathCount = 0;
          response = "{\"success\":true,\"message\":\"GPS path size updated to " + String(size) + " positions\"}";
          settingsChanged = true;
        } else {
          response = "{\"success\":false,\"message\":\"Path size must be between 5 and " + String(GPS_PATH_MAX_SIZE) + "\"}";
        }
      } else if (type == "serial") {
        if (value == "on") {
          serialOutputEnabled = true;
          serialLastActivityAt = millis();
          response = "{\"success\":true,\"message\":\"Serial output enabled\"}";
        } else if (value == "off") {
          serialOutputEnabled = false;
          response = "{\"success\":true,\"message\":\"Serial output disabled\"}";
        }
        // Note: serial on/off is runtime-only — not saved to NVS
      } else if (type == "time-format") {
        time24h = (value == "24h");
        response = "{\"success\":true,\"message\":\"Time format set to " + String(time24h ? "24h" : "12h AM/PM") + "\"}";
        settingsChanged = true;
      } else if (type == "timezone") {
        // value format: "offsetSeconds:ABBR" (e.g. "-18000:EST", "3600:CET", "0:UTC")
        int parsedOffset = 0; char parsedAbbr[8] = "UTC";
        if (value.length() > 0 && sscanf(value.c_str(), "%d:%7s", &parsedOffset, parsedAbbr) >= 1) {
          ntpTzOffsetSec = parsedOffset;
          strlcpy(ntpTzAbbr, parsedAbbr, sizeof(ntpTzAbbr));
          // Re-trigger NTP if WiFi is up
          if (WiFi.status() == WL_CONNECTED) {
            fetchNtpTime();
          }
          response = "{\"success\":true,\"message\":\"Timezone set to " + String(ntpTzAbbr) + " (UTC" + String(ntpTzOffsetSec >= 0 ? "+" : "") + String(ntpTzOffsetSec/3600) + ")\"}";
          settingsChanged = true;
        } else {
          response = "{\"success\":false,\"message\":\"Invalid timezone format\"}";
        }
      }
      
      // Save settings to NVS if any were changed
      if (settingsChanged) {
        saveSystemSettings();
      }
      
      request->send(200, "application/json", response);
    }
  );
  
  // Diagnostics API endpoint
  server.on("/api/diagnostics", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"uplinkCount\":" + String(uplinkCounter) + ",";
    json += "\"downlinkCount\":" + String(downlinkCounter) + ",";
    json += "\"frameCounterUp\":" + String(LMIC.seqnoUp) + ",";
    json += "\"frameCounterDown\":" + String(LMIC.seqnoDn) + ",";
    json += "\"loraJoined\":" + String(loraJoined ? "true" : "false") + ",";
    json += "\"useABP\":" + String(useABP ? "true" : "false") + ",";
    json += "\"sendInterval\":" + String(loraSendIntervalSec) + ",";
    json += "\"lastUplink\":\"" + String(uplinkCounter > 0 ? "Recently" : "Never") + "\",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"wifiEnabled\":" + String(wifiEnabled ? "true" : "false") + ",";
    json += "\"gpsEnabled\":" + String(gpsEnabled ? "true" : "false") + ",";
    
    // WiFi information
    json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    if (WiFi.status() == WL_CONNECTED) {
      json += "\"wifiSSID\":\"" + WiFi.SSID() + "\",";
      json += "\"wifiIP\":\"" + WiFi.localIP().toString() + "\",";
      json += "\"wifiRSSI\":" + String(WiFi.RSSI()) + ",";
    } else {
      json += "\"wifiSSID\":\"Not connected\",";
      json += "\"wifiIP\":\"0.0.0.0\",";
      json += "\"wifiRSSI\":0,";
    }
    
    // LoRa signal information
    json += "\"loraRSSI\":" + String(LMIC.rssi) + ",";
    json += "\"loraSNR\":" + String(LMIC.snr) + ",";
    json += "\"loraFrequency\":" + String(LMIC.freq) + ",";
    
    // Calculate bandwidth and spreading factor from data rate (US915)
    // DR0: SF10/125kHz, DR1: SF9/125kHz, DR2: SF8/125kHz, DR3: SF7/125kHz
    // DR4: SF8/500kHz, DR8-13: RFU
    int bandwidth = 125; // Default 125 kHz
    int spreadingFactor = 10; // Default SF10
    
    switch(LMIC.datarate) {
      case 0: spreadingFactor = 10; bandwidth = 125; break;
      case 1: spreadingFactor = 9; bandwidth = 125; break;
      case 2: spreadingFactor = 8; bandwidth = 125; break;
      case 3: spreadingFactor = 7; bandwidth = 125; break;
      case 4: spreadingFactor = 8; bandwidth = 500; break;
      default: spreadingFactor = 7; bandwidth = 125; break;
    }
    
    json += "\"loraBandwidth\":" + String(bandwidth) + ",";
    json += "\"loraSpreadingFactor\":" + String(spreadingFactor) + ",";
    json += "\"loraDataRate\":" + String(LMIC.datarate) + ",";
    json += "\"loraTxPower\":" + String(LMIC.txpow) + ",";
    
    // Battery voltage
    float batteryVoltage = 0.0;
    if (axpFound) {
      batteryVoltage = readBatteryVoltage();
    }
    json += "\"batteryVoltage\":" + String(batteryVoltage, 2) + ",";
    
    // MQTT status
    json += "\"mqttEnabled\":" + String(mqttEnabled ? "true" : "false") + ",";
    json += "\"mqttConnected\":" + String(mqttConnected ? "true" : "false") + ",";
    json += "\"mqttServer\":\"" + mqttServer + "\",";
    json += "\"mqttPublishCount\":" + String(mqttPublishCounter) + ",";

    // ------------------------------------------------------------
    // Hardware configuration & peripheral status
    // ------------------------------------------------------------
    // I2C bus
    json += "\"i2cSdaPin\":" + String(i2cSdaPin) + ",";
    json += "\"i2cSclPin\":" + String(i2cSclPin) + ",";
    json += "\"i2cClockHz\":" + String(I2C_CLOCK_HZ) + ",";

    // OLED
    json += "\"oledI2cAddr\":" + String(oledI2cAddr) + ",";
    json += "\"oledWidth\":" + String(OLED_WIDTH) + ",";
    json += "\"oledHeight\":" + String(OLED_HEIGHT) + ",";

    // BME280
    json += "\"bme280I2cAddr\":" + String(bme280I2cAddr) + ",";
    json += "\"bmeFound\":" + String(bmeFound ? "true" : "false") + ",";

    // AXP192 power management IC
    json += "\"axpFound\":" + String(axpFound ? "true" : "false") + ",";
    json += "\"axpI2cAddr\":" + String(AXP192_I2C_ADDR) + ",";

    // RTC
    json += "\"rtcFound\":" + String(rtcChipFound ? "true" : "false") + ",";
    json += "\"rtcI2cAddr\":" + String(RTC_I2C_ADDR) + ",";

    // GPS
    {
      int gpsRx = GPS_PROFILES[gpsProfileIndex].rx;
      int gpsTx = GPS_PROFILES[gpsProfileIndex].tx;
      json += "\"gpsRxPin\":" + String(gpsRx) + ",";
      json += "\"gpsTxPin\":" + String(gpsTx) + ",";
      json += "\"gpsBaud\":" + String(GPS_BAUD) + ",";
      json += "\"gpsProfile\":\"" + String(GPS_PROFILES[gpsProfileIndex].name) + "\",";
      json += "\"gpsResetPin\":" + String(GPS_RESET_PIN) + ",";
      json += "\"gpsStreamSeen\":" + String(gpsStreamSeen ? "true" : "false") + ",";
    }

    // LoRa SPI pinout
    json += "\"loraSckPin\":" + String(LORA_SCK_PIN) + ",";
    json += "\"loraMisoPin\":" + String(LORA_MISO_PIN) + ",";
    json += "\"loraMosiPin\":" + String(LORA_MOSI_PIN) + ",";
    json += "\"loraNssPin\":" + String(LORA_NSS_PIN) + ",";
    json += "\"loraRstPin\":" + String(LORA_RST_PIN) + ",";
    json += "\"loraDio0Pin\":" + String(LORA_DIO0_PIN) + ",";
    json += "\"loraDio1Pin\":" + String(LORA_DIO1_PIN) + ",";
    json += "\"loraDio2Pin\":" + String(LORA_DIO2_PIN) + ",";

    // WiFi radio details
    {
      json += "\"wifiMac\":\"" + WiFi.macAddress() + "\",";
      json += "\"wifiTxPower\":" + String(WiFi.getTxPower()) + ",";
      wifi_mode_t wm = WiFi.getMode();
      String modeStr = (wm == WIFI_MODE_STA) ? "STA" :
                       (wm == WIFI_MODE_AP)  ? "AP"  :
                       (wm == WIFI_MODE_APSTA) ? "AP+STA" : "OFF";
      json += "\"wifiMode\":\"" + modeStr + "\",";
      if (wm == WIFI_MODE_AP || wm == WIFI_MODE_APSTA) {
        json += "\"wifiApSsid\":\"" + String(WIFI_AP_NAME) + "\",";
        json += "\"wifiApIp\":\"" + WiFi.softAPIP().toString() + "\",";
      } else {
        json += "\"wifiApSsid\":\"\",";
        json += "\"wifiApIp\":\"\",";
      }
    }

    // WiFi reset / boot button
    json += "\"wifiResetPin\":" + String(WIFI_RESET_BUTTON_PIN);
    json += "}";
    
    request->send(200, "application/json", json);
  });
  
  // Reset frame counter API endpoint
  server.on("/api/reset-frame-counter", HTTP_POST, [](AsyncWebServerRequest *request){
    // Reset LMIC frame counter
    LMIC.seqnoUp = 0;
    
    String json = "{";
    json += "\"success\":true,";
    json += "\"message\":\"Frame counter reset to 0\",";
    json += "\"frameCounterUp\":" + String(LMIC.seqnoUp);
    json += "}";
    
    Serial.println("⚠️ Frame counter reset to 0 via web UI");
    logDebug("⚠️ Frame counter reset to 0 via web UI");
    
    request->send(200, "application/json", json);
  });
  
  // Reboot device API endpoint
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║ 🔄 DEVICE REBOOT REQUESTED VIA WEB UI                     ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    
    String json = "{\"success\":true,\"message\":\"Device rebooting...\"}";
    request->send(200, "application/json", json);
    
    Serial.println("⏳ Rebooting in 2 seconds...");
    delay(2000);
    ESP.restart();
  });
  
  // Factory reset API endpoint
  server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║ ⚠️  FACTORY RESET REQUESTED VIA WEB UI                     ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println("⚠️  Erasing all NVS settings...");
    
    // Erase all NVS namespaces
    preferences.begin("system", false);
    preferences.clear();
    preferences.end();
    Serial.println("  ✓ System settings cleared");
    
    preferences.begin("lorawan", false);
    preferences.clear();
    preferences.end();
    Serial.println("  ✓ LoRaWAN settings cleared");
    
    preferences.begin("mqtt", false);
    preferences.clear();
    preferences.end();
    Serial.println("  ✓ MQTT settings cleared");
    
    // Reset WiFi credentials
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    Serial.println("  ✓ WiFi credentials cleared");
    
    String json = "{\"success\":true,\"message\":\"Factory reset complete. Device will restart in AP mode.\"}";
    request->send(200, "application/json", json);
    
    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║ ✓ FACTORY RESET COMPLETE                                   ║");
    Serial.println("║   Device will restart in AP mode                           ║");
    Serial.println("║   SSID: TTGO-T-Beam-Setup                                  ║");
    Serial.println("║   Password: 12345678                                       ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println("⏳ Rebooting in 3 seconds...");
    
    delay(3000);
    ESP.restart();
  });
  
  // Hardware I2C pin configuration API - GET
  server.on("/api/hw-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"sdaPin\":"  + String(i2cSdaPin)     + ",";
    json += "\"sclPin\":"  + String(i2cSclPin)     + ",";
    json += "\"oledAddr\":" + String(oledI2cAddr)  + ",";
    json += "\"bmeAddr\":"  + String(bme280I2cAddr);
    json += "}";
    request->send(200, "application/json", json);
  });

  // Hardware I2C pin configuration API - POST
  server.on("/api/hw-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      uint8_t newSda  = doc["sdaPin"]   | (int)i2cSdaPin;
      uint8_t newScl  = doc["sclPin"]   | (int)i2cSclPin;
      uint8_t newOled = doc["oledAddr"] | (int)oledI2cAddr;
      uint8_t newBme  = doc["bmeAddr"]  | (int)bme280I2cAddr;
      if (newSda > 39 || newScl > 39) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Pin numbers must be 0-39\"}");
        return;
      }
      if (newOled != 0x3C && newOled != 0x3D) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"OLED address must be 0x3C(60) or 0x3D(61)\"}");
        return;
      }
      if (newBme != 0x76 && newBme != 0x77) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"BME280 address must be 0x76(118) or 0x77(119)\"}");
        return;
      }
      i2cSdaPin     = newSda;
      i2cSclPin     = newScl;
      oledI2cAddr   = newOled;
      bme280I2cAddr = newBme;
      preferences.begin("system", false);
      preferences.putUChar("sdaPin",   i2cSdaPin);
      preferences.putUChar("sclPin",   i2cSclPin);
      preferences.putUChar("oledAddr", oledI2cAddr);
      preferences.putUChar("bmeAddr",  bme280I2cAddr);
      preferences.end();
      Serial.printf("🔌 I2C config updated: SDA=%d SCL=%d OLED=0x%02X BME=0x%02X\n",
                    i2cSdaPin, i2cSclPin, oledI2cAddr, bme280I2cAddr);
      request->send(200, "application/json", "{\"success\":true,\"message\":\"Hardware config saved. Rebooting...\"}");
    }
  );

  // I2C bus scanner API
  server.on("/api/i2c-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    // Known I2C addresses for common T-Beam peripherals
    struct KnownDevice { uint8_t addr; const char* name; };
    static const KnownDevice KNOWN[] = {
      {0x3C, "OLED SSD1306 (default)"},
      {0x3D, "OLED SSD1306 (alt)"},
      {0x34, "AXP192 Power IC"},
      {0x51, "PCF8563 RTC"},
      {0x76, "BME280 (default)"},
      {0x77, "BME280 (alt) / BMP280"},
      {0x68, "MPU-6050 / DS3231 RTC"},
      {0x69, "MPU-6050 (alt)"},
      {0x48, "ADS1115 / TMP75"},
      {0x50, "AT24C EEPROM"},
      {0x60, "MCP4725 DAC"},
    };
    auto lookupName = [&](uint8_t a) -> const char* {
      for (auto& d : KNOWN) if (d.addr == a) return d.name;
      return "Unknown device";
    };

    String json = "{\"devices\":[";
    bool first = true;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      uint8_t err = Wire.endTransmission();
      if (err == 0) {  // ACK received — device present
        if (!first) json += ",";
        first = false;
        char addrHex[7];
        snprintf(addrHex, sizeof(addrHex), "0x%02X", addr);
        json += "{\"addr\":" + String(addr) + ",";
        json += "\"hex\":\"" + String(addrHex) + "\",";
        json += "\"name\":\"" + String(lookupName(addr)) + "\"}";
        Serial.printf("🔍 I2C scan: found 0x%02X (%s)\n", addr, lookupName(addr));
      }
      yield();
    }
    json += "]}";
    request->send(200, "application/json", json);
  });
  server.on("/api/channel-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"channels\":[";
    for (int i = 0; i < 10; i++) {
      if (i > 0) json += ",";
      json += channelEnabled[i] ? "true" : "false";
    }
    json += "]}";
    request->send(200, "application/json", json);
  });
  
  // Channel configuration API - POST
  server.on("/api/channel-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      JsonArray channels = doc["channels"].as<JsonArray>();
      if (channels.size() != 10) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Must provide 10 channel states\"}");
        return;
      }
      
      for (int i = 0; i < 10; i++) {
        channelEnabled[i] = channels[i].as<bool>();
      }
      
      Serial.println("Channel configuration updated via web UI");
      if (debugEnabled) {
        Serial.print("Enabled channels: ");
        for (int i = 0; i < 10; i++) {
          if (channelEnabled[i]) Serial.print(String(i+1) + " ");
        }
        Serial.println();
      }
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"Channel configuration saved\"}");
    }
  );
  
  // PAX counter configuration API - GET
  server.on("/api/pax-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"enabled\":" + String(paxCounterEnabled ? "true" : "false") + ",";
    json += "\"wifiEnabled\":" + String(paxWifiScanEnabled ? "true" : "false") + ",";
    json += "\"bleEnabled\":" + String(paxBleScanEnabled ? "true" : "false") + ",";
    json += "\"interval\":" + String(paxScanInterval / 1000) + ",";
    json += "\"wifiCount\":" + String(wifiDeviceCount) + ",";
    json += "\"bleCount\":" + String(bleDeviceCount) + ",";
    json += "\"totalCount\":" + String(totalPaxCount);
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // PAX counter configuration API - POST
  server.on("/api/pax-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      bool enabled = doc["enabled"].as<bool>();
      bool wifiEnabled = doc["wifiEnabled"] | true; // Default to true if not provided
      bool bleEnabled = doc["bleEnabled"] | true;   // Default to true if not provided
      uint32_t interval = doc["interval"].as<uint32_t>();
      
      if (interval < 30 || interval > 3600) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Interval must be 30-3600 seconds\"}");
        return;
      }
      
      if (!wifiEnabled && !bleEnabled) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"At least one scanning method must be enabled\"}");
        return;
      }
      
      paxCounterEnabled = enabled;
      paxWifiScanEnabled = wifiEnabled;
      paxBleScanEnabled = bleEnabled;
      paxScanInterval = interval * 1000; // Convert to milliseconds
      channelEnabled[9] = enabled; // Enable/disable channel 10 based on PAX counter state
      
      // Initialize or deinitialize BLE scanner based on enabled state
      if (enabled && bleEnabled && pBLEScan == nullptr) {
        Serial.println("Initializing BLE scanner for PAX counter...");
        NimBLEDevice::init("");
        pBLEScan = NimBLEDevice::getScan();
        pBLEScan->setActiveScan(false); // Passive scan
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        Serial.println("BLE scanner initialized");
      }
      
      // Save settings to NVS
      saveSystemSettings();
      
      Serial.print("PAX counter ");
      Serial.print(enabled ? "enabled" : "disabled");
      Serial.print(" (WiFi: ");
      Serial.print(wifiEnabled ? "ON" : "OFF");
      Serial.print(", BLE: ");
      Serial.print(bleEnabled ? "ON" : "OFF");
      Serial.print(") with interval ");
      Serial.print(interval);
      Serial.println(" seconds");
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"PAX counter settings saved\"}");
    }
  );
  
  // Debug console page
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Debug Console - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Courier New', monospace;
      background: #1e1e1e;
      color: #d4d4d4;
      padding: 0;
      margin: 0;
      height: 100vh;
      display: flex;
      flex-direction: column;
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
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 15px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 2px 10px rgba(0,0,0,0.3);
    }
    .header h1 {
      font-size: 1.2em;
      font-family: Arial, sans-serif;
    }
    .header-controls {
      display: flex;
      gap: 10px;
      align-items: center;
    }
    .btn {
      padding: 8px 15px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 0.9em;
      font-family: Arial, sans-serif;
      transition: all 0.2s;
    }
    .btn-primary {
      background: white;
      color: #667eea;
      font-weight: bold;
    }
    .btn-primary:hover {
      background: #f0f0f0;
    }
    .btn-secondary {
      background: rgba(255,255,255,0.2);
      color: white;
      border: 1px solid rgba(255,255,255,0.3);
    }
    .btn-secondary:hover {
      background: rgba(255,255,255,0.3);
    }
    .console-container {
      flex: 1;
      display: flex;
      flex-direction: column;
      overflow: hidden;
      background: #1e1e1e;
    }
    .console-header {
      padding: 10px 20px;
      background: #2d2d2d;
      border-bottom: 1px solid #3e3e3e;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .console-info {
      font-size: 0.85em;
      color: #858585;
    }
    .status-indicator {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      margin-right: 5px;
      animation: pulse 2s infinite;
    }
    .status-indicator.active {
      background: #4caf50;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
    #console {
      flex: 1;
      overflow-y: auto;
      padding: 15px 20px;
      font-size: 13px;
      line-height: 1.5;
      white-space: pre-wrap;
      word-wrap: break-word;
    }
    #console::-webkit-scrollbar {
      width: 10px;
    }
    #console::-webkit-scrollbar-track {
      background: #2d2d2d;
    }
    #console::-webkit-scrollbar-thumb {
      background: #555;
      border-radius: 5px;
    }
    #console::-webkit-scrollbar-thumb:hover {
      background: #777;
    }
    .log-line {
      margin-bottom: 2px;
    }
    .log-error {
      color: #f48771;
    }
    .log-warning {
      color: #dcdcaa;
    }
    .log-success {
      color: #4ec9b0;
    }
    .log-info {
      color: #9cdcfe;
    }
    .back-link {
      color: white;
      text-decoration: none;
      font-family: Arial, sans-serif;
    }
    .back-link:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link active">🖥️ Debug</a>
      <a href="/ota" class="nav-link">🔄 OTA</a>
      <a href="/mqtt" class="nav-link">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link">📡 WiFi</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>
  
  <div class="header">
    <h1>🖥️ Debug Console</h1>
    <div class="header-controls">
      <button class="btn btn-secondary" id="debugToggleBtn" onclick="toggleDebugMode()">
        <span id="debugStatus">🐛 Debug: Loading...</span>
      </button>
      <button class="btn btn-secondary" onclick="clearConsole()">🗑️ Clear</button>
      <button class="btn btn-secondary" onclick="toggleAutoScroll()">
        <span id="scrollBtn">📌 Auto-scroll: ON</span>
      </button>
    </div>
  </div>
  
  <div class="console-container">
    <div class="console-header">
      <div class="console-info">
        <span class="status-indicator active"></span>
        Live Serial Monitor | Messages: <span id="msgCount">0</span> |
        Last Update: <span id="lastUpdate">--</span>
      </div>
      <div class="console-info">
        Update Rate: 1s | Buffer: 100 messages
      </div>
    </div>
    <div id="console">Connecting to device...</div>
  </div>

  <!-- Command Input Bar -->
  <div class="command-bar">
    <div class="quick-cmds">
      <span style="color:#858585;font-size:0.8em;align-self:center;">Quick:</span>
      <button class="quick-btn" onclick="sendWebCmd('status')">status</button>
      <button class="quick-btn" onclick="sendWebCmd('menu')">menu</button>
      <button class="quick-btn" onclick="sendWebCmd('help')">help</button>
      <button class="quick-btn" onclick="sendWebCmd('debug-on')">debug-on</button>
      <button class="quick-btn" onclick="sendWebCmd('debug-off')">debug-off</button>
      <button class="quick-btn" onclick="sendWebCmd('serial-on')">serial-on</button>
      <button class="quick-btn" onclick="sendWebCmd('wifi-status')">wifi-status</button>
      <button class="quick-btn" onclick="sendWebCmd('gps-status')">gps-status</button>
    </div>
    <div class="cmd-input-row">
      <span class="cmd-prompt">&gt;&gt;</span>
      <input type="text" id="cmdInput" placeholder="Type command and press Enter (e.g. status, wifi-on, lora-keys)" autocomplete="off" autocorrect="off" spellcheck="false">
      <button class="btn btn-primary" onclick="submitCmd()">Send &#9654;</button>
    </div>
  </div>

  <script>
    let autoScroll = true;
    let messageCount = 0;
    let lastMessageCount = 0;
    
    function toggleAutoScroll() {
      autoScroll = !autoScroll;
      document.getElementById('scrollBtn').textContent =
        autoScroll ? '📌 Auto-scroll: ON' : '📌 Auto-scroll: OFF';
    }
    
    function clearConsole() {
      document.getElementById('console').textContent = 'Console cleared. Waiting for new messages...';
      messageCount = 0;
      document.getElementById('msgCount').textContent = '0';
    }
    
    function toggleDebugMode() {
      // Get current debug status first
      fetch('/api/system-config')
        .then(response => response.json())
        .then(data => {
          const newState = !data.debugEnabled;
          
          // Send toggle command
          fetch('/api/system-config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ type: 'debug', value: newState ? 'on' : 'off' })
          })
          .then(response => response.json())
          .then(result => {
            if (result.success) {
              updateDebugStatus();
            }
          })
          .catch(err => console.error('Error toggling debug:', err));
        });
    }
    
    function updateDebugStatus() {
      fetch('/api/system-config')
        .then(response => response.json())
        .then(data => {
          const debugBtn = document.getElementById('debugToggleBtn');
          const debugStatus = document.getElementById('debugStatus');
          if (data.debugEnabled) {
            debugStatus.textContent = '🐛 Debug: ON';
            debugBtn.style.background = 'rgba(76, 175, 80, 0.3)';
          } else {
            debugStatus.textContent = '🐛 Debug: OFF';
            debugBtn.style.background = 'rgba(255, 255, 255, 0.1)';
          }
        })
        .catch(err => console.error('Error fetching debug status:', err));
    }
    
    function colorizeLog(line) {
      // Simple colorization based on keywords
      if (line.includes('ERROR') || line.includes('FAIL') || line.includes('✗')) {
        return '<span class="log-error">' + line + '</span>';
      } else if (line.includes('WARN') || line.includes('⚠')) {
        return '<span class="log-warning">' + line + '</span>';
      } else if (line.includes('SUCCESS') || line.includes('✓') || line.includes('LOCKED')) {
        return '<span class="log-success">' + line + '</span>';
      } else if (line.includes('INFO') || line.includes('ℹ') || line.includes('GPS') || line.includes('LoRa')) {
        return '<span class="log-info">' + line + '</span>';
      }
      return line;
    }
    
    function updateConsole() {
      fetch('/api/debug-log')
        .then(response => response.json())
        .then(data => {
          const consoleDiv = document.getElementById('console');
          const messages = data.messages || [];
          
          if (messages.length > 0) {
            // Build HTML with colorization
            let html = '';
            messages.forEach(msg => {
              html += '<div class="log-line">' + colorizeLog(msg) + '</div>';
            });
            
            // Save scroll position before replacing innerHTML (replacing resets scrollTop to 0)
            const savedScroll = autoScroll ? null : consoleDiv.scrollTop;
            consoleDiv.innerHTML = html;
            messageCount = data.count || messages.length;
            
            // Auto-scroll to bottom if enabled, otherwise restore saved position
            if (autoScroll) {
              consoleDiv.scrollTop = consoleDiv.scrollHeight;
            } else if (savedScroll !== null) {
              consoleDiv.scrollTop = savedScroll;
            }
          }
          
          document.getElementById('msgCount').textContent = messageCount;
          document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
        })
        .catch(err => {
          console.error('Error fetching debug log:', err);
          document.getElementById('console').textContent =
            'Error connecting to device. Retrying...\n' + err;
        });
    }
    
    function sendWebCmd(cmdText) {
      cmdText = cmdText.trim();
      if (!cmdText) return;
      // Echo to console immediately
      const consoleDiv = document.getElementById('console');
      consoleDiv.innerHTML += '<div class="log-line" style="color:#4ec9b0;font-weight:bold;">&gt;&gt; ' + cmdText + '</div>';
      if (autoScroll) consoleDiv.scrollTop = consoleDiv.scrollHeight;
      fetch('/api/serial-command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cmd: cmdText })
      })
      .then(r => r.json())
      .then(d => { if (!d.success) {
        consoleDiv.innerHTML += '<div class="log-line log-error">✗ ' + (d.error || 'unknown error') + '</div>';
        if (autoScroll) consoleDiv.scrollTop = consoleDiv.scrollHeight;
      }})
      .catch(err => {
        consoleDiv.innerHTML += '<div class="log-line log-error">✗ Network error: ' + err + '</div>';
        if (autoScroll) consoleDiv.scrollTop = consoleDiv.scrollHeight;
      });
    }

    function submitCmd() {
      const input = document.getElementById('cmdInput');
      const val = input.value.trim();
      if (val) { sendWebCmd(val); input.value = ''; }
    }

    document.addEventListener('DOMContentLoaded', function() {
      document.getElementById('cmdInput').addEventListener('keydown', function(e) {
        if (e.key === 'Enter') submitCmd();
      });
    });

    // Initial updates
    updateConsole();
    updateDebugStatus();
    
    // Update every second
    setInterval(updateConsole, 1000);
    setInterval(updateDebugStatus, 5000); // Update debug status every 5 seconds
    
    // Pause auto-scroll when user scrolls up
    document.getElementById('console').addEventListener('scroll', function() {
      const elem = this;
      const isAtBottom = elem.scrollHeight - elem.scrollTop <= elem.clientHeight + 50;
      if (!isAtBottom && autoScroll) {
        autoScroll = false;
        document.getElementById('scrollBtn').textContent = '📌 Auto-scroll: OFF';
      }
    });
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });
  
  // Debug log API endpoint
  server.on("/api/debug-log", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"messages\":";
    json += getDebugBufferJson();
    json += ",\"count\":" + String(debugMessageCounter);
    json += ",\"bufferSize\":" + String(DEBUG_BUFFER_SIZE);
    json += "}";
    request->send(200, "application/json", json);
  });

  // API: send a serial command from the web debug console
  server.on("/api/serial-command", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) body += (char)data[i];
      int cmdStart = body.indexOf("\"cmd\":\"") + 7;
      int cmdEnd   = body.indexOf("\"", cmdStart);
      if (cmdStart > 7 && cmdEnd > cmdStart) {
        String cmd = body.substring(cmdStart, cmdEnd);
        // Remove any control chars (newlines, nulls) before queuing
        cmd.replace("\n", ""); cmd.replace("\r", ""); cmd.replace("\0", "");
        if (cmd.length() > 0 && cmd.length() <= 128) {
          pendingWebCommand = cmd;
          request->send(200, "application/json", "{\"success\":true,\"cmd\":\"" + cmd + "\"}");
        } else {
          request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid command length\"}");
        }
      } else {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing cmd field\"}");
      }
    }
  );

  // Diagnostics page
  server.on("/diagnostics", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /diagnostics page");
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Diagnostics - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
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
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    h2 {
      color: #667eea;
      margin-bottom: 15px;
      font-size: 20px;
      border-bottom: 2px solid #667eea;
      padding-bottom: 10px;
    }
    .stats-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }
    .stat-box {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 20px;
      border-radius: 10px;
      text-align: center;
    }
    .stat-label {
      font-size: 14px;
      opacity: 0.9;
      margin-bottom: 5px;
    }
    .stat-value {
      font-size: 32px;
      font-weight: bold;
    }
    .stat-unit {
      font-size: 14px;
      opacity: 0.8;
    }
    .info-row {
      display: flex;
      justify-content: space-between;
      padding: 12px;
      border-bottom: 1px solid #eee;
    }
    .info-row:last-child {
      border-bottom: none;
    }
    .info-label {
      font-weight: 600;
      color: #555;
    }
    .info-value {
      color: #333;
      font-family: 'Courier New', monospace;
    }
    .btn {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      font-size: 16px;
      font-weight: 600;
      transition: all 0.3s;
      display: inline-flex;
      align-items: center;
      gap: 8px;
    }
    .btn-danger {
      background: #f44336;
      color: white;
    }
    .btn-danger:hover {
      background: #d32f2f;
      transform: translateY(-2px);
      box-shadow: 0 4px 12px rgba(244, 67, 54, 0.4);
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
    }
    .btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .alert {
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .alert-success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .alert-error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .button-group {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-top: 15px;
    }
    @media (max-width: 768px) {
      .stats-grid {
        grid-template-columns: 1fr;
      }
      .info-row {
        flex-direction: column;
        gap: 5px;
      }
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link active">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/ota" class="nav-link">🔄 OTA</a>
      <a href="/mqtt" class="nav-link">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link">📡 WiFi</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <h1>🔧 System Diagnostics</h1>
      <p style="color: #666; margin-bottom: 20px;">LoRaWAN statistics and system health monitoring</p>
      
      <div id="alertBox" class="alert"></div>
      
      <!-- LoRa Statistics -->
      <h2>📡 LoRaWAN Statistics</h2>
      <div class="stats-grid">
        <div class="stat-box">
          <div class="stat-label">Uplink Messages</div>
          <div class="stat-value" id="uplinkCount">-</div>
          <div class="stat-unit">sent</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Downlink Messages</div>
          <div class="stat-value" id="downlinkCount">-</div>
          <div class="stat-unit">received</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Frame Counter (Up)</div>
          <div class="stat-value" id="frameCounterUp">-</div>
          <div class="stat-unit">LMIC.seqnoUp</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Frame Counter (Down)</div>
          <div class="stat-value" id="frameCounterDown">-</div>
          <div class="stat-unit">LMIC.seqnoDn</div>
        </div>
      </div>
      
      <!-- LoRa Details -->
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px; margin-bottom: 20px;">
        <div class="info-row">
          <span class="info-label">Network Status:</span>
          <span class="info-value" id="loraStatus">-</span>
        </div>
        <div class="info-row">
          <span class="info-label">Activation Mode:</span>
          <span class="info-value" id="activationMode">-</span>
        </div>
        <div class="info-row">
          <span class="info-label">Send Interval:</span>
          <span class="info-value" id="sendInterval">-</span>
        </div>
        <div class="info-row">
          <span class="info-label">Last Uplink:</span>
          <span class="info-value" id="lastUplink">-</span>
        </div>
      </div>
      
      <!-- LoRa Signal Information -->
      <h2>📡 LoRa Signal Information</h2>
      <p style="color: #666; margin-bottom: 15px;">Radio signal quality and transmission parameters</p>
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px; margin-bottom: 20px;">
        <div class="info-row">
          <span class="info-label">RSSI (Signal Strength):</span>
          <span class="info-value" id="loraRSSI">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Received Signal Strength Indicator in dBm. Closer to 0 = stronger signal. Typical range: -120 to -30 dBm
        </p>
        
        <div class="info-row">
          <span class="info-label">SNR (Signal-to-Noise Ratio):</span>
          <span class="info-value" id="loraSNR">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Signal quality in dB. Positive values indicate good signal above noise floor. Negative values can still work with LoRa
        </p>
        
        <div class="info-row">
          <span class="info-label">Frequency:</span>
          <span class="info-value" id="loraFrequency">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Radio frequency in MHz. US915 band uses 902-928 MHz
        </p>
        
        <div class="info-row">
          <span class="info-label">Bandwidth:</span>
          <span class="info-value" id="loraBandwidth">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          Channel bandwidth in kHz. Wider bandwidth = faster data rate but shorter range
        </p>
        
        <div class="info-row">
          <span class="info-label">Spreading Factor:</span>
          <span class="info-value" id="loraSpreadingFactor">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          SF7-SF12. Higher SF = longer range but slower data rate and more airtime
        </p>
        
        <div class="info-row">
          <span class="info-label">Data Rate:</span>
          <span class="info-value" id="loraDataRate">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          DR0-DR15. Combination of spreading factor and bandwidth. Higher DR = faster transmission
        </p>
        
        <div class="info-row">
          <span class="info-label">TX Power:</span>
          <span class="info-value" id="loraTxPower">-</span>
        </div>
        <p style="color: #666; font-size: 13px; margin-left: 20px;">
          Transmission power in dBm. Higher power = longer range but more battery consumption
        </p>
      </div>
      
      <!-- Actions -->
      <h2>⚙️ Maintenance Actions</h2>
      
      <!-- Frame Counter Reset -->
      <div style="margin-bottom: 30px;">
        <h3 style="color: #333; margin-bottom: 10px;">🔄 Reset Frame Counter</h3>
        <p style="color: #666; font-size: 14px; margin-bottom: 15px;">
          Resets the LoRaWAN uplink frame counter (LMIC.seqnoUp) to 0. This is useful when:
        </p>
        <ul style="color: #666; font-size: 14px; margin-left: 20px; margin-bottom: 15px;">
          <li>Re-joining the network after a long period</li>
          <li>Switching between OTAA and ABP modes</li>
          <li>Network server has lost sync with device counter</li>
          <li>Instructed by your network administrator</li>
        </ul>
        <p style="color: #ff6b6b; font-size: 13px; margin-bottom: 15px;">
          ⚠️ <strong>Warning:</strong> Only reset if you know what you're doing. Incorrect use may cause uplink failures.
        </p>
        <button class="btn btn-danger" onclick="resetFrameCounter()">
          🔄 Reset Frame Counter
        </button>
      </div>
      
      <!-- Device Reboot -->
      <div style="margin-bottom: 30px;">
        <h3 style="color: #333; margin-bottom: 10px;">🔄 Reboot Device</h3>
        <p style="color: #666; font-size: 14px; margin-bottom: 15px;">
          Performs a soft restart of the device. This will:
        </p>
        <ul style="color: #666; font-size: 14px; margin-left: 20px; margin-bottom: 15px;">
          <li>Restart the ESP32 microcontroller</li>
          <li>Reload all settings from NVS (Non-Volatile Storage)</li>
          <li>Re-initialize all sensors and peripherals</li>
          <li>Reconnect to WiFi and LoRaWAN network</li>
          <li><strong>Preserve all saved settings</strong> (WiFi, LoRa keys, MQTT config, etc.)</li>
        </ul>
        <p style="color: #4CAF50; font-size: 13px; margin-bottom: 15px;">
          ✓ <strong>Safe:</strong> All your configurations will be preserved. The device will be back online in ~30 seconds.
        </p>
        <button class="btn btn-warning" onclick="rebootDevice()">
          🔄 Reboot Device
        </button>
      </div>
      
      <!-- Factory Reset -->
      <div style="margin-bottom: 30px;">
        <h3 style="color: #333; margin-bottom: 10px;">⚠️ Factory Reset</h3>
        <p style="color: #666; font-size: 14px; margin-bottom: 15px;">
          Erases ALL saved settings and restarts the device to factory defaults. This will delete:
        </p>
        <ul style="color: #666; font-size: 14px; margin-left: 20px; margin-bottom: 15px;">
          <li><strong>WiFi credentials</strong> - Device will create AP mode for reconfiguration</li>
          <li><strong>LoRaWAN keys</strong> - Will revert to default keys from code</li>
          <li><strong>MQTT configuration</strong> - All broker settings will be cleared</li>
          <li><strong>System settings</strong> - GPS, display, sleep mode, PAX counter settings</li>
          <li><strong>Payload configuration</strong> - Channel enable/disable states</li>
        </ul>
        <p style="color: #ff6b6b; font-size: 13px; margin-bottom: 15px;">
          ⚠️ <strong>DANGER:</strong> This action cannot be undone! You will need to reconfigure everything from scratch.
        </p>
        <p style="color: #666; font-size: 13px; margin-bottom: 15px;">
          <strong>After factory reset:</strong>
        </p>
        <ol style="color: #666; font-size: 13px; margin-left: 20px; margin-bottom: 15px;">
          <li>Device will restart and create WiFi AP: <code>TTGO-T-Beam-Setup</code></li>
          <li>Connect to the AP (password: <code>12345678</code>)</li>
          <li>Configure your WiFi network</li>
          <li>Access the web dashboard to reconfigure LoRa keys and other settings</li>
        </ol>
        <button class="btn btn-danger" onclick="factoryReset()" style="background: #d32f2f;">
          ⚠️ Factory Reset (Erase All Settings)
        </button>
      </div>
      
      <!-- Refresh Stats -->
      <div style="margin-bottom: 20px;">
        <h3 style="color: #333; margin-bottom: 10px;">♻️ Refresh Statistics</h3>
        <p style="color: #666; font-size: 14px; margin-bottom: 15px;">
          Reloads all diagnostic information from the device without making any changes.
        </p>
        <button class="btn btn-primary" onclick="refreshStats()">
          ♻️ Refresh Statistics
        </button>
      </div>
    </div>
    
    <!-- Hardware Configuration -->
    <div class="card">
      <h2>🔌 Hardware Configuration</h2>

      <!-- I2C Bus -->
      <h3 style="color:#333;margin:15px 0 8px;">I²C Bus</h3>
      <div style="background:#f8f9fa;padding:15px;border-radius:10px;margin-bottom:15px;">
        <div class="info-row"><span class="info-label">SDA Pin</span><span class="info-value" id="hw-sdaPin">-</span></div>
        <div class="info-row"><span class="info-label">SCL Pin</span><span class="info-value" id="hw-sclPin">-</span></div>
        <div class="info-row"><span class="info-label">Clock Speed</span><span class="info-value" id="hw-i2cClock">-</span></div>
      </div>

      <!-- OLED Display -->
      <h3 style="color:#333;margin:15px 0 8px;">📺 OLED Display</h3>
      <div style="background:#f8f9fa;padding:15px;border-radius:10px;margin-bottom:15px;">
        <div class="info-row">
          <span class="info-label">I²C Address</span>
          <span class="info-value">
            <select id="hw-oled-sel" style="font-family:monospace;padding:4px 8px;border-radius:4px;border:1px solid #ccc;">
              <option value="60">0x3C (60) — default</option>
              <option value="61">0x3D (61) — alt</option>
            </select>
          </span>
        </div>
        <div class="info-row"><span class="info-label">Resolution</span><span class="info-value" id="hw-oledRes">-</span></div>
      </div>

      <!-- BME280 Sensor -->
      <h3 style="color:#333;margin:15px 0 8px;">🌡️ BME280 Sensor</h3>
      <div style="background:#f8f9fa;padding:15px;border-radius:10px;margin-bottom:15px;">
        <div class="info-row"><span class="info-label">Detected</span><span class="info-value" id="hw-bmeFound">-</span></div>
        <div class="info-row">
          <span class="info-label">Configured I²C Address</span>
          <span class="info-value">
            <select id="hw-bme-sel" style="font-family:monospace;padding:4px 8px;border-radius:4px;border:1px solid #ccc;">
              <option value="118">0x76 (118) — default</option>
              <option value="119">0x77 (119) — alt</option>
            </select>
          </span>
        </div>
        <div class="info-row"><span class="info-label">Bus (shared)</span><span class="info-value" id="hw-bmeBus">-</span></div>
      </div>
      <div style="margin-bottom:20px;">
        <button onclick="saveI2cAddrs()" style="background:#667eea;color:white;border:none;padding:10px 22px;border-radius:8px;cursor:pointer;font-size:14px;font-weight:600;">💾 Save Addresses &amp; Reboot</button>
        <span id="i2c-save-msg" style="margin-left:12px;font-size:13px;color:#28a745;"></span>
      </div>

      <!-- AXP192 / RTC -->
      <h3 style="color:#333;margin:15px 0 8px;">⚡ Power IC &amp; RTC</h3>
      <div style="background:#f8f9fa;padding:15px;border-radius:10px;margin-bottom:15px;">
        <div class="info-row"><span class="info-label">AXP192 Detected</span><span class="info-value" id="hw-axpFound">-</span></div>
        <div class="info-row"><span class="info-label">AXP192 I²C Address</span><span class="info-value" id="hw-axpAddr">-</span></div>
        <div class="info-row"><span class="info-label">RTC Detected</span><span class="info-value" id="hw-rtcFound">-</span></div>
        <div class="info-row"><span class="info-label">RTC I²C Address</span><span class="info-value" id="hw-rtcAddr">-</span></div>
      </div>

      <!-- GPS UART -->
      <h3 style="color:#333;margin:15px 0 8px;">🛰️ GPS UART</h3>
      <div style="background:#f8f9fa;padding:15px;border-radius:10px;margin-bottom:15px;">
        <div class="info-row"><span class="info-label">Profile</span><span class="info-value" id="hw-gpsProfile">-</span></div>
        <div class="info-row"><span class="info-label">ESP RX Pin (from GPS TX)</span><span class="info-value" id="hw-gpsRx">-</span></div>
        <div class="info-row"><span class="info-label">ESP TX Pin (to GPS RX)</span><span class="info-value" id="hw-gpsTx">-</span></div>
        <div class="info-row"><span class="info-label">Baud Rate</span><span class="info-value" id="hw-gpsBaud">-</span></div>
        <div class="info-row"><span class="info-label">Reset Pin</span><span class="info-value" id="hw-gpsRst">-</span></div>
        <div class="info-row"><span class="info-label">NMEA Stream Seen</span><span class="info-value" id="hw-gpsStream">-</span></div>
      </div>

      <!-- LoRa SPI -->
      <h3 style="color:#333;margin:15px 0 8px;">📡 LoRa SX127x SPI Pinout</h3>
      <div style="background:#f8f9fa;padding:15px;border-radius:10px;margin-bottom:15px;">
        <div class="info-row"><span class="info-label">SCK</span><span class="info-value" id="hw-loraSck">-</span></div>
        <div class="info-row"><span class="info-label">MISO</span><span class="info-value" id="hw-loraMiso">-</span></div>
        <div class="info-row"><span class="info-label">MOSI</span><span class="info-value" id="hw-loraMosi">-</span></div>
        <div class="info-row"><span class="info-label">NSS (CS)</span><span class="info-value" id="hw-loraNss">-</span></div>
        <div class="info-row"><span class="info-label">RST</span><span class="info-value" id="hw-loraRst">-</span></div>
        <div class="info-row"><span class="info-label">DIO0</span><span class="info-value" id="hw-loraDio0">-</span></div>
        <div class="info-row"><span class="info-label">DIO1</span><span class="info-value" id="hw-loraDio1">-</span></div>
        <div class="info-row"><span class="info-label">DIO2</span><span class="info-value" id="hw-loraDio2">-</span></div>
      </div>

      <!-- WiFi Radio -->
      <h3 style="color:#333;margin:15px 0 8px;">📶 WiFi Radio</h3>
      <div style="background:#f8f9fa;padding:15px;border-radius:10px;margin-bottom:15px;">
        <div class="info-row"><span class="info-label">MAC Address</span><span class="info-value" id="hw-wifiMac">-</span></div>
        <div class="info-row"><span class="info-label">Mode</span><span class="info-value" id="hw-wifiMode">-</span></div>
        <div class="info-row"><span class="info-label">TX Power</span><span class="info-value" id="hw-wifiTx">-</span></div>
        <div class="info-row"><span class="info-label">AP SSID</span><span class="info-value" id="hw-apSsid">-</span></div>
        <div class="info-row"><span class="info-label">AP IP</span><span class="info-value" id="hw-apIp">-</span></div>
        <div class="info-row"><span class="info-label">WiFi Reset/Boot Pin</span><span class="info-value" id="hw-wifiRst">-</span></div>
      </div>

      <!-- I2C Scanner -->
      <h3 style="color:#333;margin:15px 0 8px;">🔍 I²C Bus Scanner</h3>
      <p style="color:#666;font-size:13px;margin-bottom:10px;">Scans all 127 I²C addresses and reports which devices respond. Takes ~1 second.</p>
      <button id="scan-btn" onclick="scanI2C()" style="background:#28a745;color:white;border:none;padding:10px 22px;border-radius:8px;cursor:pointer;font-size:14px;font-weight:600;margin-bottom:14px;">🔍 Scan I²C Bus</button>
      <div id="i2c-scan-result" style="display:none;background:#f8f9fa;border-radius:10px;overflow:hidden;">
        <table style="width:100%;border-collapse:collapse;font-size:14px;">
          <thead>
            <tr style="background:#667eea;color:white;">
              <th style="padding:10px 14px;text-align:left;">Address (hex)</th>
              <th style="padding:10px 14px;text-align:left;">Address (dec)</th>
              <th style="padding:10px 14px;text-align:left;">Identified Device</th>
            </tr>
          </thead>
          <tbody id="i2c-scan-tbody"></tbody>
        </table>
        <p id="i2c-none-msg" style="display:none;padding:14px;color:#666;text-align:center;">No I²C devices found.</p>
      </div>
    </div>

    <!-- System Health -->
    <div class="card">
      <h2>💚 System Health</h2>
      <div class="info-row">
        <span class="info-label">Uptime:</span>
        <span class="info-value" id="uptime">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">Free Heap:</span>
        <span class="info-value" id="freeHeap">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi Status:</span>
        <span class="info-value" id="wifiStatus">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi SSID:</span>
        <span class="info-value" id="wifiSSID">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi IP Address:</span>
        <span class="info-value" id="wifiIP">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">WiFi Signal (RSSI):</span>
        <span class="info-value" id="wifiRSSI">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">GPS Status:</span>
        <span class="info-value" id="gpsStatus">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">Battery Voltage:</span>
        <span class="info-value" id="batteryVoltage">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">MQTT Status:</span>
        <span class="info-value" id="mqttStatus">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">MQTT Server:</span>
        <span class="info-value" id="mqttServer">-</span>
      </div>
      <div class="info-row">
        <span class="info-label">MQTT Publishes:</span>
        <span class="info-value" id="mqttPublishCount">-</span>
      </div>
    </div>
  </div>

  <script>
    function showAlert(message, isError = false) {
      const alertBox = document.getElementById('alertBox');
      alertBox.textContent = message;
      alertBox.className = 'alert ' + (isError ? 'alert-error' : 'alert-success');
      alertBox.style.display = 'block';
      setTimeout(() => {
        alertBox.style.display = 'none';
      }, 5000);
    }
    
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const mins = Math.floor((seconds % 3600) / 60);
      const secs = seconds % 60;
      
      if (days > 0) return `${days}d ${hours}h ${mins}m`;
      if (hours > 0) return `${hours}h ${mins}m ${secs}s`;
      if (mins > 0) return `${mins}m ${secs}s`;
      return `${secs}s`;
    }
    
    function refreshStats() {
      // Fetch diagnostics data
      fetch('/api/diagnostics')
        .then(response => response.json())
        .then(data => {
          // LoRa stats
          document.getElementById('uplinkCount').textContent = data.uplinkCount || 0;
          document.getElementById('downlinkCount').textContent = data.downlinkCount || 0;
          document.getElementById('frameCounterUp').textContent = data.frameCounterUp || 0;
          document.getElementById('frameCounterDown').textContent = data.frameCounterDown || 0;
          
          // LoRa details
          document.getElementById('loraStatus').textContent = data.loraJoined ? '✅ Joined' : '❌ Not Joined';
          document.getElementById('activationMode').textContent = data.useABP ? 'ABP' : 'OTAA';
          document.getElementById('sendInterval').textContent = data.sendInterval + ' seconds';
          document.getElementById('lastUplink').textContent = data.lastUplink || 'Never';
          
          // LoRa signal information
          document.getElementById('loraRSSI').textContent = (data.loraRSSI || 0) + ' dBm';
          document.getElementById('loraSNR').textContent = (data.loraSNR || 0) + ' dB';
          document.getElementById('loraFrequency').textContent = ((data.loraFrequency || 0) / 1000000).toFixed(2) + ' MHz';
          document.getElementById('loraBandwidth').textContent = (data.loraBandwidth || 0) + ' kHz';
          document.getElementById('loraSpreadingFactor').textContent = 'SF' + (data.loraSpreadingFactor || 7);
          document.getElementById('loraDataRate').textContent = 'DR' + (data.loraDataRate || 0);
          document.getElementById('loraTxPower').textContent = (data.loraTxPower || 0) + ' dBm';
          
          // System health
          document.getElementById('uptime').textContent = formatUptime(data.uptime || 0);
          document.getElementById('freeHeap').textContent = (data.freeHeap || 0).toLocaleString() + ' bytes';
          
          // WiFi information
          const wifiConnected = data.wifiConnected || false;
          document.getElementById('wifiStatus').textContent = wifiConnected ? '✅ Connected' : (data.wifiEnabled ? '⚠️ Enabled (Not Connected)' : '❌ Disabled');
          document.getElementById('wifiSSID').textContent = data.wifiSSID || 'N/A';
          document.getElementById('wifiIP').textContent = data.wifiIP || '0.0.0.0';
          document.getElementById('wifiRSSI').textContent = wifiConnected ? (data.wifiRSSI || 0) + ' dBm' : 'N/A';
          
          document.getElementById('gpsStatus').textContent = data.gpsEnabled ? '✅ Enabled' : '❌ Disabled';
          document.getElementById('batteryVoltage').textContent = (data.batteryVoltage || 0).toFixed(2) + ' V';
          
          // MQTT status
          let mqttStatusText = '❌ Disabled';
          if (data.mqttEnabled) {
            mqttStatusText = data.mqttConnected ? '✅ Connected' : '⚠️ Enabled (Not Connected)';
          }
          document.getElementById('mqttStatus').textContent = mqttStatusText;
          document.getElementById('mqttServer').textContent = data.mqttServer || 'Not configured';
          document.getElementById('mqttPublishCount').textContent = data.mqttPublishCount || 0;

          // Hardware configuration
          function toHex(v) { return '0x' + parseInt(v).toString(16).toUpperCase().padStart(2,'0'); }
          function gpio(v) { return 'GPIO ' + v; }

          // I2C
          document.getElementById('hw-sdaPin').textContent   = gpio(data.i2cSdaPin);
          document.getElementById('hw-sclPin').textContent   = gpio(data.i2cSclPin);
          document.getElementById('hw-i2cClock').textContent = (data.i2cClockHz / 1000) + ' kHz';

          // OLED — value shown via dropdown
          document.getElementById('hw-oledRes').textContent  = data.oledWidth + ' × ' + data.oledHeight + ' px';

          // BME280
          document.getElementById('hw-bmeFound').textContent = data.bmeFound ? '✅ Yes' : '❌ Not detected';
          document.getElementById('hw-bmeBus').textContent   = 'SDA=' + gpio(data.i2cSdaPin) + '  SCL=' + gpio(data.i2cSclPin);

          // AXP192 / RTC
          document.getElementById('hw-axpFound').textContent = data.axpFound ? '✅ Yes' : '❌ Not detected';
          document.getElementById('hw-axpAddr').textContent  = toHex(data.axpI2cAddr);
          document.getElementById('hw-rtcFound').textContent = data.rtcFound ? '✅ Yes' : '❌ Not detected';
          document.getElementById('hw-rtcAddr').textContent  = toHex(data.rtcI2cAddr);

          // GPS
          document.getElementById('hw-gpsProfile').textContent = data.gpsProfile || '-';
          document.getElementById('hw-gpsRx').textContent      = gpio(data.gpsRxPin);
          document.getElementById('hw-gpsTx').textContent      = gpio(data.gpsTxPin);
          document.getElementById('hw-gpsBaud').textContent    = (data.gpsBaud || 9600).toLocaleString() + ' baud';
          document.getElementById('hw-gpsRst').textContent     = gpio(data.gpsResetPin);
          document.getElementById('hw-gpsStream').textContent  = data.gpsStreamSeen ? '✅ Receiving NMEA' : '⚠️ No stream yet';

          // LoRa SPI
          document.getElementById('hw-loraSck').textContent  = gpio(data.loraSckPin);
          document.getElementById('hw-loraMiso').textContent = gpio(data.loraMisoPin);
          document.getElementById('hw-loraMosi').textContent = gpio(data.loraMosiPin);
          document.getElementById('hw-loraNss').textContent  = gpio(data.loraNssPin);
          document.getElementById('hw-loraRst').textContent  = gpio(data.loraRstPin);
          document.getElementById('hw-loraDio0').textContent = gpio(data.loraDio0Pin);
          document.getElementById('hw-loraDio1').textContent = gpio(data.loraDio1Pin);
          document.getElementById('hw-loraDio2').textContent = gpio(data.loraDio2Pin);

          // WiFi radio
          document.getElementById('hw-wifiMac').textContent  = data.wifiMac  || '-';
          document.getElementById('hw-wifiMode').textContent = data.wifiMode || '-';
          document.getElementById('hw-wifiTx').textContent   = (data.wifiTxPower * 0.25).toFixed(1) + ' dBm (raw ' + data.wifiTxPower + ')';
          document.getElementById('hw-apSsid').textContent   = data.wifiApSsid || '(not active)';
          document.getElementById('hw-apIp').textContent     = data.wifiApIp   || '(not active)';
          document.getElementById('hw-wifiRst').textContent  = gpio(data.wifiResetPin);

          // Populate OLED / BME dropdowns with current values
          const oledSel = document.getElementById('hw-oled-sel');
          const bmeSel  = document.getElementById('hw-bme-sel');
          if (oledSel) oledSel.value = String(data.oledI2cAddr);
          if (bmeSel)  bmeSel.value  = String(data.bme280I2cAddr);
        })
        .catch(err => {
          console.error('Error fetching diagnostics:', err);
          showAlert('Failed to fetch diagnostics data', true);
        });
    }
    
    function resetFrameCounter() {
      if (!confirm('Are you sure you want to reset the frame counter? This should only be done after re-joining the network or if instructed by your network administrator.')) {
        return;
      }
      
      fetch('/api/reset-frame-counter', {
        method: 'POST'
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ Frame counter reset successfully');
            refreshStats();
          } else {
            showAlert('❌ Failed to reset frame counter: ' + data.message, true);
          }
        })
        .catch(err => {
          console.error('Error resetting frame counter:', err);
          showAlert('❌ Network error while resetting frame counter', true);
        });
    }
    
    function rebootDevice() {
      if (!confirm('Reboot the device now?\\n\\nThe device will restart and be back online in ~30 seconds.\\n\\nAll settings will be preserved.')) {
        return;
      }
      
      showAlert('🔄 Rebooting device... Please wait 30 seconds then refresh the page.');
      
      fetch('/api/reboot', {
        method: 'POST'
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ Reboot command sent. Device is restarting...');
            setTimeout(() => {
              showAlert('⏳ Device is restarting... Refresh the page in a few seconds.');
            }, 2000);
          } else {
            showAlert('❌ Failed to reboot: ' + data.message, true);
          }
        })
        .catch(err => {
          // This is expected as device will disconnect
          console.log('Device disconnected (expected during reboot)');
          showAlert('✅ Reboot initiated. Wait 30 seconds then refresh the page.');
        });
    }
    
    function factoryReset() {
      const confirmation1 = confirm('⚠️ FACTORY RESET WARNING ⚠️\\n\\nThis will ERASE ALL SETTINGS including:\\n\\n• WiFi credentials\\n• LoRaWAN keys\\n• MQTT configuration\\n• All system settings\\n\\nAre you ABSOLUTELY SURE?');
      
      if (!confirmation1) {
        return;
      }
      
      const confirmation2 = confirm('⚠️ FINAL WARNING ⚠️\\n\\nThis action CANNOT be undone!\\n\\nYou will need to reconfigure everything from scratch.\\n\\nType YES in the next prompt to proceed.');
      
      if (!confirmation2) {
        return;
      }
      
      const userInput = prompt('Type "RESET" (all caps) to confirm factory reset:');
      
      if (userInput !== 'RESET') {
        showAlert('❌ Factory reset cancelled. Input did not match.', true);
        return;
      }
      
      showAlert('⚠️ Performing factory reset... Device will restart in AP mode.');
      
      fetch('/api/factory-reset', {
        method: 'POST'
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ Factory reset complete. Device is restarting in AP mode...\\n\\nConnect to WiFi AP: TTGO-T-Beam-Setup (password: 12345678)');
          } else {
            showAlert('❌ Failed to factory reset: ' + data.message, true);
          }
        })
        .catch(err => {
          // This is expected as device will disconnect
          console.log('Device disconnected (expected during factory reset)');
          showAlert('✅ Factory reset initiated. Connect to AP: TTGO-T-Beam-Setup (password: 12345678)');
        });
    }
    
    function scanI2C() {
      const btn = document.getElementById('scan-btn');
      const tbody = document.getElementById('i2c-scan-tbody');
      const result = document.getElementById('i2c-scan-result');
      const noneMsg = document.getElementById('i2c-none-msg');
      btn.disabled = true;
      btn.textContent = '🔄 Scanning…';
      fetch('/api/i2c-scan')
        .then(r => r.json())
        .then(data => {
          tbody.innerHTML = '';
          result.style.display = 'block';
          if (!data.devices || data.devices.length === 0) {
            noneMsg.style.display = 'block';
          } else {
            noneMsg.style.display = 'none';
            data.devices.forEach((d, i) => {
              const tr = document.createElement('tr');
              tr.style.background = i % 2 === 0 ? '#fff' : '#f8f9fa';
              tr.innerHTML = '<td style="padding:9px 14px;font-family:monospace;font-weight:bold;">' + d.hex + '</td>' +
                             '<td style="padding:9px 14px;font-family:monospace;">' + d.addr + '</td>' +
                             '<td style="padding:9px 14px;">' + d.name + '</td>';
              tbody.appendChild(tr);
            });
          }
        })
        .catch(() => { showAlert('I²C scan failed', true); })
        .finally(() => { btn.disabled = false; btn.textContent = '🔍 Scan I²C Bus'; });
    }

    function saveI2cAddrs() {
      const oledSel = document.getElementById('hw-oled-sel');
      const bmeSel  = document.getElementById('hw-bme-sel');
      const msg     = document.getElementById('i2c-save-msg');
      const oled = parseInt(oledSel.value);
      const bme  = parseInt(bmeSel.value);
      msg.textContent = 'Saving…';
      // Read current SDA/SCL from last refreshStats result
      const sdaText = document.getElementById('hw-sdaPin').textContent;
      const sclText = document.getElementById('hw-sclPin').textContent;
      const sda = parseInt(sdaText.replace('GPIO ', '')) || 21;
      const scl = parseInt(sclText.replace('GPIO ', '')) || 22;
      fetch('/api/hw-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({sdaPin: sda, sclPin: scl, oledAddr: oled, bmeAddr: bme})
      })
      .then(r => r.json())
      .then(d => {
        if (d.success) {
          msg.textContent = '✅ Saved — rebooting…';
          setTimeout(() => { fetch('/api/reboot', {method:'POST'}); }, 800);
        } else {
          msg.style.color = '#dc3545';
          msg.textContent = '❌ ' + (d.message || 'Save failed');
        }
      })
      .catch(() => { msg.style.color = '#dc3545'; msg.textContent = '❌ Request failed'; });
    }

    // Initial load and auto-refresh every 5 seconds
    refreshStats();
    setInterval(refreshStats, 10000);
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });
  
  // Payload Info page
  server.on("/payload-info", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /payload-info page");
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Payload Info - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
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
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    h2 {
      color: #667eea;
      margin-bottom: 15px;
      font-size: 20px;
      border-bottom: 2px solid #667eea;
      padding-bottom: 10px;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-bottom: 20px;
    }
    th, td {
      padding: 12px;
      text-align: left;
      border-bottom: 1px solid #eee;
    }
    th {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      font-weight: 600;
    }
    tr:hover {
      background: #f8f9fa;
    }
    .code-block {
      background: #2d2d2d;
      color: #d4d4d4;
      padding: 15px;
      border-radius: 8px;
      font-family: 'Courier New', monospace;
      font-size: 14px;
      overflow-x: auto;
      margin-bottom: 15px;
    }
    .info-box {
      background: #e3f2fd;
      border-left: 4px solid #2196f3;
      padding: 15px;
      margin-bottom: 20px;
      border-radius: 4px;
    }
    .warning-box {
      background: #fff3e0;
      border-left: 4px solid #ff9800;
      padding: 15px;
      margin-bottom: 20px;
      border-radius: 4px;
    }
    .channel-badge {
      display: inline-block;
      padding: 4px 8px;
      border-radius: 4px;
      font-size: 12px;
      font-weight: bold;
      color: white;
    }
    .ch-temp { background: #f44336; }
    .ch-humid { background: #2196f3; }
    .ch-press { background: #4caf50; }
    .ch-alt { background: #ff9800; }
    .ch-gps { background: #9c27b0; }
    .ch-analog { background: #607d8b; }
    .ch-battery { background: #ffc107; color: #333; }
    .alert {
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .alert-success {
      background: #d4edda;
      border: 1px solid #c3e6cb;
      color: #155724;
    }
    .alert-error {
      background: #f8d7da;
      border: 1px solid #f5c6cb;
      color: #721c24;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link active">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/ota" class="nav-link">🔄 OTA</a>
      <a href="/mqtt" class="nav-link">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link">📡 WiFi</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <h1>📦 CayenneLPP Payload Documentation</h1>
      <p style="color: #666; margin-bottom: 20px;">Complete channel mapping and payload structure for LoRaWAN uplinks</p>
      
      <div class="info-box">
        <strong>ℹ️ Uplink Configuration</strong><br>
        Port: <code>1</code> | Format: <code>CayenneLPP</code> | Max Size: <code>51 bytes</code> (US915 DR0)
      </div>
      
      <!-- Combined Channel Map & Configuration -->
      <h2>📋 Channel Map & Configuration</h2>
      <p style="color: #666; margin-bottom: 15px;">Enable or disable individual channels to optimize payload size</p>
      
      <div id="alertBox" class="alert" style="display: none;"></div>
      
      <table>
        <thead>
          <tr>
            <th>Channel</th>
            <th>Type</th>
            <th>Data</th>
            <th>Unit</th>
            <th>Size</th>
            <th>Source</th>
            <th style="text-align: center;">Enabled</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><span class="channel-badge ch-temp">CH 1</span></td>
            <td>Temperature</td>
            <td>Ambient temperature</td>
            <td>°C</td>
            <td>4 bytes</td>
            <td>BME280</td>
            <td style="text-align: center;"><input type="checkbox" id="ch1" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-humid">CH 2</span></td>
            <td>Relative Humidity</td>
            <td>Relative humidity</td>
            <td>%</td>
            <td>4 bytes</td>
            <td>BME280</td>
            <td style="text-align: center;"><input type="checkbox" id="ch2" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-press">CH 3</span></td>
            <td>Barometric Pressure</td>
            <td>Atmospheric pressure</td>
            <td>hPa</td>
            <td>4 bytes</td>
            <td>BME280</td>
            <td style="text-align: center;"><input type="checkbox" id="ch3" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-alt">CH 4</span></td>
            <td>Analog Input</td>
            <td>Calculated altitude</td>
            <td>meters</td>
            <td>4 bytes</td>
            <td>BME280 (calculated)</td>
            <td style="text-align: center;"><input type="checkbox" id="ch4" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-gps">CH 5</span></td>
            <td>GPS</td>
            <td>Latitude, Longitude, Altitude</td>
            <td>degrees, meters</td>
            <td>11 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch5" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 6</span></td>
            <td>Analog Input</td>
            <td>HDOP (GPS accuracy)</td>
            <td>unitless</td>
            <td>4 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch6" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 7</span></td>
            <td>Analog Input</td>
            <td>Satellite count</td>
            <td>count</td>
            <td>4 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch7" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 8</span></td>
            <td>Analog Input</td>
            <td>GPS time (seconds of day)</td>
            <td>seconds</td>
            <td>4 bytes</td>
            <td>GPS Module</td>
            <td style="text-align: center;"><input type="checkbox" id="ch8" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-battery">CH 9</span></td>
            <td>Analog Input</td>
            <td>Battery voltage</td>
            <td>volts</td>
            <td>4 bytes</td>
            <td>AXP192 PMU</td>
            <td style="text-align: center;"><input type="checkbox" id="ch9" checked onchange="updateChannelConfig()"></td>
          </tr>
          <tr>
            <td><span class="channel-badge ch-analog">CH 10</span></td>
            <td>Analog Input</td>
            <td>PAX counter (WiFi + BLE devices)</td>
            <td>count</td>
            <td>4 bytes</td>
            <td>WiFi/BLE Scan</td>
            <td style="text-align: center;"><input type="checkbox" id="ch10" onchange="updateChannelConfig()"></td>
          </tr>
        </tbody>
      </table>
      
      <div style="margin-top: 20px;">
        <button onclick="saveChannelConfig()" style="padding: 10px 20px; background: #667eea; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px;">
          💾 Save Channel Configuration
        </button>
        <button onclick="loadChannelConfig()" style="padding: 10px 20px; background: #6c757d; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; margin-left: 10px;">
          🔄 Reload Configuration
        </button>
      </div>
      
      <p style="color: #999; font-size: 14px; margin-top: 15px;">
        💡 Tip: Disable unused channels to reduce payload size and save airtime. Changes take effect on the next uplink.
      </p>
      
      <div class="warning-box">
        <strong>⚠️ Important Limitations</strong><br>
        CayenneLPP Analog Input uses 2-byte signed integer with 0.01 resolution (range: -327.68 to 327.67).<br>
        • Channel 4 (altitude) may exceed range at high elevations<br>
        • Channel 8 (seconds-of-day: 0-86400) always exceeds range<br>
        Use alternative decoding or scaled values if needed.
      </div>
      
      <!-- Payload Example -->
      <h2>📄 Example Payload</h2>
      <div class="code-block">
01 67 00 E1    // CH1: Temperature = 22.5°C
02 68 50       // CH2: Humidity = 80%
03 73 03 F1    // CH3: Pressure = 1009 hPa
04 02 01 2C    // CH4: Altitude = 300m
05 88 06 76 5E FF F2 9E 00 00 64    // CH5: GPS (lat, lon, alt)
06 02 00 0F    // CH6: HDOP = 1.5
07 02 00 08    // CH7: Satellites = 8
08 02 4E 20    // CH8: Time = 20000s (overflow)
09 02 01 04    // CH9: Battery = 2.60V
      </div>
      
      <!-- Decoder -->
      <h2>🔧 JavaScript Decoder (TTN/ChirpStack)</h2>
      <div class="code-block">
function decodeUplink(input) {
  var data = {};
  var bytes = input.bytes;
  var i = 0;
  
  while (i < bytes.length) {
    var channel = bytes[i++];
    var type = bytes[i++];
    
    switch (type) {
      case 0x67: // Temperature
        data['temperature_' + channel] = 
          ((bytes[i] << 8) | bytes[i+1]) / 10.0;
        i += 2;
        break;
      case 0x68: // Humidity
        data['humidity_' + channel] = bytes[i] / 2.0;
        i += 1;
        break;
      case 0x73: // Barometric Pressure
        data['pressure_' + channel] = 
          ((bytes[i] << 8) | bytes[i+1]) / 10.0;
        i += 2;
        break;
      case 0x02: // Analog Input
        data['analog_' + channel] = 
          ((bytes[i] << 8) | bytes[i+1]) / 100.0;
        i += 2;
        break;
      case 0x88: // GPS
        data['latitude_' + channel] = 
          ((bytes[i] << 16) | (bytes[i+1] << 8) | bytes[i+2]) / 10000.0;
        data['longitude_' + channel] = 
          ((bytes[i+3] << 16) | (bytes[i+4] << 8) | bytes[i+5]) / 10000.0;
        data['altitude_' + channel] = 
          ((bytes[i+6] << 16) | (bytes[i+7] << 8) | bytes[i+8]) / 100.0;
        i += 9;
        break;
      default:
        i = bytes.length; // Unknown type, stop parsing
    }
  }
  
  return {
    data: data,
    warnings: [],
    errors: []
  };
}
      </div>
      
      <!-- Integration Guide -->
      <h2>🔗 Network Server Integration</h2>
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px;">
        <h3 style="margin-bottom: 10px;">The Things Network (TTN)</h3>
        <ol style="margin-left: 20px; line-height: 1.8;">
          <li>Go to your application in TTN Console</li>
          <li>Navigate to <strong>Payload Formatters</strong></li>
          <li>Select <strong>Custom JavaScript formatter</strong></li>
          <li>Paste the decoder code above</li>
          <li>Test with example payload</li>
        </ol>
        
        <h3 style="margin: 20px 0 10px 0;">ChirpStack</h3>
        <ol style="margin-left: 20px; line-height: 1.8;">
          <li>Go to <strong>Device Profile</strong></li>
          <li>Select <strong>Codec</strong> tab</li>
          <li>Choose <strong>Custom JavaScript codec functions</strong></li>
          <li>Paste decoder in <strong>Decode function</strong></li>
          <li>Save and test</li>
        </ol>
      </div>
    </div>
    
    <!-- PAX Counter Settings -->
    <div class="card">
      <h2>👥 PAX Counter Settings</h2>
      <p style="color: #666; margin-bottom: 15px;">Configure crowd counting via WiFi and BLE device scanning</p>
      
      <div style="background: #f8f9fa; padding: 20px; border-radius: 10px;">
        <div style="margin-bottom: 20px;">
          <label style="display: flex; align-items: center; gap: 10px; cursor: pointer;">
            <input type="checkbox" id="paxEnabled" onchange="updatePaxSettings()" style="width: 20px; height: 20px;">
            <span style="font-weight: 600;">Enable PAX Counter</span>
          </label>
          <p style="color: #666; font-size: 14px; margin-top: 5px; margin-left: 30px;">
            Scan for nearby WiFi access points and BLE devices to estimate crowd size
          </p>
        </div>
        
        <div style="margin-bottom: 20px;">
          <label style="font-weight: 600; display: block; margin-bottom: 10px;">Scanning Methods:</label>
          <div style="margin-left: 20px;">
            <label style="display: flex; align-items: center; gap: 10px; cursor: pointer; margin-bottom: 10px;">
              <input type="checkbox" id="paxWifiEnabled" checked style="width: 18px; height: 18px;">
              <span>📶 WiFi Scanning</span>
            </label>
            <p style="color: #666; font-size: 13px; margin-left: 28px; margin-bottom: 15px;">
              Detect WiFi-enabled devices (smartphones, tablets, laptops)
            </p>
            
            <label style="display: flex; align-items: center; gap: 10px; cursor: pointer; margin-bottom: 10px;">
              <input type="checkbox" id="paxBleEnabled" checked style="width: 18px; height: 18px;">
              <span>📡 BLE Scanning</span>
            </label>
            <p style="color: #666; font-size: 13px; margin-left: 28px;">
              Detect Bluetooth Low Energy devices (wearables, beacons, smartphones)
            </p>
          </div>
        </div>
        
        <div style="margin-bottom: 20px;">
          <label style="font-weight: 600; display: block; margin-bottom: 5px;">Scan Interval (seconds):</label>
          <input type="number" id="paxInterval" value="60" min="30" max="3600" style="padding: 8px; border: 1px solid #ddd; border-radius: 5px; width: 150px;">
          <p style="color: #666; font-size: 14px; margin-top: 5px;">
            How often to perform WiFi/BLE scans (30-3600 seconds)
          </p>
        </div>
        
        <div style="background: #e3f2fd; border-left: 4px solid #2196f3; padding: 15px; margin-bottom: 20px; border-radius: 4px;">
          <strong>ℹ️ Current PAX Count</strong><br>
          <div style="margin-top: 10px; font-size: 18px;">
            WiFi Devices: <span id="wifiCount" style="font-weight: bold;">-</span><br>
            BLE Devices: <span id="bleCount" style="font-weight: bold;">-</span><br>
            Total: <span id="totalPax" style="font-weight: bold; color: #667eea;">-</span>
          </div>
        </div>
        
        <button onclick="savePaxSettings()" style="padding: 10px 20px; background: #667eea; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px;">
          💾 Save PAX Settings
        </button>
        <button onclick="loadPaxSettings()" style="padding: 10px 20px; background: #6c757d; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; margin-left: 10px;">
          🔄 Reload Settings
        </button>
      </div>
      
      <div style="background: #fff3e0; border-left: 4px solid #ff9800; padding: 15px; margin-top: 20px; border-radius: 4px;">
        <strong>⚠️ Privacy Note</strong><br>
        PAX counter only counts the number of devices, not their identities. No personal data is collected or transmitted.
      </div>
    </div>
  </div>
  
  <script>
    function showAlert(message, isError = false) {
      const alertBox = document.getElementById('alertBox');
      alertBox.textContent = message;
      alertBox.className = 'alert ' + (isError ? 'alert-error' : 'alert-success');
      alertBox.style.display = 'block';
      setTimeout(() => {
        alertBox.style.display = 'none';
      }, 5000);
    }
    
    function loadChannelConfig() {
      fetch('/api/channel-config')
        .then(response => response.json())
        .then(data => {
          for (let i = 0; i < 10; i++) {
            const checkbox = document.getElementById('ch' + (i + 1));
            if (checkbox && data.channels && data.channels[i] !== undefined) {
              checkbox.checked = data.channels[i];
            }
          }
        })
        .catch(err => {
          console.error('Error loading channel config:', err);
          showAlert('Failed to load channel configuration', true);
        });
    }
    
    function updateChannelConfig() {
      // Just visual feedback, actual save happens when user clicks Save button
    }
    
    function saveChannelConfig() {
      const channels = [];
      for (let i = 0; i < 10; i++) {
        const checkbox = document.getElementById('ch' + (i + 1));
        channels.push(checkbox ? checkbox.checked : true);
      }
      
      fetch('/api/channel-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({channels: channels})
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ Channel configuration saved successfully');
          } else {
            showAlert('❌ Failed to save: ' + data.message, true);
          }
        })
        .catch(err => {
          console.error('Error saving channel config:', err);
          showAlert('❌ Network error while saving', true);
        });
    }
    
    function loadPaxSettings() {
      fetch('/api/pax-config')
        .then(response => response.json())
        .then(data => {
          document.getElementById('paxEnabled').checked = data.enabled || false;
          document.getElementById('paxWifiEnabled').checked = data.wifiEnabled !== undefined ? data.wifiEnabled : true;
          document.getElementById('paxBleEnabled').checked = data.bleEnabled !== undefined ? data.bleEnabled : true;
          document.getElementById('paxInterval').value = data.interval || 60;
          document.getElementById('wifiCount').textContent = data.wifiCount || 0;
          document.getElementById('bleCount').textContent = data.bleCount || 0;
          document.getElementById('totalPax').textContent = data.totalCount || 0;
        })
        .catch(err => {
          console.error('Error loading PAX config:', err);
          showAlert('Failed to load PAX configuration', true);
        });
    }
    
    function updatePaxSettings() {
      // Just visual feedback
    }
    
    function savePaxSettings() {
      const enabled = document.getElementById('paxEnabled').checked;
      const wifiEnabled = document.getElementById('paxWifiEnabled').checked;
      const bleEnabled = document.getElementById('paxBleEnabled').checked;
      const interval = parseInt(document.getElementById('paxInterval').value);
      
      if (interval < 30 || interval > 3600) {
        showAlert('❌ Scan interval must be between 30 and 3600 seconds', true);
        return;
      }
      
      if (!wifiEnabled && !bleEnabled) {
        showAlert('❌ At least one scanning method (WiFi or BLE) must be enabled', true);
        return;
      }
      
      fetch('/api/pax-config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          enabled: enabled,
          wifiEnabled: wifiEnabled,
          bleEnabled: bleEnabled,
          interval: interval
        })
      })
        .then(response => response.json())
        .then(data => {
          if (data.success) {
            showAlert('✅ PAX counter settings saved successfully');
            loadPaxSettings(); // Reload to show current counts
          } else {
            showAlert('❌ Failed to save: ' + data.message, true);
          }
        })
        .catch(err => {
          console.error('Error saving PAX config:', err);
          showAlert('❌ Network error while saving', true);
        });
    }
    
    // Load configurations on page load
    loadChannelConfig();
    loadPaxSettings();
    
    // Auto-refresh PAX counts every 10 seconds
    setInterval(loadPaxSettings, 10000);
  </script>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });
  
  // About page - Developer information
  server.on("/about", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /about page");
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>About - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
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
    .container {
      max-width: 900px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 20px;
      font-size: 32px;
    }
    h2 {
      color: #667eea;
      margin: 25px 0 15px 0;
      font-size: 22px;
    }
    p {
      color: #555;
      line-height: 1.8;
      margin-bottom: 15px;
    }
    .profile {
      display: flex;
      align-items: center;
      gap: 20px;
      margin-bottom: 30px;
      padding: 20px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      border-radius: 10px;
      color: white;
    }
    .profile-icon {
      font-size: 60px;
    }
    .profile-info h2 {
      color: white;
      margin: 0 0 10px 0;
    }
    .profile-info p {
      color: rgba(255,255,255,0.9);
      margin: 0;
    }
    .links {
      display: flex;
      gap: 15px;
      flex-wrap: wrap;
      margin-top: 20px;
    }
    .link-btn {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 10px 20px;
      background: #667eea;
      color: white;
      text-decoration: none;
      border-radius: 8px;
      transition: all 0.3s;
      font-weight: 600;
    }
    .link-btn:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
    }
    .project-list {
      list-style: none;
      padding: 0;
    }
    .project-list li {
      padding: 15px;
      margin-bottom: 10px;
      background: #f8f9fa;
      border-left: 4px solid #667eea;
      border-radius: 5px;
    }
    .project-list li strong {
      color: #667eea;
    }
    .motto {
      font-style: italic;
      color: #764ba2;
      font-size: 18px;
      text-align: center;
      padding: 20px;
      background: #f8f9fa;
      border-radius: 10px;
      margin: 20px 0;
    }
  </style>
</head>
<body>
  <!-- Navigation Bar -->
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/ota" class="nav-link">🔄 OTA</a>
      <a href="/mqtt" class="nav-link">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link">📡 WiFi</a>
      <a href="/about" class="nav-link active">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <div class="profile">
        <div class="profile-icon">👨‍💻</div>
        <div class="profile-info">
          <h2>Markus van Kempen</h2>
          <p>Developer & Open Source Contributor</p>
          <p>📍 Toronto, Canada</p>
        </div>
      </div>
      
      <div class="motto">
        "No bug too small, no syntax too weird."
      </div>
      
      <h2>About the Developer</h2>
      <p>
        I have a keen interest in debating and predicting The Next Big Thing in technology.
        Based in Toronto, I build open-source developer tools that bridge enterprise platforms
        like IBM Watsonx Orchestrate and Maximo with modern AI-assisted development workflows.
      </p>
      
      <p>
        These tools are part of an open-source effort to make enterprise API development faster,
        more accessible, and AI-ready. Whether you use VS Code, Cursor, Claude, or ChatGPT,
        there's a tool here for you.
      </p>
      
      <p>
        All projects are released under the <strong>Apache 2.0 license</strong>. Contributions,
        issues, and feedback are welcome.
      </p>
      
      <h2>Key Projects</h2>
      <ul class="project-list">
        <li><strong>TTGO T-Beam Sensor Node</strong> - IoT sensor platform with LoRaWAN, GPS, and web dashboard</li>
        <li><strong>Enterprise API Tools</strong> - Developer tools for IBM Watsonx and Maximo integration</li>
        <li><strong>AI-Assisted Development</strong> - Tools for modern AI-powered coding workflows</li>
      </ul>
      
      <h2>Connect</h2>
      <div class="links">
        <a href="https://markusvankempen.github.io/" class="link-btn" target="_blank">
          🌐 Website
        </a>
        <a href="https://github.com/markusvankempen" class="link-btn" target="_blank">
          💻 GitHub
        </a>
        <a href="https://github.com/markusvankempen" class="link-btn" target="_blank">
          📦 Main Project Repo
        </a>
        <a href="https://markusvankempen.github.io/" class="link-btn" target="_blank">
          📝 Blog
        </a>
      </div>
      
      <h2>About This Project</h2>
      <p>
        This TTGO T-Beam firmware is a comprehensive IoT sensor node featuring:
      </p>
      <ul style="margin-left: 20px; color: #555; line-height: 1.8;">
        <li>LoRaWAN connectivity (OTAA & ABP)</li>
        <li>GPS tracking with real-time mapping</li>
        <li>BME280 environmental sensors</li>
        <li>WiFi web dashboard with full configuration</li>
        <li>PAX counter for crowd monitoring</li>
        <li>Configurable payload channels</li>
        <li>Power management and sleep modes</li>
        <li>Comprehensive diagnostics and debugging</li>
      </ul>
      
      <p style="margin-top: 20px;">
        <strong>License:</strong> Apache License 2.0<br>
        <strong>Version:</strong> )rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(<br>
        <strong>Build:</strong> )rawliteral" + String(BUILD_TIMESTAMP) + R"rawliteral(<br>
        <strong>Organization:</strong> Research | Floor 7½ 🏢🤏
      </p>
      
      <div style="margin-top: 30px; padding-top: 20px; border-top: 2px solid #e0e0e0; text-align: center;">
        <p style="color: #667eea; font-weight: 600; margin-bottom: 10px;">📧 Contact</p>
        <p style="margin-bottom: 5px;">
          <a href="mailto:markus.van.kempen@gmail.com" style="color: #667eea; text-decoration: none;">
            ✉️ markus.van.kempen@gmail.com
          </a>
        </p>
        <p>
          <a href="https://markusvankempen.github.io/" target="_blank" style="color: #667eea; text-decoration: none;">
            🌐 https://markusvankempen.github.io/
          </a>
        </p>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });
  // OTA Update page - Firmware update via URL or file upload
  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /ota page");
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OTA Update - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
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
    .container {
      max-width: 900px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 32px;
    }
    h2 {
      color: #667eea;
      margin: 25px 0 15px 0;
      font-size: 22px;
    }
    p {
      color: #555;
      line-height: 1.8;
      margin-bottom: 15px;
    }
    .warning {
      background: #fff3cd;
      border-left: 4px solid #ffc107;
      padding: 15px;
      margin: 20px 0;
      border-radius: 5px;
    }
    .warning strong {
      color: #856404;
    }
    .input-group {
      margin-bottom: 20px;
    }
    .input-group label {
      display: block;
      font-weight: bold;
      margin-bottom: 8px;
      color: #333;
    }
    .input-group input[type="text"],
    .input-group input[type="file"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
    }
    .input-group input:focus {
      outline: none;
      border-color: #667eea;
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
      transition: all 0.3s;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-danger {
      background: #dc3545;
      color: white;
    }
    .btn-danger:hover {
      background: #c82333;
    }
    .btn:disabled {
      background: #ccc;
      cursor: not-allowed;
    }
    .progress-container {
      display: none;
      margin-top: 20px;
    }
    .progress-bar {
      width: 100%;
      height: 30px;
      background: #f0f0f0;
      border-radius: 15px;
      overflow: hidden;
      position: relative;
    }
    .progress-fill {
      height: 100%;
      background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
      width: 0%;
      transition: width 0.3s;
      display: flex;
      align-items: center;
      justify-content: center;
      color: white;
      font-weight: bold;
    }
    .status-message {
      margin-top: 15px;
      padding: 15px;
      border-radius: 8px;
      display: none;
    }
    .status-success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .status-error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .status-info {
      background: #d1ecf1;
      color: #0c5460;
      border: 1px solid #bee5eb;
    }
    .divider {
      height: 2px;
      background: linear-gradient(90deg, transparent, #667eea, transparent);
      margin: 30px 0;
    }
  </style>
</head>
<body>
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/ota" class="nav-link active">🔄 OTA</a>
      <a href="/mqtt" class="nav-link">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link">📡 WiFi</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <h1>🔄 OTA Firmware Update</h1>
      <p>Update device firmware over-the-air via URL or file upload</p>
      
      <div class="warning">
        <strong>⚠️ Warning:</strong> Device will restart after successful update. Ensure stable power supply and WiFi connection. Do not power off during update!
      </div>

      <!-- Update from URL -->
      <h2>📥 Update from URL</h2>
      <p>Provide a direct URL to a .bin firmware file (HTTP/HTTPS)</p>
      <div class="input-group">
        <label>Firmware URL</label>
        <input type="text" id="firmware-url" placeholder="https://example.com/firmware.bin">
      </div>
      <button class="btn btn-primary" onclick="updateFromURL()">🌐 Update from URL</button>

      <div class="divider"></div>

      <!-- Update from File -->
      <h2>📤 Update from File</h2>
      <p>Upload a .bin firmware file from your computer</p>
      <div class="input-group">
        <label>Select Firmware File (.bin)</label>
        <input type="file" id="firmware-file" accept=".bin">
      </div>
      <button class="btn btn-primary" onclick="updateFromFile()">📁 Upload & Update</button>

      <!-- Progress Bar -->
      <div class="progress-container" id="progress-container">
        <h3 style="margin-bottom: 10px;">Update Progress</h3>
        <div class="progress-bar">
          <div class="progress-fill" id="progress-fill">0%</div>
        </div>
        <p id="progress-text" style="margin-top: 10px; text-align: center; color: #667eea; font-weight: bold;">Initializing...</p>
      </div>

      <!-- Status Messages -->
      <div class="status-message" id="status-message"></div>
    </div>
  </div>

  <script>
    let updateInProgress = false;

    function showProgress() {
      document.getElementById('progress-container').style.display = 'block';
      document.getElementById('status-message').style.display = 'none';
    }

    function hideProgress() {
      document.getElementById('progress-container').style.display = 'none';
    }

    function updateProgress(percent, text) {
      const fill = document.getElementById('progress-fill');
      const progressText = document.getElementById('progress-text');
      fill.style.width = percent + '%';
      fill.textContent = percent + '%';
      if (text) progressText.textContent = text;
    }

    function showStatus(message, type) {
      const statusDiv = document.getElementById('status-message');
      statusDiv.className = 'status-message status-' + type;
      statusDiv.textContent = message;
      statusDiv.style.display = 'block';
    }

    function updateFromURL() {
      if (updateInProgress) {
        alert('Update already in progress!');
        return;
      }

      const url = document.getElementById('firmware-url').value.trim();
      if (!url) {
        alert('Please enter a firmware URL');
        return;
      }

      if (!url.endsWith('.bin')) {
        alert('URL must point to a .bin file');
        return;
      }

      if (!confirm('Start OTA update from URL?\n\nDevice will restart after update.\n\nURL: ' + url)) {
        return;
      }

      updateInProgress = true;
      showProgress();
      updateProgress(0, 'Connecting to server...');

      fetch('/api/ota-url', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url: url })
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          updateProgress(100, 'Update initiated!');
          showStatus('✓ Update started successfully. Device will restart in a few seconds...', 'success');
          setTimeout(() => {
            showStatus('Device is restarting... Please wait 30 seconds then refresh the page.', 'info');
          }, 3000);
        } else {
          hideProgress();
          showStatus('✗ Update failed: ' + data.message, 'error');
          updateInProgress = false;
        }
      })
      .catch(error => {
        hideProgress();
        showStatus('✗ Error: ' + error.message, 'error');
        updateInProgress = false;
      });
    }

    function updateFromFile() {
      if (updateInProgress) {
        alert('Update already in progress!');
        return;
      }

      const fileInput = document.getElementById('firmware-file');
      const file = fileInput.files[0];
      
      if (!file) {
        alert('Please select a firmware file');
        return;
      }

      if (!file.name.endsWith('.bin')) {
        alert('File must be a .bin firmware file');
        return;
      }

      if (!confirm('Upload and install firmware?\n\nDevice will restart after update.\n\nFile: ' + file.name + '\nSize: ' + (file.size / 1024).toFixed(2) + ' KB')) {
        return;
      }

      updateInProgress = true;
      showProgress();
      updateProgress(0, 'Uploading firmware...');

      const formData = new FormData();
      formData.append('firmware', file);

      const xhr = new XMLHttpRequest();

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          updateProgress(percent, 'Uploading: ' + percent + '%');
        }
      });

      xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
          try {
            const data = JSON.parse(xhr.responseText);
            if (data.success) {
              updateProgress(100, 'Update complete!');
              showStatus('✓ Firmware uploaded successfully. Device is restarting...', 'success');
              setTimeout(() => {
                showStatus('Device is restarting... Please wait 30 seconds then refresh the page.', 'info');
              }, 3000);
            } else {
              hideProgress();
              let errorMsg = '✗ Update failed: ' + data.message;
              if (data.error_code) {
                errorMsg += ' (Error code: ' + data.error_code + ')';
              }
              if (data.details) {
                errorMsg += '\\n\\nDetails: ' + data.details;
              }
              showStatus(errorMsg, 'error');
              console.error('OTA Update Error:', data);
              updateInProgress = false;
            }
          } catch (e) {
            hideProgress();
            showStatus('✗ Error parsing response: ' + e.message, 'error');
            console.error('Parse error:', e);
            updateInProgress = false;
          }
        } else {
          hideProgress();
          showStatus('✗ Upload failed with HTTP status: ' + xhr.status + '\\n\\nCheck Serial Monitor for details.', 'error');
          console.error('HTTP Error:', xhr.status, xhr.statusText);
          updateInProgress = false;
        }
      });

      xhr.addEventListener('error', () => {
        hideProgress();
        showStatus('✗ Upload error occurred. Device may have restarted.\\n\\nCheck Serial Monitor for details.', 'error');
        console.error('XHR Error');
        updateInProgress = false;
      });

      xhr.addEventListener('abort', () => {
        hideProgress();
        showStatus('✗ Upload aborted', 'error');
        console.error('XHR Aborted');
        updateInProgress = false;
      });

      xhr.addEventListener('timeout', () => {
        hideProgress();
        showStatus('✗ Upload timeout. Device may be processing the update.\\n\\nWait 30 seconds then refresh.', 'error');
        console.error('XHR Timeout');
        updateInProgress = false;
      });

      xhr.open('POST', '/api/ota-upload');
      xhr.send(formData);
    }
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });

  // OTA Update from URL endpoint
  server.on("/api/ota-url", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
      }
      
      // Parse JSON to get URL
      int urlStart = body.indexOf("\"url\":\"") + 7;
      int urlEnd = body.indexOf("\"", urlStart);
      String firmwareURL = "";
      if (urlStart > 6 && urlEnd > urlStart) {
        firmwareURL = body.substring(urlStart, urlEnd);
      }
      
      if (firmwareURL.length() == 0) {
        request->send(200, "application/json", "{\"success\":false,\"message\":\"No URL provided\"}");
        return;
      }
      
      Serial.println("🔄 OTA Update from URL: " + firmwareURL);
      
      // Perform OTA update in background
      HTTPClient http;
      http.begin(firmwareURL);
      int httpCode = http.GET();
      
      if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        bool canBegin = Update.begin(contentLength);
        
        if (canBegin) {
          Serial.println("✓ OTA update started, size: " + String(contentLength));
          WiFiClient * stream = http.getStreamPtr();
          size_t written = Update.writeStream(*stream);
          
          if (written == contentLength) {
            Serial.println("✓ Written : " + String(written) + " successfully");
          } else {
            Serial.println("✗ Written only : " + String(written) + "/" + String(contentLength));
          }
          
          if (Update.end()) {
            if (Update.isFinished()) {
              Serial.println("✓ OTA update completed successfully!");
              request->send(200, "application/json", "{\"success\":true,\"message\":\"Update successful, restarting...\"}");
              delay(1000);
              ESP.restart();
            } else {
              Serial.println("✗ OTA update not finished");
              request->send(200, "application/json", "{\"success\":false,\"message\":\"Update not finished\"}");
            }
          } else {
            Serial.println("✗ Error Occurred. Error #: " + String(Update.getError()));
            request->send(200, "application/json", "{\"success\":false,\"message\":\"Update error: " + String(Update.getError()) + "\"}");
          }
        } else {
          Serial.println("✗ Not enough space to begin OTA");
          request->send(200, "application/json", "{\"success\":false,\"message\":\"Not enough space\"}");
        }
      } else {
        Serial.println("✗ HTTP error: " + String(httpCode));
        request->send(200, "application/json", "{\"success\":false,\"message\":\"HTTP error: " + String(httpCode) + "\"}");
      }
      http.end();
    }
  );

  // MQTT Configuration page
  server.on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /mqtt page");
    
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>MQTT Config - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 20px;
      min-height: 100vh;
    }
    .navbar {
      background: rgba(0, 0, 0, 0.3);
      backdrop-filter: blur(10px);
      padding: 15px 20px;
      border-radius: 15px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    .navbar-brand {
      color: white;
      font-size: 20px;
      font-weight: bold;
      text-decoration: none;
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
    }
    .nav-link:hover {
      background: rgba(255, 255, 255, 0.2);
    }
    .nav-link.active {
      background: rgba(255, 255, 255, 0.3);
      font-weight: bold;
    }
    .container {
      max-width: 900px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 32px;
    }
    h2 {
      color: #667eea;
      margin: 25px 0 15px 0;
      font-size: 22px;
    }
    p {
      color: #555;
      line-height: 1.8;
      margin-bottom: 15px;
    }
    .status-badge {
      display: inline-block;
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 14px;
      font-weight: bold;
      margin-left: 10px;
    }
    .status-on {
      background: #d4edda;
      color: #155724;
    }
    .status-off {
      background: #f8d7da;
      color: #721c24;
    }
    .input-group {
      margin-bottom: 20px;
    }
    .input-group label {
      display: block;
      font-weight: bold;
      margin-bottom: 8px;
      color: #333;
    }
    .input-group input {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
    }
    .input-group input:focus {
      outline: none;
      border-color: #667eea;
    }
    .input-group small {
      display: block;
      margin-top: 5px;
      color: #666;
      font-size: 12px;
    }
    .toggle-switch {
      position: relative;
      width: 60px;
      height: 30px;
      display: inline-block;
    }
    .toggle-switch input {
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
      border-radius: 30px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 22px;
      width: 22px;
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
      transform: translateX(30px);
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
      transition: all 0.3s;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5568d3;
    }
    .btn-success {
      background: #28a745;
      color: white;
    }
    .btn-success:hover {
      background: #218838;
    }
    .info-box {
      background: #e7f3ff;
      border-left: 4px solid #2196F3;
      padding: 15px;
      margin: 20px 0;
      border-radius: 5px;
    }
  </style>
</head>
<body>
  <nav class="navbar">
    <a href="/" class="navbar-brand">🛰️ TTGO T-Beam <span style="font-size: 0.8em; opacity: 0.8;">v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span></a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/ota" class="nav-link">🔄 OTA</a>
      <a href="/mqtt" class="nav-link active">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link">📡 WiFi</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>

  <div class="container">
    <div class="card">
      <h1>📨 MQTT Configuration</h1>
      <p>Configure MQTT broker to publish sensor data in JSON format</p>
      
      <div style="margin: 20px 0;">
        <label style="display: flex; align-items: center; gap: 10px;">
          <span style="font-weight: bold;">MQTT Enabled:</span>
          <label class="toggle-switch">
            <input type="checkbox" id="mqtt-enabled" onchange="toggleMQTT(this.checked)">
            <span class="slider"></span>
          </label>
          <span id="mqtt-status" class="status-badge status-off">OFF</span>
        </label>
      </div>

      <h2>🔧 Broker Settings</h2>
      <div class="input-group">
        <label>MQTT Broker Server</label>
        <input type="text" id="mqtt-server" placeholder="mqtt.example.com or 192.168.1.100" value="">
        <small>Hostname or IP address of MQTT broker</small>
      </div>

      <div class="input-group">
        <label>Port</label>
        <input type="number" id="mqtt-port" value="1883" min="1" max="65535">
        <small>Default: 1883 (unencrypted), 8883 (TLS)</small>
      </div>

      <div class="input-group">
        <label>Username (optional)</label>
        <input type="text" id="mqtt-username" placeholder="Leave empty if no auth required" value="">
      </div>

      <div class="input-group">
        <label>Password (optional)</label>
        <input type="password" id="mqtt-password" placeholder="Leave empty if no auth required">
        <small>Password is stored securely in NVS</small>
      </div>

      <div class="input-group">
        <label>Topic</label>
        <input type="text" id="mqtt-topic" value="">
        <small>MQTT topic to publish sensor data (e.g., ttgo/sensor, home/sensors/outdoor)</small>
      </div>

      <div class="input-group">
        <label>Device Name</label>
        <input type="text" id="mqtt-device-name" value="">
        <small>Device identifier in JSON payload (default: TTGO-T-Beam)</small>
      </div>

      <div class="input-group">
        <label>Publish Interval (seconds)</label>
        <input type="number" id="mqtt-interval" value="60" min="5" max="3600">
        <small>How often to publish MQTT messages (5-3600 seconds, default: 60)</small>
      </div>

      <h2>📦 Payload Content</h2>
      <p style="color: #666; margin-bottom: 15px;">Select which data to include in MQTT messages</p>
      
      <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px;">
        <label style="display: flex; align-items: center; gap: 10px; padding: 10px; background: #f8f9fa; border-radius: 8px; cursor: pointer;">
          <input type="checkbox" id="mqtt-include-timestamp" checked style="width: 20px; height: 20px;">
          <span>⏰ Timestamp</span>
        </label>
        <label style="display: flex; align-items: center; gap: 10px; padding: 10px; background: #f8f9fa; border-radius: 8px; cursor: pointer;">
          <input type="checkbox" id="mqtt-include-bme280" checked style="width: 20px; height: 20px;">
          <span>🌡️ BME280 Sensor</span>
        </label>
        <label style="display: flex; align-items: center; gap: 10px; padding: 10px; background: #f8f9fa; border-radius: 8px; cursor: pointer;">
          <input type="checkbox" id="mqtt-include-gps" checked style="width: 20px; height: 20px;">
          <span>📍 GPS Data</span>
        </label>
        <label style="display: flex; align-items: center; gap: 10px; padding: 10px; background: #f8f9fa; border-radius: 8px; cursor: pointer;">
          <input type="checkbox" id="mqtt-include-battery" checked style="width: 20px; height: 20px;">
          <span>🔋 Battery Info</span>
        </label>
        <label style="display: flex; align-items: center; gap: 10px; padding: 10px; background: #f8f9fa; border-radius: 8px; cursor: pointer;">
          <input type="checkbox" id="mqtt-include-pax" checked style="width: 20px; height: 20px;">
          <span>👥 PAX Counter</span>
        </label>
        <label style="display: flex; align-items: center; gap: 10px; padding: 10px; background: #f8f9fa; border-radius: 8px; cursor: pointer;">
          <input type="checkbox" id="mqtt-include-system" checked style="width: 20px; height: 20px;">
          <span>💻 System Status</span>
        </label>
      </div>

      <button class="btn btn-primary" onclick="saveMQTTConfig()">💾 Save Configuration</button>
      <button class="btn btn-success" onclick="testMQTT()">🧪 Test Connection</button>
      <button class="btn btn-warning" onclick="sendTestMessage()">📤 Send Test Message</button>

      <div class="info-box">
        <h3 style="margin-top: 0; color: #2196F3;">📋 JSON Payload Format</h3>
        <p>Sensor data is published in JSON format with the following structure:</p>
        <pre style="background: #f5f5f5; padding: 10px; border-radius: 5px; overflow-x: auto; font-size: 12px;">{
  "device": "TTGO-T-Beam",
  "timestamp": 12345,
  "bme280": {
    "temperature": 22.5,
    "humidity": 45.2,
    "pressure": 1013.25,
    "altitude": 123.4
  },
  "gps": {
    "latitude": 51.5074,
    "longitude": -0.1278,
    "altitude": 11.0,
    "satellites": 8,
    "hdop": 1.2,
    "speed": 0.0,
    "course": 0.0
  },
  "battery_voltage": 4.1,
  "battery_current": 150,
  "battery_charging": true,
  "pax": {
    "wifi": 5,
    "ble": 3,
    "total": 8
  },
  "system": {
    "uptime": 3600,
    "free_heap": 180000,
    "wifi_rssi": -45,
    "lora_joined": true
  }
}</pre>
      </div>

      <div id="status-message" style="display: none; margin-top: 20px; padding: 15px; border-radius: 8px;"></div>
    </div>
  </div>

  <script>
    function loadStatus() {
      fetch('/api/mqtt-config')
        .then(response => response.json())
        .then(data => {
          document.getElementById('mqtt-enabled').checked = data.enabled;
          document.getElementById('mqtt-status').textContent = data.enabled ? 'ON' : 'OFF';
          document.getElementById('mqtt-status').className = 'status-badge ' + (data.enabled ? 'status-on' : 'status-off');
          document.getElementById('mqtt-server').value = data.server || '';
          document.getElementById('mqtt-port').value = data.port || 1883;
          document.getElementById('mqtt-username').value = data.username || '';
          document.getElementById('mqtt-topic').value = data.topic || 'ttgo/sensor';
          document.getElementById('mqtt-device-name').value = data.deviceName || 'TTGO-T-Beam';
          document.getElementById('mqtt-interval').value = data.publishInterval || 60;
          
          // Load payload content settings
          document.getElementById('mqtt-include-timestamp').checked = data.includeTimestamp !== false;
          document.getElementById('mqtt-include-bme280').checked = data.includeBME280 !== false;
          document.getElementById('mqtt-include-gps').checked = data.includeGPS !== false;
          document.getElementById('mqtt-include-battery').checked = data.includeBattery !== false;
          document.getElementById('mqtt-include-pax').checked = data.includePAX !== false;
          document.getElementById('mqtt-include-system').checked = data.includeSystem !== false;
        })
        .catch(error => console.error('Error loading MQTT config:', error));
    }

    function toggleMQTT(enabled) {
      fetch('/api/mqtt-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'toggle', enabled: enabled })
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showStatus('✓ MQTT ' + (enabled ? 'enabled' : 'disabled'), 'success');
          loadStatus();
        } else {
          showStatus('✗ Failed to toggle MQTT', 'error');
        }
      })
      .catch(error => showStatus('✗ Error: ' + error.message, 'error'));
    }

    function saveMQTTConfig() {
      const config = {
        action: 'save',
        server: document.getElementById('mqtt-server').value,
        port: parseInt(document.getElementById('mqtt-port').value),
        username: document.getElementById('mqtt-username').value,
        password: document.getElementById('mqtt-password').value,
        topic: document.getElementById('mqtt-topic').value,
        deviceName: document.getElementById('mqtt-device-name').value,
        publishInterval: parseInt(document.getElementById('mqtt-interval').value),
        includeTimestamp: document.getElementById('mqtt-include-timestamp').checked,
        includeBME280: document.getElementById('mqtt-include-bme280').checked,
        includeGPS: document.getElementById('mqtt-include-gps').checked,
        includeBattery: document.getElementById('mqtt-include-battery').checked,
        includePAX: document.getElementById('mqtt-include-pax').checked,
        includeSystem: document.getElementById('mqtt-include-system').checked
      };

      if (!config.server) {
        showStatus('✗ Please enter MQTT broker server', 'error');
        return;
      }

      fetch('/api/mqtt-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showStatus('✓ MQTT configuration saved successfully', 'success');
          document.getElementById('mqtt-password').value = ''; // Clear password field
        } else {
          showStatus('✗ Failed to save configuration: ' + data.message, 'error');
        }
      })
      .catch(error => showStatus('✗ Error: ' + error.message, 'error'));
    }

    function testMQTT() {
      showStatus('🔄 Testing MQTT connection...', 'info');
      
      // Send current form values for testing without saving
      const testConfig = {
        server: document.getElementById('mqtt-server').value,
        port: parseInt(document.getElementById('mqtt-port').value),
        username: document.getElementById('mqtt-username').value,
        password: document.getElementById('mqtt-password').value
      };
      
      if (!testConfig.server) {
        showStatus('✗ Please enter MQTT broker server', 'error');
        return;
      }
      
      fetch('/api/mqtt-test', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(testConfig)
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showStatus('✓ MQTT connection successful!', 'success');
        } else {
          showStatus('✗ MQTT connection failed: ' + data.message, 'error');
        }
      })
      .catch(error => showStatus('✗ Error: ' + error.message, 'error'));
    }

    function sendTestMessage() {
      showStatus('📤 Sending test MQTT message...', 'info');
      
      fetch('/api/mqtt-publish', {
        method: 'POST'
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showStatus('✓ Test message sent successfully! Check your MQTT broker.', 'success');
        } else {
          showStatus('✗ Failed to send test message: ' + data.message, 'error');
        }
      })
      .catch(error => showStatus('✗ Error: ' + error.message, 'error'));
    }

    function showStatus(message, type) {
      const statusDiv = document.getElementById('status-message');
      statusDiv.textContent = message;
      statusDiv.style.display = 'block';
      
      if (type === 'success') {
        statusDiv.style.background = '#d4edda';
        statusDiv.style.color = '#155724';
        statusDiv.style.border = '1px solid #c3e6cb';
      } else if (type === 'error') {
        statusDiv.style.background = '#f8d7da';
        statusDiv.style.color = '#721c24';
        statusDiv.style.border = '1px solid #f5c6cb';
      } else {
        statusDiv.style.background = '#d1ecf1';
        statusDiv.style.color = '#0c5460';
        statusDiv.style.border = '1px solid #bee5eb';
      }
    }

    // Load status on page load
    loadStatus();
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });

  // MQTT Configuration API endpoints
  server.on("/api/mqtt-config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"enabled\":" + String(mqttEnabled ? "true" : "false") + ",";
    json += "\"server\":\"" + mqttServer + "\",";
    json += "\"port\":" + String(mqttPort) + ",";
    json += "\"username\":\"" + mqttUsername + "\",";
    json += "\"topic\":\"" + mqttTopic + "\",";
    json += "\"deviceName\":\"" + mqttDeviceName + "\",";
    json += "\"publishInterval\":" + String(mqttPublishInterval / 1000) + ",";
    json += "\"connected\":" + String(mqttConnected ? "true" : "false") + ",";
    json += "\"publish_count\":" + String(mqttPublishCounter) + ",";
    json += "\"includeTimestamp\":" + String(mqttIncludeTimestamp ? "true" : "false") + ",";
    json += "\"includeBME280\":" + String(mqttIncludeBME280 ? "true" : "false") + ",";
    json += "\"includeGPS\":" + String(mqttIncludeGPS ? "true" : "false") + ",";
    json += "\"includeBattery\":" + String(mqttIncludeBattery ? "true" : "false") + ",";
    json += "\"includePAX\":" + String(mqttIncludePAX ? "true" : "false") + ",";
    json += "\"includeSystem\":" + String(mqttIncludeSystem ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/mqtt-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
      }
      
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body);
      
      if (error) {
        request->send(200, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      String action = doc["action"] | "";
      
      if (action == "toggle") {
        mqttEnabled = doc["enabled"] | false;
        saveSystemSettings();
        request->send(200, "application/json", "{\"success\":true}");
      } else if (action == "save") {
        mqttServer = doc["server"] | "";
        mqttPort = doc["port"] | 1883;
        mqttUsername = doc["username"] | "";
        String password = doc["password"] | "";
        if (password.length() > 0) {
          mqttPassword = password;
        }
        mqttTopic = doc["topic"] | "ttgo/sensor";
        mqttDeviceName = doc["deviceName"] | "TTGO-T-Beam";
        mqttPublishInterval = (doc["publishInterval"] | 60) * 1000; // Convert seconds to milliseconds
        
        // Save payload content configuration
        mqttIncludeTimestamp = doc["includeTimestamp"] | true;
        mqttIncludeBME280 = doc["includeBME280"] | true;
        mqttIncludeGPS = doc["includeGPS"] | true;
        mqttIncludeBattery = doc["includeBattery"] | true;
        mqttIncludePAX = doc["includePAX"] | true;
        mqttIncludeSystem = doc["includeSystem"] | true;
        
        saveSystemSettings();
        
        // Reconnect with new settings
        if (mqttEnabled) {
          mqttClient.disconnect();
          mqttConnected = false;
          connectMQTT();
        }
        
        request->send(200, "application/json", "{\"success\":true}");
      } else {
        request->send(200, "application/json", "{\"success\":false,\"message\":\"Unknown action\"}");
      }
    }
  );

  server.on("/api/mqtt-test", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
      }
      
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body);
      
      if (error) {
        request->send(200, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      
      // Use form values for testing
      String testServer = doc["server"] | "";
      uint16_t testPort = doc["port"] | 1883;
      String testUsername = doc["username"] | "";
      String testPassword = doc["password"] | "";
      
      if (testServer.length() == 0) {
        request->send(200, "application/json", "{\"success\":false,\"message\":\"No MQTT server provided\"}");
        return;
      }
      
      // Test connection with provided values
      WiFiClient testWifiClient;
      PubSubClient testClient(testWifiClient);
      testClient.setServer(testServer.c_str(), testPort);
      
      String clientId = "TTGO-Test-" + String((uint32_t)ESP.getEfuseMac(), HEX);
      bool connected = false;
      
      if (testUsername.length() > 0) {
        connected = testClient.connect(clientId.c_str(), testUsername.c_str(), testPassword.c_str());
      } else {
        connected = testClient.connect(clientId.c_str());
      }
      
      if (connected) {
        testClient.disconnect();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Connection successful!\"}");
      } else {
        String msg = "Connection failed (code: " + String(testClient.state()) + ")";
        request->send(200, "application/json", "{\"success\":false,\"message\":\"" + msg + "\"}");
      }
    }
  );

  // MQTT Publish Test Message endpoint
  server.on("/api/mqtt-publish", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("\n📤 Manual MQTT publish requested via web UI");
    
    if (!mqttEnabled) {
      request->send(200, "application/json", "{\"success\":false,\"message\":\"MQTT is disabled\"}");
      return;
    }
    
    if (!wifiConnected) {
      request->send(200, "application/json", "{\"success\":false,\"message\":\"WiFi not connected\"}");
      return;
    }
    
    // Ensure MQTT is connected
    if (!mqttClient.connected()) {
      Serial.println("🔌 MQTT not connected, attempting to connect...");
      connectMQTT();
      delay(1000); // Give time to connect
    }
    
    if (!mqttClient.connected()) {
      String msg = "MQTT connection failed (code: " + String(mqttClient.state()) + ")";
      request->send(200, "application/json", "{\"success\":false,\"message\":\"" + msg + "\"}");
      return;
    }
    
    // Publish the message
    publishMQTT();
    
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Test message published to " + mqttTopic + "\"}");
  });
  // ============================================================================
  // WiFi Configuration Page and API Endpoints
  // ============================================================================
  
  // WiFi Configuration page
  server.on("/wifi-config", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📄 Serving /wifi-config page");
    static String html;
    if (html.isEmpty()) { html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi Configuration - TTGO T-Beam</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      padding: 0;
      min-height: 100vh;
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
      max-width: 800px;
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
    .header h1 {
      font-size: 28px;
      margin-bottom: 10px;
    }
    .content {
      padding: 30px;
    }
    .status-card {
      background: #f8f9fa;
      border-radius: 10px;
      padding: 20px;
      margin-bottom: 20px;
      border-left: 4px solid #667eea;
    }
    .status-row {
      display: flex;
      justify-content: space-between;
      padding: 10px 0;
      border-bottom: 1px solid #e0e0e0;
    }
    .status-row:last-child {
      border-bottom: none;
    }
    .status-label {
      font-weight: bold;
      color: #666;
    }
    .status-value {
      color: #333;
      font-family: monospace;
    }
    .status-connected {
      color: #28a745;
      font-weight: bold;
    }
    .status-disconnected {
      color: #dc3545;
      font-weight: bold;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      font-weight: bold;
      margin-bottom: 8px;
      color: #333;
    }
    input[type="text"],
    input[type="password"],
    select {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 14px;
    }
    input:focus, select:focus {
      outline: none;
      border-color: #667eea;
    }
    .password-wrapper {
      position: relative;
    }
    .password-toggle {
      position: absolute;
      right: 12px;
      top: 50%;
      transform: translateY(-50%);
      cursor: pointer;
      color: #666;
      user-select: none;
    }
    .btn {
      padding: 12px 30px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      cursor: pointer;
      transition: all 0.3s;
      font-weight: bold;
    }
    .btn-primary {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
    }
    .btn-primary:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .btn-secondary:hover {
      background: #5a6268;
    }
    .btn-danger {
      background: #dc3545;
      color: white;
    }
    .btn-danger:hover {
      background: #c82333;
    }
    .btn:disabled {
      opacity: 0.6;
      cursor: not-allowed;
    }
    .button-group {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-top: 20px;
    }
    .network-list {
      max-height: 300px;
      overflow-y: auto;
      border: 2px solid #ddd;
      border-radius: 8px;
      margin-bottom: 20px;
    }
    .network-item {
      padding: 15px;
      border-bottom: 1px solid #e0e0e0;
      cursor: pointer;
      transition: background 0.2s;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .network-item:hover {
      background: #f8f9fa;
    }
    .network-item:last-child {
      border-bottom: none;
    }
    .network-item.selected {
      background: #e7f3ff;
      border-left: 4px solid #667eea;
    }
    .network-name {
      font-weight: bold;
      color: #333;
    }
    .network-info {
      display: flex;
      gap: 15px;
      align-items: center;
      color: #666;
      font-size: 14px;
    }
    .signal-bars {
      display: flex;
      gap: 2px;
      align-items: flex-end;
    }
    .signal-bar {
      width: 4px;
      background: #ddd;
      border-radius: 2px;
    }
    .signal-bar.active {
      background: #28a745;
    }
    .alert {
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .alert-success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .alert-error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    .alert-info {
      background: #d1ecf1;
      color: #0c5460;
      border: 1px solid #bee5eb;
    }
    .spinner {
      border: 3px solid #f3f3f3;
      border-top: 3px solid #667eea;
      border-radius: 50%;
      width: 20px;
      height: 20px;
      animation: spin 1s linear infinite;
      display: inline-block;
      margin-left: 10px;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .help-text {
      font-size: 13px;
      color: #666;
      margin-top: 5px;
    }
  </style>
</head>
<body>
  <nav class="navbar">
    <a href="/" class="navbar-brand">
      <span>📡</span>
      <span>TTGO T-Beam v)rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(</span>
    </a>
    <div class="navbar-menu">
      <a href="/" class="nav-link">📊 Dashboard</a>
      <a href="/settings" class="nav-link">⚙️ Settings</a>
      <a href="/config" class="nav-link">🔐 LoRa Config</a>
      <a href="/payload-info" class="nav-link">📦 Payload</a>
      <a href="/diagnostics" class="nav-link">🔧 Diagnostics</a>
      <a href="/debug" class="nav-link">🖥️ Debug</a>
      <a href="/ota" class="nav-link">🔄 OTA</a>
      <a href="/mqtt" class="nav-link">📨 MQTT</a>
      <a href="/wifi-config" class="nav-link active">📡 WiFi</a>
      <a href="/about" class="nav-link">ℹ️ About</a>
    </div>
  </nav>

  <div class="page-content">
    <div class="container">
      <div class="header">
        <h1>📡 WiFi Configuration</h1>
        <p>Configure WiFi network connection</p>
      </div>

      <div class="content">
        <div id="alert-success" class="alert alert-success"></div>
        <div id="alert-error" class="alert alert-error"></div>
        <div id="alert-info" class="alert alert-info"></div>

        <div class="status-card">
          <h3 style="margin-bottom: 15px;">📊 Current Status</h3>
          <div class="status-row">
            <span class="status-label">Connection:</span>
            <span id="status-connection" class="status-value">Loading...</span>
          </div>
          <div class="status-row">
            <span class="status-label">SSID:</span>
            <span id="status-ssid" class="status-value">-</span>
          </div>
          <div class="status-row">
            <span class="status-label">IP Address:</span>
            <span id="status-ip" class="status-value">-</span>
          </div>
          <div class="status-row">
            <span class="status-label">Signal Strength:</span>
            <span id="status-rssi" class="status-value">-</span>
          </div>
          <div class="status-row">
            <span class="status-label">MAC Address:</span>
            <span id="status-mac" class="status-value">-</span>
          </div>
        </div>

        <div style="margin-bottom: 30px;">
          <h3 style="margin-bottom: 15px;">🔍 Available Networks</h3>
          <button onclick="scanNetworks()" class="btn btn-secondary" id="scan-btn">
            Scan for Networks
          </button>
          <div id="network-list" class="network-list" style="display: none; margin-top: 15px;"></div>
        </div>

        <div>
          <h3 style="margin-bottom: 15px;">🔧 Network Configuration</h3>
          
          <div class="form-group">
            <label for="ssid">Network Name (SSID)</label>
            <input type="text" id="ssid" placeholder="Enter network name" maxlength="32">
            <div class="help-text">Select from scanned networks or enter manually</div>
          </div>

          <div class="form-group">
            <label for="password">Password</label>
            <div class="password-wrapper">
              <input type="password" id="password" placeholder="Enter network password" maxlength="64">
              <span class="password-toggle" onclick="togglePassword()">👁️</span>
            </div>
            <div class="help-text">Leave empty for open networks</div>
          </div>

          <div class="button-group">
            <button onclick="connectWiFi()" class="btn btn-primary" id="connect-btn">
              Connect to Network
            </button>
            <button onclick="disconnectWiFi()" class="btn btn-secondary">
              Disconnect
            </button>
            <button onclick="resetWiFi()" class="btn btn-danger">
              Reset WiFi Settings
            </button>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    window.onload = function() { loadWiFiStatus(); };

    function showAlert(type, message) {
      const alertId = 'alert-' + type;
      const alert = document.getElementById(alertId);
      alert.textContent = message;
      alert.style.display = 'block';
      setTimeout(() => { alert.style.display = 'none'; }, 5000);
    }

    function loadWiFiStatus() {
      fetch('/api/wifi-status')
        .then(response => response.json())
        .then(data => {
          const connStatus = document.getElementById('status-connection');
          if (data.connected) {
            connStatus.textContent = 'Connected';
            connStatus.className = 'status-value status-connected';
          } else {
            connStatus.textContent = 'Disconnected';
            connStatus.className = 'status-value status-disconnected';
          }
          document.getElementById('status-ssid').textContent = data.ssid || '-';
          document.getElementById('status-ip').textContent = data.ip || '-';
          document.getElementById('status-rssi').textContent = data.rssi ? data.rssi + ' dBm' : '-';
          document.getElementById('status-mac').textContent = data.mac || '-';
        })
        .catch(error => {
          console.error('Error loading WiFi status:', error);
          showAlert('error', 'Failed to load WiFi status');
        });
    }

    function scanNetworks() {
      const btn = document.getElementById('scan-btn');
      const list = document.getElementById('network-list');
      btn.disabled = true;
      btn.innerHTML = 'Scanning<span class="spinner"></span>';
      list.innerHTML = '<div style="padding: 20px; text-align: center;">Scanning for networks...</div>';
      list.style.display = 'block';

      fetch('/api/wifi-scan')
        .then(response => response.json())
        .then(data => {
          if (data.networks && data.networks.length > 0) {
            list.innerHTML = '';
            data.networks.forEach(network => {
              const item = document.createElement('div');
              item.className = 'network-item';
              item.onclick = () => selectNetwork(network.ssid, item);
              const signalBars = getSignalBars(network.rssi);
              const securityIcon = network.encryption === 0 ? '🔓' : '🔒';
              item.innerHTML = `
                <div class="network-name">${securityIcon} ${network.ssid}</div>
                <div class="network-info">
                  <div>${signalBars}</div>
                  <div>${network.rssi} dBm</div>
                  <div>Ch ${network.channel}</div>
                </div>
              `;
              list.appendChild(item);
            });
            showAlert('success', `Found ${data.networks.length} networks`);
          } else {
            list.innerHTML = '<div style="padding: 20px; text-align: center; color: #666;">No networks found</div>';
            showAlert('info', 'No networks found. Try scanning again.');
          }
        })
        .catch(error => {
          console.error('Error scanning networks:', error);
          list.innerHTML = '<div style="padding: 20px; text-align: center; color: #dc3545;">Scan failed</div>';
          showAlert('error', 'Failed to scan networks');
        })
        .finally(() => {
          btn.disabled = false;
          btn.textContent = 'Scan for Networks';
        });
    }

    function getSignalBars(rssi) {
      const bars = [];
      const strength = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : 1;
      for (let i = 1; i <= 4; i++) {
        const height = i * 5;
        const active = i <= strength ? 'active' : '';
        bars.push(`<div class="signal-bar ${active}" style="height: ${height}px;"></div>`);
      }
      return '<div class="signal-bars">' + bars.join('') + '</div>';
    }

    function selectNetwork(ssid, element) {
      document.querySelectorAll('.network-item').forEach(item => {
        item.classList.remove('selected');
      });
      element.classList.add('selected');
      document.getElementById('ssid').value = ssid;
      showAlert('info', `Selected network: ${ssid}`);
    }

    function togglePassword() {
      const input = document.getElementById('password');
      input.type = input.type === 'password' ? 'text' : 'password';
    }

    function connectWiFi() {
      const ssid = document.getElementById('ssid').value.trim();
      const password = document.getElementById('password').value;
      if (!ssid) {
        showAlert('error', 'Please enter a network name (SSID)');
        return;
      }
      const btn = document.getElementById('connect-btn');
      btn.disabled = true;
      btn.innerHTML = 'Connecting<span class="spinner"></span>';

      // Clear any previous result banner
      const existingBanner = document.getElementById('wifi-result-banner');
      if (existingBanner) existingBanner.remove();

      fetch('/api/wifi-connect', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ssid: ssid, password: password})
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          // Start polling for the result — device keeps AP up so we stay connected
          pollForWiFiResult(ssid, 0);
        } else {
          showAlert('error', data.message || 'Connection failed');
          btn.disabled = false;
          btn.textContent = 'Connect to Network';
        }
      })
      .catch(error => {
        console.error('Error connecting:', error);
        showAlert('error', 'Failed to connect to network');
        btn.disabled = false;
        btn.textContent = 'Connect to Network';
      });
    }

    function pollForWiFiResult(ssid, attempts) {
      const maxAttempts = 18; // 18 × 2 s = 36 s total
      const btn = document.getElementById('connect-btn');
      const remaining = maxAttempts - attempts;
      btn.innerHTML = 'Testing credentials&hellip; (' + remaining * 2 + 's left)<span class="spinner"></span>';

      fetch('/api/wifi-status')
        .then(r => r.json())
        .then(data => {
          const ip = data.sta_ip || data.ip || '';
          const realIP = ip && ip !== '0.0.0.0' && ip !== '255.255.255.255';
          if (data.connected && realIP) {
            // SUCCESS — show banner with new IP and clickable link
            showWiFiSuccess(ssid, ip);
            loadWiFiStatus();
            btn.disabled = false;
            btn.textContent = 'Connect to Network';
          } else if (attempts >= maxAttempts) {
            // TIMEOUT — credentials likely wrong
            showWiFiFailed(ssid);
            loadWiFiStatus();
            btn.disabled = false;
            btn.textContent = 'Connect to Network';
          } else {
            setTimeout(() => pollForWiFiResult(ssid, attempts + 1), 2000);
          }
        })
        .catch(() => {
          if (attempts >= maxAttempts) {
            showWiFiFailed(ssid);
            btn.disabled = false;
            btn.textContent = 'Connect to Network';
          } else {
            setTimeout(() => pollForWiFiResult(ssid, attempts + 1), 2000);
          }
        });
    }

    function showWiFiSuccess(ssid, ip) {
      const banner = document.createElement('div');
      banner.id = 'wifi-result-banner';
      banner.style.cssText = 'background:#d4edda;border:1px solid #c3e6cb;color:#155724;padding:16px 20px;border-radius:8px;margin-bottom:16px;font-size:15px;';
      banner.innerHTML = '<strong>&#10003; Connected to ' + ssid + '!</strong><br>' +
        'New IP address: <strong>' + ip + '</strong>&nbsp;&nbsp;' +
        '<a href="http://' + ip + '" target="_blank" style="color:#155724;font-weight:bold;border:1px solid #155724;padding:4px 12px;border-radius:4px;text-decoration:none;">Open dashboard at new IP &rarr;</a>';
      const content = document.querySelector('.content') || document.body;
      content.insertBefore(banner, content.firstChild);
    }

    function showWiFiFailed(ssid) {
      const banner = document.createElement('div');
      banner.id = 'wifi-result-banner';
      banner.style.cssText = 'background:#f8d7da;border:1px solid #f5c6cb;color:#721c24;padding:16px 20px;border-radius:8px;margin-bottom:16px;font-size:15px;';
      banner.innerHTML = '<strong>&#x2717; Could not connect to "' + ssid + '"</strong><br>' +
        'Credentials may be wrong, or the network is out of range. Saved password has been updated — device will keep retrying in the background.';
      const content = document.querySelector('.content') || document.body;
      content.insertBefore(banner, content.firstChild);
    }

    function disconnectWiFi() {
      if (!confirm('Disconnect from current network?')) return;
      fetch('/api/wifi-disconnect', {method: 'POST'})
        .then(response => response.json())
        .then(data => {
          showAlert('success', 'Disconnected from WiFi');
          setTimeout(() => { loadWiFiStatus(); }, 1000);
        })
        .catch(error => {
          console.error('Error disconnecting:', error);
          showAlert('error', 'Failed to disconnect');
        });
    }

    function resetWiFi() {
      if (!confirm('Reset WiFi settings? This will clear saved credentials and restart the device in AP mode.')) return;
      fetch('/api/wifi-reset', {method: 'POST'})
        .then(response => response.json())
        .then(data => {
          showAlert('info', 'WiFi settings reset. Device will restart...');
          setTimeout(() => { window.location.href = '/'; }, 3000);
        })
        .catch(error => {
          console.error('Error resetting WiFi:', error);
          showAlert('error', 'Failed to reset WiFi settings');
        });
    }
  </script>
</body>
</html>
)rawliteral";
    }
    request->send(200, "text/html", html);
  });

  // WiFi Status API
  server.on("/api/wifi-status", HTTP_GET, [](AsyncWebServerRequest *request){
    wifi_mode_t mode = WiFi.getMode();
    String json = "{";
    json += "\"mode\":\"";
    if (mode == WIFI_MODE_AP) {
      json += "AP\",";
      json += "\"connected\":false,";
      json += "\"ssid\":\"" + WiFi.softAPSSID() + "\",";
      json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
      json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
    } else if (mode == WIFI_MODE_STA) {
      json += "STA\",";
      json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
      json += "\"ssid\":\"" + WiFi.SSID() + "\",";
      json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    } else if (mode == WIFI_MODE_APSTA) {
      json += "APSTA\",";
      json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
      json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
      json += "\"sta_ip\":\"" + WiFi.localIP().toString() + "\",";
    } else {
      json += "OFF\",";
      json += "\"connected\":false,";
    }
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"status\":" + String(WiFi.status());
    json += "}";
    request->send(200, "application/json", json);
  });

  // WiFi Scan API
  server.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 WiFi scan requested");
    
    // Ensure WiFi is in STA or STA+AP mode for scanning
    wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode == WIFI_MODE_NULL || currentMode == WIFI_MODE_AP) {
      Serial.println("📡 Switching to STA mode for scan");
      WiFi.mode(WIFI_MODE_APSTA); // Allow both AP and STA for scanning
      delay(100);
    }
    
    // Perform scan with yield to prevent watchdog timeout
    yield();
    int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
    yield();
    
    if (n < 0) {
      Serial.println("✗ WiFi scan failed");
      request->send(500, "application/json", "{\"networks\":[],\"error\":\"Scan failed\"}");
      return;
    }
    
    Serial.printf("📡 Found %d networks\n", n);
    
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{";
      json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      json += "\"encryption\":" + String(WiFi.encryptionType(i)) + ",";
      json += "\"channel\":" + String(WiFi.channel(i));
      json += "}";
      yield(); // Feed watchdog while building JSON
    }
    json += "]}";
    
    WiFi.scanDelete();
    request->send(200, "application/json", json);
  });

  // WiFi Connect API - saves credentials to NVS then connects
  server.on("/api/wifi-connect", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
      }
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body);
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
      }
      String ssid = doc["ssid"].as<String>();
      String password = doc["password"].as<String>();

      if (ssid.length() == 0) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"SSID cannot be empty\"}");
        return;
      }

      Serial.print("📡 Web UI connecting to: ");
      Serial.println(ssid);

      // Save credentials to NVS - same namespace/keys as wifi-set serial command
      preferences.begin("wifi", false);
      preferences.putString("ssid", ssid);
      preferences.putString("password", password);
      preferences.end();
      Serial.println("💾 Credentials saved to NVS");

      // Properly tear down AP mode if running, then switch directly to STA
      // (going through WIFI_OFF causes an IDF 'wifi:timeout when un-init' error)
      wifi_mode_t currentMode = WiFi.getMode();
      if (currentMode == WIFI_MODE_AP || currentMode == WIFI_MODE_APSTA) {
        Serial.println("📡 Stopping AP mode...");
        WiFi.softAPdisconnect(true);
        delay(500);   // let AP fully release the radio
      }
      WiFi.persistent(false);  // Use our NVS, not the library-managed flash
      // Stay in AP+STA mode so the browser session survives while we test
      // the new credentials.  The AP disappears only if/when loop() sees a
      // successful STA connection and clears wifiConfigMode.
      WiFi.mode(WIFI_AP_STA);
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // Re-enable DHCP
      delay(300);

      wifiConfigMode = false;

      // Start connection (non-blocking - response is sent immediately)
      WiFi.disconnect(false);
      delay(50);
      WiFi.begin(ssid.c_str(), password.c_str());
      Serial.println("📡 Connection started in background (AP still up for status polling)");

      String response = "{\"success\":true,\"message\":\"Connecting to " + ssid + "...\",\"status\":\"connecting\"}";
      request->send(200, "application/json", response);
    }
  );

  // WiFi Disconnect API
  server.on("/api/wifi-disconnect", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("📡 Disconnecting WiFi");
    WiFi.disconnect(true);
    wifiConnected = false;
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Disconnected\"}");
  });


  // OTA Update from file upload endpoint
  server.on("/api/ota-upload", HTTP_POST,
    // Request complete handler - send response then restart
    [](AsyncWebServerRequest *request) {
      bool success = !Update.hasError();
      if (success) {
        Serial.println("✓ OTA upload complete - restarting");
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
          "{\"success\":true,\"message\":\"Update successful, restarting...\"}");
        response->addHeader("Connection", "close");
        request->send(response);
        delay(500);
        ESP.restart();
      } else {
        int errorCode = Update.getError();
        Serial.printf("✗ OTA upload failed - error %d: %s\n", errorCode, Update.errorString());
        String jsonResponse = "{\"success\":false,\"message\":\"Update failed: ";
        jsonResponse += Update.errorString();
        jsonResponse += "\",\"error_code\":";
        jsonResponse += String(errorCode);
        jsonResponse += "}";
        request->send(200, "application/json", jsonResponse);
        otaInProgress = false;
      }
    },
    // Chunk handler - called for each data chunk as it arrives
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        // First chunk: initialise the update
        otaInProgress = true;
        Serial.printf("\n🔄 OTA upload started: %s  free-sketch: %u bytes\n",
                      filename.c_str(), ESP.getFreeSketchSpace());

        // Show OTA start on OLED
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("  Firmware Update");
        display.println("  ─────────────────");
        display.println();
        display.println("  Uploading...");
        display.println();
        display.println("  Do NOT power off!");
        display.display();

        // ── Stop every task/interrupt that might access flash during write ──
        // MQTT
        if (mqttClient.connected()) mqttClient.disconnect();

        // GPS serial
        if (gpsEnabled) { GpsSerial.end(); }

        // LMIC DIO GPIO interrupts — must be disabled BEFORE BLE deinit.
        // MCCI LMIC 4.x registers these via esp_intr_alloc(), not through the
        // Arduino GPIO ISR service, so detachInterrupt() silently fails.
        // gpio_intr_disable() kills interrupt generation at the GPIO hardware
        // register level regardless of how the ISR was registered, which is
        // exactly what we need before Update.write() disables the I-cache.
        gpio_intr_disable((gpio_num_t)LORA_DIO0_PIN);
        gpio_intr_disable((gpio_num_t)LORA_DIO1_PIN);
        gpio_intr_disable((gpio_num_t)LORA_DIO2_PIN);
        Serial.println("✓ LoRa DIO GPIO interrupts disabled");

        // NimBLE: fully deinit AFTER disabling GPIO interrupts.
        // BLE teardown can uninstall the GPIO ISR service, so GPIO must be
        // dealt with first.
        if (pBLEScan != nullptr) {
          pBLEScan->stop();
          pBLEScan = nullptr;
        }
        if (paxCounterEnabled && paxBleScanEnabled) {
          Serial.println("🛑 Deinitialising BLE before OTA flash write...");
          NimBLEDevice::deinit(true);
          Serial.println("✓ BLE deinitialised");
        }

        // Let FreeRTOS settle before we disable the instruction cache
        delay(300);

        // Use UPDATE_SIZE_UNKNOWN so the library handles sizing automatically
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Serial.printf("✗ Update.begin failed: %s\n", Update.errorString());
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("  Firmware Update");
          display.println("  ─────────────────");
          display.println();
          display.println("  ERROR:");
          display.println("  begin() failed");
          display.display();
        } else {
          Serial.println("✓ Update.begin OK");
        }
      }

      // Write this chunk
      if (!Update.hasError()) {
        if (Update.write(data, len) != len) {
          Serial.printf("✗ Update.write failed at index %u: %s\n", index, Update.errorString());
        } else if (index % (64 * 1024) == 0 && index > 0) {
          Serial.printf("⏳ OTA progress: %u bytes\n", index + len);
          // Update OLED progress every 64 KB
          uint32_t kbDone = (index + len) / 1024;
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(0, 0);
          display.println("  Firmware Update");
          display.println("  ─────────────────");
          display.println();
          display.print("  ");
          display.print(kbDone);
          display.println(" KB written");
          display.println();
          display.println("  Do NOT power off!");
          display.display();
        }
      }

      // Final chunk
      if (final) {
        if (!Update.hasError()) {
          if (Update.end(true)) {
            Serial.printf("✓ OTA finished: %u bytes written\n", index + len);
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println("  Firmware Update");
            display.println("  ─────────────────");
            display.println();
            display.println("  Upload complete!");
            display.println();
            display.println("  Restarting...");
            display.display();
          } else {
            Serial.printf("✗ Update.end failed: %s\n", Update.errorString());
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("  Firmware Update");
            display.println("  ─────────────────");
            display.println();
            display.println("  ERROR:");
            display.println("  end() failed");
            display.display();
          }
        }
      }
    }
  );

  // 404 handler - redirect to dashboard
  server.onNotFound([](AsyncWebServerRequest *request){
    // Redirect all unknown paths to dashboard
    request->redirect("/");
  });
  
  server.begin();
  Serial.println("Web server started on port 80");
}

/**
 * @brief Initialize WiFi connection with WiFiManager
 *
 * Handles WiFi connectivity with automatic fallback to AP mode:
 * 1. Checks if boot button (GPIO 0) is pressed for manual reset
 * 2. Attempts to connect using saved credentials
 * 3. If no credentials or connection fails, starts AP mode
 *
 * WiFi Manager Features:
 * - Auto-connect to saved network
 * - Captive portal for configuration
 * - Network scanning and selection
 * - Password entry
 * - Persistent credential storage
 *
 * Callbacks:
 * - setAPCallback: Called when entering config mode
 * - setSaveConfigCallback: Called when credentials saved
 *
 * @see setupWebServer() for web server initialization
 * @see WIFI_AP_NAME and WIFI_AP_PASSWORD constants
 */
