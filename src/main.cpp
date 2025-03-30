// ***************************************************
// Project: ESP32 Dual Mode Web Server (AP + STA)
// Description: Controls GPIOs and monitors internal temperature 
//              via a responsive web dashboard with Chart.js and WebSockets.
// Author: Omkar Prayag
// Email: omkar@circuitveda.com
// Date: 2025-03-30
// Version: 0.2.0
// License: MIT
// ***************************************************

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>

extern "C" uint8_t temprature_sens_read();

/* ========== WiFi Configuration ========== */
const char* ssid = "SmartHome";
const char* password = "12345678";
const char* sta_ssid = "Mingle";
const char* sta_password = "12345678123";
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

/* ========== Global Instances ========== */
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Preferences prefs;

/* ========== GPIO Setup ========== */
const uint8_t LED1pin = 2;
const uint8_t LED2pin = 5;
bool LED1status = LOW;
bool LED2status = LOW;

/* ========== Temperature Monitoring ========== */
float currentTempC = 0.0;
unsigned long previousMillis = 0;
const long interval = 1000;

/* ========== Function Prototypes ========== */
void handle_OnConnect();
void handle_led1on();
void handle_led1off();
void handle_led2on();
void handle_led2off();
void handle_temperature();
void handle_NotFound();
String SendHTML(uint8_t led1stat, uint8_t led2stat);
void saveStates();
void loadStates();
void handleGPIOControl();

/* ========== Setup ========== */
void setup() {
  Serial.begin(115200);
  pinMode(LED1pin, OUTPUT);
  pinMode(LED2pin, OUTPUT);

  loadStates();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);
  WiFi.begin(sta_ssid, sta_password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
  }

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
    json += "}";
    server.send(200, "application/json", json);
  });
  server.on("/gpio", handleGPIOControl);
  server.onNotFound(handle_NotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
      String msg = (char*)payload;
      if (msg == "getStatus") {
        String json = "{";
        json += "\"led1\":" + String(LED1status ? "true" : "false") + ",";
        json += "\"led2\":" + String(LED2status ? "true" : "false");
        json += "}";
        webSocket.sendTXT(num, json);
      }
    }
  });
}

/* ========== Loop ========== */
void loop() {
  server.handleClient();
  webSocket.loop();

  digitalWrite(LED1pin, LED1status);
  digitalWrite(LED2pin, LED2status);

  if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    uint8_t raw = temprature_sens_read();
    currentTempC = (raw - 32) / 1.8;
    Serial.printf("Temp: %.2f °C\n", currentTempC);
  }
}

/* ========== Route Handlers ========== */
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
  server.send(404, "text/plain", "Not found");
}

/* ========== Persistent Storage ========== */
void saveStates() {
  prefs.begin("gpio", false);
  prefs.putBool("led1", LED1status);
  prefs.putBool("led2", LED2status);
  prefs.end();
}

void loadStates() {
  prefs.begin("gpio", true);
  LED1status = prefs.getBool("led1", LOW);
  LED2status = prefs.getBool("led2", LOW);
  prefs.end();
}

/* ========== Dynamic GPIO Handler ========== */
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
    <title>ESP32 Dashboard</title>
    <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
    <style>
      body { font-family: Arial; text-align: center; padding: 20px; }
      .button { background: #3498db; color: white; padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; }
      .button:hover { background: #2980b9; }
      input, select { padding: 5px; margin: 5px; }
    </style>
  </head>
  <body>
    <h2>ESP32 Web Dashboard</h2>
    <p><b>LED1:</b> <span id='led1status'>)rawliteral";
  html += led1stat ? "ON" : "OFF";
  html += R"rawliteral(</span> <button class='button' onclick='toggleLED(1)'>Toggle</button></p>

    <p><b>LED2:</b> <span id='led2status'>)rawliteral";
  html += led2stat ? "ON" : "OFF";
  html += R"rawliteral(</span> <button class='button' onclick='toggleLED(2)'>Toggle</button></p>

    <p><b>Temperature:</b> <span id='temp'>--</span> °C</p>
    <canvas id='tempChart'></canvas>

    <div>
      <button class='button' onclick='exportCSV()'>Export CSV</button>
      <button class='button' onclick='resetGraph()'>Reset Graph</button>
      <button class='button' onclick='toggleYScale()' id='scaleToggle'>Auto Y-Scale</button>
    </div>

    <h3>Dynamic GPIO Control</h3>
    <input id='gpioPin' type='number' placeholder='GPIO #' />
    <select id='gpioState'>
      <option value='on'>ON</option>
      <option value='off'>OFF</option>
    </select>
    <button class='button' onclick='sendGPIO()'>Set GPIO</button>

    <script>
      let ws = new WebSocket('ws://' + location.hostname + ':81/');
      let tempData = [], timeLabels = [], seconds = 0;
      let chart, autoScale = true;

      ws.onopen = () => ws.send('getStatus');
      ws.onmessage = evt => {
        let d = JSON.parse(evt.data);
        if (d.led1 !== undefined) document.getElementById('led1status').innerText = d.led1 ? 'ON' : 'OFF';
        if (d.led2 !== undefined) document.getElementById('led2status').innerText = d.led2 ? 'ON' : 'OFF';
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
            responsive: true,
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
        setInterval(fetchTemp, 1000);
      };

      function fetchTemp() {
        fetch('/temperature').then(r => r.text()).then(t => {
          let temp = parseFloat(t);
          document.getElementById('temp').innerText = t;
          tempData.push(temp);
          timeLabels.push(seconds++);

          if (!autoScale) {
            chart.options.scales.y.min = Math.floor(temp - 3);
            chart.options.scales.y.max = Math.ceil(temp + 3);
          } else {
            chart.options.scales.y.min = undefined;
            chart.options.scales.y.max = undefined;
          }
          chart.update();
        });
      }

      function toggleLED(num) {
        let id = num === 1 ? 'led1status' : 'led2status';
        let url = (document.getElementById(id).innerText === 'ON') ? `/led${num}off` : `/led${num}on`;
        fetch(url).then(() => ws.send('getStatus'));
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
      }
    </script>
  </body>
  </html>
  )rawliteral";
  return html;
}
