#include <Arduino.h>
#include "temperature.h"

extern "C" uint8_t temprature_sens_read();
float currentTempC = 0.0;
static unsigned long previousMillis = 0;
const long interval = 1000;

void updateTemperature() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    uint8_t raw = temprature_sens_read();
    currentTempC = (raw - 32) / 1.8;
    Serial.printf("Temp: %.2f °C\n", currentTempC);
  }
}