// ***************************************************
// Project: ESP32 Dual Mode Web Server (AP + STA)
// Description: Controls GPIOs and monitors internal temperature 
//              via a responsive web dashboard with uPlot.js and WebSockets.
// Author: Omkar Prayag
// Email: omkar@circuitveda.com
// Date: 2025-03-30
// Version: 0.2.0
// License: MIT
// ***************************************************

#include <Arduino.h>
#include "fs_spiffs.h"
#include "web_server.h"
#include "gpio_control.h"
#include "temperature.h"
#include "wifi_setup.h"

void setup() {
  Serial.begin(115200);
  initSpiffs();
  printVersion();
  initGPIO();
  initWiFi();
  initWebServer();
  initWebSocket();
  loadStates();
}

void loop() {
  handleClients();
  updateTemperature();
  maintainWiFi();
}
