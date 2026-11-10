#include "Decor.h"
#include "../core/MathUtil.h"

namespace {

inline float fract(float v) { return v - floorf(v); }

// Pops in fast, stays, fades at the end of a 0..1 phase.
inline float lifeScale(float phase) { return smoothstepf(0.0f, 0.18f, phase) * (1.0f - smoothstepf(0.8f, 1.0f, phase)); }

void drawZ(Canvas &c, float x, float y, float s, float th) {
  c.thickLine(x, y, x + s, y, th, C_ON);
  c.thickLine(x + s, y, x, y + s, th, C_ON);
  c.thickLine(x, y + s, x + s, y + s, th, C_ON);
}

void zzz(Canvas &c, const FaceFrame &f, float k) {
  float bx = f.cx[1] + f.w[1] * 0.5f - 2.0f;
  float by = f.cy[1] - 6.0f;
  for (int i = 0; i < 3; i++) {
    float ph = fract(f.time * 0.38f + i / 3.0f);
    float s = (3.0f + ph * 7.0f) * k * lifeScale(ph);
    if (s < 2.0f) continue;
    float x = bx + ph * 22.0f + sinf(ph * 9.0f + i) * 2.0f;
    float y = by - ph * 30.0f;
    drawZ(c, x, y, s, s > 6.0f ? 2.0f : 1.2f);
  }
}

void tears(Canvas &c, const FaceFrame &f, float k) {
  for (int e = 0; e < 2; e++) {
    float side = e == 0 ? -1.0f : 1.0f;
    float top = f.cy[e] + f.h[e] * 0.5f + 1.0f;
    for (int i = 0; i < 2; i++) {
      float ph = fract(f.time * 0.95f + e * 0.37f + i * 0.5f);
      float y = top + ph * ph * (CANVAS_H + 6.0f - top);
      float x = f.cx[e] + side * f.w[e] * (0.18f + 0.12f * i);
      float r = (1.6f + 1.4f * smoothstepf(0.0f, 0.3f, ph)) * k;
      FaceRenderer::drawDrop(c, x, y, r);
    }
  }
}

void sweat(Canvas &c, const FaceFrame &f, float k) {
  float ph = fract(f.time * 0.55f);
  float x = f.cx[1] + f.w[1] * 0.5f + 9.0f;
  float y = f.cy[1] - f.h[1] * 0.5f - 2.0f + ph * 20.0f;
  float r = 3.4f * k * lifeScale(ph);
  if (x > CANVAS_W - 5.0f) x = CANVAS_W - 5.0f;
  if (y < 7.0f) y = 7.0f;
  FaceRenderer::drawDrop(c, x, y, r);
}

void anger(Canvas &c, const FaceFrame &f, float k) {
  float s = (1.0f + 0.18f * sinf(f.time * 9.0f)) * k;
  float cx = 114.0f, cy = 11.0f;
  float a = 2.0f * s, b = 8.0f * s, m = 5.0f * s;
  for (int q = 0; q < 4; q++) {
    float sx = (q & 1) ? 1.0f : -1.0f;
    float sy = (q & 2) ? 1.0f : -1.0f;
    // Each quadrant is a rounded corner pointing to the center.
    c.thickLine(cx + sx * b, cy + sy * a, cx + sx * m, cy + sy * a, 2.0f, C_ON);
    c.thickLine(cx + sx * m, cy + sy * a, cx + sx * a, cy + sy * m, 2.0f, C_ON);
    c.thickLine(cx + sx * a, cy + sy * m, cx + sx * a, cy + sy * b, 2.0f, C_ON);
  }
}

void hearts(Canvas &c, const FaceFrame &f, float k) {
  static const float baseX[4] = {8.0f, 64.0f, 120.0f, 64.0f};
  for (int i = 0; i < 4; i++) {
    float ph = fract(f.time * 0.42f + i * 0.27f);
    float x = baseX[i] + sinf(f.time * 2.2f + i * 1.9f) * 3.5f;
    float y = CANVAS_H + 4.0f - ph * (CANVAS_H + 10.0f);
    float r = (2.5f + 2.5f * sinf(ph * 3.14159f)) * k;
    if (i == 1 || i == 3) r *= 0.7f;
    FaceRenderer::drawHeart(c, x, y, r, r);
  }
}

void sparkle(Canvas &c, const FaceFrame &f, float k) {
  static const float px[6] = {10.0f, 118.0f, 64.0f, 6.0f, 122.0f, 64.0f};
  static const float py[6] = {9.0f, 12.0f, 6.0f, 50.0f, 52.0f, 57.0f};
  for (int i = 0; i < 6; i++) {
    float tw = sinf(f.time * 3.4f + i * 1.7f);
    if (tw <= 0.0f) continue;
    FaceRenderer::drawStar4(c, px[i], py[i], 5.5f * tw * k);
  }
}

void glyph(Canvas &c, const FaceFrame &f, float k, const char *s, float wob) {
  if (k < 0.35f) return;
  int scale = k > 0.8f ? 3 : 2;
  float x = 112.0f - (scale - 2) * 3 + sinf(f.time * 17.0f) * wob;
  float y = 3.0f + sinf(f.time * 3.2f) * 1.6f;
  c.text((int)x, (int)y, s, scale, C_ON);
}

void blush(Canvas &c, const FaceFrame &f, float k) {
  for (int e = 0; e < 2; e++) {
    float side = e == 0 ? -1.0f : 1.0f;
    float bx = f.cx[e] + side * f.w[e] * 0.25f;
    float by = f.cy[e] + f.h[e] * 0.5f + 5.0f;
    if (by > CANVAS_H - 5.0f) by = CANVAS_H - 5.0f;
    for (int i = -1; i <= 1; i++) {
      float x = bx + i * 5.0f;
      float len = 2.2f * k;
      c.thickLine(x + len, by - len, x - len, by + len, 1.4f, C_ON);
    }
  }
}

void note(Canvas &c, float x, float y, float s) {
  c.fillEllipse(x, y, 2.2f * s, 1.7f * s, C_ON);
  c.thickLine(x + 1.8f * s, y, x + 1.8f * s, y - 7.0f * s, 1.3f, C_ON);
  c.thickLine(x + 1.8f * s, y - 7.0f * s, x + 5.0f * s, y - 5.2f * s, 1.6f, C_ON);
}

void notes(Canvas &c, const FaceFrame &f, float k) {
  for (int i = 0; i < 3; i++) {
    float ph = fract(f.time * 0.5f + i / 3.0f);
    float side = (i & 1) ? 1.0f : -1.0f;
    float x = 64.0f + side * (57.0f + sinf(ph * 8.0f + i) * 3.0f);
    float y = 46.0f - ph * 42.0f;
    float s = k * lifeScale(ph);
    if (s > 0.35f) note(c, x, y, s);
  }
}

void stars(Canvas &c, const FaceFrame &f, float k) {
  for (int i = 0; i < 3; i++) {
    float a = f.time * 3.6f + i * (M_TAU / 3.0f);
    float x = 64.0f + cosf(a) * 46.0f;
    float y = 7.0f + sinf(a) * 4.0f;
    // Stars on the far side of the orbit are smaller: cheap depth cue.
    float r = (3.0f + 1.6f * sinf(a)) * k;
    FaceRenderer::drawStar4(c, x, y, r);
  }
}

void shiver(Canvas &c, const FaceFrame &f, float k) {
  float j = sinf(f.time * 40.0f) * 1.2f;
  for (int s = 0; s < 2; s++) {
    float x = s == 0 ? 5.0f : CANVAS_W - 6.0f;
    for (int i = 0; i < 3; i++) {
      float y = 16.0f + i * 14.0f + j * (i == 1 ? -1.0f : 1.0f);
      float d = 3.0f * k * (s == 0 ? 1.0f : -1.0f);
      c.thickLine(x - d, y - 3.0f, x + d, y, 1.3f, C_ON);
      c.thickLine(x + d, y, x - d, y + 3.0f, 1.3f, C_ON);
    }
  }
}

void steam(Canvas &c, const FaceFrame &f, float k) {
  // Two wisps on the left; the right side belongs to the sweat drop.
  for (int s = 0; s < 2; s++) {
    float bx = s == 0 ? 6.0f : 16.0f;
    float ph = fract(f.time * 0.6f + s * 0.5f);
    float top = 30.0f - ph * 26.0f;
    float len = 22.0f * k * lifeScale(ph);
    float px = 0.0f, py = 0.0f;
    for (int i = 0; i <= 12; i++) {
      float t = i / 12.0f;
      float y = top + t * len;
      float x = bx + sinf(t * 11.0f + f.time * 4.0f) * 2.0f;
      if (i > 0) c.thickLine(px, py, x, y, 1.4f, C_ON);
      px = x;
      py = y;
    }
  }
}

// Bright spots carved out of the eyes: the "watery eyes" look.
void glint(Canvas &c, const FaceFrame &f, float k) {
  for (int e = 0; e < 2; e++) {
    float r = fminf(f.w[e], f.h[e]) * 0.17f * k;
    if (r < 1.2f) continue;
    float wob = sinf(f.time * 5.0f + e) * 0.6f;
    float x = f.cx[e] - f.w[e] * 0.17f + f.gazeX * 1.5f;
    float y = f.cy[e] - f.h[e] * 0.2f + wob;
    c.fillCircle(x, y, r, C_OFF);
    c.fillCircle(x + f.w[e] * 0.33f, y + f.h[e] * 0.36f, r * 0.5f, C_OFF);
  }
}

// "..." hopping one after the other
void dots(Canvas &c, const FaceFrame &f, float k) {
  for (int i = 0; i < 3; i++) {
    float ph = fract(f.time * 0.9f - i * 0.17f);
    float hop = ph < 0.45f ? sinf(ph / 0.45f * 3.14159265f) : 0.0f;
    float x = 8.0f + i * 10.0f;
    float y = 10.0f - hop * 4.0f;
    c.fillCircle(x, y, (2.9f + hop * 0.9f) * k, C_ON);
  }
}

// ")))" on both sides: it is hearing something
void waves(Canvas &c, const FaceFrame &f, float k) {
  for (int s = 0; s < 2; s++) {
    float side = s == 0 ? -1.0f : 1.0f;
    float ox = s == 0 ? 17.0f : CANVAS_W - 17.0f;
    for (int i = 0; i < 3; i++) {
      float ph = fract(f.time * 1.1f - i * 0.22f);
      if (ph > 0.75f) continue;
      float r = 5.0f + i * 5.0f;
      float px = 0.0f, py = 0.0f;
      for (int j = 0; j <= 8; j++) {
        float a = (-0.95f + 1.9f * j / 8.0f);
        float x = ox + side * cosf(a) * r * k;
        float y = 32.0f + sinf(a) * r * k;
        if (j > 0) c.thickLine(px, py, x, y, 1.5f, C_ON);
        px = x;
        py = y;
      }
    }
  }
}

void tongue(Canvas &c, const FaceFrame &f, float k) {
  if (f.mouthW < 4.0f) return;
  float x = f.mouthX + f.mouthW * 0.18f;
  float y = f.mouthY + 2.0f;
  float rx = 3.6f * k, ry = (5.0f + sinf(f.time * 5.0f) * 0.8f) * k;
  c.fillEllipse(x, y + ry * 0.55f, rx, ry, C_ON);
  c.fillRect((int)(x - rx), (int)(y - 1.0f), (int)(rx * 2.0f + 1.0f), (int)(ry * 0.6f + 1.0f), C_ON);
  // the groove
  c.vline((int)x, (int)(y + 2.0f), (int)(y + ry * 1.1f), C_OFF);
}

// Zigzag mouth with two fangs.
void fangs(Canvas &c, const FaceFrame &f, float k) {
  float y = 52.0f + sinf(f.time * 30.0f) * 0.6f;
  float x0 = 64.0f - 18.0f * k + f.gazeX * 4.0f;
  float step = 6.0f * k;
  for (int i = 0; i < 6; i++) {
    float xa = x0 + i * step, xb = xa + step;
    float ya = (i & 1) ? y + 5.0f * k : y;
    float yb = (i & 1) ? y : y + 5.0f * k;
    c.thickLine(xa, ya, xb, yb, 1.6f, C_ON);
  }
  c.fillTriangle(x0 + step * 0.4f, y, x0 + step * 1.6f, y, x0 + step, y + 8.0f * k, C_ON);
  c.fillTriangle(x0 + step * 4.4f, y, x0 + step * 5.6f, y, x0 + step * 5.0f, y + 8.0f * k, C_ON);
}

}  // namespace

namespace Decor {

void draw(Canvas &c, uint32_t flag, const FaceFrame &f, float weight) {
  if (weight <= 0.02f) return;
  float k = weight > 1.0f ? 1.0f : weight;
  switch (flag) {
    case DC_ZZZ: zzz(c, f, k); break;
    case DC_TEARS: tears(c, f, k); break;
    case DC_SWEAT: sweat(c, f, k); break;
    case DC_ANGER: anger(c, f, k); break;
    case DC_HEARTS: hearts(c, f, k); break;
    case DC_SPARKLE: sparkle(c, f, k); break;
    case DC_QUESTION: glyph(c, f, k, "?", 0.0f); break;
    case DC_EXCLAIM: glyph(c, f, k, "!", 1.2f); break;
    case DC_BLUSH: blush(c, f, k); break;
    case DC_NOTES: notes(c, f, k); break;
    case DC_STARS: stars(c, f, k); break;
    case DC_SHIVER: shiver(c, f, k); break;
    case DC_STEAM: steam(c, f, k); break;
    case DC_GLINT: glint(c, f, k); break;
    case DC_DOTS: dots(c, f, k); break;
    case DC_WAVES: waves(c, f, k); break;
    case DC_TONGUE: tongue(c, f, k); break;
    case DC_FANGS: fangs(c, f, k); break;
    default: break;
  }
}

}  // namespace Decor
