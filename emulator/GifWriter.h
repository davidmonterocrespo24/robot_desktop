// Minimal animated GIF writer for 1-bit frames (two colors, LZW compressed).
// Used by the emulator to record the OLED and to export the expressions that
// the engine generates with math.
#ifndef EMU_GIFWRITER_H
#define EMU_GIFWRITER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

class GifWriter {
 public:
  ~GifWriter() { close(); }

  // `scale` repeats every pixel, the OLED is tiny on a PC screen.
  bool open(const char *path, int w, int h, int scale = 4, uint32_t rgbOff = 0x05070A, uint32_t rgbOn = 0xE8F4FF) {
    close();
    f_ = fopen(path, "wb");
    if (!f_) return false;
    w_ = w;
    h_ = h;
    scale_ = scale < 1 ? 1 : scale;
    int W = w_ * scale_, H = h_ * scale_;
    fwrite("GIF89a", 1, 6, f_);
    put16(W);
    put16(H);
    fputc(0xF0, f_);  // global table, 2 colors
    fputc(0, f_);
    fputc(0, f_);
    putRgb(rgbOff);
    putRgb(rgbOn);
    // loop forever
    fputc(0x21, f_);
    fputc(0xFF, f_);
    fputc(11, f_);
    fwrite("NETSCAPE2.0", 1, 11, f_);
    fputc(3, f_);
    fputc(1, f_);
    put16(0);
    fputc(0, f_);
    frames_ = 0;
    return true;
  }

  bool isOpen() const { return f_ != nullptr; }
  int frames() const { return frames_; }

  // `bits`: w*h/8 bytes, row-major, LSB first (the Canvas layout).
  void frame(const uint8_t *bits, int delayMs) {
    if (!f_) return;
    int W = w_ * scale_, H = h_ * scale_;
    int cs = (delayMs + 5) / 10;
    if (cs < 2) cs = 2;
    // graphic control extension
    fputc(0x21, f_);
    fputc(0xF9, f_);
    fputc(4, f_);
    fputc(0x04, f_);
    put16(cs);
    fputc(0, f_);
    fputc(0, f_);
    // image descriptor
    fputc(0x2C, f_);
    put16(0);
    put16(0);
    put16(W);
    put16(H);
    fputc(0, f_);

    const int minCode = 2;
    fputc(minCode, f_);
    const int clearCode = 1 << minCode;
    tree_.assign(4096 * 2, 0);
    int codeSize = minCode + 1, maxCode = clearCode + 1, cur = -1;
    bitBuf_ = 0;
    bitCount_ = 0;
    chunkLen_ = 0;
    code(clearCode, codeSize);

    int stride = w_ / 8;
    for (int y = 0; y < H; y++) {
      const uint8_t *row = bits + (y / scale_) * stride;
      for (int x = 0; x < W; x++) {
        int sx = x / scale_;
        int v = (row[sx >> 3] >> (sx & 7)) & 1;
        if (cur < 0) {
          cur = v;
        } else if (tree_[cur * 2 + v]) {
          cur = tree_[cur * 2 + v];
        } else {
          code(cur, codeSize);
          tree_[cur * 2 + v] = (uint16_t)(++maxCode);
          if (maxCode >= (1 << codeSize)) codeSize++;
          if (maxCode == 4095) {
            code(clearCode, codeSize);
            tree_.assign(4096 * 2, 0);
            codeSize = minCode + 1;
            maxCode = clearCode + 1;
          }
          cur = v;
        }
      }
    }
    code(cur, codeSize);
    code(clearCode, codeSize);
    code(clearCode + 1, minCode + 1);
    while (bitCount_ > 0) flushByte();
    flushChunk();
    fputc(0, f_);
    frames_++;
  }

  void close() {
    if (!f_) return;
    fputc(0x3B, f_);
    fclose(f_);
    f_ = nullptr;
  }

 private:
  void put16(int v) {
    fputc(v & 0xFF, f_);
    fputc((v >> 8) & 0xFF, f_);
  }
  void putRgb(uint32_t c) {
    fputc((c >> 16) & 0xFF, f_);
    fputc((c >> 8) & 0xFF, f_);
    fputc(c & 0xFF, f_);
  }
  void flushChunk() {
    if (!chunkLen_) return;
    fputc(chunkLen_, f_);
    fwrite(chunk_, 1, chunkLen_, f_);
    chunkLen_ = 0;
  }
  void flushByte() {
    chunk_[chunkLen_++] = (uint8_t)(bitBuf_ & 0xFF);
    bitBuf_ >>= 8;
    bitCount_ = bitCount_ > 8 ? bitCount_ - 8 : 0;
    if (chunkLen_ == 255) flushChunk();
  }
  void code(int c, int size) {
    bitBuf_ |= (uint32_t)c << bitCount_;
    bitCount_ += size;
    while (bitCount_ >= 8) flushByte();
  }

  FILE *f_ = nullptr;
  int w_ = 0, h_ = 0, scale_ = 1, frames_ = 0;
  std::vector<uint16_t> tree_;
  uint32_t bitBuf_ = 0;
  int bitCount_ = 0;
  uint8_t chunk_[256];
  int chunkLen_ = 0;
};

#endif
