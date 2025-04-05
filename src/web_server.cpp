#include <Arduino.h>
#include "web_server.h"
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "gpio_control.h"
#include "temperature.h"
#include "utilities.h"
#include <Update.h>
#include <Preferences.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"

Preferences prefs_ota;
WebServer server(80);
WebSocketsServer webSocket(81);

static int lastRSSI = 0;
unsigned long lastPush = 0;
const int rssiThreshold = 5;
bool shouldReboot = false;
String firmwareVersion = String(FW_VERSION) + " (" + String(__DATE__) + " " + String(__TIME__) + ")";

extern unsigned long bootMillis;
extern float tempBuffer[MAX_TEMP_POINTS];
extern unsigned long timeBuffer[MAX_TEMP_POINTS];
extern size_t tempIndex;
extern bool bufferFull;

void initWebServer() {
  server.on("/", handle_OnConnect);
  server.on("/led1on", handle_led1on);
  server.on("/led1off", handle_led1off);
  server.on("/led2on", handle_led2on);
  server.on("/led2off", handle_led2off);
  server.on("/temperature", handle_temperature);
  server.on("/status", []() {
    String json = "{";
    json += "\"led1\":" + String(LED1status ? "true" : "false") + ",";
    json += "\"led2\":" + String(LED2status ? "true" : "false");
    json += "\"uptime\":" + String(getUptimeMillis(bootMillis) / 1000);
    json += "}";
    server.send(200, "application/json", json);
  });
  server.on("/gpio", handleGPIOControl);
  
  server.on("/favicon.ico", []() {
    server.send(204);
  });

  server.onNotFound([]() {
    Serial.print("404 Not Found: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "Not found");
  });

  if (prefs_ota.begin("ota", false)) {
    if (!prefs_ota.isKey("version_factory")) {
      prefs_ota.putString("version_factory", firmwareVersion);
      Serial.println("✅ Factory version stored: " + firmwareVersion);
    } else {
      Serial.println("ℹ️ Factory version already exists.");
    }
    prefs_ota.end();
  } else {
    Serial.println("❌ Failed to init OTA preferences.");
  } 
  
  server.begin();
  Serial.println("HTTP server started");
}

void initWebSocket() {
  webSocket.begin();
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) {
      String historyJson = "[";
      size_t start = bufferFull ? tempIndex : 0;
      size_t count = bufferFull ? MAX_TEMP_POINTS : tempIndex;

      for (size_t i = 0; i < count; ++i) {
        size_t idx = (start + i) % MAX_TEMP_POINTS;
        float seconds = timeBuffer[idx] / 1000.0;
        historyJson += "{\"time\":" + String(seconds, 1) + ",\"temp\":" + String(tempBuffer[idx], 2) + "}";
        if (i < count - 1) historyJson += ",";
      }
      historyJson += "]";
      webSocket.sendTXT(num, "{\"history\":" + historyJson + "}");
    }

    else if (type == WStype_TEXT) {
      String msg = (char*)payload;
      if (msg == "getStatus") {
        String json = "{";
        json += "\"led1\":" + String(LED1status ? "true" : "false") + ",";
        json += "\"led2\":" + String(LED2status ? "true" : "false") + ",";
        json += "\"uptime\":" + String(getUptimeMillis(bootMillis) / 1000) + ",";
        json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
        json += "\"sta_ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"clients\":" + String(WiFi.softAPgetStationNum());
        json += "}";
        webSocket.sendTXT(num, json);
      }
    }
  });
}

void handleClients() {
  unsigned long now;
  int currentRSSI;

  server.handleClient();
  webSocket.loop();

  digitalWrite(LED1pin, LED1status);  
  digitalWrite(LED2pin, LED2status); 

  now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    currentRSSI = WiFi.RSSI();
  } else {
    currentRSSI = 0;
  }
  
  if (now - lastPush >= 1000 || abs(currentRSSI - lastRSSI) >= rssiThreshold) {
    lastPush = now;

    String json = "{";
    json += "\"temp\":" + String(currentTempC, 2) + ",";
    json += "\"uptime\":" + String(getUptimeMillis(bootMillis) / 1000);
    if (WiFi.status() == WL_CONNECTED && abs(currentRSSI - lastRSSI) >= rssiThreshold) {
      json += ",\"rssi\":" + String(currentRSSI);
      lastRSSI = currentRSSI;
    }
    json += "}";
    webSocket.broadcastTXT(json);
  }
}

void handle_OnConnect() {
    server.send(200, "text/html", SendHTML(LED1status, LED2status));
}
  
void handle_led1on() {
    LED1status = HIGH; saveStates();
    server.send(200, "application/json", "{\"led\":1,\"status\":\"on\"}");
    webSocket.broadcastTXT("{\"led1\":true}");
}

void handle_led1off() {
    LED1status = LOW; saveStates();
    server.send(200, "application/json", "{\"led\":1,\"status\":\"off\"}");
    webSocket.broadcastTXT("{\"led1\":false}");
}

void handle_led2on() {
    LED2status = HIGH; saveStates();
    server.send(200, "application/json", "{\"led\":2,\"status\":\"on\"}");
    webSocket.broadcastTXT("{\"led2\":true}");
}

void handle_led2off() {
    LED2status = LOW; saveStates();
    server.send(200, "application/json", "{\"led\":2,\"status\":\"off\"}");
    webSocket.broadcastTXT("{\"led2\":false}");
}

void handle_temperature() {
    server.send(200, "text/plain", String(currentTempC, 2));
}

void handle_NotFound() {
  Serial.println("404 Not Found: " + server.uri());   
  server.send(404, "text/plain", "Not found");
}

void handleGPIOControl() {
    if (!server.hasArg("pin") || !server.hasArg("state")) {
        server.send(400, "text/plain", "Missing pin or state");
        return;
    }
    int pin = server.arg("pin").toInt();
    String state = server.arg("state");
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state == "on" ? HIGH : LOW);
    server.send(200, "application/json", "{\"pin\":" + String(pin) + ",\"state\":\"" + state + "\"}");
}

void handleOtaUpdate() {
  prefs_ota.begin("ota", false);

  // === OTA Upload Handler ===
  server.on("/update", HTTP_POST, []() {}, []() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("OTA Start: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.println("✅ OTA Success. Rebooting soon...");

        const esp_partition_t* running = esp_ota_get_running_partition();
        String label = running ? String(running->label) : "unknown";
        
        prefs_ota.end();  
        prefs_ota.begin("ota", false);
        
        if (label == "ota_0") {
          prefs_ota.putString("version_ota_0", firmwareVersion);
        } else if (label == "ota_1") {
          prefs_ota.putString("version_ota_1", firmwareVersion);
        }

        prefs_ota.putString("lastUpdate", String(millis()));
        prefs_ota.putString("lastVersion", firmwareVersion);
        prefs_ota.putString("lastPart", label);
        prefs_ota.putString("version_factory", firmwareVersion.c_str());

        String hist = prefs_ota.getString("updateHistory", "");
        hist += firmwareVersion + " @ " + String(millis()) + " -> " + label + "\n";
        prefs_ota.putString("updateHistory", hist);
        prefs_ota.end();
        shouldReboot = true;
        server.send(200, "text/plain", "Update OK");
      } else {
        Update.printError(Serial);
        server.send(500, "text/plain", "Update Failed");
      }
    }
  });

  // === Serve Version Info ===
  server.on("/ota_version", HTTP_GET, []() {
    prefs_ota.begin("ota", true);
    String version = prefs_ota.getString("lastVersion", firmwareVersion);
    prefs_ota.end();
  
    server.send(200, "text/plain", version);
  });

  server.on("/current_version", HTTP_GET, []() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    String label = running ? String(running->label) : "unknown";
  
    String version;
    if (label == "ota_0") {
      version = prefs_ota.getString("version_ota_0", "Unknown");
    } else if (label == "ota_1") {
      version = prefs_ota.getString("version_ota_1", "Unknown");
    } else if (label == "factory") {
      version = prefs_ota.getString("version_factory", "Unknown");
    }
  
    server.send(200, "text/plain", version);
  });

  server.on("/ota_time", HTTP_GET, []() {
    prefs_ota.begin("ota", true);  
    String time = prefs_ota.getString("lastUpdate", "Never");
    prefs_ota.end();
    server.send(200, "text/plain", time);
  });

  // === Dropdown with all available versions ===
  server.on("/ota_versions", HTTP_GET, []() {
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.to<JsonArray>();

    prefs_ota.begin("ota", true);

    // Factory version
    JsonObject o0 = arr.createNestedObject();
    o0["partition"] = "factory";
    o0["label"] = prefs_ota.getString("version_factory", "Factory");

    String v0 = prefs_ota.getString("version_ota_0", "Unknown");
    JsonObject o1 = arr.createNestedObject();
    o1["partition"] = "ota_0";
    o1["label"] = v0 + " (ota_0)";

    String v1 = prefs_ota.getString("version_ota_1", "Unknown");
    JsonObject o2 = arr.createNestedObject();
    o2["partition"] = "ota_1";
    o2["label"] = v1 + " (ota_1)";

    prefs_ota.end();

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
  });

  // === Switch boot partition ===
  server.on("/switch_partition", HTTP_GET, []() {
    if (!server.hasArg("target")) {
      server.send(400, "text/plain", "Missing target partition");
      return;
    }

    String target = server.arg("target");

    const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP,
      target == "factory" ? ESP_PARTITION_SUBTYPE_APP_FACTORY :
      target == "ota_0"   ? ESP_PARTITION_SUBTYPE_APP_OTA_0 :
      target == "ota_1"   ? ESP_PARTITION_SUBTYPE_APP_OTA_1 :
                            ESP_PARTITION_SUBTYPE_ANY,
      NULL
    );

    if (!part) {
      server.send(404, "text/plain", "Partition not found");
      return;
    }

    if (esp_ota_set_boot_partition(part) == ESP_OK) {
      server.send(200, "text/plain", "✅ Boot partition set successfully.");
      shouldReboot = true;
    } else {
      server.send(500, "text/plain", "❌ Failed to set boot partition.");
    }
  });

  // === Full OTA update history list ===
  server.on("/ota_history", HTTP_GET, []() {
    String hist = prefs_ota.getString("updateHistory", "");
    hist.trim();
    hist.replace("\n", "\",\"");
    server.send(200, "application/json", "[\"" + hist + "\"]");
  });

  prefs_ota.end();
}

/* ========== HTML Generator ========== */
String SendHTML(uint8_t led1stat, uint8_t led2stat) {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset='UTF-8'>
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <title>Mingle Dashboard</title>
      <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
      <style>
        body {
          font-family: Arial, sans-serif;
          text-align: center;
          padding: 20px;
          margin: 0;
          background-color: #f7f7f7;
        }

        .switch {
          position: relative;
          display: inline-block;
          width: 50px;
          height: 24px;
          margin-left: 10px;
        }

        .switch input {
          opacity: 0;
          width: 0;
          height: 0;
        }

        .slider {
          position: absolute;
          cursor: pointer;
          top: 0; left: 0; right: 0; bottom: 0;
          background-color: #ccc;
          transition: .4s;
          border-radius: 24px;
        }

        .slider:before {
          position: absolute;
          content: "";
          height: 18px;
          width: 18px;
          left: 3px;
          bottom: 3px;
          background-color: white;
          transition: .4s;
          border-radius: 50%;
        }

        input:checked + .slider {
          background-color: #2196F3;
        }

        input:checked + .slider:before {
          transform: translateX(26px);
        }

        input, select {
          padding: 5px;
          margin: 5px;
          font-size: 16px;
        }

        .gpio-control {
          display: flex;
          justify-content: center;
          align-items: center;
          gap: 10px;
          margin: 10px 0;
          flex-wrap: wrap;
        }

        .gpio-control input,

        .gpio-control select {
          height: 40px;
          font-size: 16px;
          padding: 0 12px;
          border: 1px solid #ccc;
          border-radius: 6px;
          box-sizing: border-box;
        }

        .button {
          margin: 10px;
          height: 40px;
          font-size: 16px;
          padding: 0 16px;
          background-color: #2196F3;
          color: white;
          border: none;
          border-radius: 6px;
          cursor: pointer;
          transition: background-color 0.3s ease;
        }

        .button:hover {
          background-color: #1976D2;
        }

        .output-controls {
          display: flex;
          flex-direction: column;
          gap: 14px;
          margin: 20px auto;
          width: fit-content;
        }

        .output-control {
          display: flex;
          align-items: center;
          gap: 18px;
        }

        .label {
          font-weight: bold;
          min-width: 70px;
          text-align: right;
        }

        .lamp {
          width: 16px;
          height: 16px;
          border-radius: 50%;
          background-color: #ccc; /* OFF color */
          box-shadow: inset 0 0 2px rgba(0,0,0,0.2);
          transition: background-color 0.3s ease, box-shadow 0.3s ease;
          display: inline-block;
        }

        .lamp.on {
          background-color: #2196F3; /* toggle blue */
          box-shadow: 0 0 8px 2px #2196F3;
        }

        .lamp.off {
          background-color: #888888;
          box-shadow: none;
        }
        
        #scroll-wrapper {
          border: 1px solid #ccc;
          background: #fff;
          box-shadow: 0 2px 6px rgba(0,0,0,0.1);
          margin: 10px auto;
          padding: 10px;
          max-width: 100%;
          overflow-x: auto;
          overflow-y: hidden;
        }

        #scroll-wrapper::-webkit-scrollbar {
          height: 10px;
        }

        #scroll-wrapper::-webkit-scrollbar-thumb {
          background: #2196F3;
          border-radius: 5px;
        }

        canvas {
          width: 100% !important;
          height: auto !important;
        }

        #tempChart {
          height: 400px !important;
        }

        @media (max-width: 600px) {
          h2 {
            font-size: 1.4rem;
          }
          input, select {
            font-size: 14px;
          }
          #tempChart {
            height: 300px;
          }
        }

        footer {
        margin-top: 30px;
        padding: 16px 12px;
        background-color: #f0f0f5;
        border-radius: 12px;
        font-size: 0.9em;
        color: #444;
        text-align: center;
        box-shadow: 0 2px 6px rgba(0,0,0,0.1);
        font-family: 'Segoe UI', sans-serif;
        line-height: 1.5;
      }

      .footer-tagline {
        margin: 4px 0;
        font-size: 1.2em;
        color: #666;
        font-style: italic;
      }

      .footer-heading {
        margin: 10px 0 4px;
        font-weight: 500;
      }

      .footer-links {
        margin: 0;
      }

      .footer-links a {
        color: #3366cc;
        text-decoration: none;
      }

      .footer-copy {
        margin-top: 12px;
        color: #999;
        font-size: 0.85em;
      }
    </style>
    </head>
    <body>
      <h2>Smart Control Web Dashboard</h2>
      <div style="height: 30px;"></div>

      <!-- ========== Device Info Section ========== -->
      <h3>Device Info</h3>
      <div style="display: flex; flex-wrap: wrap; justify-content: center; gap: 30px; margin-bottom: 10px;">
        <!-- AP Info -->
        <div>
          <h4>Access Point (AP)</h4>
          <p><b>AP IP:</b> <span id='apip'>--</span></p>
          <p><b>Connected Clients:</b> <span id='clients'>0</span></p>
        </div>

        <!-- STA Info -->
        <div>
          <h4>Station (STA)</h4>
          <p><b>STA IP:</b> <span id='staip'>--</span></p>
          <p><b>STA RSSI:</b> <span id='rssi'>--</span> dBm</p>
        </div>

        <!-- Uptime Info -->
        <div>
          <h4>Uptime</h4>
          <p><b>Device Uptime:</b> <span id='uptime'>--:--:--</span></p>
          <p><b>Session Uptime: </b> <span id='session'>--:--:--</span></p>
        </div>
      </div>
      <div style="height: 30px;"></div>        

      <!-- ========== LED & Switch Section ========== -->
      <h3>Output Control</h3>
      <div style="display: flex; justify-content: center; gap: 40px; flex-wrap: wrap; margin-bottom: 20px;">
        <div class="output-controls">
          <div class="output-control">
            <span class="label">LED</span>
            <span class="lamp" id="led1status"></span>
            <label class="switch">
              <input type="checkbox" id="led1toggle" onchange="toggleLED(1)">
              <span class="slider"></span>
            </label>
          </div>

          <div class="output-control">
            <span class="label">Relay 1</span>
            <span class="lamp" id="led2status"></span>
            <label class="switch">
              <input type="checkbox" id="led2toggle" onchange="toggleLED(2)">
              <span class="slider"></span>
            </label>
          </div>

          <div class="output-control">
            <span class="label">Relay 2</span>
            <span class="lamp" id="led3status"></span>
            <label class="switch">
              <input type="checkbox" id="led3toggle" onchange="toggleLED(3)">
              <span class="slider"></span>
            </label>
          </div>
        </div>
      </div>
      <div style="height: 30px;"></div>

      <!-- ========== Dynamic GPIO Section ========== -->
      <h3>Dynamic GPIO Control</h3>
      <div class="gpio-control">
        <input id="gpioPin" type="number" placeholder="GPIO #" />
        <select id="gpioState">
          <option value="on">ON</option>
          <option value="off">OFF</option>
        </select>
        <button class="button" onclick="sendGPIO()">Set GPIO</button>
      </div>
      <div style="height: 30px;"></div>

      <!-- ========== Temperature Section ========== -->
      <h3>Temperature Monitor</h3>
      <p><b>Temperature:</b> <span id='temp'>--</span> °C</p>
      <div id="scroll-wrapper" style="overflow-x: auto; width: 100%;">
        <div style="width: 2000px;">  
          <canvas id="tempChart" height="400"></canvas>
        </div>
      </div>
      
      <div style="margin-top: 10px; font-weight: 500; color: #444;" id="tempStatsContainer">
        <p style="margin: 0;">
          <b>📉 Min:</b> <span id="tempMin">--</span> °C &nbsp;|&nbsp;
          <b>📈 Max:</b> <span id="tempMax">--</span> °C &nbsp;|&nbsp;
          <b>➗ Avg:</b> <span id="tempAvg">--</span> °C
        </p>
      </div>

      <div style="margin-top: 10px;">
      <button class='button' onclick='toggleYScale()' id='scaleToggle'>Auto Y-Scale</button>
      </div>

      <div style="margin-top: 30px;">
        <button class='button' onclick='exportCSV()'>Export CSV</button>
        <button class='button' onclick='resetGraph()'>Reset Graph</button>
      </div>
      <div style="height: 30px;"></div>        

      <section id="otaUpdateSection">
        <h3>OTA Firmware Update</h3>
        <p><strong>Current OTA Version:</strong> <span id="currentVersion">Loading...</span></p>
        <p><strong>Last Uploaded OTA Version:</strong> <span id="lastUploadedVersion">Loading...</span></p>
        <p><strong>Last Update:</strong> <span id="lastUpdate">Loading...</span></p>

        <div style="display: flex; justify-content: center; align-items: center; gap: 14px; margin: 12px 0;">
          <!-- Styled Choose File button -->
          <label for="firmwareFile" class="button" style="margin: 0; cursor: pointer; display: inline-block; text-align: center; line-height: 40px;">
            Choose File
          </label>
          <input type="file" id="firmwareFile" style="display: none;" onchange="showSelectedFile(this)">
          <button id="uploadBtn" class="button" onclick="uploadFirmware()">Upload Firmware</button>
        </div>

        <p id="selectedFileName" style="text-align: center; font-size: 14px; color: #444;"></p>

        <!-- 🟦 Progress Bar HTML starts here -->
        <div id="otaProgressWrapper" style="width: 100%; max-width: 300px; margin: 10px auto; display: none;">
          <progress id="otaProgress" value="0" max="100" style="width: 100%; height: 20px;"></progress>
          <div id="otaProgressText">0%</div>
        </div>
        <!-- 🟦 Progress Bar HTML ends here -->

        <div id="otaStatus"></div>

        <h4 style="text-align: center;">Update History</h4>

        <div style="display: flex; justify-content: center;">
          <ul id="otaHistoryList" style="
            list-style: disc;
            text-align: left;
            padding-left: 24px;
            margin: 0;
            max-width: 320px;
            font-size: 14px;
            color: #444;
            line-height: 1.6;
          "></ul>
        </div>
      </section>
      <div style="height: 160px;"></div>
      
      <h4>Switch Firmware Version</h4>
      <div style="margin: 10px 0;">
        <select id="versionSelector" class="button" style="width: auto;"></select>
        <button id="switchBtn" class="button" onclick="switchFirmware()">Switch Version</button>
      </div>
      <p id="switchStatus" style="font-size: 14px; color: #555;"></p>
      <div style="height: 160px;"></div>

      <!-- ========== Footer ========== -->
      <hr style="border: none; height: 1px; background-color: #ccc; margin: 20px auto; width: 80%;">
      <footer>
        <p class="footer-tagline">Empowering embedded intelligence at the edge 🚀</p>

        <p class="footer-heading">Need help or want to collaborate? Reach out!</p>

        <p class="footer-links">
          📧 <a href="mailto:omkar@circuitveda.com">omkar@circuitveda.com</a> &nbsp;|&nbsp;
          🌐 <a href="https://www.circuitveda.com" target="_blank">www.circuitveda.com</a>
        </p>

        <p class="footer-copy">&copy; 2025 <strong>CircuitVeda</strong>. All rights reserved.</p>
      </footer>

      <script>
        let lastClients = -1;
        let lastSTAIP = '';
        let ws = new WebSocket('ws://' + location.hostname + ':81/');
        let tempData = [], timeLabels = [], seconds = 0, sessionSeconds = 0;
        let chart, autoScale = true;
        let countdown = 10;

        function updateTempStats() {
          if (tempData.length === 0) return;

          let min = tempData[0];
          let max = tempData[0];
          let sum = 0;

          for (let i = 0; i < tempData.length; i++) {
            if (tempData[i] < min) min = tempData[i];
            if (tempData[i] > max) max = tempData[i];
            sum += tempData[i];
          }

          let avg = sum / tempData.length;

          const minEl = document.getElementById('tempMin');
          const maxEl = document.getElementById('tempMax');
          const avgEl = document.getElementById('tempAvg');

          if (minEl && maxEl && avgEl) {
            minEl.innerText = min.toFixed(2);
            maxEl.innerText = max.toFixed(2);
            avgEl.innerText = avg.toFixed(2);
          }
        }

        ws.onopen = () => {
          sessionSeconds = 0;  
          ws.send('getStatus');
        };

        ws.onmessage = evt => {
          let d = JSON.parse(evt.data);

          if (d.led1 !== undefined) {
            document.getElementById('led1status').innerHTML = d.led1
              ? "<span class='lamp on'></span>"
              : "<span class='lamp off'></span>";
            document.getElementById('led1toggle').checked = d.led1;
          }

          if (d.led2 !== undefined) {
            document.getElementById('led2status').innerHTML = d.led2
              ? "<span class='lamp on'></span>"
              : "<span class='lamp off'></span>";
            document.getElementById('led2toggle').checked = d.led2;
          }

          if (d.temp !== undefined) {
            document.getElementById('temp').innerText = d.temp.toFixed(2);
            tempData.push(d.temp);
            timeLabels.push(seconds++);

            if (tempData.length > 200) {
              tempData.shift();
              timeLabels.shift();
            }

            if (!autoScale) {
              chart.options.scales.y.min = Math.floor(d.temp - 3);
              chart.options.scales.y.max = Math.ceil(d.temp + 3);
            } else {
              chart.options.scales.y.min = undefined;
              chart.options.scales.y.max = undefined;
            }

            if (chart){
              chart.update();
              updateTempStats();
            } 
          }
          
          if (d.uptime !== undefined) {
            let h = Math.floor(d.uptime / 3600);
            let m = Math.floor((d.uptime % 3600) / 60);
            let s = d.uptime % 60;
            document.getElementById('uptime').innerText =
              h.toString().padStart(2, '0') + ':' +
              m.toString().padStart(2, '0') + ':' +
              s.toString().padStart(2, '0');
          }

          if (d.sta_ip && d.sta_ip !== lastSTAIP) {
            lastSTAIP = d.sta_ip;
            document.getElementById('staip').innerText = d.sta_ip;
          }

          if (d.rssi !== undefined) {
            document.getElementById('rssi').innerText = d.rssi;
          }

          if (d.ap_ip && document.getElementById('apip').innerText === '--') {
            document.getElementById('apip').innerText = d.ap_ip;
          }

          if (d.clients !== undefined && d.clients !== lastClients) {
            lastClients = d.clients;
            document.getElementById('clients').innerText = d.clients;
          }

          if (d.history !== undefined) {
            d.history.forEach(point => {
              tempData.push(point.temp);
              timeLabels.push(point.time);
            });

            if (d.history.length > 0) {
              seconds = d.history[d.history.length - 1].time + 1;
            }

            if (chart){
              chart.update();
              updateTempStats();
            }
          }
        };

        window.onload = () => {
          const ctx = document.getElementById('tempChart').getContext('2d');
          chart = new Chart(ctx, {
            type: 'line',
            data: {
              labels: timeLabels,
              datasets: [{
                label: 'Temperature (°C)',
                data: tempData,
                borderColor: 'rgba(75,192,192,1)',
                backgroundColor: 'rgba(75,192,192,0.2)',
                fill: true,
                tension: 0.3,
                pointRadius: 2
              }]
            },
            options: {
              animation: false,
              maintainAspectRatio: false,
              responsive: false,
              scales: {
                x: {
                  title: { display: true, text: 'Time (s)' },
                  ticks: { autoSkip: true, maxTicksLimit: 20 }
                },
                y: {
                  title: { display: true, text: 'Temperature (°C)' },
                  min: undefined,
                  max: undefined,
                  ticks: {
                    callback: v => v.toFixed(1) + ' °C'
                  }
                }
              },
              plugins: { legend: { display: false } }
            }
          });
        };

        setInterval(() => {
          sessionSeconds++;

          const h = Math.floor(sessionSeconds / 3600);
          const m = Math.floor((sessionSeconds % 3600) / 60);
          const s = sessionSeconds % 60;

          document.getElementById('session').innerText =
            h.toString().padStart(2, '0') + ':' +
            m.toString().padStart(2, '0') + ':' +
            s.toString().padStart(2, '0');
        }, 1000);

        function toggleLED(num) {
          let toggleEl = document.getElementById(`led${num}toggle`);
          let isOn = toggleEl.checked;

          let path = '';
          if (num === 1) {
            path = isOn ? '/led1on' : '/led1off';
          } else if (num === 2) {
            path = isOn ? '/led2on' : '/led2off';
          }
          fetch(path).then(() => ws.send('getStatus'));
        }

        function sendGPIO() {
          let pin = document.getElementById('gpioPin').value;
          let state = document.getElementById('gpioState').value;
          fetch(`/gpio?pin=${pin}&state=${state}`)
            .then(r => r.json()).then(j => alert(`GPIO ${j.pin} set ${j.state}`));
        }

        function resetGraph() {
          tempData = [];
          timeLabels = [];
          seconds = 0;
          chart.data.labels = timeLabels;
          chart.data.datasets[0].data = tempData;
          chart.update();
          updateTempStats();
        }

        function exportCSV() {
          let csv = 'Time(s),Temperature(°C)\n';
          for (let i = 0; i < tempData.length; i++) {
            csv += `${timeLabels[i]},${tempData[i].toFixed(2)}\n`;
          }
          const blob = new Blob([csv], { type: 'text/csv' });
          const url = URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          a.download = 'temperature_log.csv';
          a.click();
        }

        function toggleYScale() {
          autoScale = !autoScale;
          document.getElementById('scaleToggle').innerText = autoScale ? 'Auto Y-Scale' : 'Fixed Y-Scale';
          chart.options.scales.y.min = undefined;
          chart.options.scales.y.max = undefined;
          chart.update();
          updateTempStats();
        }

        function showSelectedFile(input) {
          const fileNameDisplay = document.getElementById("selectedFileName");
          const file = input.files[0];
          fileNameDisplay.innerText = file ? `Selected: ${file.name}` : "";
        }

        function uploadFirmware() {
          const fileInput = document.getElementById('firmwareFile');
          const file = fileInput.files[0];
          const status = document.getElementById('otaStatus');
          const progressBar = document.getElementById('otaProgress');
          const progressText = document.getElementById('otaProgressText');
          const progressWrapper = document.getElementById('otaProgressWrapper');

          const chooseBtn = document.querySelector("label[for='firmwareFile']");
          const uploadBtn = document.querySelector("button[onclick='uploadFirmware()']");

          if (!file) {
            alert("Please select a firmware file first.");
            return;
          }

          chooseBtn.style.pointerEvents = "none";
          chooseBtn.style.opacity = "0.5";
          uploadBtn.disabled = true;
          uploadBtn.style.opacity = "0.5";

          const xhr = new XMLHttpRequest();

          xhr.upload.onprogress = function (event) {
            if (event.lengthComputable) {
              const percent = Math.round((event.loaded / event.total) * 100);
              progressBar.value = percent;
              progressText.innerText = percent + "%";
            }
          };

          xhr.onloadstart = () => {
            progressWrapper.style.display = "block";
            progressBar.value = 0;
            progressText.innerText = "0%";
            status.innerText = "📤 Uploading firmware...";
          };

          xhr.onload = () => {
            if (xhr.status === 200) {
              let countdown = 10;
              status.innerText = `✅ Update successful. Rebooting in ${countdown} seconds...`;

              const countdownInterval = setInterval(() => {
                countdown--;
                if (countdown > 0) {
                  status.innerText = `✅ Update successful. Rebooting in ${countdown} seconds...`;
                } else {
                  clearInterval(countdownInterval);
                  status.innerText = "🔁 Checking if device is back...";

                  const tryReconnect = setInterval(() => {
                    fetch("/", { method: "HEAD", cache: "no-cache" })
                      .then(() => {
                        clearInterval(tryReconnect);
                        status.innerText = "✅ Device is back. Reloading...";
                        location.reload();
                      })
                      .catch(() => {
                        status.innerText = "⏳ Waiting for device to come back online...";
                      });
                  }, 2000);
                }
              }, 1000);
            } else {
              status.innerText = "❌ Firmware upload failed.";
              chooseBtn.style.pointerEvents = "auto";
              chooseBtn.style.opacity = "1";
              uploadBtn.disabled = false;
              uploadBtn.style.opacity = "1";
            }
          };

          xhr.onerror = () => {
            status.innerText = "❌ Network error during upload.";
            chooseBtn.style.pointerEvents = "auto";
            chooseBtn.style.opacity = "1";
            uploadBtn.disabled = false;
            uploadBtn.style.opacity = "1";
          };

          xhr.open("POST", "/update", true);
          const formData = new FormData();
          formData.append("update", file);
          xhr.send(formData);
        }

        async function loadOtaInfo() {
          try {
            const version = await fetch('/current_version').then(r => r.text());
            const lastUploaded = await fetch('/ota_version').then(r => r.text());
            const updated = await fetch('/ota_time').then(r => r.text());
            const history = await fetch('/ota_history').then(r => r.json());

            document.getElementById("currentVersion").innerText = version;
            document.getElementById("lastUploadedVersion").innerText = lastUploaded;
            document.getElementById("lastUpdate").innerText = updated;

            const list = document.getElementById("otaHistoryList");
            list.innerHTML = "";
            history.forEach(entry => {
              const li = document.createElement("li");
              li.textContent = entry;
              list.appendChild(li);
            });
          } catch (err) {
            console.error("Error loading OTA info:", err);
          }
        }
        
        async function loadVersionsDropdown() {
          const versions = await fetch('/ota_versions').then(r => r.json());
          const dropdown = document.getElementById('versionSelector');
          dropdown.innerHTML = '';

          versions.forEach(v => {
            const opt = document.createElement('option');
            opt.value = v.partition;
            opt.text = v.label;
            dropdown.appendChild(opt);
          });
        }

        async function switchFirmware() {
          const firmwareInput = document.getElementById("firmwareFile");
          const switchBtn = document.getElementById("switchBtn");
          const status = document.getElementById("switchStatus");

          const partition = sel.value;
          const versionLabel = sel.options[sel.selectedIndex].text;

          if (!partition) return;

          firmwareInput.disabled = true;
          switchBtn.disabled = true;

          status.innerText = `🔄 Switching to ${versionLabel}...`;

          try {
            const res = await fetch(`/switch_partition?target=${partition}`);
            const msg = await res.text();

            if (res.ok) {
              status.innerText = `${msg} 🔁 Rebooting...`;

              let count = 5;
              const countdownText = document.createElement("div");
              countdownText.id = "rebootCountdown";
              countdownText.style.marginTop = "10px";
              status.appendChild(countdownText);

              const timer = setInterval(() => {
                if (count > 0) {
                  countdownText.innerText = `🔁 Reloading in ${count--}s...`;
                } else {
                  clearInterval(timer);
                  location.reload();
                }
              }, 1000);
            } else {
              status.innerText = `❌ Switch failed: ${msg}`;
              firmwareInput.disabled = false;
              switchBtn.disabled = false;
            }
          } catch (err) {
            status.innerText = `❌ Error switching: ${err}`;
            firmwareInput.disabled = false;
            switchBtn.disabled = false;
          }
        }

        window.addEventListener('load', loadOtaInfo);
        window.addEventListener("load", loadVersionsDropdown);
      </script>
    </body>
    </html>
    )rawliteral";
    return html;
  }
  