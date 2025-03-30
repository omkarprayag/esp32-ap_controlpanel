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
  <link href='https://cdn.jsdelivr.net/npm/uplot@1.6.28/dist/uPlot.min.css' rel='stylesheet'>
  <script src='https://cdn.jsdelivr.net/npm/uplot@1.6.28/dist/uPlot.iife.min.js'></script>
  <style>
    :root {
      --bg: #ffffff;
      --fg: #000000;
      --button-bg: #3498db;
      --button-hover: #2980b9;
    }
    body.dark {
      --bg: #121212;
      --fg: #eeeeee;
      --button-bg: #5555aa;
      --button-hover: #444499;
    }
    body {
      font-family: Arial;
      background: var(--bg);
      color: var(--fg);
      text-align: center;
      padding: 20px;
      transition: background 0.3s, color 0.3s;
    }
    .button {
      background: var(--button-bg);
      color: white;
      padding: 10px 20px;
      margin: 5px 8px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }
    .button:hover {
      background: var(--button-hover);
    }
    input, select {
      padding: 5px;
      margin: 5px;
    }
    #chart {
      margin: 25px auto 20px;
    }
    .controls {
      margin-top: 20px;
      margin-bottom: 35px;
    }
    .control-group {
      display: flex;
      justify-content: center;
      flex-wrap: wrap;
      margin-bottom: 15px;
    }
    .metrics {
      margin: 10px auto 10px;
      font-size: 0.95em;
    }
    .temp-section {
      margin-top: 20px;
      margin-bottom: 10px;
    }
    #temp {
      font-size: 1.4em;
      font-weight: bold;
    }
    .u-tooltip {
      font-size: 13px;
      background: rgba(0,0,0,0.75);
      color: white;
      padding: 6px 10px;
      border-radius: 5px;
      pointer-events: none;
    }
    @media (max-width: 600px) {
      .button {
        padding: 8px 14px;
        font-size: 0.9em;
      }
    }
  </style>
</head>
<body>
  <div style="position:absolute;top:10px;right:10px;">
    <button class='button' onclick='toggleDark()' id='darkBtn'>🌙 Dark Mode</button>
  </div>

  <h2>ESP32 Web Dashboard (uPlot)</h2>

  <p><b>LED1:</b> <span id='led1status'>)rawliteral" + String(led1stat ? "ON" : "OFF") + R"rawliteral(</span>
  <button class='button' onclick='toggleLED(1)'>Toggle</button></p>

  <p><b>LED2:</b> <span id='led2status'>)rawliteral" + String(led2stat ? "ON" : "OFF") + R"rawliteral(</span>
  <button class='button' onclick='toggleLED(2)'>Toggle</button></p>

  <div class='temp-section'>
    <p><b>Temperature:</b> <span id='temp'>--</span> <span id='unit'>°C</span></p>
  </div>

  <div id='chart' style='width:100%;max-width:600px;height:300px;margin-bottom:40px;'></div>
  
  <div class='metrics'>
    <p>Min: <span id="tmin">--</span> &nbsp;&nbsp; Max: <span id="tmax">--</span> &nbsp;&nbsp; Avg: <span id="tavg">--</span></p>
  </div>

  <div class='controls'>
    <div class='control-group'>
      <div style="display:flex; flex-direction: column; margin: 10px;">
        <button class='button' onclick='setYAuto()'>Auto Y-Scale</button>
        <button class='button' onclick='setYFixed()'>Fixed Y-Scale</button>
      </div>
      <div style="display:flex; flex-direction: column; margin: 10px;">
        <button class='button' onclick='resetZoomX()'>Reset X-Zoom</button>
        <button class='button' onclick='resetGraph()'>Reset Graph</button>
      </div>
      <div style="display:flex; flex-direction: column; margin: 10px;">
        <button class='button' onclick='toggleUnit()'>Toggle °C/°F</button>
      </div>
      <div style="display:flex; flex-direction: column; margin: 10px;">
        <button class='button' onclick='exportCSV()'>Export CSV</button>
        <button class='button' onclick='downloadPNG()'>Download PNG</button>
      </div>
    </div>
  </div>

  <h3 style="margin-top:30px;">Dynamic GPIO Control</h3>
  <input id='gpioPin' type='number' placeholder='GPIO #' />
  <select id='gpioState'>
    <option value='on'>ON</option>
    <option value='off'>OFF</option>
  </select>
  <button class='button' onclick='sendGPIO()'>Set GPIO</button>

<script>
  let ws = new WebSocket('ws://' + location.hostname + ':81/');
  let xData = [], yData = [], seconds = 0;
  let uplot;
  let fixedYRange = { min: 20, max: 60 };
  let isCelsius = true;

  function toggleDark() {
    document.body.classList.toggle("dark");
    const dark = document.body.classList.contains("dark");
    document.getElementById("darkBtn").innerText = dark ? "☀️ Light Mode" : "🌙 Dark Mode";
    localStorage.setItem("darkMode", dark ? "true" : "false");
  }

  function updateStats() {
    if (yData.length) {
      let min = Math.min(...yData);
      let max = Math.max(...yData);
      let avg = yData.reduce((a,b) => a+b, 0) / yData.length;
      document.getElementById("tmin").innerText = min.toFixed(2);
      document.getElementById("tmax").innerText = max.toFixed(2);
      document.getElementById("tavg").innerText = avg.toFixed(2);
    }
  }

  function initChart() {
  const opts = {
    width: 600,
    height: 300,
    padding: [20, 20, 20, 40],
    cursor: {
      show: true,
      drag: { x: true, y: false },
      focus: { prox: 30 }
    },
    hooks: {
      ready: [u => {
        u.over.addEventListener("wheel", e => {
          e.preventDefault();
          const factor = e.deltaY < 0 ? 0.9 : 1.1;
          const [min, max] = u.scales.x.range;
          const center = (min + max) / 2;
          const newRange = (max - min) * factor;
          u.setScale('x', { min: center - newRange/2, max: center + newRange/2 });
        }, { passive: false });
      }]
    },
    series: [
      { label: "Time (s)" },
      {
        label: "Temp",
        stroke: "orange",
        fill: "rgba(255,165,0,0.2)",
        points: { show: true },
        value: (u, v, si, i) => {
          if (i == null) return "--";
          const t = xData[i];
          const temp = yData[i].toFixed(2);
          document.getElementById("hoverTime").innerText = `Time (s): ${t}`;
          document.getElementById("hoverTemp").innerText = `Temp: ${temp}${isCelsius ? " °C" : " °F"}`;
          return `Time: ${t}s<br>Temp: ${temp}${isCelsius ? " °C" : " °F"}`;
        }
      }
    ],
    axes: [
      { label: "Time (s)" },
      { label: "Temperature" }
    ],
    scales: {
      x: { time: false },
      y: { auto: true }
    }
  };
  uplot = new uPlot(opts, [xData, yData], document.getElementById("chart"));
}

  function fetchTemp() {
    fetch('/temperature')
      .then(r => r.text())
      .then(t => {
        let c = parseFloat(t);
        if (!isNaN(c)) {
          let val = isCelsius ? c : (c * 9/5 + 32);
          document.getElementById('temp').innerText = val.toFixed(2);
          xData.push(seconds++);
          yData.push(val);
          uplot.setData([xData, yData]);
          updateStats();
        }
      });
  }

  function toggleLED(num) {
    const id = `led${num}status`;
    const isOn = document.getElementById(id).innerText === "ON";
    fetch(`/led${num}${isOn ? 'off' : 'on'}`).then(() => ws.send('getStatus'));
  }

  function sendGPIO() {
    const pin = document.getElementById("gpioPin").value;
    const state = document.getElementById("gpioState").value;
    fetch(`/gpio?pin=${pin}&state=${state}`)
      .then(r => r.json())
      .then(j => alert(`GPIO ${j.pin} set ${j.state}`));
  }

  function resetGraph() {
    xData = []; yData = []; seconds = 0;
    uplot.setData([xData, yData]);
    updateStats();
  }

  function resetZoomX() {
    uplot.setScale('x', { min: null, max: null });
  }

  function exportCSV() {
    let csv = "Time(s),Temperature(" + (isCelsius ? "°C" : "°F") + ")\n";
    for (let i = 0; i < xData.length; i++) {
      csv += `${xData[i]},${yData[i].toFixed(2)}\n`;
    }
    const blob = new Blob([csv], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "temperature_log.csv";
    a.click();
  }

  function downloadPNG() {
    const canvas = uplot.root.querySelector("canvas");
    const link = document.createElement("a");
    link.download = "chart.png";
    link.href = canvas.toDataURL("image/png");
    link.click();
  }

  function toggleUnit() {
    isCelsius = !isCelsius;
    document.getElementById("unit").innerText = isCelsius ? "°C" : "°F";
    yData = yData.map(v => isCelsius ? (v - 32) * 5/9 : (v * 9/5 + 32));
    uplot.setData([xData, yData]);
    updateStats();
  }

  function setYAuto() {
    uplot.setScale("y", { min: null, max: null });
  }

  function setYFixed() {
    uplot.setScale("y", fixedYRange);
  }

  window.onload = () => {
    if (localStorage.getItem("darkMode") === "true") {
      document.body.classList.add("dark");
      document.getElementById("darkBtn").innerText = "☀️ Light Mode";
    }
    initChart();
    setInterval(fetchTemp, 1000);
  };
</script>
</body>
</html>
)rawliteral";
  return html;
}
