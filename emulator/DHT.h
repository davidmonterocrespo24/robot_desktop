// DHT sensor for the emulator: returns the values set in the window.
#ifndef DHT_H
#define DHT_H

#include <math.h>
#include <stdint.h>

#include "EmuHw.h"

#define DHT11 11
#define DHT12 12
#define DHT21 21
#define DHT22 22

class DHT {
 public:
  DHT(uint8_t /*pin*/, uint8_t /*type*/, uint8_t /*count*/ = 6) {}
  void begin(uint8_t /*usec*/ = 55) {}
  float readTemperature(bool fahrenheit = false, bool /*force*/ = false) {
    if (!emuHw.dhtOk) return NAN;
    return fahrenheit ? emuHw.tempC * 1.8f + 32.0f : emuHw.tempC;
  }
  float readHumidity(bool /*force*/ = false) { return emuHw.dhtOk ? emuHw.humidity : NAN; }
};

#endif
