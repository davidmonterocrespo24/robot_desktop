// The two ears: servo motion with easing, gestures and auto release.
#ifndef BODY_EARS_H
#define BODY_EARS_H

#include <Arduino.h>
#include <ESP32Servo.h>

#include "../core/MathUtil.h"
#include "../face/FaceTypes.h"

class Ears {
 public:
  void begin(uint32_t seed);
  // `want` comes from the face engine (FaceEngine::ears()).
  void update(const EarDef &want, uint32_t nowMs);

  // Logical angles being commanded right now (0 = back, 90 = up).
  float left() const { return left_.pos; }
  float right() const { return right_.pos; }

  // Converts a logical angle to / from the servo angle of each side.
  static int toServo(bool leftEar, float logical);
  static float toLogical(bool leftEar, int servo);

 private:
  void drive(Servo &s, int pin, int angle, int &last, uint32_t &lastMove, uint32_t nowMs);

  Servo servoL_, servoR_;
  Spring left_, right_;
  Rng rng_;
  uint32_t lastMs_ = 0;
  float phase_ = 0.0f;
  int lastL_ = -1, lastR_ = -1;
  uint32_t movedL_ = 0, movedR_ = 0;
  // twitch gesture
  uint32_t twitchNext_ = 0, twitchEnd_ = 0;
  int twitchEar_ = 0;
  bool started_ = false;
};

#endif
