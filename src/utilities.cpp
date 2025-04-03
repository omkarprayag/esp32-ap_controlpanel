#include <Arduino.h>
#include "utilities.h"

unsigned long getUptimeMillis(unsigned long bootMillis) {
    return millis() - bootMillis;
  }