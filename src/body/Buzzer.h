// Robot voice: short chirps made of frequency sweeps, played without
// blocking the animation.
#ifndef BODY_BUZZER_H
#define BODY_BUZZER_H

#include <Arduino.h>

#include "../face/FaceTypes.h"

class Buzzer {
 public:
  // One piece of a chirp: a glide between two frequencies (0 = silence).
  struct Sweep {
    uint16_t f0, f1;  // Hz
    uint16_t ms;
  };

  void begin();
  void play(uint8_t chirp, uint32_t nowMs);
  void stop();
  void update(uint32_t nowMs);
  void setMuted(bool m);
  bool muted() const { return muted_; }
  bool playing() const { return seq_ != nullptr; }

 private:
  void tone_(uint16_t hz);

  const Sweep *seq_ = nullptr;
  uint8_t count_ = 0, index_ = 0;
  uint32_t segStart_ = 0;
  uint16_t lastHz_ = 0;
  bool muted_ = false;
  bool ready_ = false;

  static const Sweep *sequence(uint8_t chirp, uint8_t &count);
};

#endif
