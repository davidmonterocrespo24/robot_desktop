#include "Ears.h"

#include "../../Config.h"

void Ears::begin(uint32_t seed) {
  rng_.seed(seed);
  servoL_.setPeriodHertz(50);
  servoR_.setPeriodHertz(50);
  left_.pos = right_.pos = 10.0f;
  started_ = false;
}

int Ears::toServo(bool leftEar, float logical) {
  logical = clampf(logical, (float)EAR_MIN_ANGLE, (float)EAR_MAX_ANGLE);
  int a = leftEar ? EAR_LEFT_ZERO + EAR_LEFT_DIR * (int)lroundf(logical)
                  : EAR_RIGHT_ZERO + EAR_RIGHT_DIR * (int)lroundf(logical);
  if (a < 0) a = 0;
  if (a > 180) a = 180;
  return a;
}

float Ears::toLogical(bool leftEar, int servo) {
  return leftEar ? (float)(servo - EAR_LEFT_ZERO) * EAR_LEFT_DIR : (float)(servo - EAR_RIGHT_ZERO) * EAR_RIGHT_DIR;
}

void Ears::drive(Servo &s, int pin, int angle, int &last, uint32_t &lastMove, uint32_t nowMs) {
  if (angle != last) {
    if (!s.attached()) s.attach(pin, 500, 2400);
    s.write(angle);
    last = angle;
    lastMove = nowMs;
  } else if (EAR_DETACH_MS > 0 && s.attached() && nowMs - lastMove > EAR_DETACH_MS) {
    // A hobby servo holding still only buzzes and drains the battery.
    s.detach();
  }
}

void Ears::update(const EarDef &want, uint32_t nowMs) {
  if (!started_) {
    started_ = true;
    lastMs_ = nowMs;
    twitchNext_ = nowMs + 3000;
  }
  uint32_t dms = nowMs - lastMs_;
  lastMs_ = nowMs;
  if (dms > 100) dms = 100;
  float dt = dms * 0.001f;

  phase_ += dt * want.hz;
  if (phase_ > 1000.0f) phase_ -= 1000.0f;
  float s = sinf(M_TAU * phase_);

  float tl = want.left, tr = want.right;
  float omega = 9.0f;  // how fast the ears follow
  switch (want.gesture) {
    case EG_WIGGLE:
      tl += want.amp * s;
      tr += want.amp * s;
      omega = 16.0f;
      break;
    case EG_FLAP:
      tl += want.amp * s;
      tr -= want.amp * s;
      omega = 16.0f;
      break;
    case EG_TREMBLE:
      tl += want.amp * s;
      tr -= want.amp * s;
      omega = 40.0f;
      break;
    case EG_TWITCH:
      if ((int32_t)(nowMs - twitchNext_) >= 0) {
        twitchEar_ = rng_.rangeInt(0, 3);  // left, right or both
        twitchEnd_ = nowMs + 160;
        float every = want.hz > 0.01f ? 1000.0f / want.hz : 5000.0f;
        twitchNext_ = nowMs + (uint32_t)(every * rng_.range(0.5f, 1.6f));
      }
      if ((int32_t)(nowMs - twitchEnd_) < 0) {
        if (twitchEar_ != 1) tl += want.amp;
        if (twitchEar_ != 0) tr += want.amp;
        omega = 30.0f;
      }
      break;
    default:
      break;
  }

  left_.update(tl, omega, dt);
  right_.update(tr, omega, dt);

  drive(servoL_, PIN_SERVO_LEFT, toServo(true, left_.pos), lastL_, movedL_, nowMs);
  drive(servoR_, PIN_SERVO_RIGHT, toServo(false, right_.pos), lastR_, movedR_, nowMs);
}
