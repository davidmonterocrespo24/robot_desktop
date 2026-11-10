// 1-bit 128x64 framebuffer with the few primitives the face engine needs.
// Pure C++ (no Arduino dependency) so the exact same pixels are produced on
// the ESP32 and in the Windows emulator.
#ifndef CORE_CANVAS_H
#define CORE_CANVAS_H

#include <stdint.h>

#define CANVAS_W 128
#define CANVAS_H 64
#define CANVAS_STRIDE (CANVAS_W / 8)
#define CANVAS_BYTES (CANVAS_STRIDE * CANVAS_H)

// Draw colors
enum : uint8_t {
  C_OFF = 0,  // clear pixel
  C_ON = 1,   // light pixel
  C_XOR = 2   // invert pixel
};

class Canvas {
 public:
  // Row-major, LSB-first inside each byte (same layout as XBM).
  uint8_t buf[CANVAS_BYTES];

  void clear();
  void fill();

  inline bool get(int x, int y) const {
    if ((unsigned)x >= CANVAS_W || (unsigned)y >= CANVAS_H) return false;
    return (buf[y * CANVAS_STRIDE + (x >> 3)] >> (x & 7)) & 1;
  }

  inline void set(int x, int y, uint8_t c = C_ON) {
    if ((unsigned)x >= CANVAS_W || (unsigned)y >= CANVAS_H) return;
    uint8_t &b = buf[y * CANVAS_STRIDE + (x >> 3)];
    uint8_t m = (uint8_t)(1u << (x & 7));
    if (c == C_ON) b |= m;
    else if (c == C_OFF) b &= (uint8_t)~m;
    else b ^= m;
  }

  // Horizontal span, x0..x1 inclusive.
  void hline(int x0, int x1, int y, uint8_t c = C_ON);
  void vline(int x, int y0, int y1, uint8_t c = C_ON);
  void fillRect(int x, int y, int w, int h, uint8_t c = C_ON);
  void rect(int x, int y, int w, int h, uint8_t c = C_ON);

  void line(int x0, int y0, int x1, int y1, uint8_t c = C_ON);
  // Line with round caps; thickness in pixels (sub-pixel endpoints allowed).
  void thickLine(float x0, float y0, float x1, float y1, float thickness, uint8_t c = C_ON);

  void fillCircle(float cx, float cy, float r, uint8_t c = C_ON);
  void fillEllipse(float cx, float cy, float rx, float ry, uint8_t c = C_ON);
  void ring(float cx, float cy, float r, float thickness, uint8_t c = C_ON);
  void fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint8_t c = C_ON);

  // 5x7 font, integer scale. Returns the x position after the text.
  int text(int x, int y, const char *s, int scale = 1, uint8_t c = C_ON);
  static int textWidth(const char *s, int scale = 1);

  // Copy to a SSD1306 page buffer (u8g2 full-buffer layout: 8 pages of 128
  // bytes, each byte is a vertical strip of 8 pixels, LSB on top).
  void toPages(uint8_t *dst) const;
};

#endif
