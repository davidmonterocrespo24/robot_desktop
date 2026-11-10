// ESP32Servo for the emulator: the angles go to EmuHw, where the 3D robot
// picks them up.
#ifndef ESP32_Servo_h
#define ESP32_Servo_h

#include "Arduino.h"
#include "EmuHw.h"

class ESP32PWM {
 public:
  static void allocateTimer(int) {}
};

class Servo {
 public:
  void setPeriodHertz(int) {}
  int attach(int pin, int minUs = 544, int maxUs = 2400) {
    (void)minUs;
    (void)maxUs;
    if (pin < 0 || pin >= EMU_PINS) return 0;
    pin_ = pin;
    emuHw.servo[pin].attached = true;
    emuHw.servo[pin].everUsed = true;
    return 1;
  }
  void detach() {
    if (pin_ >= 0) emuHw.servo[pin_].attached = false;
    pin_ = -1;
  }
  bool attached() const { return pin_ >= 0; }
  void write(int angle) {
    if (pin_ < 0) return;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    emuHw.servo[pin_].angle = angle;
    emuHw.servo[pin_].writes++;
  }
  void writeMicroseconds(int us) { write((int)((us - 544) * 180L / (2400 - 544))); }
  int read() const { return pin_ >= 0 ? emuHw.servo[pin_].angle : 0; }

 private:
  int pin_ = -1;
};

#endif
