// The "hardware" of the emulator: what the mocked Arduino libraries read and
// write. The window (main.cpp) shows it and lets you move the inputs.
#ifndef EMU_EMUHW_H
#define EMU_EMUHW_H

#include <stdint.h>
#include <string>

#define EMU_PINS 48

struct EmuServo {
  bool attached = false;
  bool everUsed = false;
  int angle = 90;        // last angle written by the sketch
  uint32_t writes = 0;
};

struct EmuHw {
  // simulated clock, advanced by the window
  double clockMs = 0.0;

  // SSD1306 panel memory, page layout (8 pages x 128 bytes)
  uint8_t panel[1024] = {0};
  uint8_t contrast = 255;
  uint32_t panelUpdates = 0;
  uint32_t i2cBytes = 0;     // total bytes sent to the display
  uint32_t i2cHz = 400000;

  // pins
  int digitalIn[EMU_PINS] = {0};
  int digitalOut[EMU_PINS] = {0};
  int analogIn[EMU_PINS] = {0};

  EmuServo servo[EMU_PINS];

  // DHT sensor
  float tempC = 24.0f;
  float humidity = 50.0f;
  bool dhtOk = true;

  // buzzer (LEDC tone), 0 = silent
  int buzzerHz = 0;

  // characters typed into the "serial monitor"
  std::string serialIn;
  // last lines printed by the sketch
  std::string serialOut;
};

extern EmuHw emuHw;

#endif
