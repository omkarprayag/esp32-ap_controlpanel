#include <Arduino.h>
#include "web_server.h"
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "gpio_control.h"
#include "temperature.h"
#include "utilities.h"

WebServer server(80);
WebSocketsServer webSocket(81);

static int lastRSSI = 0;
unsigned long lastPush = 0;
const int rssiThreshold = 5;

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
      </script>
    </body>
    </html>
    )rawliteral";
    return html;
  }
  