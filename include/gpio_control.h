#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H
#include <Arduino.h>
#include <Preferences.h>

extern bool LED1status;
extern bool LED2status;

void initGPIO();
void saveStates();
void loadStates();

#endif