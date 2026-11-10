#include "Brain.h"

#include "../../Config.h"

// While nobody is around, the temperature is shown this often (0 = never).
#ifndef INFO_EVERY_MS
#define INFO_EVERY_MS (2UL * 60 * 1000)
#endif

static inline bool due(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }
static inline uint32_t left(uint32_t now, uint32_t t) { return due(now, t) ? 0 : t - now; }

void Brain::begin(FaceEngine *face, uint32_t seed) {
  face_ = face;
  rng_.seed(seed);
  started_ = false;
  act_ = prev_ = SLEEPING;
  why_ = "power on";
  since_ = 0;
  changes_ = 0;
  hold_ = 0;
  mood_ = MOOD_NORMAL;
  social_ = annoy_ = 0.0f;
  chirp_ = CH_NONE;
  voice_ = false;
}

const char *Brain::activityName(uint8_t a) {
  static const char *const N[] = {"sleeping", "waking", "awake", "reacting", "dozing", "show", "info", "manual"};
  return a < ACTIVITY_COUNT ? N[a] : "-";
}

bool Brain::showInfo() const { return act_ == INFO && face_->curtain() >= 0.99f; }

uint8_t Brain::takeChirp() {
  uint8_t c = chirp_;
  chirp_ = CH_NONE;
  return c;
}

Brain::Timers Brain::timers() const {
  Timers t;
  bool timed = act_ == WAKING || act_ == REACTING || act_ == DOZING || act_ == INFO || act_ == MANUAL;
  t.stateLeft = timed ? left(now_, until_) : 0;
  t.stateTotal = timed ? hold_ : 0;
  t.idleIn = left(now_, idleNext_);
  t.climateIn = left(now_, climateNext_);
  t.infoIn = left(now_, infoNext_);
  t.snoreIn = left(now_, snoreNext_);
  uint32_t quiet = now_ - darkSince_;
  if (alone_ < quiet) quiet = alone_;
  t.darkFor = wasDark_ ? quiet : 0;
  return t;
}

// Every change of state goes through here.
void Brain::enter(Activity a, const char *why) {
  prev_ = act_;
  act_ = a;
  why_ = why;
  since_ = now_;
  changes_++;
}

void Brain::holdFor(uint32_t ms) {
  until_ = now_ + ms;
  hold_ = ms;
}

void Brain::scheduleIdle() { idleNext_ = now_ + (uint32_t)rng_.rangeInt((int)IDLE_MIN_MS, (int)IDLE_MAX_MS); }

void Brain::react(uint8_t expr, uint32_t holdMs, bool urgent, bool voice, const char *why) {
  face_->goTo(expr, urgent);
  enter(REACTING, why);
  holdFor(holdMs);
  if (voice) voice_ = true;
}

uint8_t Brain::restFace() const {
  switch (mood_) {
    case MOOD_HAPPY: return EX_HAPPY;
    case MOOD_SAD: return EX_SAD;
    case MOOD_ANGRY: return EX_ANGRY;
    default: return alone_ > T_BORED_MS ? (uint8_t)EX_BORED : (uint8_t)EX_NORMAL;
  }
}

void Brain::goRest(const char *why) {
  rest_ = restFace();
  face_->goTo(rest_);
  if (act_ != AWAKE) enter(AWAKE, why);
  scheduleIdle();
}

void Brain::updateMood() {
  uint8_t m = MOOD_NORMAL;
  if (annoy_ > 0.7f || (mood_ == MOOD_ANGRY && annoy_ > 0.35f)) m = MOOD_ANGRY;
  else if (alone_ > T_LONELY_MS) m = MOOD_SAD;
  else if (social_ > 0.6f || (mood_ == MOOD_HAPPY && social_ > 0.3f)) m = MOOD_HAPPY;
  mood_ = m;
}

// A face close to the resting one, inside the same mood.
uint8_t Brain::pickIdle() {
  uint8_t hub = ExpressionGraph::hub(mood_);
  uint8_t nb[20], ok[20];
  int n = ExpressionGraph::neighbours(hub, nb, 20);
  int k = 0;
  for (int i = 0; i < n; i++) {
    uint8_t e = nb[i];
    if (e == EX_OVAL || e == EX_HOT || e == EX_COLD) continue;
    if (Expressions::get(e).mood != mood_) continue;
    ok[k++] = e;
  }
  if (k == 0) return hub;
  uint8_t pick = ok[rng_.rangeInt(0, k)];
  // Sometimes wander one step further (Glee -> Laugh, Awe -> Love ...).
  if (rng_.chance(0.3f)) {
    n = ExpressionGraph::neighbours(pick, nb, 20);
    k = 0;
    for (int i = 0; i < n; i++) {
      if (nb[i] != hub && Expressions::get(nb[i]).mood == mood_) ok[k++] = nb[i];
    }
    if (k > 0) pick = ok[rng_.rangeInt(0, k)];
  }
  return pick;
}

// An act that fits the mood.
int Brain::pickAct() {
  int ok[48], k = 0;
  for (int i = 0; i < Acts::count() && k < 48; i++) {
    if (Acts::get(i).moods & IN_MOOD(mood_)) ok[k++] = i;
  }
  return k ? ok[rng_.rangeInt(0, k)] : -1;
}

bool Brain::playAct(const char *name, const char *why) {
  int i = Acts::find(name);
  if (i < 0 || !face_->playAct(i, restFace())) return false;
  enter(SHOW, why);
  voice_ = true;
  return true;
}

int Brain::pickClip() {
  int n = Clips::count();
  if (n == 0) return -1;
  int ok[64], k = 0;
  for (int i = 0; i < n && k < 64; i++) {
    uint8_t m = Clips::get(i).mood;
    if (m == mood_ || (mood_ == MOOD_HAPPY && m == MOOD_NORMAL)) ok[k++] = i;
  }
  return k ? ok[rng_.rangeInt(0, k)] : -1;
}

void Brain::startDoze(const char *why) {
  enter(DOZING, why);
  dozeStep_ = 0;
  face_->goTo(EX_SLEEPY);
  holdFor(3500);
}

// ---- manual control ---------------------------------------------------------------
void Brain::force(uint8_t expr, uint32_t holdMs) {
  if (expr >= EX_COUNT) return;
  if (act_ == INFO) face_->setCurtain(false);
  face_->stopShow();
  face_->goTo(expr, false);
  enter(MANUAL, "a face was requested");
  holdFor(holdMs);
  voice_ = true;
}

void Brain::forceClip(int clip) {
  if (act_ == INFO) face_->setCurtain(false);
  face_->stopShow();
  if (face_->playClip(clip, restFace())) enter(SHOW, "a clip was requested");
}

void Brain::forceAct(int act) {
  if (act < 0 || act >= Acts::count()) return;
  if (act_ == INFO) face_->setCurtain(false);
  face_->stopShow();
  // Asleep it keeps sleeping after the act, awake it goes back to its mood.
  bool asleep = act_ == SLEEPING && Acts::get(act).base == EX_ASLEEP;
  if (face_->playAct(act, asleep ? (uint8_t)EX_ASLEEP : restFace())) {
    enter(SHOW, "an act was requested");
    voice_ = true;
  }
}

void Brain::say(const char *text) {
  Acts::setText(text);
  forceAct(Acts::find("Say"));
}

void Brain::requestInfo(const char *why) {
  if (act_ == SLEEPING || act_ == INFO) return;
  face_->stopShow();
  face_->setCurtain(true);
  enter(INFO, why);
  holdFor(INFO_SCREEN_MS);
}

// ---------------------------------------------------------------------------------
void Brain::update(const SensorState &s, uint32_t nowMs) {
  now_ = nowMs;
  if (!started_) {
    started_ = true;
    lastMs_ = nowMs;
    lastSeen_ = nowMs;
    since_ = nowMs;
    climateNext_ = nowMs + 20000;
    infoNext_ = nowMs + INFO_EVERY_MS;
    snoreNext_ = nowMs + 20000;
    // Boot: the screen lights up like an old TV.
    face_->snapTo(EX_NORMAL);
    if (!playAct("PowerOn", "power on")) goRest("power on");
  }
  uint32_t dms = nowMs - lastMs_;
  lastMs_ = nowMs;
  float dt = (dms > 1000 ? 1000 : dms) * 0.001f;

  // ---- feelings fade with time
  social_ = fmaxf(0.0f, social_ - dt / 240.0f);
  annoy_ = fmaxf(0.0f, annoy_ - dt / 40.0f);
  if (s.motion) lastSeen_ = nowMs;
  uint32_t aloneBefore = alone_;
  alone_ = nowMs - lastSeen_;

  // ---- darkness timer
  if (s.dark && !wasDark_) darkSince_ = nowMs;
  wasDark_ = s.dark;

  // ---- hand waved over the light sensor N times -> temperature screen
  if (s.lightEdge) {
    if (waves_ == 0 || nowMs - waveStart_ > 6000) {
      waves_ = 0;
      waveStart_ = nowMs;
    }
    waves_++;
    if (waves_ >= LIGHT_WAVES_FOR_INFO) {
      waves_ = 0;
      if (s.climateValid && autonomous_) requestInfo("hand waved over the light sensor");
    }
  }

  // ---- voice: only reactions talk, idle faces stay quiet
  uint8_t c = face_->takeChirp();
  if (c != CH_NONE && (voice_ || face_->actActive())) {
    chirp_ = c;
    voice_ = false;
  }

  if (!autonomous_) {
    if (act_ == INFO && due(nowMs, until_)) {
      face_->setCurtain(false);
      enter(MANUAL, "7 s are over");
    }
    return;
  }

  switch (act_) {
    case SLEEPING: {
      if (s.sound) {
        // Woken up by a noise: startled.
        annoy_ = fminf(1.2f, annoy_ + 0.2f);
        lastSeen_ = nowMs;
        react(EX_SURPRISED, 2200, true, true, "a noise woke it up");
      } else if (s.motionEdge || s.lightEdge) {
        lastSeen_ = nowMs;
        face_->goTo(EX_SLEEPY, true);
        enter(WAKING, s.motionEdge ? "somebody arrived" : "the light is back");
        holdFor(1800);
        voice_ = false;
      } else if (due(nowMs, snoreNext_) && !face_->busy() && face_->current() == EX_ASLEEP) {
        // Still asleep: snores from time to time.
        snoreNext_ = nowMs + (uint32_t)rng_.rangeInt(25000, 70000);
        int snore = Acts::find("Snore");
        if (snore >= 0) {
          face_->playAct(snore, EX_ASLEEP);
          voice_ = true;
        }
      }
      return;
    }

    case WAKING: {
      if (!due(nowMs, until_)) return;
      updateMood();
      chirp_ = CH_WAKE;
      goRest("it is fully awake");
      return;
    }

    case DOZING: {
      if (s.motionEdge || s.sound || s.lightEdge) {
        lastSeen_ = nowMs;
        if (s.sound) react(EX_SURPRISED, 2000, true, true, "a noise");
        else goRest(s.motionEdge ? "somebody arrived" : "the light is back");
        return;
      }
      if (!due(nowMs, until_)) return;
      dozeStep_++;
      if (dozeStep_ == 1) {
        face_->goTo(EX_YAWN);
        voice_ = true;
        holdFor(2600);
      } else {
        face_->goTo(EX_ASLEEP);
        voice_ = true;
        enter(SLEEPING, "it fell asleep");
      }
      return;
    }

    case INFO: {
      if (due(nowMs, until_) || s.sound) {
        face_->setCurtain(false);
        infoNext_ = nowMs + INFO_EVERY_MS;
        goRest(s.sound ? "a noise" : "7 s are over");
      }
      return;
    }

    case MANUAL: {
      if (due(nowMs, until_) && !face_->busy()) {
        lastSeen_ = nowMs;
        if (face_->current() == EX_ASLEEP) enter(SLEEPING, "the requested face was Asleep");
        else goRest("the requested time is over");
      }
      return;
    }

    default:
      break;
  }

  // ---- AWAKE / REACTING / SHOW: events first ---------------------------------------
  if (s.sound) {
    annoy_ = fminf(1.2f, annoy_ + 0.35f);
    lastSeen_ = nowMs;
    uint8_t e = EX_SURPRISED;
    if (annoy_ >= 1.0f) e = EX_FURIOUS;
    else if (annoy_ >= 0.7f) e = EX_ANGRY;
    else if (annoy_ >= 0.4f) e = EX_SCARED;
    updateMood();
    react(e, 2600, true, true, "a noise");
    return;
  }

  if (s.motionEdge) {
    social_ = fminf(1.0f, social_ + 0.3f);
    if (act_ != SHOW && mood_ != MOOD_ANGRY) {
      if (aloneBefore > T_LONELY_MS) {
        // Somebody came back after a long time alone.
        social_ = 1.0f;
        updateMood();
        const char *why = "somebody came back after 10 min alone";
        if (!playAct(rng_.chance(0.5f) ? "Fireworks" : "Heartbeat", why)) react(EX_EXCITED, 4500, false, true, why);
        greeted_ = true;
        return;
      } else if (act_ == AWAKE && !greeted_) {
        // First person of the day
        greeted_ = true;
        Acts::setText("HOLA!");
        if (playAct("Say", "first person since it woke up")) return;
      } else if (act_ == AWAKE) {
        updateMood();
        if (mood_ == MOOD_HAPPY) {
          static const uint8_t GREET[] = {EX_GLEE, EX_WINK, EX_EXCITED, EX_PROUD, EX_LOVE};
          react(GREET[rng_.rangeInt(0, 5)], 3000, false, true, "somebody arrived and it is happy");
        } else {
          react(EX_CURIOUS, 2600, false, true, "somebody arrived");
          face_->lookAt(rng_.chance(0.5f) ? -0.8f : 0.8f, 0.15f, 1600);
        }
      }
    }
  }

  // ---- going to sleep
  // (counted from the last time somebody moved: in a dark room with people it
  // stays awake)
  uint32_t quiet = nowMs - darkSince_;
  if (alone_ < quiet) quiet = alone_;
  bool darkLong = s.dark && quiet > T_DARK_TO_SLEEP_MS;
  if ((darkLong || alone_ > T_SLEEP_MS) && act_ != SHOW && !(act_ == REACTING && !due(nowMs, until_))) {
    greeted_ = false;
    startDoze(darkLong ? "dark and still for 8 s" : "alone for 45 min");
    return;
  }

  updateMood();

  if (act_ == SHOW) {
    if (!face_->busy()) goRest("the show ended");
    return;
  }

  if (act_ == REACTING) {
    if (due(nowMs, until_) && !face_->moving()) goRest("the reaction is over");
    return;
  }

  // ---- AWAKE ------------------------------------------------------------------------
  if (restFace() != rest_) {
    goRest("mood changed");
    return;
  }

  // Body feelings
  if (s.climateValid && due(nowMs, climateNext_) && !face_->busy()) {
    climateNext_ = nowMs + 45000;
    if (s.tempC >= HOT_ABOVE_C) {
      react(EX_HOT, 5000, false, true, "it is hot");
      return;
    }
    if (s.tempC <= COLD_BELOW_C) {
      react(EX_COLD, 5000, false, true, "it is cold");
      return;
    }
  }

  // Alone: be a small weather station from time to time.
  if (INFO_EVERY_MS > 0 && s.climateValid && alone_ > T_BORED_MS && due(nowMs, infoNext_) && !face_->busy()) {
    requestInfo("alone for more than 2 min");
    return;
  }

  if (due(nowMs, idleNext_) && !face_->busy()) {
    float dice = rng_.unit();
    if (dice < ACT_CHANCE) {
      int act = pickAct();
      if (act >= 0 && face_->playAct(act, rest_)) {
        enter(SHOW, "idle: an act");
        voice_ = true;
        return;
      }
    } else if (dice < ACT_CHANCE + CLIP_CHANCE && (mood_ == MOOD_NORMAL || mood_ == MOOD_HAPPY)) {
      int clip = pickClip();
      if (clip >= 0 && face_->playClip(clip, rest_)) {
        enter(SHOW, "idle: a clip");
        return;
      }
    }
    react(pickIdle(), (uint32_t)rng_.rangeInt(2500, 5200), false, false, "idle: a face");
  }
}
