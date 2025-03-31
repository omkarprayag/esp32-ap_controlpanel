#include "gpio_control.h"

const uint8_t LED1pin = 2;
const uint8_t LED2pin = 5;
bool LED1status = LOW;
bool LED2status = LOW;
Preferences prefs;

void initGPIO() {
  pinMode(LED1pin, OUTPUT);
  pinMode(LED2pin, OUTPUT);
}

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
