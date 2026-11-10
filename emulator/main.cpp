// Windows emulator of the robot.
//
// The sketch (../esp32-eyes.ino) runs unmodified on top of mocked Arduino
// libraries. This window shows what the hardware would do: the 3D robot from
// the CAD with its OLED and ears, and the panel lets you play with the sensors.
//
//   robot_emu.exe                       interactive
//   robot_emu.exe --script "<steps>"    automated run, e.g. for screenshots:
//        "2:expr=Happy; 4:shot=happy.png; 5:motion=1; 8:shot=b.png; 8:quit"
//     steps are time:command[=value]; commands: expr act say clip motion light
//     sound temp hum auto xray colors states cam=yaw,pitch,dist info shot record quit
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <string>
#include <vector>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "EmuHw.h"
#include "GifWriter.h"
#include "Scene.h"
#include "StateView.h"

#include "../Config.h"
#include "../src/body/Buzzer.h"
#include "../src/body/Ears.h"
#include "../src/brain/Brain.h"
#include "../src/brain/Sensors.h"
#include "../src/face/FaceEngine.h"

// ---- the sketch ------------------------------------------------------------------
extern void setup();
extern void loop();
extern FaceEngine face;
extern Brain brain;
extern Sensors sensors;
extern Ears ears;
extern Buzzer buzzer;

// The emulator can always make noise, even if the robot has no microphone.
#if PIN_MIC >= 0
static const int MIC_PIN = PIN_MIC;
#else
static const int MIC_PIN = -1;
#endif

// ---- layout ------------------------------------------------------------------------
static const int WIN_W = 1400, WIN_H = 880;
static const int PANEL_W = 520;

static const Color BG_TOP = {30, 34, 44, 255};
static const Color BG_BOT = {12, 13, 18, 255};
static const Color PANEL = {20, 22, 29, 255};
static const Color PANEL_2 = {30, 33, 43, 255};
static const Color LINE = {52, 58, 74, 255};
static const Color TXT = {214, 222, 236, 255};
static const Color DIM = {128, 138, 158, 255};
static const Color ACC = {96, 200, 255, 255};
static const Color OK_C = {96, 226, 140, 255};
static const Color WARN = {255, 190, 92, 255};
static const Color HOT = {255, 110, 110, 255};

static const Color MOOD_COLOR[MOOD_COUNT] = {
  {150, 200, 255, 255}, {255, 214, 102, 255}, {122, 150, 255, 255},
  {255, 104, 104, 255}, {255, 160, 84, 255},  {178, 140, 255, 255}};

// ---- state of the emulator -----------------------------------------------------------
struct Sim {
  double time = 0.0;     // simulated seconds
  float speed = 1.0f;
  bool paused = false;
  // inputs
  bool motionKey = false, motionLatch = false;
  float light = 1.0f;
  bool lightsOff = false;
  double soundUntil = 0.0;
  float temp = 24.0f, hum = 50.0f;
  // simulated servos (the real ones need time to move)
  float earL = 10.0f, earR = 10.0f;
  // camera
  float yaw = -26.0f, pitch = 14.0f, dist = 300.0f;
  Vector3 pan = {0, 0, 0};
  // recording
  GifWriter gif;
  double gifNext = 0.0;
  std::string gifPath;
  bool stateView = true;  // live state machine under the robot
  int showSel = 0;  // acts first, then clips
  int hoverNode = -1;
  std::string toast;
  double toastUntil = 0.0;
};

static Sim sim;
static Scene scene;
static StateView stateView;
static Texture2D oledTex;
static Color oledPixels[128 * 64];
static uint32_t i2cLast = 0, i2cPerSec = 0;
static double i2cStamp = 0.0;

static void toast(const std::string &s) {
  sim.toast = s;
  sim.toastUntil = GetTime() + 2.5;
}

// ---- hardware <-> simulation -----------------------------------------------------------
static void applyInputs() {
  emuHw.digitalIn[PIN_PIR] = (sim.motionKey || sim.motionLatch) ? HIGH : LOW;
  float l = sim.lightsOff ? 0.02f : sim.light;
  int raw = (int)(300.0f + 3400.0f * l);
#if LDR_BRIGHT_IS_LOW
  raw = 4000 - raw;
#endif
  emuHw.analogIn[PIN_LDR] = raw;
  if (MIC_PIN >= 0) {
    bool on = sim.time < sim.soundUntil;
    emuHw.digitalIn[MIC_PIN] = on ? MIC_ACTIVE_LEVEL : !MIC_ACTIVE_LEVEL;
  }
  emuHw.tempC = sim.temp;
  emuHw.humidity = sim.hum;
}

static void clap() {
  if (MIC_PIN >= 0) sim.soundUntil = sim.time + 0.06;
  else toast("PIN_MIC is -1 in Config.h: the robot has no microphone");
}

// A hobby servo turns about 60 degrees in 0.1 s.
static void stepServos(float dt) {
  const float slew = 600.0f * dt;
  float tl = Ears::toLogical(true, emuHw.servo[PIN_SERVO_LEFT].angle);
  float tr = Ears::toLogical(false, emuHw.servo[PIN_SERVO_RIGHT].angle);
  if (emuHw.servo[PIN_SERVO_LEFT].everUsed) sim.earL += Clamp(tl - sim.earL, -slew, slew);
  if (emuHw.servo[PIN_SERVO_RIGHT].everUsed) sim.earR += Clamp(tr - sim.earR, -slew, slew);
}

static void panelToBits(uint8_t *bits) {
  memset(bits, 0, 1024);
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++)
      if (emuHw.panel[(y >> 3) * 128 + x] & (1 << (y & 7))) bits[y * 16 + (x >> 3)] |= (uint8_t)(1 << (x & 7));
}

static void stepSim(double seconds) {
  const double step = 0.004;
  double end = sim.time + seconds;
  while (sim.time < end) {
    sim.time += step;
    emuHw.clockMs = sim.time * 1000.0;
    applyInputs();
    loop();
    stepServos((float)step);
    if (sim.gif.isOpen() && sim.time >= sim.gifNext) {
      uint8_t bits[1024];
      panelToBits(bits);
      sim.gif.frame(bits, 40);
      sim.gifNext += 0.040;
    }
  }
}

static void updateOledTexture() {
  const Color on = {200, 236, 255, 255};
  const Color off = {3, 5, 9, 255};
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++)
      oledPixels[y * 128 + x] = (emuHw.panel[(y >> 3) * 128 + x] & (1 << (y & 7))) ? on : off;
  UpdateTexture(oledTex, oledPixels);
}

static void toggleRecording() {
  if (sim.gif.isOpen()) {
    int n = sim.gif.frames();
    sim.gif.close();
    toast(TextFormat("saved %s (%d frames)", sim.gifPath.c_str(), n));
    return;
  }
  MakeDirectory("captures");
  time_t t = time(nullptr);
  struct tm *tmv = localtime(&t);
  char name[96];
  strftime(name, sizeof(name), "captures/oled_%Y%m%d_%H%M%S.gif", tmv);
  sim.gifPath = name;
  if (sim.gif.open(name, 128, 64, 4)) {
    sim.gifNext = sim.time;
    toast("recording the OLED... press R to stop");
  }
}

static void saveShot(const char *path) {
  Image img = LoadImageFromScreen();
  ExportImage(img, path);
  UnloadImage(img);
}

// ---- camera -----------------------------------------------------------------------------
static Camera3D makeCamera(int viewW, int viewH, int winW, int winH) {
  Camera3D cam = {0};
  cam.fovy = 34.0f;
  cam.projection = CAMERA_PERSPECTIVE;
  cam.up = {0, 0, 1};
  float yaw = sim.yaw * DEG2RAD, pitch = sim.pitch * DEG2RAD;
  // yaw 0 looks at the face of the robot, which points to -Y
  Vector3 dir = {sinf(yaw) * cosf(pitch), -cosf(yaw) * cosf(pitch), sinf(pitch)};
  Vector3 target = Vector3Add(scene.loaded() ? scene.center() : Vector3{-34, 20, -20}, sim.pan);
  // A shorter area shows a smaller robot: step back.
  float dist = sim.dist * powf((float)winH / (float)viewH, 0.9f);
  Vector3 pos = Vector3Add(target, Vector3Scale(dir, dist));

  // The panels cover the right side and the bottom of the window: slide the
  // camera so the robot sits in the middle of the free area.
  float aspect = (float)winW / (float)winH;
  float half = dist * tanf(cam.fovy * 0.5f * DEG2RAD);
  float ndcX = ((float)viewW * 0.5f - (float)winW * 0.5f) / ((float)winW * 0.5f);
  float ndcY = 1.0f - (float)viewH / (float)winH;  // up is positive
  Vector3 fwd = Vector3Normalize(Vector3Subtract(target, pos));
  Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, cam.up));
  Vector3 up = Vector3CrossProduct(right, fwd);
  Vector3 shift = Vector3Add(Vector3Scale(right, -ndcX * half * aspect), Vector3Scale(up, -ndcY * half));
  cam.position = Vector3Add(pos, shift);
  cam.target = Vector3Add(target, shift);
  return cam;
}

// ---- tiny immediate mode widgets ----------------------------------------------------------
static void label(const char *s, int x, int y, Color c = TXT, int size = 10) { DrawText(s, x, y, size, c); }

static bool hit(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

static bool button(Rectangle r, const char *text, bool active = false, Color accent = ACC) {
  bool over = hit(r);
  Color bg = active ? Fade(accent, 0.30f) : (over ? Color{44, 49, 64, 255} : PANEL_2);
  DrawRectangleRounded(r, 0.25f, 6, bg);
  DrawRectangleRoundedLines(r, 0.25f, 6, active ? accent : LINE);
  int w = MeasureText(text, 10);
  DrawText(text, (int)(r.x + (r.width - w) / 2), (int)(r.y + (r.height - 10) / 2), 10, active ? WHITE : TXT);
  return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int dragId = 0;
static bool slider(int id, Rectangle r, float *v, float lo, float hi, Color accent = ACC) {
  bool over = hit(r);
  if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dragId = id;
  if (dragId == id && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragId = 0;
  bool changed = false;
  if (dragId == id) {
    float t = Clamp((GetMousePosition().x - r.x) / r.width, 0.0f, 1.0f);
    *v = lo + (hi - lo) * t;
    changed = true;
  }
  float t = Clamp((*v - lo) / (hi - lo), 0.0f, 1.0f);
  float cy = r.y + r.height / 2;
  DrawRectangleRounded({r.x, cy - 3, r.width, 6}, 1.0f, 4, PANEL_2);
  DrawRectangleRounded({r.x, cy - 3, r.width * t, 6}, 1.0f, 4, Fade(accent, 0.8f));
  DrawCircle((int)(r.x + r.width * t), (int)cy, (over || dragId == id) ? 7.0f : 6.0f, accent);
  return changed;
}

static void bar(Rectangle r, float t, Color c) {
  DrawRectangleRounded(r, 1.0f, 4, PANEL_2);
  if (t > 0.01f) DrawRectangleRounded({r.x, r.y, r.width * Clamp(t, 0.0f, 1.0f), r.height}, 1.0f, 4, c);
}

static void led(int x, int y, bool on, Color c) {
  DrawCircle(x, y, 6, on ? c : Color{46, 50, 62, 255});
  if (on) DrawCircle(x, y, 9, Fade(c, 0.25f));
}

static void section(const char *title, int x, int y, int w) {
  DrawText(title, x, y, 10, DIM);
  int tw = MeasureText(title, 10);
  DrawLine(x + tw + 8, y + 5, x + w, y + 5, LINE);
}

// ---- graph map --------------------------------------------------------------------------------
static Vector2 nodePos[EX_COUNT];

static void layoutGraph(Rectangle r) {
  static const Vector2 CENTER[MOOD_COUNT] = {{0.50f, 0.40f}, {0.13f, 0.32f}, {0.87f, 0.30f},
                                             {0.85f, 0.80f}, {0.15f, 0.82f}, {0.50f, 0.92f}};
  static const Vector2 RAD[MOOD_COUNT] = {{0.21f, 0.34f}, {0.115f, 0.27f}, {0.105f, 0.24f},
                                          {0.09f, 0.17f}, {0.10f, 0.15f},  {0.10f, 0.0f}};
  static const float START[MOOD_COUNT] = {-90, -70, -110, -60, -120, 0};
  for (int m = 0; m < MOOD_COUNT; m++) {
    uint8_t hub = ExpressionGraph::hub((uint8_t)m);
    std::vector<int> members;
    for (int i = 0; i < EX_COUNT; i++)
      if (Expressions::get((uint8_t)i).mood == m && i != hub) members.push_back(i);
    nodePos[hub] = {r.x + CENTER[m].x * r.width, r.y + CENTER[m].y * r.height};
    int n = (int)members.size();
    for (int k = 0; k < n; k++) {
      float x, y;
      if (m == MOOD_SLEEPY) {
        // a row, the hub in the middle
        int step = k / 2 + 1;
        x = CENTER[m].x + ((k & 1) ? RAD[m].x : -RAD[m].x) * step;
        y = CENTER[m].y;
      } else {
        // crowded clusters use two rings
        float ring = (n > 9 && (k & 1)) ? 0.6f : 1.0f;
        float a = (START[m] + 360.0f * k / n) * DEG2RAD;
        x = CENTER[m].x + cosf(a) * RAD[m].x * ring;
        y = CENTER[m].y + sinf(a) * RAD[m].y * ring;
      }
      nodePos[members[k]] = {r.x + x * r.width, r.y + y * r.height};
    }
  }
}

static bool onRoute(uint8_t a, uint8_t b) {
  if (!face.busy()) return false;
  uint8_t prev = face.current();
  // hop in progress
  if (face.moving()) {
    uint8_t h = face.heading();
    if ((prev == a && h == b) || (prev == b && h == a)) return true;
    prev = h;
  }
  for (int i = face.pathPosition(); i < face.pathLength(); i++) {
    uint8_t n = face.pathNode(i);
    if ((prev == a && n == b) || (prev == b && n == a)) return true;
    prev = n;
  }
  return false;
}

static void drawGraph(Rectangle r) {
  DrawRectangleRounded(r, 0.03f, 6, {14, 15, 20, 255});
  layoutGraph({r.x + 14, r.y + 12, r.width - 28, r.height - 26});

  for (int i = 0; i < ExpressionGraph::edgeCount(); i++) {
    const GraphEdge &e = ExpressionGraph::edge(i);
    bool hubs = Expressions::get(e.a).mood != Expressions::get(e.b).mood;
    DrawLineEx(nodePos[e.a], nodePos[e.b], 1.0f, hubs ? Color{60, 66, 84, 255} : Color{44, 49, 64, 255});
  }
  for (int i = 0; i < ExpressionGraph::edgeCount(); i++) {
    const GraphEdge &e = ExpressionGraph::edge(i);
    if (onRoute(e.a, e.b)) DrawLineEx(nodePos[e.a], nodePos[e.b], 2.5f, ACC);
  }
  // the traveller
  if (face.moving()) {
    Vector2 a = nodePos[face.current()], b = nodePos[face.heading()];
    float t = Clamp(face.progress(), 0.0f, 1.0f);
    DrawCircleV({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}, 4.0f, WHITE);
  }

  sim.hoverNode = -1;
  Vector2 mp = GetMousePosition();
  for (int i = 0; i < EX_COUNT; i++) {
    if (Vector2Distance(mp, nodePos[i]) < 9.0f) sim.hoverNode = i;
  }

  for (int i = 0; i < EX_COUNT; i++) {
    const ExpressionDef &d = Expressions::get((uint8_t)i);
    bool hub = ExpressionGraph::hub(d.mood) == i;
    Color c = MOOD_COLOR[d.mood];
    bool cur = !face.clipActive() && face.heading() == i;
    if (cur && face.actActive()) DrawRing(nodePos[i], 11.0f, 13.0f, (float)GetTime() * 200.0f,
                                         (float)GetTime() * 200.0f + 250.0f, 24, ACC);
    bool tgt = face.busy() && face.target() == i;
    float rad = hub ? 7.0f : 4.5f;
    if (cur) DrawCircleV(nodePos[i], rad + 6.0f, Fade(c, 0.28f));
    DrawCircleV(nodePos[i], rad, cur ? WHITE : Fade(c, hub ? 1.0f : 0.75f));
    if (tgt || sim.hoverNode == i) DrawRing(nodePos[i], rad + 3.0f, rad + 4.5f, 0, 360, 24, tgt ? ACC : WHITE);
    if (hub || cur || tgt || sim.hoverNode == i) {
      int w = MeasureText(d.name, 10);
      int tx = (int)nodePos[i].x - w / 2, ty = (int)nodePos[i].y + (int)rad + 5;
      if (tx < r.x + 2) tx = (int)r.x + 2;
      if (tx + w > r.x + r.width - 2) tx = (int)(r.x + r.width) - w - 2;
      DrawRectangle(tx - 2, ty - 1, w + 4, 12, {14, 15, 20, 220});
      DrawText(d.name, tx, ty, 10, (cur || tgt) ? WHITE : (hub ? c : TXT));
    }
  }
  if (sim.hoverNode >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hit(r)) {
    brain.force((uint8_t)sim.hoverNode, 12000);
  }
}

static void playSelected() {
  if (sim.showSel < Acts::count()) brain.forceAct(sim.showSel);
  else brain.forceClip(sim.showSel - Acts::count());
}

// ---- panel ----------------------------------------------------------------------------------
static void drawGauge(int cx, int cy, float angle, float target, bool attached, const char *name) {
  const float r = 26.0f;
  DrawCircleSector({(float)cx, (float)cy}, r, 180, 360, 24, PANEL_2);
  DrawCircleSectorLines({(float)cx, (float)cy}, r, 180, 360, 24, LINE);
  auto hand = [&](float a, Color c, float w) {
    float rad = (180.0f + a) * DEG2RAD;
    DrawLineEx({(float)cx, (float)cy}, {cx + cosf(rad) * (r - 3), cy + sinf(rad) * (r - 3)}, w, c);
  };
  hand(target, Fade(ACC, 0.45f), 2.0f);
  hand(angle, attached ? OK_C : DIM, 3.0f);
  DrawCircle(cx, cy, 3, TXT);
  label(name, cx + 34, cy - 26, TXT);
  label(TextFormat("servo %3d deg", (int)target), cx + 34, cy - 12, DIM);
  label(attached ? "attached" : "released", cx + 34, cy + 2, attached ? OK_C : DIM);
}

static void drawPanel(int x0, int winH) {
  DrawRectangle(x0, 0, PANEL_W, winH, PANEL);
  DrawLine(x0, 0, x0, winH, LINE);
  const int x = x0 + 16, w = PANEL_W - 32;
  int y = 12;

  DrawText("ROBOT EYES", x, y, 20, TXT);
  DrawText("emulator", x + 142, y + 8, 10, DIM);
  const char *clock = TextFormat("%s  t=%.1fs  x%.0f   %d fps", sim.paused ? "PAUSED" : "running", sim.time,
                                 sim.speed, GetFPS());
  label(clock, x + w - MeasureText(clock, 10), y + 8, sim.paused ? WARN : DIM);
  y += 32;

  // ---- OLED
  const int scale = 3;
  Rectangle o = {(float)(x + (w - 128 * scale) / 2), (float)y, 128.0f * scale, 64.0f * scale};
  DrawRectangleRounded({o.x - 8, o.y - 8, o.width + 16, o.height + 16}, 0.04f, 6, {8, 9, 12, 255});
  DrawTexturePro(oledTex, {0, 0, 128, 64}, o, {0, 0}, 0, WHITE);
  // pixel grid, as seen on the glass
  for (int i = 1; i < 128; i++) DrawLine((int)o.x + i * scale, (int)o.y, (int)o.x + i * scale, (int)(o.y + o.height), {0, 0, 0, 60});
  for (int j = 1; j < 64; j++) DrawLine((int)o.x, (int)o.y + j * scale, (int)(o.x + o.width), (int)o.y + j * scale, {0, 0, 0, 60});
  if (sim.gif.isOpen()) {
    DrawCircle((int)o.x + 10, (int)o.y + 10, 5, fmod(GetTime(), 1.0) < 0.5 ? HOT : Fade(HOT, 0.3f));
    label(TextFormat("REC %d", sim.gif.frames()), (int)o.x + 20, (int)o.y + 5, HOT);
  }
  y += 64 * scale + 12;
  float ms = i2cPerSec * 9.0f / (float)emuHw.i2cHz * 1000.0f / 30.0f;
  label(TextFormat("SSD1306 128x64   I2C %u bytes/s   ~%.1f ms per frame at %u kHz", i2cPerSec, ms, emuHw.i2cHz / 1000), x, y, DIM);
  y += 20;

  // ---- state
  section("STATE", x, y, x0 + PANEL_W - 16 - x);
  y += 16;
  uint8_t mood = brain.mood();
  label("mood", x, y, DIM);
  label(Expressions::moodName(mood), x + 60, y, MOOD_COLOR[mood]);
  label("doing", x + 170, y, DIM);
  label(brain.autonomous() ? brain.activityName() : "manual (auto off)", x + 215, y, brain.autonomous() ? TXT : WARN);
  label("alone", x + 360, y, DIM);
  label(TextFormat("%us", brain.aloneMs() / 1000), x + 400, y, TXT);
  y += 16;
  label("face", x, y, DIM);
  if (face.clipActive()) {
    label(TextFormat("clip %s  frame %d/%d", face.clip()->name, face.clipFrame() + 1, face.clip()->frames), x + 60, y, ACC);
  } else if (face.actActive()) {
    label(TextFormat("act %s  %.1f / %.1f s   (on %s)", face.act()->name, face.actTime(), face.act()->duration,
                     Expressions::name(face.current())),
          x + 60, y, ACC);
  } else {
    std::string route = Expressions::name(face.current());
    if (face.busy()) {
      if (face.moving()) route += std::string(" > ") + Expressions::name(face.heading());
      for (int i = face.pathPosition(); i < face.pathLength(); i++) route += std::string(" > ") + Expressions::name(face.pathNode(i));
    }
    label(route.c_str(), x + 60, y, face.busy() ? ACC : TXT);
  }
  y += 16;
  label("social", x, y, DIM);
  bar({(float)x + 60, (float)y + 2, 150, 6}, brain.social(), MOOD_COLOR[MOOD_HAPPY]);
  label("annoy", x + 240, y, DIM);
  bar({(float)x + 290, (float)y + 2, 150, 6}, brain.annoy() / 1.2f, MOOD_COLOR[MOOD_ANGRY]);
  y += 20;

  // ---- graph
  section("EMOTION GRAPH   click a face to go there", x, y, x0 + PANEL_W - 16 - x);
  y += 14;
  drawGraph({(float)x, (float)y, (float)w, 212});
  y += 218;

  // ---- shows: acts and clips
  {
    int total = Acts::count() + Clips::count();
    label("show", x, y + 7, DIM);
    if (button({(float)x + 40, (float)y, 24, 24}, "<")) sim.showSel = (sim.showSel + total - 1) % total;
    if (sim.showSel < Acts::count()) {
      const ActDef &a = Acts::get(sim.showSel);
      label(TextFormat("%d/%d  act  %s", sim.showSel + 1, total, a.name), x + 72, y + 2, TXT);
      label(TextFormat("computed, %.1f s, starts on %s", a.duration,
                       a.base < EX_COUNT ? Expressions::name(a.base) : "any face"),
            x + 72, y + 14, DIM);
    } else {
      const ClipDef &c = Clips::get(sim.showSel - Acts::count());
      label(TextFormat("%d/%d  clip  %s", sim.showSel + 1, total, c.name), x + 72, y + 2, TXT);
      label(TextFormat("stored GIF, %d frames, %.1f KB", c.frames, c.size / 1024.0f), x + 72, y + 14, DIM);
    }
    if (button({(float)x + w - 100, (float)y, 24, 24}, ">")) sim.showSel = (sim.showSel + 1) % total;
    if (button({(float)x + w - 70, (float)y, 70, 24}, "PLAY [0]")) playSelected();
    y += 30;
  }

  // ---- sensors
  section("SENSORS", x, y, x0 + PANEL_W - 16 - x);
  y += 16;
  const SensorState &s = sensors.state();
  int row = 26;

  led(x + 8, y + 11, s.motion, OK_C);
  label("PIR motion", x + 24, y + 6, TXT);
  label(TextFormat("GPIO%d", PIN_PIR), x + 110, y + 6, DIM);
  if (button({(float)x + 170, (float)y, 110, 22}, "hold  [M]", sim.motionKey, OK_C)) sim.motionLatch = !sim.motionLatch;
  if (button({(float)x + 288, (float)y, 110, 22}, sim.motionLatch ? "latched ON" : "latch", sim.motionLatch, OK_C)) sim.motionLatch = !sim.motionLatch;
  y += row;

  led(x + 8, y + 11, sim.time < sim.soundUntil + 0.15, WARN);
  label("Sound", x + 24, y + 6, TXT);
#ifdef EMU_VIRTUAL_MIC
  label(TextFormat("GPIO%d virtual", MIC_PIN), x + 84, y + 6, DIM);
#else
  label(MIC_PIN >= 0 ? TextFormat("GPIO%d", MIC_PIN) : "not wired", x + 110, y + 6, MIC_PIN >= 0 ? DIM : WARN);
#endif
  if (button({(float)x + 170, (float)y, 110, 22}, "clap  [S]", false, WARN)) clap();
  y += row;

  led(x + 8, y + 11, !s.dark, {255, 236, 150, 255});
  label("Light", x + 24, y + 6, TXT);
  label(TextFormat("GPIO%d", PIN_LDR), x + 110, y + 6, DIM);
  slider(1, {(float)x + 170, (float)y, 170, 22}, &sim.light, 0.0f, 1.0f, {255, 236, 150, 255});
  if (button({(float)x + 352, (float)y, 56, 22}, sim.lightsOff ? "OFF [L]" : "on [L]", sim.lightsOff, WARN)) sim.lightsOff = !sim.lightsOff;
  label(TextFormat("adc %4d  %s", s.lightRaw, s.dark ? "dark" : "bright"), x + 416, y + 6, DIM);
  y += row;

  led(x + 8, y + 11, s.climateValid, ACC);
  label("Temp", x + 24, y + 6, TXT);
  label(TextFormat("GPIO%d", PIN_DHT), x + 110, y + 6, DIM);
  slider(2, {(float)x + 170, (float)y, 170, 22}, &sim.temp, 0.0f, 45.0f, sim.temp >= HOT_ABOVE_C ? HOT : (sim.temp <= COLD_BELOW_C ? ACC : OK_C));
  label(TextFormat("%.1f C", sim.temp), x + 352, y + 6, TXT);
  label(TextFormat("read %.0f C", s.tempC), x + 416, y + 6, DIM);
  y += row;

  label("Humidity", x + 24, y + 6, TXT);
  slider(3, {(float)x + 170, (float)y, 170, 22}, &sim.hum, 0.0f, 100.0f, ACC);
  label(TextFormat("%.0f %%", sim.hum), x + 352, y + 6, TXT);
  if (button({(float)x + 416, (float)y, 72, 22}, "show  [I]")) brain.requestInfo();
  y += row + 2;

  // ---- actuators
  section("ACTUATORS", x, y, x0 + PANEL_W - 16 - x);
  y += 44;
  drawGauge(x + 30, y, sim.earL, Ears::toLogical(true, emuHw.servo[PIN_SERVO_LEFT].angle),
            emuHw.servo[PIN_SERVO_LEFT].attached, TextFormat("left ear  GPIO%d", PIN_SERVO_LEFT));
  label(TextFormat("ear %3.0f deg", sim.earL), x + 64, y + 16, TXT);
  drawGauge(x + 200, y, sim.earR, Ears::toLogical(false, emuHw.servo[PIN_SERVO_RIGHT].angle),
            emuHw.servo[PIN_SERVO_RIGHT].attached, TextFormat("right ear GPIO%d", PIN_SERVO_RIGHT));
  label(TextFormat("ear %3.0f deg", sim.earR), x + 234, y + 16, TXT);
  led(x + 370, y - 16, emuHw.buzzerHz > 0, WARN);
  label(TextFormat("buzzer GPIO%d", PIN_BUZZER), x + 384, y - 21, TXT);
  label(emuHw.buzzerHz > 0 ? TextFormat("%d Hz", emuHw.buzzerHz) : (buzzer.muted() ? "muted  [N]" : "silent"), x + 384, y - 7, DIM);
  y += 32;

  // ---- help
  DrawLine(x, y, x0 + PANEL_W - 16, y, LINE);
  y += 7;
  label("1-9 faces   TAB next face   [ ] choose show   0 play it   Y yes   U no   H hola   A auto on/off", x, y, DIM);
  y += 14;
  label("Q states   X x-ray   C colors   R record GIF   P shot   F fast   ENTER pause   ALT eyes follow mouse", x, y, DIM);
}

// ---- script (automated runs) --------------------------------------------------------------------
struct Step {
  double t;
  std::string cmd, val;
  bool done = false;
};

static std::vector<Step> parseScript(const char *s) {
  std::vector<Step> out;
  std::string all = s;
  size_t pos = 0;
  while (pos < all.size()) {
    size_t end = all.find(';', pos);
    if (end == std::string::npos) end = all.size();
    std::string item = all.substr(pos, end - pos);
    pos = end + 1;
    size_t a = item.find_first_not_of(" \t");
    if (a == std::string::npos) continue;
    item = item.substr(a);
    size_t colon = item.find(':');
    if (colon == std::string::npos) continue;
    Step st;
    st.t = atof(item.substr(0, colon).c_str());
    std::string rest = item.substr(colon + 1);
    size_t eq = rest.find('=');
    st.cmd = rest.substr(0, eq);
    st.val = eq == std::string::npos ? "" : rest.substr(eq + 1);
    while (!st.val.empty() && st.val.back() == ' ') st.val.pop_back();
    while (!st.cmd.empty() && st.cmd.back() == ' ') st.cmd.pop_back();
    out.push_back(st);
  }
  std::stable_sort(out.begin(), out.end(), [](const Step &a, const Step &b) { return a.t < b.t; });
  return out;
}

static std::string pendingShot;
static bool wantQuit = false;

static void runStep(const Step &st) {
  const char *v = st.val.c_str();
  if (st.cmd == "expr") {
    uint8_t e = Expressions::find(v);
    if (e != EX_NONE) brain.force(e, 600000);
    else fprintf(stderr, "script: unknown expression %s\n", v);
  } else if (st.cmd == "clip") {
    int c = Clips::find(v);
    if (c >= 0) brain.forceClip(c);
  } else if (st.cmd == "act") {
    int a = Acts::find(v);
    if (a >= 0) brain.forceAct(a);
    else fprintf(stderr, "script: unknown act %s\n", v);
  } else if (st.cmd == "say") brain.say(v);
  else if (st.cmd == "motion") sim.motionLatch = atoi(v) != 0;
  else if (st.cmd == "light") sim.light = (float)atof(v), sim.lightsOff = false;
  else if (st.cmd == "sound") clap();
  else if (st.cmd == "temp") sim.temp = (float)atof(v);
  else if (st.cmd == "hum") sim.hum = (float)atof(v);
  else if (st.cmd == "auto") brain.setAutonomous(atoi(v) != 0);
  else if (st.cmd == "xray") scene.setSkin(atoi(v));
  else if (st.cmd == "states") sim.stateView = atoi(v) != 0;
  else if (st.cmd == "colors") scene.setRealColors(atoi(v) != 0);
  else if (st.cmd == "info") brain.requestInfo();
  else if (st.cmd == "cam") sscanf(v, "%f,%f,%f", &sim.yaw, &sim.pitch, &sim.dist);
  else if (st.cmd == "shot") pendingShot = st.val;
  else if (st.cmd == "record") toggleRecording();
  else if (st.cmd == "quit") wantQuit = true;
  else fprintf(stderr, "script: unknown command %s\n", st.cmd.c_str());
}

// ---- buzzer sound ------------------------------------------------------------------------------
static void audioCallback(void *buffer, unsigned int frames) {
  static float phase = 0.0f;
  static float gain = 0.0f;
  short *out = (short *)buffer;
  for (unsigned int i = 0; i < frames; i++) {
    int hz = emuHw.buzzerHz;
    float want = hz > 0 ? 1.0f : 0.0f;
    gain += (want - gain) * 0.01f;  // no clicks
    if (hz > 0) phase += (float)hz / 44100.0f;
    if (phase >= 1.0f) phase -= 1.0f;
    // a piezo is a square wave with the edges rounded off
    float sq = phase < 0.5f ? 1.0f : -1.0f;
    float sn = sinf(phase * 2.0f * PI);
    out[i] = (short)((sq * 0.35f + sn * 0.65f) * gain * 5200.0f);
  }
}

// ---- input --------------------------------------------------------------------------------------
static void handleKeys(int viewW, int viewH) {
  static const uint8_t FAV[9] = {EX_NORMAL, EX_HAPPY, EX_SAD, EX_ANGRY, EX_SURPRISED,
                                 EX_ASLEEP, EX_LOVE,  EX_DIZZY, EX_LAUGH};
  for (int i = 0; i < 9; i++)
    if (IsKeyPressed(KEY_ONE + i)) brain.force(FAV[i], 12000);
  if (IsKeyPressed(KEY_ZERO)) playSelected();
  {
    int total = Acts::count() + Clips::count();
    if (IsKeyPressed(KEY_LEFT_BRACKET)) sim.showSel = (sim.showSel + total - 1) % total;
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) sim.showSel = (sim.showSel + 1) % total;
  }
  if (IsKeyPressed(KEY_Y)) brain.forceAct(Acts::find("Yes"));
  if (IsKeyPressed(KEY_U)) brain.forceAct(Acts::find("No"));
  if (IsKeyPressed(KEY_H)) brain.say("HOLA!");
  if (IsKeyPressed(KEY_TAB)) {
    int d = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? EX_COUNT - 1 : 1;
    brain.force((uint8_t)((face.target() + d) % EX_COUNT), 12000);
  }
  if (IsKeyPressed(KEY_SPACE)) face.blink();
  if (IsKeyPressed(KEY_A)) {
    brain.setAutonomous(!brain.autonomous());
    toast(brain.autonomous() ? "autonomous behaviour ON" : "autonomous behaviour OFF");
  }
  if (IsKeyPressed(KEY_X)) {
    scene.setSkin(scene.skin() + 1);
    static const char *N[] = {"solid", "x-ray", "inside only"};
    toast(N[scene.skin()]);
  }
  if (IsKeyPressed(KEY_C)) {
    scene.setRealColors(!scene.realColors());
    toast(scene.realColors() ? "colors of the printed robot" : "colors of the CAD");
  }
  if (IsKeyPressed(KEY_Q)) sim.stateView = !sim.stateView;
  if (IsKeyPressed(KEY_R)) toggleRecording();
  if (IsKeyPressed(KEY_P)) {
    MakeDirectory("captures");
    time_t t = time(nullptr);
    char name[96];
    strftime(name, sizeof(name), "captures/shot_%Y%m%d_%H%M%S.png", localtime(&t));
    pendingShot = name;
  }
  if (IsKeyPressed(KEY_N)) buzzer.setMuted(!buzzer.muted());
  if (IsKeyPressed(KEY_I)) brain.requestInfo();
  if (IsKeyPressed(KEY_L)) sim.lightsOff = !sim.lightsOff;
  if (IsKeyPressed(KEY_S)) clap();
  if (IsKeyPressed(KEY_ENTER)) sim.paused = !sim.paused;
  sim.motionKey = IsKeyDown(KEY_M);
  sim.speed = IsKeyDown(KEY_F) ? 20.0f : 1.0f;

  // mouse on the 3D view
  Vector2 m = GetMousePosition();
  bool inView = m.x < viewW && m.y < viewH && dragId == 0;
  if (inView) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsKeyDown(KEY_LEFT_ALT)) {
      Vector2 d = GetMouseDelta();
      sim.yaw -= d.x * 0.35f;
      sim.pitch = Clamp(sim.pitch + d.y * 0.3f, -12.0f, 85.0f);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
      Vector2 d = GetMouseDelta();
      float k = sim.dist * 0.0016f;
      float yaw = sim.yaw * DEG2RAD;
      sim.pan.x -= cosf(yaw) * d.x * k;
      sim.pan.y -= sinf(yaw) * d.x * k;
      sim.pan.z += d.y * k;
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) sim.dist = Clamp(sim.dist * (1.0f - wheel * 0.1f), 90.0f, 700.0f);
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) sim.yaw = -26, sim.pitch = 14, sim.dist = 300, sim.pan = {0, 0, 0};
    if (IsKeyDown(KEY_LEFT_ALT)) {
      float gx = (m.x / viewW) * 2.0f - 1.0f, gy = 1.0f - (m.y / viewH) * 2.0f;
      face.lookAt(gx * 1.3f, gy * 1.3f, 250);
    }
  }
}

// -------------------------------------------------------------------------------------------------
int main(int argc, char **argv) {
  std::vector<Step> script;
  const char *assets = "assets/robot";
  bool mute = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--script") && i + 1 < argc) script = parseScript(argv[++i]);
    else if (!strcmp(argv[i], "--assets") && i + 1 < argc) assets = argv[++i];
    else if (!strcmp(argv[i], "--mute")) mute = true;
  }
  bool scripted = !script.empty();

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
  InitWindow(WIN_W, WIN_H, "Robot Eyes - ESP32 emulator");
  SetWindowMinSize(1100, 760);
  SetTargetFPS(60);
  SetExitKey(KEY_NULL);

  // Run from anywhere: the assets live next to the executable.
  ChangeDirectory(GetApplicationDirectory());
  scene.load(assets);

  Image img = GenImageColor(128, 64, BLACK);
  oledTex = LoadTextureFromImage(img);
  UnloadImage(img);
  SetTextureFilter(oledTex, TEXTURE_FILTER_POINT);

  AudioStream stream = {0};
  bool audio = false;
  if (!mute && !scripted) {
    InitAudioDevice();
    if (IsAudioDeviceReady()) {
      SetAudioStreamBufferSizeDefault(1024);
      stream = LoadAudioStream(44100, 16, 1);
      SetAudioStreamCallback(stream, audioCallback);
      PlayAudioStream(stream);
      audio = true;
    }
  }

  applyInputs();
  setup();
  sim.time = emuHw.clockMs / 1000.0;

  while (!WindowShouldClose() && !wantQuit) {
    int winW = GetScreenWidth(), winH = GetScreenHeight();
    int viewW = winW - PANEL_W;
    int statesH = sim.stateView ? StateView::HEIGHT : 0;
    if (statesH > winH - 200) statesH = winH - 200;
    int viewH = winH - statesH;

    if (scripted) {
      // as fast as possible up to the next step
      double until = sim.time + 1.0 / 30.0;
      for (auto &st : script) {
        if (!st.done) {
          until = st.t;
          break;
        }
      }
      if (until > sim.time) stepSim(until - sim.time);
      for (auto &st : script) {
        if (!st.done && st.t <= sim.time + 1e-6) {
          st.done = true;
          runStep(st);
          if (!pendingShot.empty() || wantQuit) break;
        }
      }
      bool left = false;
      for (auto &st : script) left |= !st.done;
      if (!left && pendingShot.empty()) wantQuit = true;
    } else {
      handleKeys(viewW, viewH);
      if (!sim.paused) stepSim(fmin(GetFrameTime(), 0.05f) * sim.speed);
    }

    if (sim.time - i2cStamp >= 0.5) {
      i2cPerSec = (uint32_t)((emuHw.i2cBytes - i2cLast) / (sim.time - i2cStamp));
      i2cLast = emuHw.i2cBytes;
      i2cStamp = sim.time;
    }
    updateOledTexture();

    BeginDrawing();
    ClearBackground(BG_BOT);
    DrawRectangleGradientV(0, 0, viewW, winH, BG_TOP, BG_BOT);

    if (scene.loaded()) {
      Camera3D cam = makeCamera(viewW, viewH, winW, winH);
      // The default near plane (1 cm... of millimetres) wastes the depth
      // buffer: parts that are 0.1 mm apart would flicker.
      rlSetClipPlanes(20.0, 3000.0);
      BeginMode3D(cam);
      scene.draw(cam, sim.earL, sim.earR, oledTex, 1.0f);
      EndMode3D();
    } else {
      label("3D model not found: run tools/export_cad.py (see README)", 24, 24, WARN, 20);
    }

    // overlay of the 3D view
    label(TextFormat("%s   %s", Expressions::name(face.heading()), Expressions::moodName(brain.mood())), 18, 14,
          MOOD_COLOR[brain.mood()], 20);
    static const char *SKIN[] = {"solid", "x-ray", "inside"};
    label(TextFormat("view: %s  |  %s colors  |  %d parts, %dk triangles from the CAD", SKIN[scene.skin()],
                     scene.realColors() ? "real" : "CAD", scene.partCount(), scene.triangles() / 1000),
          18, viewH - 22, DIM);
    if (GetTime() < sim.toastUntil) {
      int tw = MeasureText(sim.toast.c_str(), 20);
      DrawRectangleRounded({(float)(viewW - tw) / 2 - 14, (float)viewH - 70, (float)tw + 28, 34}, 0.4f, 6, Fade(BLACK, 0.7f));
      label(sim.toast.c_str(), (viewW - tw) / 2, viewH - 63, WHITE, 20);
    }

    if (statesH > 0) {
      stateView.draw({0, (float)viewH, (float)viewW, (float)statesH}, brain, face, sensors.state());
    }

    drawPanel(viewW, winH);

    if (!pendingShot.empty()) {
      rlDrawRenderBatchActive();
      saveShot(pendingShot.c_str());
      if (!scripted) toast("saved " + pendingShot);
      pendingShot.clear();
    }
    EndDrawing();
  }

  if (sim.gif.isOpen()) sim.gif.close();
  if (audio) {
    StopAudioStream(stream);
    UnloadAudioStream(stream);
    CloseAudioDevice();
  }
  UnloadTexture(oledTex);
  scene.unload();
  CloseWindow();
  return 0;
}
