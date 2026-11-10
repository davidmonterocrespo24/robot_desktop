#include "Canvas.h"
#include "Font5x7.h"
#include <math.h>
#include <string.h>

void Canvas::clear() { memset(buf, 0x00, sizeof(buf)); }
void Canvas::fill() { memset(buf, 0xFF, sizeof(buf)); }

void Canvas::hline(int x0, int x1, int y, uint8_t c) {
  if ((unsigned)y >= CANVAS_H) return;
  if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
  if (x1 < 0 || x0 >= CANVAS_W) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= CANVAS_W) x1 = CANVAS_W - 1;

  uint8_t *row = buf + y * CANVAS_STRIDE;
  int b0 = x0 >> 3, b1 = x1 >> 3;
  uint8_t m0 = (uint8_t)(0xFF << (x0 & 7));
  uint8_t m1 = (uint8_t)(0xFF >> (7 - (x1 & 7)));
  for (int b = b0; b <= b1; b++) {
    uint8_t m = 0xFF;
    if (b == b0) m &= m0;
    if (b == b1) m &= m1;
    if (c == C_ON) row[b] |= m;
    else if (c == C_OFF) row[b] &= (uint8_t)~m;
    else row[b] ^= m;
  }
}

void Canvas::vline(int x, int y0, int y1, uint8_t c) {
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
  for (int y = y0; y <= y1; y++) set(x, y, c);
}

void Canvas::fillRect(int x, int y, int w, int h, uint8_t c) {
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++) hline(x, x + w - 1, y + j, c);
}

void Canvas::rect(int x, int y, int w, int h, uint8_t c) {
  if (w <= 0 || h <= 0) return;
  hline(x, x + w - 1, y, c);
  hline(x, x + w - 1, y + h - 1, c);
  vline(x, y + 1, y + h - 2, c);
  vline(x + w - 1, y + 1, y + h - 2, c);
}

void Canvas::line(int x0, int y0, int x1, int y1, uint8_t c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    set(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Distance-to-segment test per pixel inside the bounding box. The shapes we
// draw with this are small (decorations), so the brute force scan is cheap.
void Canvas::thickLine(float x0, float y0, float x1, float y1, float thickness, uint8_t c) {
  float r = thickness * 0.5f;
  if (r < 0.5f) r = 0.5f;
  float minx = (x0 < x1 ? x0 : x1) - r, maxx = (x0 > x1 ? x0 : x1) + r;
  float miny = (y0 < y1 ? y0 : y1) - r, maxy = (y0 > y1 ? y0 : y1) + r;
  int ix0 = (int)floorf(minx), ix1 = (int)ceilf(maxx);
  int iy0 = (int)floorf(miny), iy1 = (int)ceilf(maxy);
  if (ix0 < 0) ix0 = 0;
  if (iy0 < 0) iy0 = 0;
  if (ix1 >= CANVAS_W) ix1 = CANVAS_W - 1;
  if (iy1 >= CANVAS_H) iy1 = CANVAS_H - 1;

  float dx = x1 - x0, dy = y1 - y0;
  float len2 = dx * dx + dy * dy;
  float r2 = r * r;
  for (int y = iy0; y <= iy1; y++) {
    for (int x = ix0; x <= ix1; x++) {
      float px = x + 0.5f - x0, py = y + 0.5f - y0;
      float t = len2 > 0.0f ? (px * dx + py * dy) / len2 : 0.0f;
      if (t < 0.0f) t = 0.0f;
      if (t > 1.0f) t = 1.0f;
      float ex = px - t * dx, ey = py - t * dy;
      if (ex * ex + ey * ey <= r2) set(x, y, c);
    }
  }
}

void Canvas::fillEllipse(float cx, float cy, float rx, float ry, uint8_t c) {
  if (rx <= 0.0f || ry <= 0.0f) return;
  int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
  for (int y = y0; y <= y1; y++) {
    float v = (y + 0.5f - cy) / ry;
    float k = 1.0f - v * v;
    if (k <= 0.0f) continue;
    float half = rx * sqrtf(k);
    int xa = (int)ceilf(cx - half - 0.5f);
    int xb = (int)floorf(cx + half - 0.5f);
    if (xb >= xa) hline(xa, xb, y, c);
  }
}

void Canvas::fillCircle(float cx, float cy, float r, uint8_t c) { fillEllipse(cx, cy, r, r, c); }

void Canvas::ring(float cx, float cy, float r, float thickness, uint8_t c) {
  float ro = r + thickness * 0.5f, ri = r - thickness * 0.5f;
  if (ri < 0.0f) ri = 0.0f;
  int y0 = (int)floorf(cy - ro), y1 = (int)ceilf(cy + ro);
  int x0 = (int)floorf(cx - ro), x1 = (int)ceilf(cx + ro);
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float d2 = dx * dx + dy * dy;
      if (d2 <= ro * ro && d2 >= ri * ri) set(x, y, c);
    }
  }
}

void Canvas::fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint8_t c) {
  float minx = fminf(x0, fminf(x1, x2)), maxx = fmaxf(x0, fmaxf(x1, x2));
  float miny = fminf(y0, fminf(y1, y2)), maxy = fmaxf(y0, fmaxf(y1, y2));
  float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  if (area == 0.0f) return;
  for (int y = (int)floorf(miny); y <= (int)ceilf(maxy); y++) {
    for (int x = (int)floorf(minx); x <= (int)ceilf(maxx); x++) {
      float px = x + 0.5f, py = y + 0.5f;
      float w0 = (x1 - x0) * (py - y0) - (px - x0) * (y1 - y0);
      float w1 = (x2 - x1) * (py - y1) - (px - x1) * (y2 - y1);
      float w2 = (x0 - x2) * (py - y2) - (px - x2) * (y0 - y2);
      bool in = area > 0.0f ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
      if (in) set(x, y, c);
    }
  }
}

int Canvas::text(int x, int y, const char *s, int scale, uint8_t c) {
  if (scale < 1) scale = 1;
  for (; *s; s++) {
    unsigned char ch = (unsigned char)*s;
    if (ch < 0x20 || ch > 0x7E) ch = '?';
    const uint8_t *g = FONT5X7[ch - 0x20];
    for (int col = 0; col < 5; col++) {
      uint8_t bits = g[col];
      for (int row = 0; row < 7; row++) {
        if (bits & (1 << row)) {
          if (scale == 1) set(x + col, y + row, c);
          else fillRect(x + col * scale, y + row * scale, scale, scale, c);
        }
      }
    }
    x += 6 * scale;
  }
  return x;
}

int Canvas::textWidth(const char *s, int scale) {
  int n = (int)strlen(s);
  return n > 0 ? (n * 6 - 1) * scale : 0;
}

void Canvas::toPages(uint8_t *dst) const {
  memset(dst, 0, CANVAS_BYTES);
  for (int y = 0; y < CANVAS_H; y++) {
    const uint8_t *row = buf + y * CANVAS_STRIDE;
    uint8_t *page = dst + (y >> 3) * CANVAS_W;
    uint8_t bit = (uint8_t)(1u << (y & 7));
    for (int xb = 0; xb < CANVAS_STRIDE; xb++) {
      uint8_t v = row[xb];
      if (!v) continue;
      for (int i = 0; i < 8; i++) {
        if (v & (1u << i)) page[xb * 8 + i] |= bit;
      }
    }
  }
}
