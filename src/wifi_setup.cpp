#include <WiFi.h>
#include "wifi_setup.h"
#include <ESPmDNS.h>
#include <utilities.h>

const char* ssid = "SmartHome";     // AP
const char* password = "12345678";
const char* sta_ssid = "Mingle";    // STA
const char* sta_password = "12345678123";

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000;
bool staConnected = false;
int reconnectAttempts = 0;
const int maxReconnectAttempts = 5;
long rssi;
bool mdnsStarted = false;

void initWiFi() {
  unsigned long startAttemptTime;
  bool success;

  WiFi.mode(WIFI_AP_STA);

  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.softAP(ssid, password);
  Serial.println("[AP] SoftAP started");
  Serial.print("[AP] IP address: ");
  Serial.println(WiFi.softAPIP());

  Serial.println();
  Serial.println("======= Connect to ESP32 AP =======");
  Serial.print("SSID:     "); Serial.println(ssid);
  Serial.print("Password: "); Serial.println(password);
  Serial.println("===================================");
  
  if (MDNS.begin(DEVICE_NAME)) {
    Serial.println();
    Serial.println("🚀 Smart Dashboard started successfully!");
    Serial.println("📡 To access the dashboard, open your browser and enter:");
    Serial.println("[AP] 👉 http://" + String(DEVICE_NAME) + ".local");
    Serial.println("💡 Make sure you're connected to the " + String(ssid) + " Wi-Fi network.");
    Serial.println("🔁 If the above doesn't work, try: http://" + WiFi.softAPIP().toString());
    Serial.println();
  }

  Serial.print("[STA] Connecting to WiFi: ");
  Serial.println(sta_ssid);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(sta_ssid, sta_password);

  startAttemptTime = millis();
  success = false;
  while (millis() - startAttemptTime < 10000) { 
    if (WiFi.status() == WL_CONNECTED) {
      success = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }

  if (success) {
    Serial.println("\n[STA] Connected!");
    Serial.print("[STA] IP address: ");
    Serial.println(WiFi.localIP());
    staConnected = true;

    if (!mdnsStarted && MDNS.begin(DEVICE_NAME)) {
      mdnsStarted = true;
      Serial.println();
      Serial.println("🚀 Smart Dashboard started successfully for the " + String(sta_ssid) + " network!");
      Serial.println("📡 To access the dashboard, open your browser and enter:");
      Serial.println("[STA] 👉 http://" + String(DEVICE_NAME) + ".local");
      Serial.println("💡 Make sure your PC or phone is connected to the " + String(sta_ssid) + " Wi-Fi network.");
      Serial.println("🔁 If the above doesn't work, try: http://" + WiFi.localIP().toString());
      Serial.println();
    }
  } else {
    Serial.println("\n[STA] Failed to connect");
    Serial.print("[STA] Status code: ");
    Serial.println(WiFi.status());
    staConnected = false;
  }

  rssi = WiFi.RSSI();
  Serial.println("[STA] RSSI: " + String(WiFi.RSSI()) + " dBm");
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!staConnected) {
      Serial.println("[STA] Reconnected!");
      Serial.print("[STA] IP address: ");
      Serial.println(WiFi.localIP());
      staConnected = true;
      reconnectAttempts = 0;

      if (!mdnsStarted && MDNS.begin(DEVICE_NAME)) {
        mdnsStarted = true;
        Serial.println();
        Serial.println("🚀 Smart Dashboard started successfully for the " + String(sta_ssid) + " network!");
        Serial.println("📡 To access the dashboard, open your browser and enter:");
        Serial.println("[STA] 👉 http://" + String(DEVICE_NAME) + ".local");
        Serial.println("💡 Make sure your PC or phone is connected to the " + String(sta_ssid) + " Wi-Fi network.");
        Serial.println("🔁 If the above doesn't work, try: http://" + WiFi.localIP().toString());
        Serial.println();
      }
    }
    return;
  }

  if (staConnected) {
    Serial.println("[STA] Lost connection!");
    staConnected = false;
    reconnectAttempts = 1;
    lastReconnectAttempt = millis();
  }

  if (!staConnected && reconnectAttempts > 0 && reconnectAttempts <= maxReconnectAttempts) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= reconnectInterval) {
      Serial.printf("[STA] Reconnect attempt %d/%d...\n", reconnectAttempts, maxReconnectAttempts);
      WiFi.disconnect();
      WiFi.begin(sta_ssid, sta_password);
      lastReconnectAttempt = now;
      reconnectAttempts++;
    }
  } else if (reconnectAttempts > maxReconnectAttempts) {
    Serial.println("[STA] Max reconnect attempts reached. Giving up.");
    Serial.println("[STA] Restart the device for re-connecting.");
  }
}