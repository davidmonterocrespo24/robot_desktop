// 3D view of the robot built from the CAD meshes (see tools/export_cad.py).
#ifndef EMU_SCENE_H
#define EMU_SCENE_H

#include <string>
#include <vector>

#include "raylib.h"

// How the outside of the robot is drawn.
enum SkinMode { SKIN_SOLID = 0, SKIN_XRAY, SKIN_HIDDEN, SKIN_MODES };

struct ScenePart {
  std::string file, role, label;
  Mesh mesh = {0};
  bool skin = false;  // shell, head and legs: what x-ray makes transparent
  std::vector<unsigned char> cadColors, realColors;
  BoundingBox box = {0};
};

class Scene {
 public:
  bool load(const char *dir);
  void unload();

  // Angles are logical ear angles in degrees (0 = lying back, 90 = up).
  void draw(const Camera3D &cam, float earLeft, float earRight, Texture2D oled, float oledGlow);

  void setRealColors(bool real);
  bool realColors() const { return real_; }
  void setSkin(int mode) { skin_ = mode % SKIN_MODES; }
  int skin() const { return skin_; }

  Vector3 center() const { return center_; }
  float radius() const { return radius_; }
  float floorZ() const { return floorZ_; }
  int triangles() const { return triangles_; }
  int partCount() const { return (int)parts_.size(); }
  bool loaded() const { return !parts_.empty(); }

 private:
  void drawEar(bool left, float angleDeg, Color tint);
  void drawFloor();
  void drawOled(Texture2D oled, float glow);

  std::vector<ScenePart> parts_;
  Mesh ear_ = {0};
  Shader shader_ = {0};
  Material material_ = {0};
  int locLight_ = -1, locView_ = -1, locUnlit_ = -1;
  bool real_ = true;
  int skin_ = SKIN_SOLID;
  Vector3 center_ = {0, 0, 0};
  float radius_ = 60.0f;
  float floorZ_ = -62.0f;
  int triangles_ = 0;

  // measured on the CAD
  Vector3 shaftRight_ = {-7.7f, 39.85f, 6.4f};   // tip of the servo shaft, +X side
  Vector3 shaftLeft_ = {-61.3f, 39.85f, 6.4f};
  // lit area of the display: x0, x1, z0, z1 and the plane y
  float oledX0_ = -45.11f, oledX1_ = -23.55f, oledZ0_ = -8.05f, oledZ1_ = 3.17f, oledY_ = -3.96f;
};

#endif
