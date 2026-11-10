// Acts about everyday things: a book, music, a butterfly, a coffee...
#include <string.h>

#include "ActKit.h"
#include "ActList.h"

namespace {

// Moves the eyes out of the way of something drawn in the middle or below.
void makeRoom(ActFrame &f, float k, float apart, float up, float narrower, float shorter) {
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x += apart * k;
    p.y -= up * k;
    p.w *= 1.0f - narrower * k;
    p.h *= 1.0f - shorter * k;
  }
}

void quad(Canvas &c, float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, uint8_t color) {
  c.fillTriangle(ax, ay, bx, by, cx, cy, color);
  c.fillTriangle(ax, ay, cx, cy, dx, dy, color);
}

// Slides in from below at the start and back down at the end. 0 = hidden.
float riseFromBelow(const ActFrame &f, float in0, float in1, float outBefore) {
  return outBack(seg(f.t, in0, in1)) * (1.0f - easeIn(seg(f.t, f.duration - outBefore, f.duration - outBefore + 0.5f)));
}

}  // namespace

// ---- Book: reads, turns the page and gets to the good part ----------------------------
static const float BOOK_TURN = 3.0f;

void actBookPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  makeRoom(f, k, 0.0f, 11.0f, 0.08f, 0.26f);
  float gx;
  if (t < BOOK_TURN) gx = lerpf(-0.75f, -0.1f, fractf(seg(t, 0.8f, 2.9f) * 2.999f));
  else gx = lerpf(0.1f, 0.75f, fractf(seg(t, 3.5f, 4.5f) * 1.999f));
  float wow = bump(t, 4.5f, 5.3f);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidT += 0.10f * k - 0.10f * wow;
    p.h *= 1.0f + 0.28f * wow;
  }
  f.gazeX = gx * k * (1.0f - wow);
  f.gazeY = -0.8f * k;
  blinkAt(f, 2.0f);
  blinkAt(f, 3.3f);
  setEars(f, 112.0f + 30.0f * wow, 112.0f + 30.0f * wow);
  if (f.crossed(BOOK_TURN)) f.chirp = CH_BLIP;
  if (f.crossed(4.5f)) f.chirp = CH_SURPRISE;
}

static void bookLines(Canvas &c, float x0, float x1, float yOuter, float yInner, bool left, int seed) {
  // the page is a little lower at the spine
  for (int line = 0; line < 4; line++) {
    float x = x0;
    for (int n = 0; x < x1 - 2.0f; n++) {
      float len = 4.0f + hash01(seed + line * 13 + n * 5) * 7.0f;
      float end = fminf(x + len, x1);
      for (float px = x; px <= end; px += 1.0f) {
        float u = left ? (px - 22.0f) / 42.0f : (106.0f - px) / 42.0f;
        float y = lerpf(yOuter, yInner, u) + 3.0f + line * 3.0f;
        c.set((int)px, (int)y, C_OFF);
      }
      x = end + 3.0f;
    }
  }
}

void actBookDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float rise = riseFromBelow(f, 0.2f, 0.7f, 0.7f);
  if (rise < 0.02f) return;
  float oy = (1.0f - rise) * 28.0f;
  float yo = 45.0f + oy, yi = 49.0f + oy;  // top of the page at the edge and at the spine
  const float H = 15.0f;
  quad(c, 64, yi - 1, 21, yo - 1, 21, yo + H + 1, 64, yi + H + 1, C_OFF);
  quad(c, 64, yi - 1, 107, yo - 1, 107, yo + H + 1, 64, yi + H + 1, C_OFF);
  quad(c, 64, yi, 22, yo, 22, yo + H, 64, yi + H, C_ON);
  quad(c, 64, yi, 106, yo, 106, yo + H, 64, yi + H, C_ON);
  int page = f.t < BOOK_TURN + 0.25f ? 0 : 40;
  bookLines(c, 26.0f, 61.0f, yo, yi, true, 3 + page);
  bookLines(c, 68.0f, 103.0f, yo, yi, false, 23 + page);
  c.vline(64, (int)yi, (int)(yi + H), C_OFF);

  // the page that turns
  float u = seg(f.t, BOOK_TURN, BOOK_TURN + 0.5f);
  if (u > 0.0f && u < 1.0f) {
    float a = u * ACT_PI;
    float x = 64.0f + 42.0f * cosf(a), lift = 11.0f * sinf(a);
    float ye = lerpf(yi, yo, fabsf(cosf(a))) - lift;
    quad(c, 64, yi, x, ye, x, ye + H, 64, yi + H, C_ON);
    c.thickLine(x, ye, x, ye + H, 1.4f, C_OFF);
    c.thickLine(64, yi, x, ye, 1.2f, C_OFF);
    c.thickLine(64, yi + H, x, ye + H, 1.2f, C_OFF);
  }
}

// ---- Headphones: lost in the music ------------------------------------------------------
static float headBob(float t) { return fabsf(sinf(t * M_TAU * 1.0f)); }

void actHeadphonesPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.5f, 0.6f);
  float best = inOut(seg(t, 3.2f, 3.5f)) * (1.0f - inOut(seg(t, 4.6f, 4.9f)));  // the part it likes
  EyeParams shut = {-5.0f, -2.0f, 34.0f, 18.0f, 9.0f, 9.0f, 0.22f, 0, -0.75f, 0.58f, 0, 0.75f, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x -= 5.0f * k;
    mixEye(p, shut, k * (1.0f - best));
    p.lidB += 0.28f * best * k;
    p.lidBBend += 0.32f * best * k;
  }
  float bob = headBob(t) * k * (1.0f + best);
  f.dy[0] = f.dy[1] = -2.5f * bob;
  f.dx[0] = f.dx[1] = 2.5f * sinf(t * M_TAU * 0.5f) * k;
  f.pose.mouth.w = 12.0f * k;
  f.pose.mouth.curve = 3.0f;
  f.pose.mouth.open = 0.5f * best;
  f.pose.mouth.y = 21.0f;
  f.pose.mouth.tilt = 0.0f;
  setEars(f, 95.0f, 95.0f, EG_FLAP, 26.0f * k, 1.0f);
  if (f.crossed(0.6f) || f.crossed(3.2f)) f.chirp = CH_TUNE;
}

void actHeadphonesDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t, k = envelope(f, 0.5f, 0.6f);
  float in = outBack(seg(t, 0.1f, 0.6f)), out = easeIn(seg(t, f.duration - 0.6f, f.duration - 0.1f));
  float oy = -56.0f * (1.0f - in) - 70.0f * out - 2.5f * headBob(t) * k;
  float sway = 2.5f * sinf(t * M_TAU * 0.5f) * k;
  const float cx = 64.0f + sway, cy = 36.0f + oy;
  // band
  float px = 0.0f, py = 0.0f;
  for (int i = 0; i <= 24; i++) {
    float a = ACT_PI + ACT_PI * i / 24.0f;
    float x = cx + cosf(a) * 56.0f, y = cy + sinf(a) * 35.0f;
    if (i > 0) c.thickLine(px, py, x, y, 3.0f, C_ON);
    px = x;
    py = y;
  }
  // cups
  for (int s = -1; s <= 1; s += 2) {
    float x = cx + s * 57.0f;
    c.fillEllipse(x, cy, 8.5f, 14.5f, C_OFF);
    c.fillEllipse(x, cy, 7.0f, 13.0f, C_ON);
    c.fillEllipse(x, cy, 3.0f, 8.0f, C_OFF);
  }
  // notes leaving the cups
  for (int i = 0; i < 4; i++) {
    float ph = fractf(t * 0.55f + i * 0.25f);
    float side = (i & 1) ? 1.0f : -1.0f;
    float x = 64.0f + side * (43.0f + sinf(ph * 7.0f + i) * 3.0f);
    float y = 50.0f - ph * 50.0f;
    float s = k * inOut(seg(ph, 0.0f, 0.2f)) * (1.0f - seg(ph, 0.75f, 1.0f));
    if (s < 0.4f) continue;
    c.fillEllipse(x, y, 2.4f * s, 1.8f * s, C_ON);
    c.thickLine(x + 2.0f * s, y, x + 2.0f * s, y - 7.0f * s, 1.3f, C_ON);
    c.thickLine(x + 2.0f * s, y - 7.0f * s, x + 5.0f * s, y - 5.0f * s, 1.6f, C_ON);
  }
}

// ---- Butterfly: watches it, runs after it and ends up with it on the head ----------------
namespace {
bool butterflyAt(float t, float &x, float &y) {
  float wx = 96.0f + 22.0f * sinf(2.3f * t), wy = 20.0f + 12.0f * sinf(3.7f * t + 1.0f);
  if (t < 0.3f) return false;
  if (t < 0.9f) {
    float a = easeOut(seg(t, 0.3f, 0.9f));
    x = lerpf(140.0f, wx, a);
    y = lerpf(6.0f, wy, a);
  } else if (t < 2.2f) {
    x = wx, y = wy;
  } else if (t < 3.1f) {
    x = lerpf(wx, -16.0f, easeIn(seg(t, 2.2f, 3.1f)));
    y = wy;
  } else if (t < 4.3f) {
    return false;
  } else if (t < 5.3f) {
    float a = easeOut(seg(t, 4.3f, 5.3f));
    x = lerpf(146.0f, 64.0f, a);
    y = lerpf(22.0f, 8.0f, a) + 5.0f * sinf(t * 9.0f) * (1.0f - a);
  } else if (t < 6.4f) {
    x = 64.0f, y = 8.0f;
  } else {
    float a = easeIn(seg(t, 6.4f, 7.0f));
    x = lerpf(64.0f, 86.0f, a);
    y = lerpf(8.0f, -16.0f, a);
  }
  return true;
}

float chaseX(float t) {
  if (t < 2.4f) return 0.0f;
  if (t < 3.3f) return -150.0f * easeIn(seg(t, 2.4f, 3.3f));
  if (t < 4.6f) return 150.0f;
  if (t < 5.5f) return 150.0f * (1.0f - outBack(seg(t, 4.6f, 5.5f)));
  return 0.0f;
}
}  // namespace

void actButterflyPose(ActFrame &f) {
  float t = f.t, x = 0.0f, y = 0.0f;
  float dx = chaseX(t);
  bool running = (t > 2.4f && t < 3.3f) || (t > 4.6f && t < 5.5f);
  float crouch = bump(t, 2.1f, 2.45f);
  float sits = inOut(seg(t, 5.4f, 5.8f)) * (1.0f - inOut(seg(t, 6.4f, 6.8f)));
  float pant = bump(t, 5.5f, 6.3f);
  if (butterflyAt(t, x, y)) {
    float k = envelope(f, 0.3f, 0.4f);
    f.gazeX = clampf((x - (64.0f + dx)) / 60.0f, -1.0f, 1.0f) * k;
    f.gazeY = clampf((32.0f - y) / 26.0f, -1.0f, 1.0f) * k;
  }
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    f.dx[e] = dx;
    f.dy[e] = 3.0f * crouch + (running ? -4.0f * fabsf(sinf(t * 13.0f)) : 0.0f);
    if (running) {
      p.w *= 1.15f;
      p.h *= 0.9f;
    }
    p.h *= (1.0f - 0.1f * crouch) * (1.0f + 0.06f * pant * sinf(t * 22.0f));
    p.x -= 3.0f * sits;
    p.y += 3.0f * sits;
    p.lidB += 0.26f * sits;
    p.lidBBend += 0.3f * sits;
  }
  if (running) setEars(f, 20.0f, 20.0f);
  else if (sits > 0.3f) setEars(f, 100.0f, 100.0f, EG_WIGGLE, 8.0f, 1.2f);
  else setEars(f, 95.0f + 30.0f * f.gazeX, 95.0f - 30.0f * f.gazeX, EG_TWITCH, 10.0f, 1.5f);
  if (f.crossed(0.5f)) f.chirp = CH_QUESTION;
  if (f.crossed(2.4f)) f.chirp = CH_WHOOSH;
  if (f.crossed(5.4f)) f.chirp = CH_HAPPY;
}

void actButterflyDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float x, y;
  if (!butterflyAt(f.t, x, y)) return;
  bool sitting = f.t > 5.3f && f.t < 6.4f;
  float flap = fabsf(cosf(f.t * (sitting ? 5.0f : 17.0f)));
  float w = 1.5f + 5.5f * flap;
  for (int pass = 0; pass < 2; pass++) {
    // first a dark rim, then the wings
    float g = pass == 0 ? 1.5f : 0.0f;
    uint8_t col = pass == 0 ? C_OFF : C_ON;
    for (int s = -1; s <= 1; s += 2) {
      c.fillEllipse(x + s * (w + 0.5f), y - 2.0f, w + g, 4.4f + g, col);
      c.fillEllipse(x + s * (w * 0.8f + 0.5f), y + 3.2f, w * 0.8f + g, 3.0f + g, col);
    }
  }
  if (w > 3.2f) {
    c.fillCircle(x - w - 0.5f, y - 2.0f, 1.2f, C_OFF);
    c.fillCircle(x + w + 0.5f, y - 2.0f, 1.2f, C_OFF);
  }
  c.thickLine(x, y - 4.0f, x, y + 5.0f, 1.8f, C_ON);
  c.line((int)x, (int)(y - 4.0f), (int)(x - 3.0f), (int)(y - 8.0f), C_ON);
  c.line((int)x, (int)(y - 4.0f), (int)(x + 3.0f), (int)(y - 8.0f), C_ON);
}

// ---- Coffee: half asleep until the first sip ---------------------------------------------
void actCoffeePose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  float sleepy = inOut(seg(t, 0.2f, 0.9f)) * (1.0f - seg(t, 3.3f, 3.45f));
  float sip = inOut(seg(t, 2.2f, 2.5f)) * (1.0f - inOut(seg(t, 3.0f, 3.3f)));
  float jolt = outBack(seg(t, 3.4f, 3.6f)) * (1.0f - inOut(seg(t, 4.6f, 5.2f)));
  makeRoom(f, k, 9.0f, 9.0f, 0.12f, 0.15f);
  EyeParams shut = {9.0f, -9.0f, 30.0f, 16.0f, 9.0f, 9.0f, 0.55f, 0, 0.7f, 0.22f, 0, -0.7f, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidT += 0.45f * sleepy * (1.0f - sip);
    mixEye(p, shut, sip);
    p.h *= 1.0f + 0.38f * jolt;
    p.w *= 1.0f + 0.08f * jolt;
    p.pupil = 0.26f * clamp01(jolt);
    p.pupilW = 1.0f;
  }
  float buzz = bump(t, 3.4f, 4.5f);
  f.dx[0] = f.dx[1] = sinf(t * 62.0f) * 1.6f * buzz;
  f.gazeY = -0.6f * k * (1.0f - sip) * (1.0f - clamp01(jolt));
  f.pose.mouth.w = 0.0f;
  if (jolt > 0.2f) setEars(f, 150.0f, 150.0f, EG_TREMBLE, 7.0f * buzz, 13.0f);
  else setEars(f, lerpf(80.0f, 28.0f, sleepy), lerpf(80.0f, 28.0f, sleepy));
  if (f.crossed(0.3f)) f.chirp = CH_YAWN;
  if (f.crossed(3.4f)) f.chirp = CH_SURPRISE;
}

void actCoffeeDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float rise = outBack(seg(t, 1.0f, 1.5f)) * (1.0f - easeIn(seg(t, f.duration - 1.0f, f.duration - 0.5f)));
  if (rise < 0.02f) return;
  float sip = inOut(seg(t, 2.2f, 2.5f)) * (1.0f - inOut(seg(t, 3.0f, 3.3f)));
  float top = 68.0f - 24.0f * rise - 8.0f * sip;
  // handle, mug, coffee
  c.ring(75.5f, top + 8.0f, 5.0f, 2.2f, C_ON);
  c.fillRect(53, (int)top - 1, 22, 19, C_OFF);
  c.fillRect(54, (int)top, 20, 16, C_ON);
  c.fillRect(55, (int)top + 16, 18, 1, C_ON);
  c.fillRect(56, (int)top + 2, 16, 2, C_OFF);
  // steam
  float hot = rise * (1.0f - sip) * (1.0f - seg(t, 3.3f, 3.6f));
  if (hot > 0.5f) {
    for (int s = 0; s < 2; s++) {
      float bx = 60.0f + s * 8.0f, px = 0.0f, py = 0.0f;
      for (int i = 0; i <= 8; i++) {
        float u = i / 8.0f;
        float x = bx + sinf(u * 6.0f + t * 4.0f + s * 2.0f) * 2.0f;
        float y = top - 3.0f - u * 11.0f;
        if (i > 0) c.thickLine(px, py, x, y, 1.3f, C_ON);
        px = x;
        py = y;
      }
    }
  }
}

// ---- Sunglasses: they come down slowly, as they must ------------------------------------
static float glassesY(float t, float duration) {
  float y = lerpf(-44.0f, 0.0f, seg(t, 0.3f, 1.7f));
  y -= 2.5f * bump(t, 1.7f, 1.95f);
  y += lerpf(0.0f, -52.0f, easeIn(seg(t, duration - 0.6f, duration - 0.1f)));
  return y;
}

void actSunglassesPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.3f, 0.4f);
  float cool = inOut(seg(t, 1.9f, 2.2f)) * (1.0f - seg(t, f.duration - 0.7f, f.duration - 0.4f));
  // behind the glasses the eyes are half shut, and hidden
  float covered = seg(t, 1.0f, 1.7f) * (1.0f - seg(t, f.duration - 0.6f, f.duration - 0.3f));
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidT += 0.25f * k * (1.0f - covered);
    p.h = lerpf(p.h, 17.0f, covered);
  }
  f.dy[0] = f.dy[1] = -2.0f * cool;
  f.pose.mouth.w = 18.0f * cool;
  f.pose.mouth.curve = 3.0f;
  f.pose.mouth.open = 0.0f;
  f.pose.mouth.y = 23.0f;
  f.pose.mouth.tilt = 5.0f;
  setEars(f, lerpf(f.ears.left, 125.0f, cool), lerpf(f.ears.right, 70.0f, cool));
  if (f.crossed(1.7f)) f.chirp = CH_BLIP;
  if (f.crossed(2.2f)) f.chirp = CH_HAPPY;
}

void actSunglassesDraw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  (void)face;
  float gy = 32.0f + glassesY(t, f.duration) + f.dy[0];
  if (gy < -16.0f) return;
  EyeParams lens = {0, 0, 44.0f, 25.0f, 9.0f, 3.2f, 0, 0, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  const float lx = 40.0f, rx = 88.0f;
  // frame, then the dark glass
  c.thickLine(lx + 20.0f, gy - 7.0f, rx - 20.0f, gy - 7.0f, 3.0f, C_ON);
  c.thickLine(lx - 21.0f, gy - 8.0f, 0.0f, gy - 12.0f, 2.4f, C_ON);
  c.thickLine(rx + 21.0f, gy - 8.0f, 127.0f, gy - 12.0f, 2.4f, C_ON);
  for (int e = 0; e < 2; e++) {
    float x = e == 0 ? lx : rx;
    FaceRenderer::drawEye(c, x, gy, lens, EK_SHAPE, e == 0, 0.0f, 1.5f, C_OFF);
    FaceRenderer::drawEye(c, x, gy, lens, EK_SHAPE, e == 0, 0.0f, 0.0f, C_ON);
    FaceRenderer::drawEye(c, x, gy, lens, EK_SHAPE, e == 0, 0.0f, -2.2f, C_OFF);
    // the glint that crosses the glass, or the one that stays
    float s = seg(t, 2.2f, 2.9f);
    if (s > 0.0f && s < 1.0f) {
      float xo = lerpf(-17.0f, 17.0f, s);
      c.thickLine(x + xo - 3.0f, gy + 7.0f, x + xo + 3.0f, gy - 7.0f, 2.6f, C_ON);
      c.thickLine(x + xo - 8.0f, gy + 7.0f, x + xo - 2.0f, gy - 7.0f, 1.2f, C_ON);
    } else {
      c.thickLine(x - 14.0f, gy - 1.0f, x - 9.0f, gy - 7.0f, 1.8f, C_ON);
    }
  }
}

// ---- Rocket: 3, 2, 1 ------------------------------------------------------------------
static const float LIFT_OFF = 3.0f;

static float rocketBase(float t) {
  float y = 64.0f + 34.0f * (1.0f - outBack(seg(t, 0.2f, 0.7f)));
  if (t > LIFT_OFF) {
    float u = t - LIFT_OFF;
    y -= 70.0f * u * u;
  }
  return y;
}

void actRocketPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  makeRoom(f, k, 10.0f, 4.0f, 0.12f, 0.10f);
  float tip = rocketBase(t) - 30.0f;
  float counting = seg(t, 1.1f, 1.3f) * (1.0f - seg(t, LIFT_OFF - 0.1f, LIFT_OFF));
  float gy = clampf((32.0f - tip) / 30.0f, -0.6f, 1.0f);
  f.gazeY = lerpf(gy, 0.75f, counting) * k;
  float rumble = seg(t, LIFT_OFF - 0.3f, LIFT_OFF) * (1.0f - seg(t, LIFT_OFF + 0.6f, LIFT_OFF + 1.0f));
  f.dx[0] = f.dx[1] = sinf(t * 57.0f) * 1.7f * rumble;
  float awe = bump(t, 3.9f, 5.1f);
  for (int e = 0; e < 2; e++) f.pose.eye[e].h *= 1.0f + 0.14f * awe;
  float up = seg(t, LIFT_OFF, LIFT_OFF + 0.4f);
  setEars(f, lerpf(70.0f, 140.0f, up), lerpf(70.0f, 140.0f, up), rumble > 0.3f ? EG_TREMBLE : EG_STILL, 6.0f, 12.0f);
  if (f.crossed(1.2f) || f.crossed(1.8f) || f.crossed(2.4f)) f.chirp = CH_BLIP;
  if (f.crossed(LIFT_OFF)) f.chirp = CH_WHOOSH;
  if (f.crossed(4.3f)) f.chirp = CH_DING;
}

void actRocketDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  if (t > f.duration - 0.4f) return;

  if (t >= 1.2f && t < LIFT_OFF) {
    const char *n = t < 1.8f ? "3" : (t < 2.4f ? "2" : "1");
    c.text(59, 2, n, 2, C_ON);
  }

  float b = rocketBase(t);
  if (b > -6.0f) {
    const float x = 64.0f;
    // smoke stays on the ground
    for (int i = 0; i < 6; i++) {
      float u = t - (LIFT_OFF - 0.2f + i * 0.12f);
      if (u < 0.0f || u > 1.5f) continue;
      float side = (i & 1) ? 1.0f : -1.0f;
      float sx = x + side * (5.0f + 30.0f * easeOut(u / 1.5f));
      float r = 3.0f + 5.0f * u / 1.5f;
      c.fillCircle(sx, 61.0f - 2.0f * u, r, C_ON);
      c.fillCircle(sx, 61.0f - 2.0f * u, r - 1.6f, C_OFF);
    }
    // flame
    if (t > LIFT_OFF - 0.25f) {
      float len = 8.0f + 7.0f * hash01((int)(t * 30.0f));
      c.fillTriangle(x - 4.5f, b - 3.0f, x + 4.5f, b - 3.0f, x, b - 3.0f + len, C_ON);
      c.fillTriangle(x - 2.0f, b - 3.0f, x + 2.0f, b - 3.0f, x, b - 3.0f + len * 0.5f, C_OFF);
    }
    // body
    c.fillRect((int)x - 6, (int)(b - 26.0f), 12, 21, C_ON);
    c.fillTriangle(x - 6.0f, b - 26.0f, x + 6.0f, b - 26.0f, x, b - 37.0f, C_ON);
    c.fillTriangle(x - 6.0f, b - 15.0f, x - 12.0f, b - 3.0f, x - 6.0f, b - 5.0f, C_ON);
    c.fillTriangle(x + 6.0f, b - 15.0f, x + 12.0f, b - 3.0f, x + 6.0f, b - 5.0f, C_ON);
    c.fillRect((int)x - 3, (int)(b - 5.0f), 6, 3, C_ON);
    c.fillCircle(x, b - 20.0f, 2.8f, C_OFF);
  }
  // it is a star now
  FaceRenderer::drawStar4(c, 64.0f, 6.0f, 5.5f * bump(t, 4.2f, 5.0f));
}

// ---- Snow: a flake lands on its nose -------------------------------------------------------
namespace {
void flake(Canvas &c, float x, float y, float r) {
  if (r < 1.2f) {
    c.set((int)x, (int)y, C_ON);
    return;
  }
  for (int i = 0; i < 3; i++) {
    float a = i * (ACT_PI / 3.0f) + 0.3f;
    c.thickLine(x - cosf(a) * r, y - sinf(a) * r, x + cosf(a) * r, y + sinf(a) * r, 1.1f, C_ON);
  }
}

bool heroFlake(float t, float &x, float &y, float &r) {
  if (t < 1.4f || t > 4.5f) return false;
  float fall = seg(t, 1.4f, 3.4f);
  x = 64.0f + 7.0f * sinf(t * 1.6f) * (1.0f - seg(t, 2.6f, 3.4f));
  y = lerpf(-6.0f, 35.0f, fall);
  r = 3.6f * (1.0f - seg(t, 3.7f, 4.5f));  // melts
  return r > 0.4f;
}
}  // namespace

void actSnowPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  float cross = inOut(seg(t, 3.1f, 3.5f)) * (1.0f - inOut(seg(t, 4.4f, 4.8f)));
  float shiver = bump(t, 4.4f, 5.4f);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.pupil = 0.42f * k;
    p.pupilW = 1.0f;
    p.px = lerpf(0.0f, 0.9f, cross);
    p.py = lerpf(-0.7f, 0.3f, cross);
    p.lidT += 0.2f * shiver;
    p.lidB += 0.1f * shiver;
  }
  f.gazeY = 0.35f * k * (1.0f - cross);
  f.dx[0] = f.dx[1] = sinf(t * 48.0f) * 2.0f * shiver;
  f.halo = true;
  setEars(f, lerpf(85.0f, 40.0f, shiver), lerpf(85.0f, 40.0f, shiver), shiver > 0.2f ? EG_TREMBLE : EG_STILL, 6.0f, 13.0f);
  if (f.crossed(3.4f)) f.chirp = CH_BLIP;
  if (f.crossed(4.5f)) f.chirp = CH_SCARED;
}

void actSnowDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  float t = f.t;
  if (layer == ACT_FRONT) {
    float x, y, r;
    if (heroFlake(t, x, y, r)) {
      c.fillCircle(x, y, r + 1.5f, C_OFF);
      flake(c, x, y, r);
    }
    return;
  }
  if (layer != ACT_BACK) return;
  for (int i = 0; i < 18; i++) {
    float speed = 10.0f + hash01(i * 5) * 11.0f;
    float y = t * speed - 8.0f - hash01(i * 11) * 60.0f;
    if (y < -4.0f || y > 66.0f) continue;
    float x = hash01(i * 3) * 128.0f + 4.0f * sinf(t * (0.8f + hash01(i)) + i);
    flake(c, x, y, hash01(i * 7) > 0.6f ? 2.6f : 1.0f);
  }
  // it settles on the ground
  float g = 5.0f * seg(t, 1.0f, 4.5f) * (1.0f - seg(t, f.duration - 0.8f, f.duration - 0.2f));
  if (g > 0.5f) {
    for (int x = 0; x < CANVAS_W; x++) {
      int h = (int)(g * (0.7f + 0.3f * sinf(x * 0.15f)));
      if (h > 0) c.vline(x, CANVAS_H - h, CANVAS_H - 1, C_ON);
    }
  }
}

// ---- Search: a magnifying glass looks at it --------------------------------------------
namespace {
const float LENS_R = 15.0f;

void lensAt(float t, float &x, float &y) {
  struct Key {
    float t, x, y;
  };
  static const Key K[] = {{0.2f, 146, 78}, {1.0f, 88, 32}, {1.9f, 88, 32}, {2.6f, 40, 32},
                          {3.5f, 40, 32},  {4.0f, 64, 44}, {4.4f, 64, 44}, {5.0f, -24, -22}};
  const int n = (int)(sizeof(K) / sizeof(K[0]));
  if (t <= K[0].t) {
    x = K[0].x, y = K[0].y;
    return;
  }
  for (int i = 1; i < n; i++) {
    if (t <= K[i].t) {
      float a = inOut(seg(t, K[i - 1].t, K[i].t));
      x = lerpf(K[i - 1].x, K[i].x, a);
      y = lerpf(K[i - 1].y, K[i].y, a);
      return;
    }
  }
  x = K[n - 1].x, y = K[n - 1].y;
}
}  // namespace

void actSearchPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.3f, 0.4f);
  float x, y;
  lensAt(t, x, y);
  f.gazeX = clampf((x - 64.0f) / 64.0f, -1.0f, 1.0f) * 0.5f * k;
  f.gazeY = clampf((32.0f - y) / 32.0f, -1.0f, 1.0f) * 0.5f * k;
  for (int e = 0; e < 2; e++) {
    float ex = e == 0 ? EYE_ANCHOR_LX : EYE_ANCHOR_RX;
    float d = sqrtf((x - ex) * (x - ex) + (y - 32.0f) * (y - 32.0f));
    float near = clamp01(1.0f - d / 30.0f);
    f.pose.eye[e].h *= 1.0f + 0.10f * near;
  }
  setEars(f, 95.0f + 35.0f * f.gazeX * 2.0f, 95.0f - 35.0f * f.gazeX * 2.0f, EG_TWITCH, 8.0f, 1.0f);
  if (f.crossed(1.0f) || f.crossed(2.6f)) f.chirp = CH_QUESTION;
}

void actSearchDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_POST) return;
  float x, y;
  lensAt(f.t, x, y);
  if (x < -LENS_R - 16.0f || x > CANVAS_W + LENS_R + 16.0f) return;

  // what is under the glass, bigger
  Canvas src;
  memcpy(src.buf, c.buf, CANVAS_BYTES);
  int x0 = (int)floorf(x - LENS_R), x1 = (int)ceilf(x + LENS_R);
  int y0 = (int)floorf(y - LENS_R), y1 = (int)ceilf(y + LENS_R);
  for (int py = y0; py <= y1; py++) {
    for (int px = x0; px <= x1; px++) {
      float dx = px + 0.5f - x, dy = py + 0.5f - y;
      if (dx * dx + dy * dy > LENS_R * LENS_R) continue;
      bool on = src.get((int)floorf(x + dx / 1.7f), (int)floorf(y + dy / 1.7f));
      c.set(px, py, on ? C_ON : C_OFF);
    }
  }
  // handle and rim
  const float d = 0.7071f;
  float hx0 = x + (LENS_R + 2.0f) * d, hy0 = y + (LENS_R + 2.0f) * d;
  float hx1 = x + (LENS_R + 15.0f) * d, hy1 = y + (LENS_R + 15.0f) * d;
  c.thickLine(hx0, hy0, hx1, hy1, 7.0f, C_OFF);
  c.thickLine(hx0, hy0, hx1, hy1, 4.0f, C_ON);
  c.ring(x, y, LENS_R + 3.2f, 1.6f, C_OFF);
  c.ring(x, y, LENS_R + 1.0f, 2.4f, C_ON);
  c.thickLine(x - 9.0f, y - 4.0f, x - 5.0f, y - 9.0f, 1.4f, C_XOR);  // reflection
}

// ---- Photo: say cheese ---------------------------------------------------------------------
static const float FLASH = 2.4f;

void actPhotoPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.3f, 0.5f);
  float smile = inOut(seg(t, 0.6f, 1.0f)) * (1.0f - seg(t, FLASH, FLASH + 0.1f));
  float dazed = seg(t, FLASH + 0.1f, FLASH + 0.2f) * (1.0f - inOut(seg(t, 3.3f, 3.9f)));
  makeRoom(f, k * (1.0f - dazed), 3.0f, 5.0f, 0.04f, 0.06f);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidB += 0.08f * smile;  // it was already smiling
    p.lidT += 0.30f * dazed;
    p.lidB += 0.20f * dazed;
    f.blink[e] = dazed * (0.45f + 0.45f * sinf(t * 31.0f + e));
  }
  f.dx[0] = f.dx[1] = 2.5f * sinf(t * 3.0f) * dazed;
  f.pose.mouth.w = 0.0f;  // the camera is where the mouth would be
  setEars(f, lerpf(105.0f, 40.0f, dazed), lerpf(105.0f, 40.0f, dazed));
  if (f.crossed(0.9f) || f.crossed(1.4f) || f.crossed(1.9f)) f.chirp = CH_BLIP;
  if (f.crossed(FLASH)) f.chirp = CH_POP;
}

void actPhotoDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  float t = f.t;
  if (layer == ACT_FRONT) {
    if (t >= FLASH) return;
    float rise = outBack(seg(t, 0.2f, 0.6f));
    float top = 66.0f - 19.0f * rise;
    // camera
    c.fillRect(50, (int)top - 1, 28, 18, C_OFF);
    c.fillRect(51, (int)top, 26, 16, C_ON);
    c.fillRect(55, (int)top - 3, 8, 3, C_ON);
    c.fillCircle(64.0f, top + 8.0f, 5.5f, C_OFF);
    c.ring(64.0f, top + 8.0f, 3.2f, 1.4f, C_ON);
    c.fillRect(71, (int)top + 2, 3, 2, C_OFF);
    // 3, 2, 1
    for (int i = 0; i < 3; i++) {
      float x = 52.0f + i * 12.0f;
      if (t > 0.9f + i * 0.5f) c.fillCircle(x, 5.0f, 3.0f, C_ON);
      else c.ring(x, 5.0f, 2.4f, 1.0f, C_ON);
    }
    return;
  }
  if (layer != ACT_POST) return;
  if (t >= FLASH && t < FLASH + 0.10f) {
    c.fill();
  } else if (t >= FLASH + 0.10f && t < FLASH + 0.20f) {
    for (int i = 0; i < CANVAS_BYTES; i++) c.buf[i] ^= 0xFF;
  } else if (t >= FLASH + 0.20f && t < FLASH + 1.5f) {
    // spots that stay in the eyes
    float u = seg(t, FLASH + 0.2f, FLASH + 1.5f);
    static const float SX[3] = {30.0f, 78.0f, 104.0f}, SY[3] = {20.0f, 44.0f, 18.0f};
    for (int i = 0; i < 3; i++) {
      c.fillCircle(SX[i] + 6.0f * u * (i - 1), SY[i] - 5.0f * u, (9.0f - i * 2.0f) * (1.0f - u), C_XOR);
    }
  }
}
