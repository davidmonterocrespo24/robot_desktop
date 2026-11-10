// Behaviour: turns what the sensors feel into moods and expressions.
//
// Two slow feelings drive the mood:
//   social  - grows when people move around, fades in minutes
//   annoy   - grows with loud noises, fades in seconds
// Events (somebody arrives, a bang, lights off...) trigger short reactions;
// between events the robot idles around the hub of its mood in the graph.
//
// The state machine is drawn in doc/maquina-de-estados.html, and live in the
// emulator (key Q).
#ifndef BRAIN_BRAIN_H
#define BRAIN_BRAIN_H

#include "../face/FaceEngine.h"
#include "Sensors.h"

class Brain {
 public:
  // SHOW = an act or a clip is playing
  enum Activity : uint8_t { SLEEPING, WAKING, AWAKE, REACTING, DOZING, SHOW, INFO, MANUAL, ACTIVITY_COUNT };

  // Clocks of the state machine, for displays and debugging. Milliseconds.
  struct Timers {
    uint32_t stateLeft, stateTotal;  // timed states: what is left and how long it was (0 = not timed)
    uint32_t idleIn;                 // until it does something on its own
    uint32_t climateIn;              // until the next hot / cold check
    uint32_t infoIn;                 // until the next temperature screen (only counts when alone)
    uint32_t snoreIn;                // until the next snore (only counts asleep)
    uint32_t darkFor;                // dark and still for this long, 0 = there is light
  };

  void begin(FaceEngine *face, uint32_t seed);
  void update(const SensorState &s, uint32_t nowMs);

  // ---- manual control (serial console, emulator)
  void force(uint8_t expr, uint32_t holdMs);
  void forceClip(int clip);
  void forceAct(int act);
  // Speech balloon with a short text.
  void say(const char *text);
  void requestInfo(const char *why = "asked for it");
  void setAutonomous(bool on) { autonomous_ = on; }
  bool autonomous() const { return autonomous_; }

  // ---- state
  Activity activity() const { return act_; }
  // State it was in before the last change.
  Activity previous() const { return prev_; }
  const char *activityName() const { return activityName(act_); }
  static const char *activityName(uint8_t a);
  // What caused the last change of state, in a few words.
  const char *reason() const { return why_; }
  // Number of changes of state so far (to notice a new one).
  uint32_t changes() const { return changes_; }
  uint32_t stateAgeMs() const { return now_ - since_; }
  Timers timers() const;

  uint8_t mood() const { return mood_; }
  float social() const { return social_; }
  float annoy() const { return annoy_; }
  uint32_t aloneMs() const { return alone_; }
  // True while the temperature screen must be drawn instead of the face.
  bool showInfo() const;
  // Sound to play now (CH_NONE most of the time).
  uint8_t takeChirp();

 private:
  void enter(Activity a, const char *why);
  void holdFor(uint32_t ms);
  void react(uint8_t expr, uint32_t holdMs, bool urgent, bool voice, const char *why);
  void goRest(const char *why);
  uint8_t restFace() const;
  uint8_t pickIdle();
  int pickClip();
  int pickAct();
  bool playAct(const char *name, const char *why);
  void updateMood();
  void scheduleIdle();
  void startDoze(const char *why);

  FaceEngine *face_ = nullptr;
  Rng rng_;
  bool autonomous_ = true;
  bool started_ = false;
  Activity act_ = SLEEPING, prev_ = SLEEPING;
  const char *why_ = "power on";
  uint32_t since_ = 0, changes_ = 0;
  uint8_t mood_ = MOOD_NORMAL;
  uint8_t rest_ = EX_NORMAL;

  uint32_t now_ = 0, lastMs_ = 0;
  uint32_t lastSeen_ = 0, alone_ = 0;
  float social_ = 0.0f, annoy_ = 0.0f;

  uint32_t until_ = 0, hold_ = 0;  // end and length of the current reaction / step
  uint32_t idleNext_ = 0;
  uint8_t dozeStep_ = 0;
  uint32_t darkSince_ = 0;
  bool wasDark_ = false;
  uint32_t climateNext_ = 0, infoNext_ = 0;
  uint8_t waves_ = 0;
  uint32_t waveStart_ = 0;
  uint32_t snoreNext_ = 0;
  bool greeted_ = false;

  bool voice_ = false;
  uint8_t chirp_ = CH_NONE;
};

#endif
