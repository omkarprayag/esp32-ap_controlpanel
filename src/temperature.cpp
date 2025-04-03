#include <Arduino.h>
#include "temperature.h"

extern "C" uint8_t temprature_sens_read();

float currentTempC = 0.0;
unsigned long previousMillis = 0;
const unsigned long interval = 1000;
float tempBuffer[MAX_TEMP_POINTS];
unsigned long timeBuffer[MAX_TEMP_POINTS];
size_t tempIndex = 0;
bool bufferFull = false;

void updateTemperature() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    uint8_t raw = temprature_sens_read();
    currentTempC = (raw - 32) / 1.8;
    Serial.printf("Temp: %.2f °C\n", currentTempC);

    tempBuffer[tempIndex] = currentTempC;
    timeBuffer[tempIndex] = currentMillis;

    tempIndex = (tempIndex + 1) % MAX_TEMP_POINTS;
    if (tempIndex == 0) bufferFull = true;
  }
}