#include <WiFi.h>
#include <WebServer.h>

extern "C" uint8_t temprature_sens_read();

const char* ssid = "SmartHome";
const char* password = "12345678";

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

uint8_t LED1pin = 2;
bool LED1status = LOW;

uint8_t LED2pin = 5;
bool LED2status = LOW;

float currentTempC = 0.0;
unsigned long previousMillis = 0;
const long interval = 1000;

// Function prototypes
void handle_OnConnect();
void handle_led1on();
void handle_led1off();
void handle_led2on();
void handle_led2off();
void handle_temperature();
void handle_NotFound();
String SendHTML(uint8_t led1stat, uint8_t led2stat);

void setup() {
  Serial.begin(115200);
  pinMode(LED1pin, OUTPUT);
  pinMode(LED2pin, OUTPUT);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);

  server.on("/", handle_OnConnect);
  server.on("/led1on", handle_led1on);
  server.on("/led1off", handle_led1off);
  server.on("/led2on", handle_led2on);
  server.on("/led2off", handle_led2off);
  server.on("/temperature", handle_temperature); // new AJAX route
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  digitalWrite(LED1pin, LED1status ? HIGH : LOW);
  digitalWrite(LED2pin, LED2status ? HIGH : LOW);

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    uint8_t raw = temprature_sens_read();
    currentTempC = (raw - 32) / 1.8;
    Serial.print("Internal Temperature: ");
    Serial.print(currentTempC);
    Serial.println(" °C");
  }
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(LED1status, LED2status));
}

void handle_led1on() {
  LED1status = HIGH;
  server.send(200, "application/json", "{\"led\":1,\"status\":\"on\"}");
}

void handle_led1off() {
  LED1status = LOW;
  server.send(200, "application/json", "{\"led\":1,\"status\":\"off\"}");
}

void handle_led2on() {
  LED2status = HIGH;
  server.send(200, "application/json", "{\"led\":2,\"status\":\"on\"}");
}

void handle_led2off() {
  LED2status = LOW;
  server.send(200, "application/json", "{\"led\":2,\"status\":\"off\"}");
}

void handle_temperature() {
  server.send(200, "text/plain", String(currentTempC, 2));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(uint8_t led1stat, uint8_t led2stat) {
  String ptr = "<!DOCTYPE html><html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
  ptr += "<title>ESP32 Control</title>\n";

  // CSS
  ptr += "<style>html {font-family: Helvetica; text-align: center;} ";
  ptr += "body{margin-top: 30px;} h1, h3 {color: #444;} ";
  ptr += ".button {display: inline-block;width: auto;background-color: #3498db;border: none;color: white;padding: 10px 20px;text-decoration: none;font-size: 18px;margin: 10px;border-radius: 4px;cursor: pointer;} ";
  ptr += ".button:hover {background-color: #2980b9;} ";
  ptr += ".button-group {margin-top: 10px;} ";
  ptr += "p {font-size: 14px;color: #888;}</style>\n";

  // Chart.js & JS
  ptr += "<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n";
  ptr += "<script>\n";
  ptr += "let tempData = [], timeLabels = [], chart;\n";
  ptr += "let seconds = 0, autoScale = true;\n";

  ptr += "window.onload = function() {\n";
  ptr += "  const ctx = document.getElementById('tempChart').getContext('2d');\n";
  ptr += "  chart = new Chart(ctx, {\n";
  ptr += "    type: 'line',\n";
  ptr += "    data: { labels: timeLabels, datasets: [{ label: 'Temperature (°C)', data: tempData, borderColor: 'rgba(75,192,192,1)', backgroundColor: 'rgba(75,192,192,0.2)', fill: true, tension: 0.2, pointRadius: 2 }] },\n";
  ptr += "    options: { responsive: true, animation: false, scales: { x: { title: { display: true, text: 'Time (s)' } }, y: { title: { display: true, text: 'Temperature (°C)' }, beginAtZero: false, ticks: { callback: value => value.toFixed(1) + ' °C' } } }, plugins: { legend: { display: false } } }\n";
  ptr += "  });\n";
  ptr += "  setInterval(fetchTemperature, 1000);\n";
  ptr += "  setInterval(updateUptime, 1000);\n";
  ptr += "  setupLEDButton(1);\n";
  ptr += "  setupLEDButton(2);\n";
  ptr += "};\n";

  ptr += "function fetchTemperature() {\n";
  ptr += "  fetch('/temperature')\n";
  ptr += "    .then(r => r.text())\n";
  ptr += "    .then(temp => {\n";
  ptr += "      document.getElementById('tempValue').innerText = temp + ' °C';\n";
  ptr += "      if (tempData.length >= 30) { tempData.shift(); timeLabels.shift(); }\n";
  ptr += "      tempData.push(parseFloat(temp));\n";
  ptr += "      timeLabels.push(seconds++);\n";
  ptr += "      chart.options.scales.y.min = autoScale ? undefined : 55;\n";
  ptr += "      chart.options.scales.y.max = autoScale ? undefined : 60;\n";
  ptr += "      chart.update();\n";
  ptr += "    });\n";
  ptr += "}\n";

  ptr += "function updateUptime() {\n";
  ptr += "  let hrs = Math.floor(seconds / 3600);\n";
  ptr += "  let mins = Math.floor((seconds % 3600) / 60);\n";
  ptr += "  let secs = seconds % 60;\n";
  ptr += "  document.getElementById('uptime').innerText = `${hrs}h ${mins}m ${secs}s`;\n";
  ptr += "}\n";

  ptr += "function downloadCSV() {\n";
  ptr += "  let csv = 'Time(s),Temperature(°C)\\n';\n";
  ptr += "  for (let i = 0; i < tempData.length; i++) {\n";
  ptr += "    csv += timeLabels[i] + ',' + tempData[i].toFixed(2) + '\\n';\n";
  ptr += "  }\n";
  ptr += "  const blob = new Blob([csv], { type: 'text/csv' });\n";
  ptr += "  const url = URL.createObjectURL(blob);\n";
  ptr += "  const a = document.createElement('a');\n";
  ptr += "  a.href = url;\n";
  ptr += "  a.download = 'temperature_data.csv';\n";
  ptr += "  a.click();\n";
  ptr += "}\n";

  ptr += "function toggleYScale() {\n";
  ptr += "  autoScale = !autoScale;\n";
  ptr += "  document.getElementById('scaleToggle').innerText = autoScale ? 'Fixed Y-Scale' : 'Auto Y-Scale';\n";
  ptr += "}\n";

  ptr += "function resetGraph() {\n";
  ptr += "  tempData = [];\n";
  ptr += "  timeLabels = [];\n";
  ptr += "  seconds = 0;\n";
  ptr += "  chart.data.labels = timeLabels;\n";
  ptr += "  chart.data.datasets[0].data = tempData;\n";
  ptr += "  chart.update();\n";
  ptr += "}\n";

  // New robust LED control
  ptr += "function updateLEDStatus(led, isOn) {\n";
  ptr += "  document.getElementById(`led${led}status`).innerText = isOn ? 'ON' : 'OFF';\n";
  ptr += "  const btn = document.getElementById(`led${led}btn`);\n";
  ptr += "  btn.innerText = isOn ? 'Turn OFF' : 'Turn ON';\n";
  ptr += "  btn.dataset.state = isOn ? 'on' : 'off';\n";
  ptr += "}\n";

  ptr += "function setupLEDButton(led) {\n";
  ptr += "  const btn = document.getElementById(`led${led}btn`);\n";
  ptr += "  btn.addEventListener('click', () => {\n";
  ptr += "    const isCurrentlyOn = btn.dataset.state === 'on';\n";
  ptr += "    const url = led === 1 ? (isCurrentlyOn ? '/led1off' : '/led1on') : (isCurrentlyOn ? '/led2off' : '/led2on');\n";
  ptr += "    fetch(url)\n";
  ptr += "      .then(r => r.json())\n";
  ptr += "      .then(data => updateLEDStatus(data.led, data.status === 'on'));\n";
  ptr += "  });\n";
  ptr += "}\n";

  ptr += "</script>\n";

  // HTML Body
  ptr += "</head><body>\n";
  ptr += "<h1>ESP32 Web Server</h1>\n";
  ptr += "<h3>Access Point (AP) Mode</h3>\n";

  ptr += "<p><b>SSID:</b> " + String(ssid) + "</p>\n";
  ptr += "<p><b>IP Address:</b> " + WiFi.softAPIP().toString() + "</p>\n";
  ptr += "<p><b>RSSI:</b> " + String(WiFi.RSSI()) + " dBm</p>\n";
  ptr += "<p><b>Uptime:</b> <span id='uptime'>0s</span></p>\n";

  ptr += "<p>LED1 Status: <span id='led1status'>" + String(led1stat ? "ON" : "OFF") + "</span></p>\n";
  ptr += "<button id='led1btn' class='button' data-state='" + String(led1stat ? "on" : "off") + "'>" + String(led1stat ? "Turn OFF" : "Turn ON") + "</button>\n";

  ptr += "<p>LED2 Status: <span id='led2status'>" + String(led2stat ? "ON" : "OFF") + "</span></p>\n";
  ptr += "<button id='led2btn' class='button' data-state='" + String(led2stat ? "on" : "off") + "'>" + String(led2stat ? "Turn OFF" : "Turn ON") + "</button>\n";

  ptr += "<p>Current Temperature: <span id='tempValue'>" + String(currentTempC, 2) + " °C</span></p>\n";
  ptr += "<canvas id='tempChart' width='300' height='200'></canvas>\n";

  ptr += "<div class='button-group'>\n";
  ptr += "<button class='button' onclick='downloadCSV()'>Export CSV</button>\n";
  ptr += "<button class='button' id='scaleToggle' onclick='toggleYScale()'>Fixed Y-Scale</button>\n";
  ptr += "<button class='button' onclick='resetGraph()'>Reset Graph</button>\n";
  ptr += "</div>\n";

  ptr += "</body></html>\n";
  return ptr;
}
