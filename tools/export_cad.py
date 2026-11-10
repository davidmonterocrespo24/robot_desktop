# Exports the FreeCAD model of the robot to the meshes used by the emulator.
#
# Run it with FreeCAD's own Python (headless):
#
#   "C:/Program Files/FreeCAD 1.0/bin/freecadcmd.exe" -c "exec(open('tools/export_cad.py').read())"
#
# Optional environment variables:
#   CAD_IN   FreeCAD file     (default: doc/Robot ESP32 V2 - copia.FCStd)
#   CAD_OUT  output folder    (default: emulator/assets/robot)
#
# Every top level object of the document becomes one .mesh file (triangles with
# normals and the colors of the CAD faces) and one line in parts.txt:
#
#   file|role|label|triangles
#
# .mesh layout (little endian): "RMSH", uint32 vertex count, then
# positions (3 float each), normals (3 float each), colors (4 bytes each).
import math
import os
import re
import struct
import zipfile

import FreeCAD

ROOT = os.getcwd()
SRC = os.environ.get("CAD_IN", os.path.join(ROOT, "doc", "Robot ESP32 V2 - copia.FCStd"))
OUT = os.environ.get("CAD_OUT", os.path.join(ROOT, "emulator", "assets", "robot"))

# What each object of the CAD is. Matched against "Name Label", first hit wins.
# The role decides how the emulator treats the part (x-ray, real colors...).
ROLES = [
    (r"^Fusion037 ", "shell"),          # body
    (r"^Fusion040 ", "head"),           # chassis and frame of the display
    (r"^Fusion024 ", "leg_right"),
    (r"^Fusion027 ", "leg_left"),
    (r"Servo001", "servo_left"),
    (r"Servo", "servo_right"),
    (r"OLED", "oled"),
    (r"ESP32", "esp32"),
    (r"Charger", "charger"),
    (r"Bateria", "battery"),
    (r"^_Unsaved_001 ", "dht"),
    (r"^Fusion03[34] ", "sensor"),      # the two barrels on the front
    (r"^Body", "plate"),
]

# Tessellation tolerance in mm: the skin is seen from close, the electronics
# only need to be recognizable.
FINE = ("shell", "head", "leg_right", "leg_left", "sensor", "plate")
TOL_FINE = 0.10
TOL_COARSE = 0.40

SKIP = ("App::Line", "App::Plane", "App::Origin", "Sketcher::SketchObject")


def role_of(obj):
    key = "%s %s" % (obj.Name, obj.Label)
    for rx, role in ROLES:
        if re.search(rx, key):
            return role
    return "part"


def read_view(path):
    """Colors per object / per face, read from the GUI part of the file."""
    zf = zipfile.ZipFile(path)
    gui = zf.read("GuiDocument.xml").decode("utf-8", "ignore")
    view = {}
    for m in re.finditer(r'<ViewProvider name="([^"]+)"[^>]*>(.*?)</ViewProvider>', gui, flags=re.S):
        name, blk = m.group(1), m.group(2)
        d = {}
        c = re.search(r'<Property name="ShapeColor"[^>]*>\s*<PropertyColor value="(\d+)"', blk)
        if c:
            d["color"] = int(c.group(1))
        f = re.search(r'<Property name="DiffuseColor"[^>]*>\s*<ColorList file="([^"]+)"', blk)
        if f:
            try:
                raw = zf.read(f.group(1))
                n = struct.unpack("<I", raw[:4])[0]
                d["faces"] = list(struct.unpack("<%dI" % n, raw[4:4 + 4 * n]))
            except Exception:
                pass
        view[name] = d
    return view


def rgb(v):
    return ((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255)


def leaves(obj, parent):
    if obj.TypeId in SKIP:
        return
    if obj.TypeId in ("App::Part", "App::DocumentObjectGroup"):
        pl = parent.multiply(obj.Placement) if hasattr(obj, "Placement") else parent
        for child in obj.Group:
            for x in leaves(child, pl):
                yield x
    elif hasattr(obj, "Shape") and not obj.Shape.isNull():
        yield obj, parent


def tessellate(obj, parent, view, tol, out):
    shape = obj.Shape.copy()
    if not parent.isIdentity():
        shape.Placement = parent.multiply(shape.Placement)
    vi = view.get(obj.Name, {})
    base = rgb(vi.get("color", 0xCCCCCC00))
    fcols = vi.get("faces", [])
    faces = shape.Faces
    for i, face in enumerate(faces):
        col = base
        if len(fcols) == len(faces):
            col = rgb(fcols[i])
        elif len(fcols) == 1:
            col = rgb(fcols[0])
        try:
            verts, tris = face.tessellate(tol)
        except Exception:
            continue
        flip = face.Orientation == "Reversed"
        # normals averaged inside the face: curved faces look smooth, edges stay sharp
        acc = [[0.0, 0.0, 0.0] for _ in verts]
        keep = []
        for a, b, c in tris:
            if flip:
                b, c = c, b
            n = (verts[b] - verts[a]).cross(verts[c] - verts[a])
            if n.Length < 1e-12:
                continue
            keep.append((a, b, c))
            for k in (a, b, c):
                acc[k][0] += n.x
                acc[k][1] += n.y
                acc[k][2] += n.z
        for tri in keep:
            for k in tri:
                n = acc[k]
                ln = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
                p = verts[k]
                out.append((p.x, p.y, p.z, n[0] / ln, n[1] / ln, n[2] / ln, col[0], col[1], col[2]))


def main():
    os.makedirs(OUT, exist_ok=True)
    for old in os.listdir(OUT):
        if old.endswith(".mesh"):
            os.remove(os.path.join(OUT, old))

    view = read_view(SRC)
    doc = FreeCAD.openDocument(SRC)
    lines = []
    total = 0
    for root in [o for o in doc.Objects if not o.InList and o.TypeId not in SKIP]:
        role = role_of(root)
        tol = TOL_FINE if role in FINE else TOL_COARSE
        verts = []
        for obj, parent in leaves(root, FreeCAD.Placement()):
            tessellate(obj, parent, view, tol, verts)
        if not verts:
            continue
        name = re.sub(r"[^A-Za-z0-9_]", "_", root.Name) + ".mesh"
        with open(os.path.join(OUT, name), "wb") as fh:
            fh.write(b"RMSH")
            fh.write(struct.pack("<I", len(verts)))
            fh.write(b"".join(struct.pack("<3f", v[0], v[1], v[2]) for v in verts))
            fh.write(b"".join(struct.pack("<3f", v[3], v[4], v[5]) for v in verts))
            fh.write(b"".join(struct.pack("<4B", v[6], v[7], v[8], 255) for v in verts))
        label = re.sub(r"[|\r\n]", " ", root.Label)
        lines.append("%s|%s|%s|%d" % (name, role, label, len(verts) // 3))
        total += len(verts) // 3
        print("  %-12s %-44s %7d triangles" % (role, label, len(verts) // 3))

    with open(os.path.join(OUT, "parts.txt"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# file|role|label|triangles   (generated by tools/export_cad.py from %s)\n" % os.path.basename(SRC))
        fh.write("\n".join(lines) + "\n")
    print("%d parts, %d triangles -> %s" % (len(lines), total, OUT))


main()
