#include "Scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raymath.h"
#include "rlgl.h"

namespace {

const char *VS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPos;
out vec3 fragNormal;
out vec4 fragColor;
void main() {
  fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
  fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
  fragColor = vertexColor;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char *FS = R"(#version 330
in vec3 fragPos;
in vec3 fragNormal;
in vec4 fragColor;
out vec4 finalColor;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 lightDir;
void main() {
  vec3 n = normalize(fragNormal);
  vec3 v = normalize(viewPos - fragPos);
  if (dot(n, v) < 0.0) n = -n;  // CAD faces are not always wound the same way
  vec3 l = normalize(-lightDir);
  float diff = max(dot(n, l), 0.0);
  float fill = max(dot(n, normalize(vec3(-0.7, -0.5, 0.25))), 0.0);
  float hemi = 0.5 + 0.5 * n.z;
  vec3 h = normalize(l + v);
  float spec = pow(max(dot(n, h), 0.0), 40.0) * 0.22;
  vec3 base = pow(fragColor.rgb, vec3(2.2)) * colDiffuse.rgb;
  vec3 c = base * (0.16 + 0.22 * hemi + 0.72 * diff + 0.20 * fill) + vec3(spec) * diff;
  float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
  c += rim * 0.05;
  c = pow(c, vec3(1.0 / 2.2));
  float a = colDiffuse.a;
  // see-through skin: faces seen edge-on are more opaque, like glass
  if (a < 0.999) a = clamp(a + rim * 0.5, 0.0, 1.0);
  finalColor = vec4(c, a);
}
)";

bool readMesh(const char *path, ScenePart &p) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  char magic[4];
  unsigned int n = 0;
  if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "RMSH", 4) != 0 || fread(&n, 4, 1, f) != 1 || n == 0 || n % 3) {
    fclose(f);
    return false;
  }
  Mesh m = {0};
  m.vertexCount = (int)n;
  m.triangleCount = (int)n / 3;
  m.vertices = (float *)MemAlloc(n * 3 * sizeof(float));
  m.normals = (float *)MemAlloc(n * 3 * sizeof(float));
  m.colors = (unsigned char *)MemAlloc(n * 4);
  bool ok = fread(m.vertices, sizeof(float), n * 3, f) == n * 3 && fread(m.normals, sizeof(float), n * 3, f) == n * 3 &&
            fread(m.colors, 1, n * 4, f) == n * 4;
  fclose(f);
  if (!ok) {
    MemFree(m.vertices);
    MemFree(m.normals);
    MemFree(m.colors);
    return false;
  }
  p.mesh = m;
  p.cadColors.assign(m.colors, m.colors + n * 4);
  p.box = GetMeshBoundingBox(m);
  return true;
}

// Colors of the robot as it was printed: white body, black face, blue servos.
void paintReal(ScenePart &p) {
  p.realColors = p.cadColors;
  size_t n = p.cadColors.size() / 4;
  const std::string &r = p.role;
  for (size_t i = 0; i < n; i++) {
    unsigned char *c = &p.realColors[i * 4];
    if (r == "shell" || r == "plate") {
      c[0] = 238, c[1] = 238, c[2] = 233;
    } else if (r == "head") {
      bool frame = c[1] > c[0] + 40 && c[1] > c[2] + 40;  // painted green in the CAD
      if (frame) c[0] = 22, c[1] = 22, c[2] = 25;
      else c[0] = 238, c[1] = 238, c[2] = 233;
    } else if (r == "leg_left" || r == "leg_right") {
      c[0] = 226, c[1] = 226, c[2] = 222;
    } else if (r == "servo_left" || r == "servo_right") {
      c[0] = 46, c[1] = 98, c[2] = 210;
    } else if (r == "sensor") {
      c[0] = 46, c[1] = 46, c[2] = 50;
    }
  }
}

void addTri(std::vector<float> &v, std::vector<float> &n, Vector3 a, Vector3 b, Vector3 c, Vector3 nn) {
  Vector3 t[3] = {a, b, c};
  for (auto &q : t) {
    v.push_back(q.x), v.push_back(q.y), v.push_back(q.z);
    n.push_back(nn.x), n.push_back(nn.y), n.push_back(nn.z);
  }
}

// The ears are not in the CAD: a tapered, slightly curved blade on a hub.
// Local frame: X thickness, Y along the blade, Z across it.
Mesh buildEar() {
  const float R = 6.0f, L = 46.0f, T = 3.6f;
  std::vector<Vector2> o;
  auto mid = [&](float a) { return 3.0f * sinf(PI * a / L); };
  auto half = [&](float a) { return 5.4f + (2.1f - 5.4f) * powf(a / L, 0.85f); };
  for (int i = 0; i <= 18; i++) {  // back of the hub
    float ang = (55.0f + 250.0f * i / 18.0f) * DEG2RAD;
    o.push_back({R * cosf(ang), R * sinf(ang)});
  }
  for (int i = 1; i <= 16; i++) {  // lower edge
    float a = 5.0f + (L - 5.0f) * i / 16.0f;
    o.push_back({a, mid(a) - half(a)});
  }
  for (int i = 1; i < 8; i++) {  // round tip
    float ang = (-90.0f + 180.0f * i / 8.0f) * DEG2RAD;
    o.push_back({L + 2.1f * cosf(ang), mid(L) + 2.1f * sinf(ang)});
  }
  for (int i = 16; i >= 1; i--) {  // upper edge
    float a = 5.0f + (L - 5.0f) * i / 16.0f;
    o.push_back({a, mid(a) + half(a)});
  }

  std::vector<float> v, n;
  Vector2 c = {9.0f, 0.6f};
  size_t m = o.size();
  for (size_t i = 0; i < m; i++) {
    Vector2 p = o[i], q = o[(i + 1) % m];
    addTri(v, n, {T / 2, c.x, c.y}, {T / 2, p.x, p.y}, {T / 2, q.x, q.y}, {1, 0, 0});
    addTri(v, n, {-T / 2, c.x, c.y}, {-T / 2, q.x, q.y}, {-T / 2, p.x, p.y}, {-1, 0, 0});
    Vector2 e = {q.x - p.x, q.y - p.y};
    float len = sqrtf(e.x * e.x + e.y * e.y);
    if (len < 1e-5f) continue;
    Vector3 nn = {0, e.y / len, -e.x / len};
    // make sure the side faces away from the middle
    if (nn.y * (p.x - c.x) + nn.z * (p.y - c.y) < 0) nn = Vector3Negate(nn);
    addTri(v, n, {-T / 2, p.x, p.y}, {T / 2, p.x, p.y}, {T / 2, q.x, q.y}, nn);
    addTri(v, n, {-T / 2, p.x, p.y}, {T / 2, q.x, q.y}, {-T / 2, q.x, q.y}, nn);
  }

  Mesh mesh = {0};
  mesh.vertexCount = (int)v.size() / 3;
  mesh.triangleCount = mesh.vertexCount / 3;
  mesh.vertices = (float *)MemAlloc((unsigned)(v.size() * sizeof(float)));
  mesh.normals = (float *)MemAlloc((unsigned)(n.size() * sizeof(float)));
  mesh.colors = (unsigned char *)MemAlloc((unsigned)mesh.vertexCount * 4);
  memcpy(mesh.vertices, v.data(), v.size() * sizeof(float));
  memcpy(mesh.normals, n.data(), n.size() * sizeof(float));
  for (int i = 0; i < mesh.vertexCount; i++) {
    mesh.colors[i * 4 + 0] = 62;
    mesh.colors[i * 4 + 1] = 222;
    mesh.colors[i * 4 + 2] = 70;
    mesh.colors[i * 4 + 3] = 255;
  }
  UploadMesh(&mesh, false);
  return mesh;
}

}  // namespace

bool Scene::load(const char *dir) {
  std::string base = dir;
  FILE *f = fopen((base + "/parts.txt").c_str(), "r");
  if (!f) {
    TraceLog(LOG_WARNING, "SCENE: %s/parts.txt not found, the 3D robot is not available", dir);
    return false;
  }

  shader_ = LoadShaderFromMemory(VS, FS);
  locLight_ = GetShaderLocation(shader_, "lightDir");
  locView_ = GetShaderLocation(shader_, "viewPos");
  material_ = LoadMaterialDefault();
  material_.shader = shader_;

  char line[512];
  Vector3 mn = {1e9f, 1e9f, 1e9f}, mx = {-1e9f, -1e9f, -1e9f};
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
    char *file = strtok(line, "|");
    char *role = strtok(nullptr, "|");
    char *label = strtok(nullptr, "|");
    if (!file || !role) continue;
    ScenePart p;
    p.file = file;
    p.role = role;
    p.label = label ? label : "";
    if (!readMesh((base + "/" + p.file).c_str(), p)) {
      TraceLog(LOG_WARNING, "SCENE: cannot read %s", p.file.c_str());
      continue;
    }
    p.skin = p.role == "shell" || p.role == "head" || p.role == "leg_left" || p.role == "leg_right" || p.role == "plate";
    paintReal(p);

    // Places that the emulator animates are measured on the meshes themselves,
    // so a re-exported CAD keeps working.
    const float *v = p.mesh.vertices;
    int n = p.mesh.vertexCount;
    if (p.role == "servo_left" || p.role == "servo_right") {
      bool right = p.role == "servo_right";
      float tip = right ? p.box.max.x : p.box.min.x;
      Vector3 lo = {0, 1e9f, 1e9f}, hi = {0, -1e9f, -1e9f};
      for (int i = 0; i < n; i++) {
        if (fabsf(v[i * 3] - tip) > 0.3f) continue;
        lo.y = fminf(lo.y, v[i * 3 + 1]), hi.y = fmaxf(hi.y, v[i * 3 + 1]);
        lo.z = fminf(lo.z, v[i * 3 + 2]), hi.z = fmaxf(hi.z, v[i * 3 + 2]);
      }
      Vector3 s = {tip, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
      if (right) shaftRight_ = s;
      else shaftLeft_ = s;
    } else if (p.role == "oled") {
      float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f, y = 1e9f;
      const unsigned char *c = p.cadColors.data();
      for (int i = 0; i < n; i++) {
        // the lit area is the dark blue face of the module
        if (!(c[i * 4] < 12 && c[i * 4 + 1] < 12 && c[i * 4 + 2] > 50)) continue;
        x0 = fminf(x0, v[i * 3]), x1 = fmaxf(x1, v[i * 3]);
        z0 = fminf(z0, v[i * 3 + 2]), z1 = fmaxf(z1, v[i * 3 + 2]);
        y = fminf(y, v[i * 3 + 1]);
      }
      if (x1 > x0 && z1 > z0) oledX0_ = x0, oledX1_ = x1, oledZ0_ = z0, oledZ1_ = z1, oledY_ = y;
    }

    memcpy(p.mesh.colors, p.realColors.data(), p.realColors.size());
    UploadMesh(&p.mesh, true);
    mn = Vector3Min(mn, p.box.min);
    mx = Vector3Max(mx, p.box.max);
    triangles_ += p.mesh.triangleCount;
    parts_.push_back(p);
  }
  fclose(f);
  if (parts_.empty()) return false;

  center_ = Vector3Scale(Vector3Add(mn, mx), 0.5f);
  radius_ = Vector3Length(Vector3Subtract(mx, mn)) * 0.5f;
  floorZ_ = mn.z;
  ear_ = buildEar();
  real_ = true;
  TraceLog(LOG_INFO, "SCENE: %d parts, %d triangles", (int)parts_.size(), triangles_);
  return true;
}

void Scene::unload() {
  for (auto &p : parts_) UnloadMesh(p.mesh);
  parts_.clear();
  if (ear_.vertexCount) UnloadMesh(ear_);
  ear_ = {0};
  if (shader_.id) UnloadShader(shader_);
  shader_ = {0};
}

void Scene::setRealColors(bool real) {
  if (real == real_) return;
  real_ = real;
  for (auto &p : parts_) {
    const std::vector<unsigned char> &src = real ? p.realColors : p.cadColors;
    UpdateMeshBuffer(p.mesh, 3, src.data(), (int)src.size(), 0);
  }
}

void Scene::drawFloor() {
  const float z = floorZ_ - 0.05f;
  const Vector3 c = {center_.x, center_.y, z};
  rlDisableBackfaceCulling();
  // desk
  rlBegin(RL_TRIANGLES);
  const int seg = 48;
  const float r = 260.0f;
  for (int i = 0; i < seg; i++) {
    float a0 = i * 2.0f * PI / seg, a1 = (i + 1) * 2.0f * PI / seg;
    rlColor4ub(58, 64, 78, 255);
    rlVertex3f(c.x, c.y, z);
    rlColor4ub(24, 27, 34, 0);
    rlVertex3f(c.x + cosf(a0) * r, c.y + sinf(a0) * r, z);
    rlVertex3f(c.x + cosf(a1) * r, c.y + sinf(a1) * r, z);
  }
  // soft shadow under the robot
  for (int ring = 0; ring < 3; ring++) {
    float rx = 46.0f + ring * 16.0f, ry = 52.0f + ring * 16.0f;
    for (int i = 0; i < seg; i++) {
      float a0 = i * 2.0f * PI / seg, a1 = (i + 1) * 2.0f * PI / seg;
      rlColor4ub(0, 0, 0, 70);
      rlVertex3f(c.x, c.y + 4.0f, z + 0.02f + ring * 0.01f);
      rlColor4ub(0, 0, 0, 0);
      rlVertex3f(c.x + cosf(a0) * rx, c.y + 4.0f + sinf(a0) * ry, z + 0.02f + ring * 0.01f);
      rlVertex3f(c.x + cosf(a1) * rx, c.y + 4.0f + sinf(a1) * ry, z + 0.02f + ring * 0.01f);
    }
  }
  rlEnd();
  // grid, 10 mm
  for (int i = -12; i <= 12; i++) {
    unsigned char a = (unsigned char)(i % 5 == 0 ? 46 : 20);
    Color col = {150, 170, 210, a};
    float k = i * 10.0f;
    DrawLine3D({c.x + k, c.y - 120, z + 0.04f}, {c.x + k, c.y + 120, z + 0.04f}, col);
    DrawLine3D({c.x - 120, c.y + k, z + 0.04f}, {c.x + 120, c.y + k, z + 0.04f}, col);
  }
  rlDrawRenderBatchActive();
}

void Scene::drawEar(bool left, float angleDeg, Color tint) {
  Vector3 s = left ? shaftLeft_ : shaftRight_;
  float out = left ? -2.4f : 2.4f;
  Matrix m = MatrixMultiply(MatrixRotateX(angleDeg * DEG2RAD), MatrixTranslate(s.x + out, s.y, s.z));
  material_.maps[MATERIAL_MAP_DIFFUSE].color = tint;
  DrawMesh(ear_, material_, m);
}

void Scene::drawOled(Texture2D oled, float glow) {
  const float y = oledY_ - 0.06f;
  auto quad = [&](float grow, float yy, unsigned char alpha) {
    float gx = (oledX1_ - oledX0_) * grow * 0.5f, gz = (oledZ1_ - oledZ0_) * grow * 0.5f;
    float x0 = oledX0_ - gx, x1 = oledX1_ + gx, z0 = oledZ0_ - gz, z1 = oledZ1_ + gz;
    float u0 = -grow * 0.5f, u1 = 1.0f + grow * 0.5f;
    rlSetTexture(oled.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, alpha);
    rlNormal3f(0, -1, 0);
    rlTexCoord2f(u0, u0);
    rlVertex3f(x0, yy, z1);
    rlTexCoord2f(u0, u1);
    rlVertex3f(x0, yy, z0);
    rlTexCoord2f(u1, u1);
    rlVertex3f(x1, yy, z0);
    rlTexCoord2f(u1, u0);
    rlVertex3f(x1, yy, z1);
    rlEnd();
    rlSetTexture(0);
  };

  rlDisableBackfaceCulling();
  quad(0.0f, y, 255);
  rlDrawRenderBatchActive();

  if (glow > 0.01f) {
    // light spilling around the lit pixels
    SetTextureFilter(oled, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(oled, TEXTURE_WRAP_CLAMP);
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    quad(0.0f, y - 0.05f, (unsigned char)(90 * glow));
    quad(0.10f, y - 0.10f, (unsigned char)(40 * glow));
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
    SetTextureFilter(oled, TEXTURE_FILTER_POINT);
  }
}

void Scene::draw(const Camera3D &cam, float earLeft, float earRight, Texture2D oled, float oledGlow) {
  if (parts_.empty()) return;

  Vector3 light = Vector3Normalize({0.35f, 0.75f, -0.9f});
  SetShaderValue(shader_, locLight_, &light, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader_, locView_, &cam.position, SHADER_UNIFORM_VEC3);

  drawFloor();
  rlDisableBackfaceCulling();

  const Matrix id = MatrixIdentity();
  material_.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
  for (auto &p : parts_) {
    if (p.skin && skin_ != SKIN_SOLID) continue;
    DrawMesh(p.mesh, material_, id);
  }
  drawEar(true, earLeft, WHITE);
  drawEar(false, earRight, WHITE);

  drawOled(oled, oledGlow);

  if (skin_ == SKIN_XRAY) {
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    material_.maps[MATERIAL_MAP_DIFFUSE].color = {255, 255, 255, 46};
    for (auto &p : parts_) {
      if (p.skin) DrawMesh(p.mesh, material_, id);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    material_.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
  }
  rlEnableBackfaceCulling();
}
