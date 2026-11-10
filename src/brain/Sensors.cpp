#include "Sensors.h"

#include <DHT.h>

#include "../../Config.h"

static DHT dht(PIN_DHT, DHT_MODEL);

void Sensors::begin() {
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LDR, INPUT);
#if PIN_MIC >= 0
  pinMode(PIN_MIC, INPUT);
#endif
  dht.begin();
}

void Sensors::poll() {
#if PIN_MIC >= 0
  if (digitalRead(PIN_MIC) == MIC_ACTIVE_LEVEL) soundLatch_ = true;
#endif
}

void Sensors::update(uint32_t nowMs) {
  if (!started_) {
    started_ = true;
    nextLight_ = nowMs;
    nextClimate_ = nowMs + 2500;  // the DHT needs a moment after power up
  }

  // ---- motion
  bool m = digitalRead(PIN_PIR) == HIGH;
  st_.motionEdge = m && !st_.motion;
  st_.motion = m;

  // ---- sound: one event per noise, then deaf for a moment (its own buzzer
  // and servos would retrigger it)
  st_.sound = false;
  if (soundLatch_) {
    soundLatch_ = false;
    if ((int32_t)(nowMs - soundMuteUntil_) >= 0) {
      st_.sound = true;
      soundMuteUntil_ = nowMs + 700;
    }
  }

  // ---- light, 10 times per second
  st_.lightEdge = st_.darkEdge = false;
  if ((int32_t)(nowMs - nextLight_) >= 0) {
    nextLight_ = nowMs + 100;
    int raw = analogRead(PIN_LDR);
    st_.lightRaw = raw;
    if (lightFilt_ < 0.0f) lightFilt_ = (float)raw;
    lightFilt_ += ((float)raw - lightFilt_) * 0.35f;

    // The range of the sensor is learned while it runs.
    int f = (int)lightFilt_;
    if (f < rawMin_) rawMin_ = f;
    if (f > rawMax_) rawMax_ = f;
    int span = rawMax_ - rawMin_;
    if (span >= LDR_MIN_SPAN) {
      float n = (lightFilt_ - (float)rawMin_) / (float)span;
#if LDR_BRIGHT_IS_LOW
      n = 1.0f - n;
#endif
      st_.light = n;
      if (!st_.dark && n < DARK_BELOW) {
        st_.dark = true;
        st_.darkEdge = true;
      } else if (st_.dark && n > BRIGHT_ABOVE) {
        st_.dark = false;
        st_.lightEdge = true;
      }
    } else {
      // Not enough contrast seen yet to tell dark from bright.
      st_.light = 1.0f;
      st_.dark = false;
    }
  }

  // ---- climate, every 10 seconds (the reading blocks for ~25 ms)
  if ((int32_t)(nowMs - nextClimate_) >= 0) {
    nextClimate_ = nowMs + 10000;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      st_.tempC = t;
      st_.humidity = h;
      st_.climateValid = true;
    }
  }
}
