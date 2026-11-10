// Frame-by-frame animations (converted from GIF files by tools/gif2clip.py).
//
// Frames are stored compressed: each one is either a key frame or the XOR
// difference with the previous frame, then run-length encoded. A typical
// 128x64 animation shrinks to 5-10% of its raw size.
//
//   frame   := type ops...            (until 1024 bytes were produced)
//   type    := 0 key frame | 1 XOR delta
//   op      := 0x00..0x7F  copy (op + 1) literal bytes
//            | 0x80..0xBE  (op - 0x7F) zero bytes
//            | 0xBF n      (n + 64) zero bytes
//            | 0xC0..0xFF v  repeat v (op - 0xBE) times
#ifndef CLIPS_CLIP_H
#define CLIPS_CLIP_H

#include "../core/Canvas.h"

struct ClipDef {
  const char *name;
  uint16_t frames;
  uint32_t size;          // bytes of `data`
  const uint8_t *data;    // encoded frames, back to back
  const uint8_t *delays;  // one per frame, in 1/100 s
  uint8_t mood;           // Mood that likes to play this clip
};

namespace Clips {
int count();
const ClipDef &get(int i);
// -1 when not found (case insensitive)
int find(const char *name);
}  // namespace Clips

class ClipPlayer {
 public:
  void start(const ClipDef *clip, uint32_t nowMs);
  void stop();
  // Advances the animation. Returns false once the last frame was shown.
  bool update(uint32_t nowMs);
  void render(Canvas &c) const;

  bool active() const { return clip_ != nullptr; }
  const ClipDef *clip() const { return clip_; }
  int frame() const { return frame_; }

 private:
  void decodeNext();

  const ClipDef *clip_ = nullptr;
  int frame_ = -1;
  uint32_t pos_ = 0;
  uint32_t nextMs_ = 0;
  uint8_t pixels_[CANVAS_BYTES];
};

#endif
