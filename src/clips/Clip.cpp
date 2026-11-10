#include "Clip.h"
#include <ctype.h>
#include <string.h>

int Clips::find(const char *name) {
  for (int i = 0; i < count(); i++) {
    const char *a = get(i).name, *b = name;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++;
      b++;
    }
    if (!*a && !*b) return i;
  }
  return -1;
}

void ClipPlayer::start(const ClipDef *clip, uint32_t nowMs) {
  clip_ = clip;
  frame_ = -1;
  pos_ = 0;
  nextMs_ = nowMs;
  memset(pixels_, 0, sizeof(pixels_));
  if (clip_ && clip_->frames == 0) clip_ = nullptr;
}

void ClipPlayer::stop() { clip_ = nullptr; }

void ClipPlayer::decodeNext() {
  const uint8_t *d = clip_->data;
  const uint32_t end = clip_->size;
  if (pos_ >= end) return;

  uint8_t type = d[pos_++];
  if (type == 0) memset(pixels_, 0, sizeof(pixels_));

  int out = 0;
  while (out < CANVAS_BYTES && pos_ < end) {
    uint8_t op = d[pos_++];
    if (op < 0x80) {
      int n = op + 1;
      while (n-- > 0 && out < CANVAS_BYTES && pos_ < end) pixels_[out++] ^= d[pos_++];
    } else if (op < 0xBF) {
      out += op - 0x7F;
    } else if (op == 0xBF) {
      if (pos_ < end) out += d[pos_++] + 64;
    } else {
      int n = op - 0xBE;
      uint8_t v = pos_ < end ? d[pos_++] : 0;
      while (n-- > 0 && out < CANVAS_BYTES) pixels_[out++] ^= v;
    }
  }
  frame_++;
}

bool ClipPlayer::update(uint32_t nowMs) {
  if (!clip_) return false;
  // Catch up if the caller was late, but never decode more than a few frames.
  int guard = 4;
  while ((int32_t)(nowMs - nextMs_) >= 0 && guard-- > 0) {
    if (frame_ + 1 >= clip_->frames) {
      clip_ = nullptr;
      return false;
    }
    decodeNext();
    uint32_t d = clip_->delays ? clip_->delays[frame_] * 10u : 80u;
    if (d < 20) d = 20;
    nextMs_ += d;
  }
  if (guard <= 0) nextMs_ = nowMs;
  return true;
}

void ClipPlayer::render(Canvas &c) const { memcpy(c.buf, pixels_, CANVAS_BYTES); }
