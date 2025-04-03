#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#define MAX_TEMP_POINTS 600  // adjust as RAM capacity 10min @1/Sec

void updateTemperature();
extern float currentTempC;

#endif