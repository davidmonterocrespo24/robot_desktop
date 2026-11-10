#include "FaceRenderer.h"
#include "../core/MathUtil.h"

namespace {

// Superellipse cut by two eyelid curves, scanned row by row.
void drawShapeEye(Canvas &c, float cx, float cy, const EyeParams &p, float inner, float grow, uint8_t color) {
  float w = p.w < 2.0f ? 2.0f : p.w;
  float h = p.h < 2.0f ? 2.0f : p.h;
  float hw = w * 0.5f, hh = h * 0.5f;
  float nT = p.nTop < 1.2f ? 1.2f : p.nTop;
  float nB = p.nBot < 1.2f ? 1.2f : p.nBot;

  // Eyelids never close completely: a closed eye is drawn by squashing `h`,
  // so at least a sliver of 1.5 px is always left between the two lids.
  float lidT = clampf(p.lidT, 0.0f, 0.95f);
  float lidB = clampf(p.lidB, 0.0f, 0.95f);
  float gap = h * (1.0f - lidT - lidB);
  if (gap < 1.5f) {
    float k = (h - 1.5f) / (h * (lidT + lidB));
    lidT *= k;
    lidB *= k;
  }

  float slopeT = tanf(p.lidTAng * DEG2RADF) * inner;
  float slopeB = -tanf(p.lidBAng * DEG2RADF) * inner;
  float topBase = -hh + lidT * h - grow;
  float botBase = hh - lidB * h + grow;
  float bendT = p.lidTBend * hh;
  float bendB = p.lidBBend * hh;
  bool lids = lidT > 0.001f || lidB > 0.001f || slopeT != 0.0f || slopeB != 0.0f || bendT != 0.0f || bendB != 0.0f;

  // the outline of the grown copy
  float ghw = hw + grow, ghh = hh + grow;

  int y0 = (int)floorf(cy - ghh), y1 = (int)ceilf(cy + ghh);
  for (int y = y0; y <= y1; y++) {
    float v = y + 0.5f - cy;
    float t = fabsf(v) / ghh;
    if (t >= 1.0f) continue;
    float n = v < 0.0f ? nT : nB;
    float half = ghw * powf(1.0f - powf(t, n), 1.0f / n);
    int xa = (int)ceilf(cx - half - 0.5f);
    int xb = (int)floorf(cx + half - 0.5f);
    if (xb < xa) continue;
    if (!lids) {
      c.hline(xa, xb, y, color);
      continue;
    }
    int start = -1;
    for (int x = xa; x <= xb; x++) {
      float u = x + 0.5f - cx;
      float q = u / hw;
      float k = 1.0f - q * q;
      float vt = topBase + slopeT * u + bendT * k;
      float vb = botBase + slopeB * u - bendB * k;
      bool on = (v >= vt) && (v <= vb);
      if (on && start < 0) start = x;
      if (!on && start >= 0) {
        c.hline(start, x - 1, y, color);
        start = -1;
      }
    }
    if (start >= 0) c.hline(start, xb, y, color);
  }
}

void drawPupil(Canvas &c, float cx, float cy, const EyeParams &p) {
  if (p.pupil < 0.02f) return;
  float w = p.w < 2.0f ? 2.0f : p.w, h = p.h < 2.0f ? 2.0f : p.h;
  float r = fminf(w, h) * 0.5f * p.pupil;
  float rx = r * (p.pupilW > 0.001f ? p.pupilW : 1.0f), ry = r;
  // A slit is as tall as the eye allows.
  if (p.pupilW > 0.001f && p.pupilW < 0.6f) ry = fminf(h * 0.42f, r * 1.7f);
  if (rx < 0.7f || ry < 0.7f) return;
  float roomX = w * 0.5f - rx - 1.5f, roomY = h * 0.5f - ry - 1.5f;
  if (roomX < 0.0f) roomX = 0.0f;
  if (roomY < 0.0f) roomY = 0.0f;
  float x = cx + clampf(p.px, -1.0f, 1.0f) * roomX;
  float y = cy + clampf(p.py, -1.0f, 1.0f) * roomY;
  c.fillEllipse(x, y, rx, ry, C_OFF);
  // a glint gives it life
  if (rx > 3.0f && ry > 3.0f) c.fillCircle(x - rx * 0.35f, y - ry * 0.35f, fmaxf(1.0f, rx * 0.28f), C_ON);
}

void drawCrossEye(Canvas &c, float cx, float cy, float hw, float hh, float grow, uint8_t color) {
  float th = fminf(hw, hh) * 0.45f;
  if (th < 2.0f) th = 2.0f;
  float ex = hw - th * 0.5f, ey = hh - th * 0.5f;
  if (ey < 0.5f) ey = 0.5f;
  c.thickLine(cx - ex, cy - ey, cx + ex, cy + ey, th + grow * 2.0f, color);
  c.thickLine(cx - ex, cy + ey, cx + ex, cy - ey, th + grow * 2.0f, color);
}

void drawChevronEye(Canvas &c, float cx, float cy, float hw, float hh, float inner, float grow, uint8_t color) {
  float th = fminf(hw, hh) * 0.42f;
  if (th < 2.0f) th = 2.0f;
  float ex = hw - th * 0.5f, ey = hh - th * 0.5f;
  if (ey < 0.5f) ey = 0.5f;
  // Tip points to the nose.
  float tipX = cx + inner * ex * 0.7f;
  float backX = cx - inner * ex;
  c.thickLine(backX, cy - ey, tipX, cy, th + grow * 2.0f, color);
  c.thickLine(backX, cy + ey, tipX, cy, th + grow * 2.0f, color);
}

void drawSpiralEye(Canvas &c, float cx, float cy, float hw, float hh, float time, float dir, float grow,
                   uint8_t color) {
  const float turns = 2.6f;
  const int steps = 56;
  float rot = time * 6.0f * dir;
  float px = cx, py = cy;
  for (int i = 1; i <= steps; i++) {
    float f = (float)i / steps;
    float a = f * turns * M_TAU + rot;
    float x = cx + cosf(a) * hw * f;
    float y = cy + sinf(a) * hh * f;
    c.thickLine(px, py, x, y, 2.0f + grow * 2.0f, color);
    px = x;
    py = y;
  }
}

// Rings that are born in the middle and grow until they leave the eye.
void drawRingEye(Canvas &c, float cx, float cy, float hw, float hh, float time, uint8_t color) {
  if (hh < 2.5f) {
    c.fillEllipse(cx, cy, hw, hh, color);
    return;
  }
  const float spacing = 0.34f;  // of the radius
  float shift = time * 0.55f;
  shift -= floorf(shift);
  int x0 = (int)floorf(cx - hw), x1 = (int)ceilf(cx + hw);
  int y0 = (int)floorf(cy - hh), y1 = (int)ceilf(cy + hh);
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      float u = (x + 0.5f - cx) / hw, v = (y + 0.5f - cy) / hh;
      float r = sqrtf(u * u + v * v);
      if (r > 1.0f) continue;
      float k = r / spacing - shift;
      k -= floorf(k);
      if (k < 0.5f) c.set(x, y, color);
    }
  }
}

}  // namespace

namespace FaceRenderer {

void drawHeart(Canvas &c, float cx, float cy, float rx, float ry, uint8_t color) {
  if (rx < 0.5f || ry < 0.5f) return;
  int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
  int x0 = (int)floorf(cx - rx), x1 = (int)ceilf(cx + rx);
  for (int y = y0; y <= y1; y++) {
    int start = -1;
    for (int x = x0; x <= x1 + 1; x++) {
      bool on = false;
      if (x <= x1) {
        // (X^2 + Y^2 - 1)^3 - X^2 Y^3 <= 0, with the curve fitted to the box
        float X = (x + 0.5f - cx) / rx * 1.16f;
        float Y = -(y + 0.5f - cy) / ry * 1.12f + 0.13f;
        float a = X * X + Y * Y - 1.0f;
        on = (a * a * a - X * X * Y * Y * Y) <= 0.0f;
      }
      if (on && start < 0) start = x;
      if (!on && start >= 0) {
        c.hline(start, x - 1, y, color);
        start = -1;
      }
    }
  }
}

void drawDrop(Canvas &c, float cx, float cy, float r, uint8_t color) {
  if (r < 0.6f) return;
  c.fillCircle(cx, cy, r, color);
  c.fillTriangle(cx - r * 0.92f, cy - r * 0.35f, cx + r * 0.92f, cy - r * 0.35f, cx, cy - r * 2.4f, color);
}

void drawStar4(Canvas &c, float cx, float cy, float r, uint8_t color) {
  if (r < 0.8f) {
    c.set((int)floorf(cx), (int)floorf(cy), color);
    return;
  }
  // Four thin spikes: |x|^0.5 + |y|^0.5 <= r^0.5
  int ir = (int)ceilf(r);
  float k = sqrtf(r);
  for (int j = -ir; j <= ir; j++) {
    for (int i = -ir; i <= ir; i++) {
      if (sqrtf(fabsf((float)i)) + sqrtf(fabsf((float)j)) <= k + 0.05f) {
        c.set((int)floorf(cx) + i, (int)floorf(cy) + j, color);
      }
    }
  }
}

void drawStar5(Canvas &c, float cx, float cy, float rx, float ry, float rot, uint8_t color) {
  if (rx < 1.0f || ry < 1.0f) return;
  const float inner = 0.46f;
  float px[10], py[10];
  for (int i = 0; i < 10; i++) {
    float a = rot - M_TAU * 0.25f + i * (M_TAU / 10.0f);
    float k = (i & 1) ? inner : 1.0f;
    px[i] = cx + cosf(a) * rx * k;
    py[i] = cy + sinf(a) * ry * k + ry * 0.06f;
  }
  for (int i = 0; i < 10; i++) {
    int j = (i + 1) % 10;
    c.fillTriangle(cx, cy + ry * 0.06f, px[i], py[i], px[j], py[j], color);
  }
}

void drawBall(Canvas &c, float cx, float cy, float rx, float ry) {
  c.fillEllipse(cx, cy, rx + 1.6f, ry + 1.6f, C_OFF);
  c.fillEllipse(cx, cy, rx, ry, C_ON);
}

void drawEye(Canvas &c, float cx, float cy, const EyeParams &p, uint8_t kind, bool left, float time, float grow,
             uint8_t color) {
  float inner = left ? 1.0f : -1.0f;
  float hw = (p.w < 2.0f ? 2.0f : p.w) * 0.5f;
  float hh = (p.h < 2.0f ? 2.0f : p.h) * 0.5f;
  switch (kind) {
    case EK_HEART:
      drawHeart(c, cx, cy, hw + grow, hh + grow, color);
      break;
    case EK_CROSS:
      drawCrossEye(c, cx, cy, hw, hh, grow, color);
      break;
    case EK_CHEVRON:
      drawChevronEye(c, cx, cy, hw, hh, inner, grow, color);
      break;
    case EK_SPIRAL:
      drawSpiralEye(c, cx, cy, hw, hh, time, inner, grow, color);
      break;
    case EK_STAR:
      drawStar5(c, cx, cy, hw + grow, hh + grow, sinf(time * 2.4f) * 0.22f * inner, color);
      break;
    case EK_RING:
      if (grow > 0.0f) c.fillEllipse(cx, cy, hw + grow, hh + grow, color);
      else drawRingEye(c, cx, cy, hw, hh, time, color);
      break;
    default:
      drawShapeEye(c, cx, cy, p, inner, grow, color);
      if (grow == 0.0f && color == C_ON) drawPupil(c, cx, cy, p);
      break;
  }
}

void drawMouth(Canvas &c, float cx, float cy, const MouthParams &m) {
  if (m.w < 2.0f) return;
  float hw = m.w * 0.5f;
  float open = clamp01(m.open);
  float gapMax = m.w * 0.42f;
  int x0 = (int)ceilf(cx - hw - 0.5f), x1 = (int)floorf(cx + hw - 0.5f);

  // Columns first, so the outline can be closed from both sides.
  int top[64], bot[64];
  int n = x1 - x0 + 1;
  if (n <= 0) return;
  if (n > 64) n = 64;
  for (int i = 0; i < n; i++) {
    float q = (x0 + i + 0.5f - cx) / hw;
    float k = 1.0f - q * q;
    if (k < 0.0f) k = 0.0f;
    float yc = cy + m.curve * k - m.curve * 0.5f - m.tilt * q * 0.5f;
    // Closed mouth: a 2 px stroke. Open mouth: a hollow lens that grows from
    // the stroke (an outline reads better than a third white blob).
    float half = 1.0f + open * gapMax * sqrtf(k);
    top[i] = (int)floorf(yc - half + 0.5f);
    bot[i] = (int)floorf(yc + half - 0.5f);
    if (bot[i] < top[i]) bot[i] = top[i];
  }
  for (int i = 0; i < n; i++) {
    int x = x0 + i;
    if (bot[i] - top[i] < 5 || i == 0 || i == n - 1) {
      c.vline(x, top[i], bot[i], C_ON);
      continue;
    }
    // Outline, joined with the neighbours so steep parts have no holes.
    int ta = top[i] + 1, tb = bot[i] - 1;
    if (i > 0 && top[i - 1] > ta) ta = top[i - 1];
    if (i + 1 < n && top[i + 1] > ta) ta = top[i + 1];
    if (i > 0 && bot[i - 1] < tb) tb = bot[i - 1];
    if (i + 1 < n && bot[i + 1] < tb) tb = bot[i + 1];
    if (ta >= tb) {
      c.vline(x, top[i], bot[i], C_ON);
    } else {
      c.vline(x, top[i], ta, C_ON);
      c.vline(x, tb, bot[i], C_ON);
    }
  }
}

}  // namespace FaceRenderer
