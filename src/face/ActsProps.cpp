// Acts where the face plays with an object drawn next to it.
#include <string.h>

#include "ActKit.h"
#include "ActList.h"

namespace {

// Point of a tilted ellipse around the middle of the screen. `z` tells if it
// is on the near side (> 0) or behind the face.
struct OrbitPoint {
  float x, y, z;
};

OrbitPoint orbitAt(float theta, float rx, float ry, float tilt, float cx = 64.0f, float cy = 32.0f) {
  float c = cosf(theta), s = sinf(theta);
  float ct = cosf(tilt), st = sinf(tilt);
  OrbitPoint p;
  p.x = cx + rx * c * ct - ry * s * st;
  p.y = cy + rx * c * st + ry * s * ct;
  p.z = s;
  return p;
}

void drawBubble(Canvas &c, float x, float y, float r) {
  if (r < 1.2f) return;
  c.ring(x, y, r, 1.3f, C_ON);
  if (r > 3.5f) c.thickLine(x - r * 0.45f, y - r * 0.15f, x - r * 0.15f, y - r * 0.45f, 1.2f, C_ON);
}

void drawPop(Canvas &c, float x, float y, float u, float size) {
  // short rays flying away from the middle
  for (int i = 0; i < 8; i++) {
    float a = i * (M_TAU / 8.0f) + 0.3f;
    float r0 = size * (0.6f + u * 1.2f), r1 = r0 + size * 0.5f * (1.0f - u);
    c.thickLine(x + cosf(a) * r0, y + sinf(a) * r0, x + cosf(a) * r1, y + sinf(a) * r1, 1.3f, C_ON);
  }
}

void drawCloud(Canvas &c, float x, float y, float s) {
  c.fillCircle(x - 9.0f * s, y + 2.0f * s, 6.0f * s, C_ON);
  c.fillCircle(x, y - 2.0f * s, 8.5f * s, C_ON);
  c.fillCircle(x + 10.0f * s, y + 1.5f * s, 7.0f * s, C_ON);
  c.fillRect((int)(x - 9.0f * s), (int)(y + 2.0f * s), (int)(20.0f * s), (int)(6.0f * s), C_ON);
}

}  // namespace

// ---- Notify: a bell rings on the corner --------------------------------------------
void actNotifyPose(ActFrame &f) {
  float look = inOut(seg(f.t, 0.2f, 0.45f)) * (1.0f - inOut(seg(f.t, 2.3f, 2.7f)));
  f.gazeX = 0.85f * look;
  f.gazeY = 0.55f * look;
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].h *= 1.0f + 0.10f * look;
    f.pose.eye[e].w *= 1.0f + 0.04f * look;
    f.dx[e] = -16.0f * look;  // makes room for the bell
    f.dy[e] = 5.0f * look;
  }
  setEars(f, lerpf(f.ears.left, 118.0f, look), lerpf(f.ears.right, 118.0f, look), EG_TWITCH, 10.0f, 2.0f);
  if (f.crossed(0.2f)) f.chirp = CH_DING;
}

void actNotifyDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float s = outBack(seg(t, 0.15f, 0.5f)) * (1.0f - inOut(seg(t, 2.2f, 2.5f)));
  if (s < 0.05f) return;
  float swing = t > 0.5f ? sinf((t - 0.5f) * 24.0f) * 0.38f * (1.0f - seg(t, 0.5f, 1.8f)) : 0.0f;
  const float ox = 110.0f, oy = 3.0f;  // the bell hangs from here
  c.fillCircle(ox, oy + 12.0f * s, 16.0f * s, C_OFF);
  float ca = cosf(swing), sa = sinf(swing);
  auto X = [&](float u, float v) { return ox + s * (u * ca - v * sa); };
  auto Y = [&](float u, float v) { return oy + s * (u * sa + v * ca); };

  c.fillCircle(X(0, 1), Y(0, 1), 1.6f * s, C_ON);                        // handle
  c.fillCircle(X(0, 8), Y(0, 8), 5.6f * s, C_ON);                        // dome
  c.fillTriangle(X(-5.6f, 8), Y(-5.6f, 8), X(5.6f, 8), Y(5.6f, 8), X(-8, 15), Y(-8, 15), C_ON);
  c.fillTriangle(X(5.6f, 8), Y(5.6f, 8), X(8, 15), Y(8, 15), X(-8, 15), Y(-8, 15), C_ON);
  c.thickLine(X(-9, 15.5f), Y(-9, 15.5f), X(9, 15.5f), Y(9, 15.5f), 2.0f * s, C_ON);  // rim
  c.fillCircle(X(0, 19), Y(0, 19), 2.0f * s, C_ON);                      // clapper

  // sound
  float ring = 1.0f - seg(t, 0.5f, 1.8f);
  if (t > 0.5f && ring > 0.05f && fractf(t * 5.0f) < 0.6f) {
    for (int side = -1; side <= 1; side += 2) {
      float bx = ox + side * 13.0f;
      c.thickLine(bx, 9.0f, bx + side * 3.0f, 12.0f, 1.2f, C_ON);
      c.thickLine(bx + side * 3.0f, 12.0f, bx, 15.0f, 1.2f, C_ON);
    }
  }
}

// ---- Orbit: a little moon goes around the face ------------------------------------
static float orbitTheta(float t) { return -ACT_PI * 0.5f + M_TAU * 2.0f * inOut(seg(t, 0.3f, 3.2f)); }
static float orbitShow(float t) { return seg(t, 0.2f, 0.5f) * (1.0f - seg(t, 3.1f, 3.4f)); }

void actOrbitPose(ActFrame &f) {
  float th = orbitTheta(f.t), k = orbitShow(f.t);
  f.gazeX = cosf(th) * 0.9f * k;
  f.gazeY = -sinf(th) * 0.55f * k;
  f.halo = true;
  setEars(f, 85.0f + 25.0f * cosf(th) * k, 85.0f - 25.0f * cosf(th) * k);
  if (f.crossed(0.3f)) f.chirp = CH_WHOOSH;
}

void actOrbitDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer == ACT_POST) return;
  float k = orbitShow(f.t);
  if (k < 0.05f) return;
  float th = orbitTheta(f.t);
  for (int i = 6; i >= 0; i--) {
    OrbitPoint p = orbitAt(th - i * 0.13f, 57.0f, 17.0f, -0.2f);
    float r = 3.6f * k * (1.0f - i / 7.5f);
    bool front = p.z > 0.0f;
    if (front != (layer == ACT_FRONT)) continue;
    if (front) FaceRenderer::drawBall(c, p.x, p.y, r, r);
    else c.fillCircle(p.x, p.y, r, C_ON);
  }
}

// ---- Swirl: ribbons spin around the face ------------------------------------------
void actSwirlPose(ActFrame &f) {
  float k = envelope(f, 0.25f, 0.4f);
  f.halo = true;
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].lidB += 0.10f * k;
    f.pose.eye[e].lidBBend += 0.15f * k;
  }
  setEars(f, f.ears.left, f.ears.right, EG_WIGGLE, 16.0f * k, 3.0f);
  if (f.crossed(0.1f)) f.chirp = CH_WHOOSH;
}

void actSwirlDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer == ACT_POST) return;
  float k = envelope(f, 0.25f, 0.4f);
  if (k < 0.05f) return;
  static const float TILT[3] = {-0.42f, 0.18f, 0.6f};
  static const float RY[3] = {12.0f, 19.0f, 26.0f};
  for (int j = 0; j < 3; j++) {
    float head = j * 2.1f + M_TAU * 1.5f * f.t;
    float sweep = 0.55f * M_TAU * k;
    const int n = 22;
    OrbitPoint prev = orbitAt(head, 58.0f, RY[j], TILT[j]);
    for (int i = 1; i <= n; i++) {
      float a = head - sweep * i / n;
      OrbitPoint p = orbitAt(a, 58.0f, RY[j], TILT[j]);
      bool front = p.z > 0.0f;
      if (front == (layer == ACT_FRONT)) {
        float th = 2.4f * (1.0f - (float)i / n) + 0.8f;
        if (front) c.thickLine(prev.x, prev.y, p.x, p.y, th + 2.4f, C_OFF);
        c.thickLine(prev.x, prev.y, p.x, p.y, th, C_ON);
      }
      prev = p;
    }
  }
}

// ---- Burst: explodes and comes back -------------------------------------------------
void actBurstPose(ActFrame &f) {
  float t = f.t;
  if (t < 0.35f) {
    float k = inOut(t / 0.35f);
    for (int e = 0; e < 2; e++) {
      f.pose.eye[e].x = lerpf(f.pose.eye[e].x, -24.0f, k);
      f.pose.eye[e].w = lerpf(f.pose.eye[e].w, 3.0f, k);
      f.pose.eye[e].h = lerpf(f.pose.eye[e].h, 3.0f, k);
    }
    setEars(f, 40.0f, 40.0f);
  } else if (t < 1.15f) {
    f.eyeOn[0] = f.eyeOn[1] = false;
    setEars(f, 140.0f, 140.0f, EG_TREMBLE, 8.0f, 12.0f);
  } else {
    float k = outBack(seg(t, 1.15f, 1.6f));
    for (int e = 0; e < 2; e++) {
      f.pose.eye[e].w *= k;
      f.pose.eye[e].h *= k;
    }
    setEars(f, 95.0f, 95.0f);
  }
  f.pose.mouth.w = 0.0f;
  blinkAt(f, 2.0f);
  blinkAt(f, 2.3f);
  if (f.crossed(0.35f)) f.chirp = CH_POP;
}

void actBurstDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  // shock wave
  float w = seg(t, 0.35f, 0.6f);
  if (w > 0.0f && w < 1.0f) c.ring(64.0f, 32.0f, 4.0f + 34.0f * easeOut(w), 2.2f * (1.0f - w) + 0.6f, C_ON);
  // sparks flying out
  for (int i = 0; i < 12; i++) {
    float u = t - (0.35f + hash01(i * 3) * 0.10f);
    const float life = 0.75f;
    if (u < 0.0f || u > life) continue;
    float a = i * (M_TAU / 12.0f) + hash01(i * 5) * 0.5f + u * 1.6f;
    float d = 60.0f * (1.0f - powf(0.78f, u * 12.0f));
    c.fillCircle(64.0f + cosf(a) * d, 32.0f + sinf(a) * d * 0.55f, 2.6f * (1.0f - u / life) + 0.6f, C_ON);
  }
  // ... and gathering again where the eyes will be
  float g = seg(t, 0.9f, 1.2f);
  if (g > 0.0f && g < 1.0f) {
    for (int e = 0; e < 2; e++) {
      float ex = e == 0 ? EYE_ANCHOR_LX : EYE_ANCHOR_RX;
      for (int i = 0; i < 7; i++) {
        float a = i * (M_TAU / 7.0f) + g * 3.0f + e;
        float d = 30.0f * (1.0f - easeIn(g));
        c.fillCircle(ex + cosf(a) * d, EYE_ANCHOR_Y + sinf(a) * d * 0.7f, 1.0f + g * 1.2f, C_ON);
      }
    }
  }
}

// ---- Heartbeat: a heart grows between the eyes ----------------------------------------
static float heartSize(float t) {
  float grow = outBack(seg(t, 0.3f, 0.7f));
  float beat = bump(t, 0.95f, 1.15f) + 0.7f * bump(t, 1.2f, 1.4f) + bump(t, 1.85f, 2.05f) + 0.7f * bump(t, 2.1f, 2.3f);
  float leave = 1.0f - seg(t, 2.7f, 3.3f) * 0.6f;
  return 9.0f * grow * (1.0f + 0.22f * beat) * leave;
}
static float heartY(float t) { return lerpf(35.0f, -14.0f, easeIn(seg(t, 2.6f, 3.3f))); }

void actHeartbeatPose(ActFrame &f) {
  float t = f.t;
  float there = inOut(seg(t, 0.3f, 0.7f)) * (1.0f - inOut(seg(t, 2.7f, 3.2f)));
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].x += 7.0f * there;  // make room
    f.pose.eye[e].w *= 1.0f - 0.12f * there;
  }
  f.gazeY = -0.25f * there + 0.8f * bump(t, 2.7f, 3.5f);
  f.halo = true;
  float beat = bump(t, 0.95f, 1.15f) + bump(t, 1.85f, 2.05f);
  setEars(f, 100.0f + 14.0f * beat, 100.0f + 14.0f * beat);
  if (f.crossed(0.95f)) f.chirp = CH_LOVE;
}

void actHeartbeatDraw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float r = heartSize(f.t);
  if (r < 0.8f) return;
  float x = 64.0f + face.gazeX * 4.0f, y = heartY(f.t);
  FaceRenderer::drawHeart(c, x, y, r + 1.8f, r + 1.8f, C_OFF);
  FaceRenderer::drawHeart(c, x, y, r, r, C_ON);
}

// ---- Rain: a cloud stops over the face ------------------------------------------------
static float cloudX(float t) {
  if (t < 1.2f) return lerpf(-34.0f, 64.0f, easeOut(seg(t, 0.0f, 1.2f)));
  return lerpf(64.0f, 166.0f, easeIn(seg(t, 4.0f, 5.0f)));
}

void actRainPose(ActFrame &f) {
  float t = f.t;
  float wet = inOut(seg(t, 1.3f, 1.7f)) * (1.0f - inOut(seg(t, 3.9f, 4.3f)));
  f.gazeX = clampf((cloudX(t) - 64.0f) / 64.0f, -1.0f, 1.0f) * 0.8f;
  f.gazeY = 0.75f * (1.0f - wet) * envelope(f, 0.3f, 0.5f);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].lidT += 0.30f * wet;
    f.pose.eye[e].y += 6.0f * wet;
  }
  // shakes the water off
  float shake = bump(t, 4.3f, 4.9f);
  f.dx[0] = f.dx[1] = sinf(t * 46.0f) * 3.0f * shake;
  f.halo = true;
  setEars(f, lerpf(28.0f, 8.0f, wet), lerpf(28.0f, 8.0f, wet), shake > 0.1f ? EG_FLAP : EG_STILL, 30.0f * shake, 8.0f);
  if (f.crossed(1.3f)) f.chirp = CH_SAD;
}

void actRainDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t, x = cloudX(t);
  if (t > 1.2f && t < 4.0f) {
    float k = seg(t, 1.2f, 1.5f) * (1.0f - seg(t, 3.7f, 4.0f));
    for (int i = 0; i < 9; i++) {
      float ph = fractf(t * 1.5f + hash01(i * 3) * 1.0f);
      if (hash01(i * 11 + (int)(t * 1.5f + hash01(i * 3))) > k) continue;
      float dx = x - 18.0f + i * 4.5f;
      float y = 16.0f + ph * 52.0f;
      c.thickLine(dx, y, dx - 1.5f, y + 5.0f, 1.4f, C_ON);
    }
  }
  drawCloud(c, x, 8.0f, 1.0f);
}

// ---- ShootingStar ------------------------------------------------------------------
static float starU(float t) { return seg(t, 0.5f, 1.5f); }

void actShootingStarPose(ActFrame &f) {
  float t = f.t, u = starU(t);
  float watching = inOut(seg(t, 0.3f, 0.6f)) * (1.0f - inOut(seg(t, 2.6f, 3.1f)));
  f.gazeX = lerpf(0.9f, -0.9f, inOut(u)) * watching;
  f.gazeY = lerpf(0.9f, 0.35f, u) * watching;
  float awe = inOut(seg(t, 1.3f, 1.7f)) * (1.0f - inOut(seg(t, 2.6f, 3.1f)));
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].h *= 1.0f + 0.14f * awe;
    f.pose.eye[e].w *= 1.0f + 0.06f * awe;
  }
  f.halo = true;
  setEars(f, 95.0f + 20.0f * watching, 95.0f + 20.0f * watching);
  if (f.crossed(0.5f)) f.chirp = CH_WHOOSH;
  if (f.crossed(1.6f)) f.chirp = CH_SURPRISE;
}

void actShootingStarDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t, u = starU(t);
  if (u > 0.0f && u < 1.0f) {
    float x = lerpf(134.0f, -8.0f, u), y = lerpf(0.0f, 26.0f, u * u);
    FaceRenderer::drawStar4(c, x, y, 7.0f);
    c.fillCircle(x, y, 2.2f, C_ON);
    for (int i = 1; i <= 14; i++) {
      float v = u - i * 0.02f;
      if (v < 0.0f) break;
      float tx = lerpf(134.0f, -8.0f, v), ty = lerpf(0.0f, 26.0f, v * v);
      if (i < 7 || (i & 1)) c.fillCircle(tx, ty, 2.2f - i * 0.13f, C_ON);
    }
  }
  // the sky keeps twinkling for a moment
  float tw = seg(t, 1.4f, 1.7f) * (1.0f - seg(t, 2.5f, 2.9f));
  if (tw > 0.05f) {
    static const float SX[5] = {8.0f, 120.0f, 62.0f, 20.0f, 108.0f};
    static const float SY[5] = {10.0f, 8.0f, 5.0f, 54.0f, 56.0f};
    for (int i = 0; i < 5; i++) {
      float s = sinf(t * 7.0f + i * 1.9f);
      if (s > 0.0f) FaceRenderer::drawStar4(c, SX[i], SY[i], 4.5f * s * tw);
    }
  }
}

// ---- Fireworks -----------------------------------------------------------------------
static const float FW_X[3] = {22.0f, 106.0f, 64.0f};
static const float FW_Y[3] = {16.0f, 14.0f, 9.0f};
static const float FW_T[3] = {0.3f, 1.3f, 2.4f};
static const float FW_RISE = 0.6f, FW_LIFE = 1.0f;

void actFireworksPose(ActFrame &f) {
  float t = f.t, gx = 0.0f, gy = 0.0f;
  for (int i = 0; i < 3; i++) {
    float w = bump(t, FW_T[i], FW_T[i] + FW_RISE + FW_LIFE);
    gx += (FW_X[i] - 64.0f) / 48.0f * w;
    gy += 0.9f * w;
    if (f.crossed(FW_T[i])) f.chirp = CH_WHOOSH;
    if (f.crossed(FW_T[i] + FW_RISE)) f.chirp = CH_POP;
  }
  f.gazeX = clampf(gx, -1.0f, 1.0f);
  f.gazeY = clampf(gy, 0.0f, 1.0f);
  float k = envelope(f, 0.3f, 0.5f);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].w *= 1.0f - 0.14f * k;
    f.pose.eye[e].h *= 1.0f - 0.10f * k;
    f.pose.eye[e].y += 5.0f * k;
  }
  f.halo = true;
  setEars(f, 110.0f, 110.0f, EG_WIGGLE, 14.0f * k, 2.5f);
}

void actFireworksDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  for (int i = 0; i < 3; i++) {
    float u = t - FW_T[i];
    if (u < 0.0f) continue;
    if (u < FW_RISE) {
      float r = easeOut(u / FW_RISE);
      float y = lerpf(66.0f, FW_Y[i], r);
      c.fillCircle(FW_X[i], y, 1.6f, C_ON);
      c.thickLine(FW_X[i], y + 2.0f, FW_X[i], y + 2.0f + 9.0f * (1.0f - r), 1.0f, C_ON);
      continue;
    }
    float v = (u - FW_RISE) / FW_LIFE;
    if (v > 1.0f) continue;
    float d = 27.0f * easeOut(v);
    for (int j = 0; j < 14; j++) {
      float a = j * (M_TAU / 14.0f) + i;
      float x = FW_X[i] + cosf(a) * d;
      float y = FW_Y[i] + sinf(a) * d * 0.8f + 14.0f * v * v;  // gravity
      if (v > 0.6f && ((j + (int)(t * 20.0f)) & 1)) continue;  // they flicker as they die
      c.fillCircle(x, y, 2.2f * (1.0f - v) + 0.6f, C_ON);
      if (v < 0.5f) c.fillCircle(FW_X[i] + cosf(a) * d * 0.55f, FW_Y[i] + sinf(a) * d * 0.45f, 1.1f, C_ON);
    }
  }
}

// ---- Snore: the bubble of the sleeper ----------------------------------------------------
static float snoreR(float t) {
  if (t < 1.2f) return 6.0f * inOut(seg(t, 0.2f, 1.2f));
  if (t < 2.0f) return lerpf(6.0f, 3.0f, inOut(seg(t, 1.2f, 2.0f)));
  if (t < 3.4f) return lerpf(3.0f, 11.0f, inOut(seg(t, 2.0f, 3.4f)));
  return 0.0f;
}

void actSnorePose(ActFrame &f) {
  float t = f.t;
  // wakes up with the pop, half way, and falls asleep again
  float startle = outBack(seg(t, 3.4f, 3.55f)) * (1.0f - inOut(seg(t, 4.0f, 4.8f)));
  EyeParams open = {0.0f, 0.0f, 34.0f, 24.0f, 4.0f, 4.0f, 0.30f, -4.0f, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) mixEye(f.pose.eye[e], open, clamp01(startle));
  float breath = (snoreR(t) - 3.0f) / 8.0f;
  f.dy[0] = f.dy[1] = -1.5f * breath;
  f.gazeX = 0.5f * bump(t, 3.6f, 4.3f);
  setEars(f, 10.0f + 50.0f * startle, 10.0f + 50.0f * startle);
  if (f.crossed(3.4f)) f.chirp = CH_POP;
}

void actSnoreDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t, r = snoreR(t);
  const float nx = 60.0f, ny = 46.0f;
  if (r > 1.0f) drawBubble(c, nx + r * 0.7f, ny + r * 0.7f, r);
  float u = seg(t, 3.4f, 3.75f);
  if (u > 0.0f && u < 1.0f) drawPop(c, nx + 7.7f, ny + 7.7f, u, 9.0f);
}

// ---- Loading: watches a progress bar ----------------------------------------------------
static float loadProgress(float t) {
  // stutters like the real ones
  float u = seg(t, 0.4f, 2.7f);
  return clamp01(0.55f * easeOut(seg(u, 0.0f, 0.35f)) + 0.2f * inOut(seg(u, 0.45f, 0.6f)) +
                 0.25f * inOut(seg(u, 0.8f, 1.0f)));
}

void actLoadingPose(ActFrame &f) {
  float t = f.t;
  float k = envelope(f, 0.35f, 0.4f);
  float p = loadProgress(t);
  float done = inOut(seg(t, 2.7f, 2.95f));
  f.gazeX = lerpf(-0.8f, 0.8f, p) * k * (1.0f - done);
  f.gazeY = -0.75f * k * (1.0f - done);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].y -= 9.0f * k;
    f.pose.eye[e].h *= 1.0f - 0.12f * k;
    f.pose.eye[e].lidB += 0.35f * done * k;
    f.pose.eye[e].lidBBend += 0.35f * done * k;
  }
  setEars(f, lerpf(90.0f, 120.0f, p), lerpf(90.0f, 120.0f, p), done > 0.5f ? EG_WIGGLE : EG_STILL, 14.0f, 3.0f);
  if (f.crossed(2.7f)) f.chirp = CH_DING;
}

void actLoadingDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float k = envelope(f, 0.35f, 0.4f);
  if (k < 0.3f) return;
  int w = (int)(100.0f * k), x0 = 64 - w / 2, y0 = 54;
  c.rect(x0, y0, w, 8, C_ON);
  int fill = (int)((w - 4) * loadProgress(f.t));
  if (fill > 0) c.fillRect(x0 + 2, y0 + 2, fill, 4, C_ON);
  // flashes when it is done
  if (f.t > 2.7f && f.t < 3.1f && fractf(f.t * 8.0f) < 0.5f) c.fillRect(x0, y0, w, 8, C_XOR);
}

// ---- Bubbles -------------------------------------------------------------------------
static const int BUBBLES = 8, POPPED = 2;
static const float POP_T = 2.8f;

static void bubbleAt(int i, float t, float &x, float &y, float &r) {
  float speed = 16.0f + hash01(i * 3) * 14.0f;
  float start = hash01(i * 7) * 1.6f;
  r = 3.0f + hash01(i * 5) * 4.5f;
  x = 10.0f + i * 15.0f + sinf(t * (1.4f + hash01(i) * 1.5f) + i) * 5.0f;
  y = 72.0f - (t - start) * speed;
}

void actBubblesPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  float x, y, r;
  bubbleAt(POPPED, t < POP_T ? t : POP_T, x, y, r);
  f.gazeX = clampf((x - 64.0f) / 60.0f, -1.0f, 1.0f) * k;
  f.gazeY = clampf((32.0f - y) / 30.0f, -1.0f, 1.0f) * k;
  f.halo = true;
  blinkBoth(f, bump(t, POP_T, POP_T + 0.3f));
  float jump = bump(t, POP_T, POP_T + 0.4f);
  f.dy[0] = f.dy[1] = -3.0f * jump;
  setEars(f, 95.0f + 30.0f * jump, 95.0f + 30.0f * jump, EG_WIGGLE, 8.0f * k, 1.2f);
  if (f.crossed(POP_T)) f.chirp = CH_POP;
}

void actBubblesDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  for (int i = 0; i < BUBBLES; i++) {
    float x, y, r;
    if (i == POPPED && t >= POP_T) {
      bubbleAt(i, POP_T, x, y, r);
      float u = seg(t, POP_T, POP_T + 0.35f);
      if (u < 1.0f) drawPop(c, x, y, u, r);
      continue;
    }
    bubbleAt(i, t, x, y, r);
    if (y < -10.0f || y > 74.0f) continue;
    drawBubble(c, x, y, r);
  }
}

// ---- Say: a speech balloon with a short text ------------------------------------------
void actSayPose(ActFrame &f) {
  float k = envelope(f, 0.3f, 0.35f);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].y += 11.0f * k;
    f.pose.eye[e].h *= 1.0f - 0.24f * k;
    f.pose.eye[e].w *= 1.0f - 0.10f * k;
    f.pose.eye[e].lidB += 0.20f * k;
    f.pose.eye[e].lidBBend += 0.25f * k;
  }
  f.pose.mouth.w = 0.0f;
  // bobs with every letter
  int n = (int)strlen(Acts::text());
  float typing = seg(f.t, 0.4f, 0.4f + n * 0.09f);
  if (typing > 0.0f && typing < 1.0f) f.dy[0] = f.dy[1] = -1.5f * fabsf(sinf(f.t * 17.0f));
  setEars(f, 100.0f, 100.0f, EG_WIGGLE, 10.0f * k, 2.0f);
  if (f.crossed(0.35f)) f.chirp = CH_HELLO;
}

void actSayDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float k = outBack(seg(f.t, 0.15f, 0.45f)) * (1.0f - inOut(seg(f.t, f.duration - 0.35f, f.duration)));
  if (k < 0.1f) return;
  const char *txt = Acts::text();
  int n = (int)strlen(txt);
  int scale = n <= 9 ? 2 : 1;
  int tw = Canvas::textWidth(txt, scale);
  int bw = (int)((tw + 14) * clamp01(k)), bh = (int)((7 * scale + 8) * fminf(k, 1.06f));
  if (bw < 8 || bh < 6) return;
  int x0 = 64 - bw / 2, y0 = 1;
  // balloon
  c.fillRect(x0 + 2, y0, bw - 4, bh, C_ON);
  c.fillRect(x0, y0 + 2, bw, bh - 4, C_ON);
  c.fillRect(x0 + 1, y0 + 1, bw - 2, bh - 2, C_ON);
  c.fillTriangle(58.0f, (float)(y0 + bh - 1), 68.0f, (float)(y0 + bh - 1), 61.0f, (float)(y0 + bh + 5), C_ON);
  if (k < 0.9f) return;
  // letters, one by one
  int shown = (int)((f.t - 0.4f) / 0.09f);
  if (shown > n) shown = n;
  if (shown <= 0) return;
  char part[24];
  memcpy(part, txt, (size_t)shown);
  part[shown] = 0;
  c.text(64 - tw / 2, y0 + 4, part, scale, C_OFF);
}

// ---- Fly: follows a fly until it lands on its nose -------------------------------------
static void flyAt(float t, float &x, float &y) {
  const float land = 3.2f, sit = 3.6f, leave = 4.6f;
  float wx = 64.0f + 52.0f * sinf(2.1f * t + 1.0f) + 6.0f * sinf(9.0f * t);
  float wy = 28.0f + 22.0f * sinf(3.3f * t) + 4.0f * sinf(11.0f * t + 2.0f);
  if (t < 0.4f) {
    // comes in from the top right corner
    float a = easeOut(t / 0.4f);
    x = lerpf(136.0f, wx, a);
    y = lerpf(-6.0f, wy, a);
  } else if (t < land) {
    x = wx, y = wy;
  } else if (t < sit) {
    float a = inOut((t - land) / (sit - land));
    x = lerpf(wx, 64.0f, a);
    y = lerpf(wy, 33.0f, a);
  } else if (t < leave) {
    x = 64.0f, y = 33.0f;
  } else {
    float a = easeIn(seg(t, leave, leave + 0.5f));
    x = lerpf(64.0f, 140.0f, a);
    y = lerpf(33.0f, -10.0f, a);
  }
}

void actFlyPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.35f, 0.5f);
  float x, y;
  flyAt(t, x, y);
  float sitting = inOut(seg(t, 3.3f, 3.7f)) * (1.0f - inOut(seg(t, 4.55f, 4.75f)));
  float gx = clampf((x - 64.0f) / 56.0f, -1.0f, 1.0f), gy = clampf((32.0f - y) / 26.0f, -1.0f, 1.0f);
  // the body of the eye follows a little, the pupil does the rest
  f.gazeX = gx * 0.35f * k * (1.0f - sitting);
  f.gazeY = gy * 0.35f * k * (1.0f - sitting);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    float inner = e == 0 ? 1.0f : -1.0f;
    p.pupil = 0.44f * k;
    p.pupilW = 1.0f;
    // px is measured towards the nose, the fly is measured on the screen
    p.px = lerpf(gx * inner * 0.9f, 0.95f, sitting);
    p.py = lerpf(-gy * 0.9f, 0.15f, sitting);
    p.x -= 3.0f * sitting;
  }
  // shakes it off
  float shake = bump(t, 4.5f, 5.0f);
  f.dx[0] = f.dx[1] = sinf(t * 50.0f) * 4.0f * shake;
  blinkBoth(f, bump(t, 4.75f, 5.05f));
  setEars(f, 90.0f + 30.0f * gx * k, 90.0f - 30.0f * gx * k, shake > 0.1f ? EG_FLAP : EG_TWITCH, 20.0f, 6.0f);
  if (f.crossed(3.6f)) f.chirp = CH_QUESTION;
}

void actFlyDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  if (f.t > 5.1f) return;
  float x, y;
  flyAt(f.t, x, y);
  bool sitting = f.t > 3.6f && f.t < 4.6f;
  c.fillEllipse(x, y, 4.2f, 3.6f, C_OFF);
  c.fillEllipse(x, y, 2.6f, 2.0f, C_ON);
  // wings: a blur while it flies, folded when it sits
  bool up = sitting || ((int)(f.t * 40.0f) & 1);
  float wy = up ? -3.2f : -1.2f;
  c.thickLine(x - 1.0f, y - 1.0f, x - 4.5f, y + wy, 1.2f, C_ON);
  c.thickLine(x + 1.0f, y - 1.0f, x + 4.5f, y + wy, 1.2f, C_ON);
}

// ---- Ball: watches a ball bounce across ------------------------------------------------
static const float BALL_T0 = 0.3f, BALL_T1 = 4.0f;

static void ballAt(float t, float &x, float &y, float &squash) {
  float u = seg(t, BALL_T0, BALL_T1);
  x = lerpf(-8.0f, 136.0f, u);
  float b = u * 4.0f;
  int n = (int)floorf(b);
  if (n > 3) n = 3;
  float fr = b - n;
  float h = 46.0f * powf(0.72f, (float)n);
  float up = 4.0f * fr * (1.0f - fr);
  y = 58.0f - h * up;
  squash = clamp01(1.0f - up * 6.0f);  // only near the floor
}

void actBallPose(ActFrame &f) {
  float k = envelope(f, 0.3f, 0.45f);
  float x, y, sq;
  ballAt(f.t, x, y, sq);
  f.gazeX = clampf((x - 64.0f) / 60.0f, -1.0f, 1.0f) * k;
  f.gazeY = clampf((32.0f - y) / 28.0f, -1.0f, 1.0f) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].y -= 5.0f * k;
  f.halo = true;
  setEars(f, 95.0f + f.gazeX * 25.0f, 95.0f - f.gazeX * 25.0f);
  float u = seg(f.t, BALL_T0, BALL_T1) * 4.0f, up = seg(f.tPrev, BALL_T0, BALL_T1) * 4.0f;
  if (floorf(u) != floorf(up) && u < 4.0f) f.chirp = CH_BLIP;
}

void actBallDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  if (f.t < BALL_T0 || f.t > BALL_T1) return;
  float x, y, sq;
  ballAt(f.t, x, y, sq);
  float rx = 4.5f * (1.0f + 0.35f * sq), ry = 4.5f * (1.0f - 0.35f * sq);
  FaceRenderer::drawBall(c, x, y + 4.5f - ry, rx, ry);
  // floor shadow
  c.hline((int)(x - 3.0f - 2.0f * sq), (int)(x + 3.0f + 2.0f * sq), 63, C_ON);
}

// ---- Hula: spins a hoop ------------------------------------------------------------------
static const float HULA_HZ = 1.5f;

void actHulaPose(ActFrame &f) {
  float k = envelope(f, 0.5f, 0.6f);
  float s = sinf(f.t * M_TAU * HULA_HZ), c2 = cosf(f.t * M_TAU * HULA_HZ);
  f.dx[0] = f.dx[1] = 4.5f * s * k;
  f.dy[0] = f.dy[1] = -1.5f * fabsf(c2) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].y -= 3.0f * k;
  f.halo = true;
  setEars(f, 100.0f, 100.0f, EG_FLAP, 22.0f * k, HULA_HZ);
  if (f.crossed(0.5f)) f.chirp = CH_HAPPY;
}

void actHulaDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer == ACT_POST) return;
  float t = f.t;
  float in = outBack(seg(t, 0.1f, 0.6f)), out = easeIn(seg(t, f.duration - 0.6f, f.duration));
  float cy = lerpf(-14.0f, 38.0f, in) - out * 60.0f;
  float ph = t * M_TAU * HULA_HZ;
  float cx = 64.0f - 5.0f * sinf(ph);
  float tilt = 0.10f * cosf(ph);
  const int n = 40;
  for (int i = 0; i < n; i++) {
    float a = i * (M_TAU / n) + ph * 0.5f;
    OrbitPoint p = orbitAt(a, 52.0f, 9.0f, tilt, cx, cy);
    bool front = p.z > 0.0f;
    if (front != (layer == ACT_FRONT)) continue;
    OrbitPoint q = orbitAt(a + M_TAU / n, 52.0f, 9.0f, tilt, cx, cy);
    if (front) c.thickLine(p.x, p.y, q.x, q.y, 4.2f, C_OFF);
    c.thickLine(p.x, p.y, q.x, q.y, 1.8f, C_ON);
    if (front && (i % 4) == 0) c.fillCircle(p.x, p.y, 1.0f, C_OFF);  // beads, so the spin is seen
  }
}

// ---- Dance: eyes to the beat over an equalizer -------------------------------------------
static const float BEAT_HZ = 2.0f;

void actDancePose(ActFrame &f) {
  float k = envelope(f, 0.4f, 0.5f);
  float s = sinf(f.t * M_TAU * BEAT_HZ * 0.5f);
  f.dy[0] = -6.0f * fmaxf(0.0f, s) * k;
  f.dy[1] = -6.0f * fmaxf(0.0f, -s) * k;
  f.dx[0] = f.dx[1] = 3.0f * sinf(f.t * M_TAU * BEAT_HZ * 0.25f) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].y -= 6.0f * k;
  setEars(f, 95.0f, 95.0f, EG_FLAP, 30.0f * k, BEAT_HZ * 0.5f);
  if (f.crossed(0.4f)) f.chirp = CH_HAPPY;
  if (f.crossed(2.4f)) f.chirp = CH_LAUGH;
}

void actDanceDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float k = envelope(f, 0.4f, 0.5f);
  if (k < 0.05f) return;
  for (int i = 0; i < 12; i++) {
    float v = fabsf(sinf(f.t * (3.0f + i * 0.7f) + i * 1.3f));
    // the beat kicks every bar
    float beat = 1.0f - fractf(f.t * BEAT_HZ);
    float h = (2.0f + 9.0f * v + 3.0f * beat * beat) * k;
    int x = 5 + i * 10;
    c.fillRect(x, 64 - (int)h, 7, (int)h, C_ON);
  }
}

// ---- Plant: a flower grows between the eyes ---------------------------------------------
static float plantRise(float t, float dur) { return outBack(seg(t, 0.2f, 0.6f)) * (1.0f - easeIn(seg(t, dur - 0.7f, dur - 0.2f))); }

void actPlantPose(ActFrame &f) {
  float t = f.t;
  float k = envelope(f, 0.4f, 0.5f);
  float bloom = inOut(seg(t, 2.5f, 3.2f)) * (1.0f - seg(t, f.duration - 0.8f, f.duration - 0.3f));
  f.gazeY = lerpf(-0.8f, -0.25f, seg(t, 0.9f, 2.6f)) * k;
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x += 9.0f * k;  // opens a gap for the plant
    p.y -= 9.0f * k;
    p.w *= 1.0f - 0.14f * k;
    p.h *= 1.0f - 0.22f * k;
    p.lidB += 0.30f * bloom;
    p.lidBBend += 0.35f * bloom;
  }
  f.halo = true;
  setEars(f, 90.0f + 25.0f * bloom, 90.0f + 25.0f * bloom, bloom > 0.5f ? EG_WIGGLE : EG_STILL, 12.0f, 2.0f);
  if (f.crossed(0.9f)) f.chirp = CH_QUESTION;
  if (f.crossed(2.6f)) f.chirp = CH_HAPPY;
}

void actPlantDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float rise = plantRise(t, f.duration);
  if (rise < 0.02f) return;
  float base = 64.0f + 12.0f * (1.0f - rise);  // top of the pot
  float by = base - 9.0f;
  // pot
  c.fillTriangle(55.0f, by, 73.0f, by, 58.0f, by + 9.0f, C_ON);
  c.fillTriangle(73.0f, by, 70.0f, by + 9.0f, 58.0f, by + 9.0f, C_ON);
  c.fillRect(53, (int)by - 2, 22, 3, C_ON);

  float grow = inOut(seg(t, 0.9f, 2.4f));
  if (grow <= 0.0f) return;
  float sway = sinf(t * 2.2f) * 2.0f;
  float h = 22.0f * grow;
  // stem, bending with the sway
  float px = 64.0f, py = by - 2.0f;
  for (int i = 1; i <= 10; i++) {
    float u = i / 10.0f;
    float x = 64.0f + sway * u * u, y = by - 2.0f - h * u;
    c.thickLine(px, py, x, y, 1.8f, C_ON);
    px = x;
    py = y;
  }
  // leaves
  float l1 = outBack(seg(t, 1.4f, 1.8f)), l2 = outBack(seg(t, 1.8f, 2.2f));
  if (l1 > 0.05f) c.fillEllipse(64.0f - 5.0f * l1 + sway * 0.1f, by - 2.0f - h * 0.35f, 5.0f * l1, 2.2f * l1, C_ON);
  if (l2 > 0.05f) c.fillEllipse(64.0f + 5.0f * l2 + sway * 0.3f, by - 2.0f - h * 0.6f, 5.0f * l2, 2.2f * l2, C_ON);
  // flower
  float bloom = outBack(seg(t, 2.5f, 3.1f));
  float bud = seg(t, 2.1f, 2.5f);
  if (bloom > 0.05f) {
    for (int i = 0; i < 5; i++) {
      float a = i * (M_TAU / 5.0f) + t * 0.6f;
      c.fillCircle(px + cosf(a) * 4.6f * bloom, py + sinf(a) * 4.6f * bloom, 3.0f * bloom, C_ON);
    }
    c.fillCircle(px, py, 2.4f * bloom, C_OFF);
    c.fillCircle(px, py, 1.0f * bloom, C_ON);
  } else if (bud > 0.05f) {
    c.fillCircle(px, py, 2.4f * bud, C_ON);
  }
}

// ---- Drink: sips a bubble tea ------------------------------------------------------------
void actDrinkPose(ActFrame &f) {
  float t = f.t;
  float k = envelope(f, 0.4f, 0.5f);
  float sip = inOut(seg(t, 1.0f, 1.4f)) * (1.0f - inOut(seg(t, 3.6f, 4.0f)));
  // closed and smiling while it drinks
  EyeParams arc = {9.0f, -10.0f, 32.0f, 18.0f, 9.0f, 9.0f, 0.22f, 0, -0.75f, 0.58f, 0, 0.75f, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x += 9.0f * k;
    p.y -= 9.0f * k;
    p.w *= 1.0f - 0.12f * k;
    p.h *= 1.0f - 0.15f * k;
    mixEye(p, arc, sip);
  }
  float gulp = fabsf(sinf(t * M_TAU * 1.6f)) * sip;
  f.dy[0] = f.dy[1] = -1.6f * gulp;
  f.gazeY = -0.6f * k * (1.0f - sip);
  f.halo = true;
  f.pose.mouth.w = 0.0f;
  // "ahh"
  float ahh = bump(t, 4.0f, 4.7f);
  if (ahh > 0.05f) {
    f.pose.mouth.w = 12.0f * ahh;
    f.pose.mouth.open = 0.8f;
    f.pose.mouth.curve = 2.0f;
    f.pose.mouth.y = 20.0f;
  }
  setEars(f, 95.0f, 95.0f, sip > 0.5f ? EG_WIGGLE : EG_STILL, 10.0f, 1.6f);
  if (f.crossed(1.2f)) f.chirp = CH_HAPPY;
  if (f.crossed(4.0f)) f.chirp = CH_LAUGH;
}

void actDrinkDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float rise = outBack(seg(t, 0.2f, 0.7f)) * (1.0f - easeIn(seg(t, f.duration - 1.1f, f.duration - 0.6f)));
  if (rise < 0.02f) return;
  float top = 66.0f - 24.0f * rise;  // rim of the cup
  float bottom = top + 22.0f;
  const float cx = 64.0f;
  // cup, drawn as a glass: outline only
  c.fillTriangle(cx - 11.0f, top, cx + 11.0f, top, cx - 8.0f, bottom, C_OFF);
  c.fillTriangle(cx + 11.0f, top, cx + 8.0f, bottom, cx - 8.0f, bottom, C_OFF);
  c.thickLine(cx - 10.0f, top, cx - 7.5f, bottom, 1.6f, C_ON);
  c.thickLine(cx + 10.0f, top, cx + 7.5f, bottom, 1.6f, C_ON);
  c.thickLine(cx - 7.5f, bottom, cx + 7.5f, bottom, 1.6f, C_ON);
  c.thickLine(cx - 12.0f, top, cx + 12.0f, top, 2.2f, C_ON);  // lid
  // tea going down
  float level = lerpf(top + 4.0f, bottom - 7.0f, seg(t, 1.3f, 3.6f));
  float half = lerpf(9.0f, 7.0f, (level - top) / 22.0f);
  c.hline((int)(cx - half), (int)(cx + half), (int)level, C_ON);
  // pearls
  for (int i = 0; i < 5; i++) {
    float x = cx - 5.0f + i * 2.5f, y = bottom - 3.0f - (i & 1) * 2.6f;
    // one goes up the straw with every sip
    float up = fractf(t * 1.6f + i * 0.2f);
    bool sucked = t > 1.3f && t < 3.6f && i == ((int)(t * 1.6f) % 5);
    if (sucked) {
      x = lerpf(x, cx + 3.0f, up);
      y = lerpf(y, top - 8.0f, up);
    }
    c.fillCircle(x, y, 1.5f, C_ON);
  }
  // straw
  c.thickLine(cx + 2.0f, bottom - 3.0f, cx + 5.0f, top - 11.0f, 4.0f, C_OFF);
  c.thickLine(cx + 1.0f, bottom - 3.0f, cx + 4.0f, top - 11.0f, 1.3f, C_ON);
  c.thickLine(cx + 3.6f, bottom - 3.0f, cx + 6.6f, top - 11.0f, 1.3f, C_ON);
}
