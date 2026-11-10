// Reads and cleans the sensors. The rest of the code only sees events and
// normalized values, never raw pins.
#ifndef BRAIN_SENSORS_H
#define BRAIN_SENSORS_H

#include <Arduino.h>

struct SensorState {
  // motion (PIR)
  bool motion = false;      // somebody is moving right now
  bool motionEdge = false;  // ... and it just started (true for one frame)
  // light (LDR)
  int lightRaw = 0;
  float light = 1.0f;       // 0 = dark, 1 = bright
  bool dark = false;
  bool lightEdge = false;   // dark -> bright, one frame
  bool darkEdge = false;    // bright -> dark, one frame
  // sound
  bool sound = false;       // loud noise heard since the last frame
  // climate (DHT)
  bool climateValid = false;
  float tempC = 0.0f;
  float humidity = 0.0f;
};

class Sensors {
 public:
  void begin();
  // Call as often as possible: catches the short pulse of the sound sensor.
  void poll();
  // Call once per frame.
  void update(uint32_t nowMs);
  const SensorState &state() const { return st_; }

 private:
  SensorState st_;
  bool started_ = false;
  bool soundLatch_ = false;
  uint32_t soundMuteUntil_ = 0;
  uint32_t nextLight_ = 0, nextClimate_ = 0;
  float lightFilt_ = -1.0f;
  int rawMin_ = 4095, rawMax_ = 0;
};

#endif
