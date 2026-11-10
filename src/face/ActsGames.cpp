// Acts where the eyes become the pieces of a game.
#include "ActKit.h"
#include "ActList.h"

// ---- Pacman: the eyes turn into pellets... and get eaten -----------------------------
namespace {
const float PAC_T0 = 0.7f, PAC_T1 = 3.7f;
const float PELLET_X[5] = {16.0f, 40.0f, 64.0f, 88.0f, 112.0f};  // 1 and 3 are the eyes
const float PAC_R = 12.0f;

float pacX(float t) { return lerpf(-16.0f, 144.0f, seg(t, PAC_T0, PAC_T1)); }
// moment when pacman reaches a pellet
float eatTime(int i) { return PAC_T0 + (PELLET_X[i] - 4.0f + 16.0f) / 160.0f * (PAC_T1 - PAC_T0); }
}  // namespace

void actPacmanPose(ActFrame &f) {
  float t = f.t;
  float toPellet = inOut(seg(t, 0.0f, 0.5f));
  float back = outBack(seg(t, 4.2f, 4.7f));
  for (int e = 0; e < 2; e++) {
    int pellet = e == 0 ? 1 : 3;
    if (t < 4.2f) {
      mixEye(f.pose.eye[e], dotEye(0.0f, 0.0f, 8.0f), toPellet);
      if (t >= eatTime(pellet)) f.eyeOn[e] = false;
    } else {
      f.pose.eye[e].w *= back;
      f.pose.eye[e].h *= back;
    }
  }
  f.pose.mouth.w = 0.0f;
  // what just happened?
  f.gazeX = -0.8f * bump(t, 4.75f, 5.0f) + 0.8f * bump(t, 5.0f, 5.25f);
  blinkAt(f, 4.95f);
  float chase = seg(t, PAC_T0, PAC_T1);
  setEars(f, lerpf(80.0f, 20.0f, toPellet) + 80.0f * back, lerpf(80.0f, 20.0f, toPellet) + 80.0f * back,
          (chase > 0.0f && chase < 1.0f) ? EG_TREMBLE : EG_STILL, 6.0f, 9.0f);
  for (int i = 0; i < 5; i++) {
    if (f.crossed(eatTime(i))) f.chirp = CH_BLIP;
  }
  if (f.crossed(4.2f)) f.chirp = CH_QUESTION;
}

void actPacmanDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  // pellets that are not eyes
  float k = inOut(seg(t, 0.0f, 0.5f));
  for (int i = 0; i < 5; i += 2) {
    if (t < eatTime(i)) c.fillCircle(PELLET_X[i], EYE_ANCHOR_Y, 4.0f * k, C_ON);
  }
  if (t < PAC_T0 || t > PAC_T1) return;

  float x = pacX(t);
  float mouth = 0.12f + 0.75f * fabsf(sinf(t * 13.0f));  // half angle, radians
  int x0 = (int)floorf(x - PAC_R), x1 = (int)ceilf(x + PAC_R);
  int y0 = (int)floorf(EYE_ANCHOR_Y - PAC_R), y1 = (int)ceilf(EYE_ANCHOR_Y + PAC_R);
  for (int py = y0; py <= y1; py++) {
    for (int px = x0; px <= x1; px++) {
      float dx = px + 0.5f - x, dy = py + 0.5f - EYE_ANCHOR_Y;
      if (dx * dx + dy * dy > PAC_R * PAC_R) continue;
      if (dx > 0.0f && fabsf(atan2f(dy, dx)) < mouth) continue;
      c.set(px, py, C_ON);
    }
  }
  c.fillCircle(x + 1.5f, EYE_ANCHOR_Y - 6.5f, 1.6f, C_OFF);
}

// ---- Pong: each eye is a paddle --------------------------------------------------------
namespace {
const float PONG_T0 = 0.6f;
const float WALL_L = 13.0f, WALL_R = 115.0f;  // faces of the paddles
const float PONG_VX = 127.5f, PONG_VY = 47.0f;
const float MISS_T = 4.2f;  // third time the ball reaches the right side

void pongBall(float t, float &x, float &y) {
  float u = (t - PONG_T0);
  if (u < 0.0f) u = 0.0f;
  const float span = WALL_R - WALL_L;
  float ux = u * PONG_VX + span * 0.5f;
  float xx = fmodf(ux, span * 2.0f);
  x = WALL_L + (xx < span ? xx : span * 2.0f - xx);
  if (t > MISS_T) x = WALL_R + (t - MISS_T) * PONG_VX;  // nobody was there
  float uy = u * PONG_VY + 26.0f;
  float yy = fmodf(uy, 104.0f);
  y = 6.0f + (yy < 52.0f ? yy : 104.0f - yy);
}
}  // namespace

void actPongPose(ActFrame &f) {
  float t = f.t;
  float k = inOut(seg(t, 0.0f, 0.5f)) * (1.0f - inOut(seg(t, 4.8f, 5.3f)));
  EyeParams paddle = {33.0f, 0.0f, 5.0f, 22.0f, 6.0f, 6.0f, 0, 0, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  float bx, by, lx, ly;
  pongBall(t - 0.06f, bx, by);
  pongBall(t - 0.5f, lx, ly);  // the right one reacts late, that is why it loses
  for (int e = 0; e < 2; e++) {
    mixEye(f.pose.eye[e], paddle, k);
    float follow = e == 0 ? by : (t > 3.4f ? ly : by);
    f.dy[e] = clampf(follow - EYE_ANCHOR_Y, -20.0f, 20.0f) * k;
  }
  f.pose.mouth.w = 0.0f;
  // the loser lowers its head
  float lost = bump(t, 4.4f, 5.4f);
  f.dy[1] += 5.0f * lost * (1.0f - k);
  blinkAt(f, 5.5f);
  setEars(f, 110.0f - 70.0f * lost, 110.0f - 70.0f * lost);
  // hits: right 1.0 and 2.6, left 1.8 and 3.4
  if (f.crossed(1.0f) || f.crossed(1.8f) || f.crossed(2.6f) || f.crossed(3.4f)) f.chirp = CH_BLIP;
  if (f.crossed(4.35f)) f.chirp = CH_SAD;
}

void actPongDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  float k = inOut(seg(t, 0.0f, 0.5f)) * (1.0f - inOut(seg(t, 4.8f, 5.3f)));
  if (k < 0.5f) return;
  for (int y = 1; y < CANVAS_H; y += 6) c.vline(64, y, y + 2, C_ON);  // net
  if (t >= PONG_T0 && t < MISS_T + 0.2f) {
    float x, y;
    pongBall(t, x, y);
    c.fillCircle(x, y, 2.6f, C_ON);
  }
  if (t > MISS_T + 0.1f && t < 4.8f && fractf(t * 5.0f) < 0.6f) {
    c.text(46, 4, "1", 2, C_ON);
    c.text(72, 4, "0", 2, C_ON);
  }
}
