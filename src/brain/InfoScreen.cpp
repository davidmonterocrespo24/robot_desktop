#include "InfoScreen.h"

#include <stdio.h>

#include "../core/MathUtil.h"

namespace {

void sun(Canvas &c, float cx, float cy, float t) {
  c.fillCircle(cx, cy, 9.0f, C_ON);
  for (int i = 0; i < 8; i++) {
    float a = i * (M_TAU / 8.0f) + t * 0.6f;
    float r0 = 12.5f, r1 = 17.0f + sinf(t * 3.0f + i) * 1.2f;
    c.thickLine(cx + cosf(a) * r0, cy + sinf(a) * r0, cx + cosf(a) * r1, cy + sinf(a) * r1, 2.0f, C_ON);
  }
}

void cloud(Canvas &c, float cx, float cy) {
  c.fillCircle(cx - 9.0f, cy + 3.0f, 7.0f, C_ON);
  c.fillCircle(cx, cy - 3.0f, 10.0f, C_ON);
  c.fillCircle(cx + 10.0f, cy + 2.0f, 8.0f, C_ON);
  c.fillRect((int)(cx - 9.0f), (int)(cy + 3.0f), 20, 8, C_ON);
}

void rain(Canvas &c, float cx, float cy, float t) {
  for (int i = 0; i < 4; i++) {
    float ph = t * 1.6f + i * 0.27f;
    ph -= floorf(ph);
    float x = cx - 12.0f + i * 8.0f;
    float y = cy + 13.0f + ph * 12.0f;
    c.thickLine(x, y, x - 2.0f, y + 4.0f, 1.5f, C_ON);
  }
}

}  // namespace

namespace InfoScreen {

void draw(Canvas &c, float tempC, float humidity, float time) {
  c.clear();

  // ---- icon chosen by the humidity
  const float ix = 24.0f, iy = 26.0f;
  if (humidity >= 80.0f) {
    cloud(c, ix, iy - 6.0f);
    rain(c, ix, iy - 6.0f, time);
  } else if (humidity >= 60.0f) {
    sun(c, ix + 6.0f, iy - 6.0f, time);
    // The cloud hides part of the sun: erase a halo first.
    c.fillCircle(ix - 9.0f, iy + 9.0f, 9.0f, C_OFF);
    c.fillCircle(ix, iy + 3.0f, 12.0f, C_OFF);
    c.fillCircle(ix + 10.0f, iy + 8.0f, 10.0f, C_OFF);
    cloud(c, ix, iy + 6.0f);
  } else {
    sun(c, ix, iy, time);
  }

  // ---- temperature, big
  char txt[16];
  int t = (int)lroundf(tempC);
  snprintf(txt, sizeof(txt), "%d", t);
  int scale = 4;
  int w = Canvas::textWidth(txt, scale);
  int x = 58;
  c.text(x, 6, txt, scale, C_ON);
  // degree sign and unit
  c.ring(x + w + 6.0f, 9.5f, 3.0f, 2.0f, C_ON);
  c.text(x + w + 12, 6, "C", 2, C_ON);

  // ---- humidity
  snprintf(txt, sizeof(txt), "%d%%", (int)lroundf(humidity));
  c.text(72, 44, txt, 2, C_ON);
  // small drop in front of the number
  c.fillCircle(62.0f, 54.0f, 3.5f, C_ON);
  c.fillTriangle(58.8f, 52.8f, 65.2f, 52.8f, 62.0f, 45.0f, C_ON);
}

}  // namespace InfoScreen
