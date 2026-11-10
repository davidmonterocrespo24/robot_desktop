// Rasterizer of the procedural face: turns eye / mouth parameters into pixels.
#ifndef FACE_FACERENDERER_H
#define FACE_FACERENDERER_H

#include "../core/Canvas.h"
#include "FaceTypes.h"

// Everything the decorations need to know about the frame being drawn.
struct FaceFrame {
  float cx[2], cy[2];  // eye centers on screen
  float w[2], h[2];    // eye sizes after all the layers were applied
  float gazeX, gazeY;  // -1..1
  float mouthX, mouthY, mouthW;
  float time;          // seconds, free running
};

namespace FaceRenderer {

// `left` tells on which side of the screen the eye is (needed to mirror the
// slopes: the nose side of the left eye is on its right).
// Here `p.px / p.py` are the final position of the pupil in screen terms
// (-1 = left / up edge of the eye, +1 = right / down edge).
// `grow` enlarges the shape by that many pixels and `color` paints it: drawing
// first a grown dark copy leaves an outline that separates the eye from
// whatever is behind it.
void drawEye(Canvas &c, float cx, float cy, const EyeParams &p, uint8_t kind, bool left, float time,
             float grow = 0.0f, uint8_t color = C_ON);

void drawMouth(Canvas &c, float cx, float cy, const MouthParams &m);

// Primitive shapes reused by the decorations and the acts.
void drawHeart(Canvas &c, float cx, float cy, float rx, float ry, uint8_t color = C_ON);
void drawDrop(Canvas &c, float cx, float cy, float r, uint8_t color = C_ON);
void drawStar4(Canvas &c, float cx, float cy, float r, uint8_t color = C_ON);
void drawStar5(Canvas &c, float cx, float cy, float rx, float ry, float rot, uint8_t color = C_ON);
// Filled disc with a dark rim, visible over both the dark screen and a lit eye.
void drawBall(Canvas &c, float cx, float cy, float rx, float ry);

}  // namespace FaceRenderer

#endif
