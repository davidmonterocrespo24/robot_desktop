// Command line companion of the face engine (no window, no hardware).
// It runs the very same code that goes into the ESP32 and writes what the
// OLED would show as GIF files.
//
//   facetool list                       expressions, clips and graph size
//   facetool path  <from> <to>          route over the emotion graph
//   facetool gif   <expr> <out.gif> [from] [hold_s]
//   facetool trip  <out.gif> <expr> <expr> ...   a walk through several faces
//   facetool act   <name> <out.gif>     an act (computed animation)
//   facetool say   <text> <out.gif>     the speech balloon
//   facetool clip  <name> <out.gif>     a stored GIF clip, decoded by the engine
//   facetool sheet <out.gif> [t]        every expression on one page
//   facetool strip <act> <out.gif> [n]  n moments of an act side by side
//   facetool all   <dir>                one GIF per expression + sheet
//   facetool acts  <dir>                one GIF per act
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p, 0755)
#endif

#include "../src/core/Font5x7.h"
#include "../src/face/FaceEngine.h"
#include "GifWriter.h"

static const int FRAME_MS = 33;

static uint8_t exprOrDie(const char *name) {
  uint8_t id = Expressions::find(name);
  if (id == EX_NONE) {
    fprintf(stderr, "unknown expression '%s' (see: facetool list)\n", name);
    exit(1);
  }
  return id;
}

// Runs the engine until it is idle (plus `holdMs`), writing every frame.
static void record(FaceEngine &face, GifWriter &gif, uint32_t &now, uint32_t holdMs, uint32_t maxMs = 60000) {
  Canvas c;
  uint32_t idleSince = 0;
  bool idle = false;
  uint32_t start = now;
  while (now - start < maxMs) {
    now += FRAME_MS;
    face.update(now);
    face.render(c);
    gif.frame(c.buf, FRAME_MS);
    if (!face.busy()) {
      if (!idle) {
        idle = true;
        idleSince = now;
      }
      if (now - idleSince >= holdMs) break;
    } else {
      idle = false;
    }
  }
}

static int cmdList() {
  printf("Expressions (%d):\n", (int)EX_COUNT);
  for (int i = 0; i < EX_COUNT; i++) {
    const ExpressionDef &d = Expressions::get((uint8_t)i);
    uint8_t nb[16];
    int n = ExpressionGraph::neighbours((uint8_t)i, nb, 16);
    printf("  %-12s %-7s ->", d.name, Expressions::moodName(d.mood));
    for (int k = 0; k < n; k++) printf(" %s", Expressions::name(nb[k]));
    printf("\n");
  }
  printf("Graph: %d nodes, %d edges\n", (int)EX_COUNT, ExpressionGraph::edgeCount());
  printf("Acts (%d):\n", Acts::count());
  for (int i = 0; i < Acts::count(); i++) {
    const ActDef &a = Acts::get(i);
    printf("  %-14s %4.1f s  on %s\n", a.name, a.duration, a.base < EX_COUNT ? Expressions::name(a.base) : "any face");
  }
  printf("Clips (%d):\n", Clips::count());
  long total = 0;
  for (int i = 0; i < Clips::count(); i++) {
    const ClipDef &c = Clips::get(i);
    printf("  %-24s %4d frames %6u bytes  %s\n", c.name, c.frames, (unsigned)c.size, Expressions::moodName(c.mood));
    total += c.size + c.frames;
  }
  printf("  total %ld bytes\n", total);
  return 0;
}

static int cmdPath(const char *a, const char *b) {
  uint8_t from = exprOrDie(a), to = exprOrDie(b);
  uint8_t p[GRAPH_MAX_PATH];
  int n = ExpressionGraph::path(from, to, p, GRAPH_MAX_PATH);
  printf("%s", Expressions::name(from));
  uint8_t prev = from;
  int ms = 0;
  for (int i = 0; i < n; i++) {
    const GraphEdge *e = ExpressionGraph::find(prev, p[i]);
    ms += e ? e->ms : 0;
    printf(" -> %s", Expressions::name(p[i]));
    prev = p[i];
  }
  printf("   (%d hops, %d ms at full length)\n", n, ms);
  return 0;
}

static int cmdGif(const char *expr, const char *out, const char *from, float hold) {
  FaceEngine face;
  face.begin(1234, exprOrDie(from));
  GifWriter gif;
  if (!gif.open(out, CANVAS_W, CANVAS_H, 4)) {
    fprintf(stderr, "cannot write %s\n", out);
    return 1;
  }
  uint32_t now = 1000;
  record(face, gif, now, 600);
  face.goTo(exprOrDie(expr));
  record(face, gif, now, (uint32_t)(hold * 1000));
  printf("%s: %d frames\n", out, gif.frames());
  return 0;
}

static int cmdTrip(const char *out, int n, char **names) {
  FaceEngine face;
  face.begin(99, exprOrDie(names[0]));
  GifWriter gif;
  if (!gif.open(out, CANVAS_W, CANVAS_H, 4)) return 1;
  uint32_t now = 1000;
  record(face, gif, now, 800);
  for (int i = 1; i < n; i++) {
    face.goTo(exprOrDie(names[i]));
    record(face, gif, now, 1800);
  }
  printf("%s: %d frames\n", out, gif.frames());
  return 0;
}

static int cmdAct(const char *name, const char *out) {
  int idx = Acts::find(name);
  if (idx < 0) {
    fprintf(stderr, "unknown act '%s' (see: facetool list)\n", name);
    return 1;
  }
  const ActDef &a = Acts::get(idx);
  FaceEngine face;
  face.begin(11, a.base < EX_COUNT ? a.base : (uint8_t)EX_NORMAL);
  GifWriter gif;
  if (!gif.open(out, CANVAS_W, CANVAS_H, 4)) return 1;
  uint32_t now = 1000;
  record(face, gif, now, 400);
  face.playAct(idx);
  record(face, gif, now, 600);
  printf("%s: %d frames\n", out, gif.frames());
  return 0;
}

static int cmdActs(const char *dir) {
  std::string d = dir;
  MAKE_DIR(dir);
  for (int i = 0; i < Acts::count(); i++) {
    std::string out = d + "/" + Acts::get(i).name + ".gif";
    if (cmdAct(Acts::get(i).name, out.c_str())) return 1;
  }
  return 0;
}

static int cmdClip(const char *name, const char *out) {
  int idx = Clips::find(name);
  if (idx < 0) {
    fprintf(stderr, "unknown clip '%s'\n", name);
    return 1;
  }
  FaceEngine face;
  face.begin(7, EX_NORMAL);
  GifWriter gif;
  if (!gif.open(out, CANVAS_W, CANVAS_H, 4)) return 1;
  uint32_t now = 1000;
  record(face, gif, now, 500);
  face.playClip(idx, EX_NORMAL);
  record(face, gif, now, 1000);
  printf("%s: %d frames\n", out, gif.frames());
  return 0;
}

// ---- contact sheet -------------------------------------------------------------
struct Page {
  int w, h, stride;
  std::vector<uint8_t> bits;
  Page(int w_, int h_) : w((w_ + 7) / 8 * 8), h(h_), stride(w / 8), bits((size_t)stride * h_, 0) {}
  void set(int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    bits[(size_t)y * stride + (x >> 3)] |= (uint8_t)(1 << (x & 7));
  }
  void text(int x, int y, const char *s) {
    for (; *s; s++, x += 6) {
      unsigned char ch = (unsigned char)*s;
      if (ch < 0x20 || ch > 0x7E) ch = '?';
      for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
          if (FONT5X7[ch - 0x20][col] & (1 << row)) set(x + col, y + row);
    }
  }
  void box(int x, int y, int bw, int bh) {
    for (int i = 0; i < bw; i += 2) {
      set(x + i, y);
      set(x + i, y + bh - 1);
    }
    for (int j = 0; j < bh; j += 2) {
      set(x, y + j);
      set(x + bw - 1, y + j);
    }
  }
};

static int cmdSheet(const char *out, float t) {
  const int cols = 5, cellW = CANVAS_W + 8, cellH = CANVAS_H + 16;
  int rows = (EX_COUNT + cols - 1) / cols;
  Page page(cols * cellW + 8, rows * cellH + 8);
  for (int i = 0; i < EX_COUNT; i++) {
    FaceEngine face;
    face.begin(5 + i, (uint8_t)i);
    face.setAutoGaze(false);
    face.setAutoBlink(false);
    Canvas c;
    uint32_t now = 0;
    for (int k = 0; k < (int)(t * 1000 / FRAME_MS) + 1; k++) {
      now += FRAME_MS;
      face.update(now);
    }
    face.render(c);
    int ox = 8 + (i % cols) * cellW, oy = 8 + (i / cols) * cellH;
    for (int y = 0; y < CANVAS_H; y++)
      for (int x = 0; x < CANVAS_W; x++)
        if (c.get(x, y)) page.set(ox + x, oy + y);
    page.box(ox - 1, oy - 1, CANVAS_W + 2, CANVAS_H + 2);
    page.text(ox + 1, oy + CANVAS_H + 3, Expressions::name((uint8_t)i));
  }
  GifWriter gif;
  if (!gif.open(out, page.w, page.h, 2)) return 1;
  gif.frame(page.bits.data(), 1000);
  printf("%s: %dx%d\n", out, page.w * 2, page.h * 2);
  return 0;
}

// Moments of an act, evenly spaced, on one page: enough to judge it at a glance.
static int cmdStrip(const char *name, const char *out, int n) {
  int idx = Acts::find(name);
  if (idx < 0) {
    fprintf(stderr, "unknown act '%s'\n", name);
    return 1;
  }
  const ActDef &a = Acts::get(idx);
  if (n < 2) n = 2;
  const int cols = 6, cellW = CANVAS_W + 4, cellH = CANVAS_H + 4;
  int rows = (n + cols - 1) / cols;
  Page page(cols * cellW + 4, rows * cellH + 4);
  FaceEngine face;
  face.begin(11, a.base < EX_COUNT ? a.base : (uint8_t)EX_NORMAL);
  face.setAutoBlink(false);
  face.setAutoGaze(false);
  uint32_t now = 1000;
  face.update(now);
  face.playAct(idx);
  Canvas c;
  int shot = 0;
  uint32_t start = now;
  while (shot < n) {
    now += FRAME_MS;
    face.update(now);
    float t = (now - start) * 0.001f;
    float want = a.duration * (shot + 0.5f) / n;
    if (t < want) continue;
    face.render(c);
    int ox = 4 + (shot % cols) * cellW, oy = 4 + (shot / cols) * cellH;
    for (int y = 0; y < CANVAS_H; y++)
      for (int x = 0; x < CANVAS_W; x++)
        if (c.get(x, y)) page.set(ox + x, oy + y);
    page.box(ox - 1, oy - 1, CANVAS_W + 2, CANVAS_H + 2);
    shot++;
  }
  GifWriter gif;
  if (!gif.open(out, page.w, page.h, 2)) return 1;
  gif.frame(page.bits.data(), 1000);
  return 0;
}

static int cmdAll(const char *dir) {
  std::string d = dir;
  MAKE_DIR(dir);
  for (int i = 0; i < EX_COUNT; i++) {
    std::string out = d + "/" + Expressions::name((uint8_t)i) + ".gif";
    const char *from = (i == EX_NORMAL) ? "Asleep" : "Normal";
    if (cmdGif(Expressions::name((uint8_t)i), out.c_str(), from, 3.0f)) return 1;
  }
  return cmdSheet((d + "/_sheet.gif").c_str(), 1.0f);
}

int main(int argc, char **argv) {
  const char *cmd = argc > 1 ? argv[1] : "";
  if (!strcmp(cmd, "list")) return cmdList();
  if (!strcmp(cmd, "path") && argc >= 4) return cmdPath(argv[2], argv[3]);
  if (!strcmp(cmd, "gif") && argc >= 4)
    return cmdGif(argv[2], argv[3], argc > 4 ? argv[4] : "Normal", argc > 5 ? (float)atof(argv[5]) : 3.0f);
  if (!strcmp(cmd, "trip") && argc >= 5) return cmdTrip(argv[2], argc - 3, argv + 3);
  if (!strcmp(cmd, "clip") && argc >= 4) return cmdClip(argv[2], argv[3]);
  if (!strcmp(cmd, "act") && argc >= 4) return cmdAct(argv[2], argv[3]);
  if (!strcmp(cmd, "say") && argc >= 4) {
    Acts::setText(argv[2]);
    return cmdAct("Say", argv[3]);
  }
  if (!strcmp(cmd, "strip") && argc >= 4) return cmdStrip(argv[2], argv[3], argc > 4 ? atoi(argv[4]) : 12);
  if (!strcmp(cmd, "acts") && argc >= 3) return cmdActs(argv[2]);
  if (!strcmp(cmd, "sheet") && argc >= 3) return cmdSheet(argv[2], argc > 3 ? (float)atof(argv[3]) : 1.0f);
  if (!strcmp(cmd, "all") && argc >= 3) return cmdAll(argv[2]);
  fprintf(stderr,
          "usage:\n"
          "  facetool list\n"
          "  facetool path  <from> <to>\n"
          "  facetool gif   <expr> <out.gif> [from] [hold_s]\n"
          "  facetool trip  <out.gif> <expr> <expr> ...\n"
          "  facetool act   <name> <out.gif>\n"
          "  facetool say   <text> <out.gif>\n"
          "  facetool strip <act> <out.gif> [n]\n"
          "  facetool acts  <dir>\n"
          "  facetool clip  <name> <out.gif>\n"
          "  facetool sheet <out.gif> [t]\n"
          "  facetool all   <dir>\n");
  return 1;
}
