#include "Buzzer.h"

#include "../../Config.h"

// LEDC channel 6 lives on timer 3, away from the timer the servos use.
#define BUZZER_CHANNEL 6

// {from Hz, to Hz, ms}
namespace {
typedef Buzzer::Sweep S;
const S HELLO[] = {{880, 1320, 90}, {0, 0, 40}, {1320, 1760, 90}, {0, 0, 40}, {1760, 2640, 140}};
const S HAPPY[] = {{1200, 1800, 70}, {0, 0, 30}, {1800, 2400, 70}, {0, 0, 30}, {2400, 3200, 110}};
const S SAD[] = {{900, 600, 220}, {0, 0, 60}, {600, 380, 320}};
const S ANGRY[] = {{320, 260, 120}, {0, 0, 40}, {300, 220, 120}, {0, 0, 40}, {260, 180, 220}};
const S SURPRISE[] = {{700, 2800, 130}};
const S QUESTION[] = {{900, 1100, 90}, {0, 0, 50}, {1100, 1900, 160}};
const S SLEEP[] = {{700, 500, 260}, {0, 0, 120}, {500, 330, 380}};
const S WAKE[] = {{400, 900, 160}, {0, 0, 60}, {900, 1500, 120}};
const S LOVE[] = {{1500, 2000, 100}, {2000, 1500, 100}, {1500, 2500, 180}};
const S SCARED[] = {{2600, 2200, 50}, {2200, 2700, 50}, {2700, 2100, 50}, {2100, 2800, 50}, {2800, 1500, 120}};
const S LAUGH[] = {{1600, 1900, 55}, {0, 0, 45}, {1600, 1900, 55}, {0, 0, 45},
                   {1700, 2000, 55}, {0, 0, 45}, {1800, 2200, 70}};
const S YAWN[] = {{500, 1000, 350}, {1000, 420, 600}};
const S BLIP[] = {{1500, 1900, 45}};
const S DING[] = {{1760, 1760, 90}, {0, 0, 30}, {2640, 2640, 180}};
const S POP[] = {{900, 2600, 35}, {2600, 500, 60}};
const S WHOOSH[] = {{300, 1800, 160}, {1800, 900, 120}};
const S POWER_ON[] = {{200, 600, 180}, {600, 1400, 160}, {0, 0, 40}, {1800, 2200, 120}};
const S POWER_OFF[] = {{1600, 700, 200}, {700, 150, 320}};
const S SNEEZE[] = {{2400, 3400, 60}, {3400, 300, 180}};
const S RAWR[] = {{180, 320, 140}, {320, 240, 90}, {240, 380, 110}, {380, 160, 260}};
const S TUNE[] = {{523, 523, 110}, {0, 0, 20}, {659, 659, 110}, {0, 0, 20}, {784, 784, 110}, {0, 0, 20},
                  {1047, 1047, 170}, {0, 0, 20}, {784, 784, 110}, {0, 0, 20}, {659, 659, 170}};
}  // namespace

#define PICK(a)                                \
  count = (uint8_t)(sizeof(a) / sizeof(a[0])); \
  return a

const Buzzer::Sweep *Buzzer::sequence(uint8_t chirp, uint8_t &count) {
  switch (chirp) {
    case CH_HELLO: PICK(HELLO);
    case CH_HAPPY: PICK(HAPPY);
    case CH_SAD: PICK(SAD);
    case CH_ANGRY: PICK(ANGRY);
    case CH_SURPRISE: PICK(SURPRISE);
    case CH_QUESTION: PICK(QUESTION);
    case CH_SLEEP: PICK(SLEEP);
    case CH_WAKE: PICK(WAKE);
    case CH_LOVE: PICK(LOVE);
    case CH_SCARED: PICK(SCARED);
    case CH_LAUGH: PICK(LAUGH);
    case CH_YAWN: PICK(YAWN);
    case CH_BLIP: PICK(BLIP);
    case CH_DING: PICK(DING);
    case CH_POP: PICK(POP);
    case CH_WHOOSH: PICK(WHOOSH);
    case CH_POWER_ON: PICK(POWER_ON);
    case CH_POWER_OFF: PICK(POWER_OFF);
    case CH_SNEEZE: PICK(SNEEZE);
    case CH_RAWR: PICK(RAWR);
    case CH_TUNE: PICK(TUNE);
    default:
      count = 0;
      return nullptr;
  }
}

void Buzzer::begin() {
#if PIN_BUZZER >= 0
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BUZZER, 2000, 8);
#else
  ledcSetup(BUZZER_CHANNEL, 2000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
#endif
  ready_ = true;
  tone_(0);
#endif
}

void Buzzer::tone_(uint16_t hz) {
  if (!ready_ || hz == lastHz_) return;
  lastHz_ = hz;
#if PIN_BUZZER >= 0
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(PIN_BUZZER, hz);
#else
  ledcWriteTone(BUZZER_CHANNEL, hz);
#endif
#endif
}

void Buzzer::setMuted(bool m) {
  muted_ = m;
  if (m) stop();
}

void Buzzer::play(uint8_t chirp, uint32_t nowMs) {
  if (muted_ || !ready_) return;
  uint8_t n = 0;
  const Sweep *s = sequence(chirp, n);
  if (!s) return;
  seq_ = s;
  count_ = n;
  index_ = 0;
  segStart_ = nowMs;
}

void Buzzer::stop() {
  seq_ = nullptr;
  tone_(0);
}

void Buzzer::update(uint32_t nowMs) {
  if (!seq_) return;
  while (index_ < count_ && nowMs - segStart_ >= seq_[index_].ms) {
    segStart_ += seq_[index_].ms;
    index_++;
  }
  if (index_ >= count_) {
    stop();
    return;
  }
  const Sweep &s = seq_[index_];
  if (s.f0 == 0) {
    tone_(0);
    return;
  }
  float t = (float)(nowMs - segStart_) / (float)s.ms;
  // Steps of 20 Hz are plenty, and avoid reprogramming the timer every frame.
  int hz = (int)(s.f0 + (s.f1 - (int)s.f0) * t);
  hz = (hz / 20) * 20;
  tone_((uint16_t)hz);
}
