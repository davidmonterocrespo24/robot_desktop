# Print files of the robot, taken from the FreeCAD model, plus the two ears that the
# model does not have.
#
# Run it with FreeCAD's own Python (headless), from the root of the repository:
#
#   "C:/Program Files/FreeCAD 1.0/bin/freecadcmd.exe" -c "exec(open('cad/export_stl.py').read())"
#
# Writes:
#   cad/stl/*.stl            one file per printed part, already lying in its print position
#   cad/ensamblado/*.stl     the same parts where they go on the robot (to see how it closes)
#   cad/esp32_eyes.step      the whole robot, electronics included, ears at 90 degrees
#   cad/export_stl.log       what was written, and the checks
#
# The parts of the model are exported as they are. The checks only report: where two parts
# of the model touch, and whether the ears hit anything while they turn.
import math
import os
import struct

import FreeCAD
import numpy as np
import Part

V = FreeCAD.Vector
ROOT = os.getcwd()
SRC = os.path.join(ROOT, "doc", "Robot ESP32 V2 - copia.FCStd")
OUT_STL = os.path.join(ROOT, "cad", "stl")
OUT_ASM = os.path.join(ROOT, "cad", "ensamblado")
LOG = os.path.join(ROOT, "cad", "export_stl.log")
PLA = 1.24          # g/cm3
DEBUG_LIE = False

_log = open(LOG, "w", encoding="utf-8", newline="\n")


def print(*a):      # noqa: A001  FreeCAD's console does not flush stdout: write a log
    _log.write(" ".join(str(x) for x in a) + "\n")
    _log.flush()


# --------------------------------------------------------------------------------------
# what is printed (object of the model -> file); +X is the right of someone looking at the face
# --------------------------------------------------------------------------------------
PRINTED = [
    # file              object        copies  what it is
    ("carcasa",         "Fusion037",  1, "carcasa blanca: frente, techo y costados"),
    ("chasis",          "Fusion040",  1, "chasis negro: marco de la pantalla, servos, ESP32"),
    ("pata_derecha",    "Fusion024",  1, "pata derecha (mirando al robot de frente)"),
    ("pata_izquierda",  "Fusion027",  1, "pata izquierda"),
    ("tapon_frontal",   "Fusion033",  2, "tapon de los agujeros de abajo del frente"),
]
SAME_AS = {"Fusion034": "tapon_frontal"}      # the second plug, identical to the first
# Two bodies of the model are left out: they sit almost entirely inside the chassis
# (856 of the 1182 mm3 of Cuerpo042) and look like modelling helpers.
LEFT_OUT = ["Body042", "Body055"]

ELECTRONICS = [
    ("servo_derecho", "Part__Feature"), ("servo_izquierdo", "Part__Feature061"),
    ("oled", "User_Library_128x64I2COLEDDisplayAdafruit"), ("esp32", "ESP32_DEV_KIT_V1_ASM"),
    ("cargador", "_Unsaved_"), ("bateria", "Body034"), ("dht11", "_Unsaved_001"),
]

# --------------------------------------------------------------------------------------
# the ears, for the GH-S37D servo (measured on its model in the CAD)
# --------------------------------------------------------------------------------------
SHAFT_Y, SHAFT_Z = 39.85, 6.40
TIP = {1: -7.70, -1: -61.30}          # end of the output spline, right and left servo
SPLINE_D = 3.5                         # the spline: 3.5 mm, 4 mm long over the gear dome
SOCKET_D = 3.4                         # pressed onto the spline
SCREW_D, HEAD_D = 2.2, 4.4             # the horn screw of the servo
EAR_L, EAR_T = 46.0, 3.6               # the blade of the emulator (emulator/Scene.cpp)
HUB_R = 5.0


def ear_x(s):
    """Planes of the ear on side s (1 right, -1 left), from the servo outwards."""
    tip = TIP[s]
    return {
        "inner": tip - s * 3.4,        # 0.6 mm over the gear dome
        "socket": tip + s * 0.3,       # the socket goes 0.3 mm past the end of the spline
        "head": tip + s * 1.3,         # seat of the screw head
        "outer": tip + s * 2.3,
    }


def ear_outline():
    """(u, v): u along the blade from the pivot, v across it. Same curve as the emulator."""
    L = EAR_L

    def mid(a):
        return 3.0 * math.sin(math.pi * a / L)

    def half(a):
        return 5.4 + (2.1 - 5.4) * (a / L) ** 0.85

    pts = []
    for i in range(19):                                   # back of the hub
        ang = math.radians(55.0 + 250.0 * i / 18.0)
        pts.append((6.0 * math.cos(ang), 6.0 * math.sin(ang)))
    for i in range(1, 25):                                # lower edge
        a = 5.0 + (L - 5.0) * i / 24.0
        pts.append((a, mid(a) - half(a)))
    for i in range(1, 12):                                # round tip
        ang = math.radians(-90.0 + 180.0 * i / 12.0)
        pts.append((L + 2.1 * math.cos(ang), mid(L) + 2.1 * math.sin(ang)))
    for i in range(24, 0, -1):                            # upper edge
        a = 5.0 + (L - 5.0) * i / 24.0
        pts.append((a, mid(a) + half(a)))
    return pts


def xcyl(r, x0, x1):
    a, b = min(x0, x1), max(x0, x1)
    return Part.makeCylinder(r, b - a, V(a, SHAFT_Y, SHAFT_Z), V(1, 0, 0))


def make_ear(s):
    """The ear standing up (logical angle 90). At 90 the blade points up and bends to the front."""
    x = ear_x(s)
    b0, b1 = sorted((x["outer"] - s * EAR_T, x["outer"]))
    # at 90 degrees: u goes up (+Z), v goes to the front (-Y)
    pts = [V(b0, SHAFT_Y - v, SHAFT_Z + u) for (u, v) in ear_outline()]
    blade = Part.Face(Part.makePolygon(pts + [pts[0]])).extrude(V(b1 - b0, 0, 0))
    ear = blade.fuse(xcyl(HUB_R, x["inner"], x["outer"]))
    ear = ear.cut(xcyl(SOCKET_D / 2, x["inner"] - s * 1.0, x["socket"]))
    ear = ear.cut(xcyl(SCREW_D / 2, x["inner"], x["outer"] + s * 1.0))
    ear = ear.cut(xcyl(HEAD_D / 2, x["head"], x["outer"] + s * 1.0))
    return ear.removeSplitter()


def ear_at(shape, logical):
    """The ear (modelled at 90) turned to a logical angle: 0 = lying back, 90 = up, 180 = front."""
    e = shape.copy()
    e.rotate(V(0, SHAFT_Y, SHAFT_Z), V(1, 0, 0), logical - 90.0)
    return e


# --------------------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------------------
def write_stl(shape, path, tol=0.03):
    verts, tris = shape.tessellate(tol)
    with open(path, "wb") as fh:
        fh.write(b"esp32-eyes".ljust(80, b" "))
        fh.write(struct.pack("<I", len(tris)))
        for a, b, c in tris:
            pa, pb, pc = verts[a], verts[b], verts[c]
            n = (pb - pa).cross(pc - pa)
            if n.Length > 1e-12:
                n.normalize()
            fh.write(struct.pack("<12fH", n.x, n.y, n.z, pa.x, pa.y, pa.z, pb.x, pb.y, pb.z, pc.x, pc.y, pc.z, 0))
    return len(tris)


# the ways a part can lie: one of its six sides on the bed, or one of its big flat faces
AXES = [("-Z", V(0, 0, -1)), ("+Z", V(0, 0, 1)), ("-Y", V(0, -1, 0)), ("+Y", V(0, 1, 0)),
        ("-X", V(-1, 0, 0)), ("+X", V(1, 0, 0))]


def turned(shape, rot):
    """The part turned by rot, centred on the bed and resting on it."""
    s = shape.copy()
    if rot.Angle > 1e-9:
        s.rotate(V(0, 0, 0), rot.Axis, math.degrees(rot.Angle))
    bb = s.BoundBox
    s.translate(V(-(bb.XMin + bb.XMax) / 2, -(bb.YMin + bb.YMax) / 2, -bb.ZMin))
    return s


def mesh_arrays(shape):
    """Triangles of the part: corners (T, 3, 3), unit normals (T, 3), areas (T,)."""
    verts, tris = shape.tessellate(0.1)
    pts = np.array([(v.x, v.y, v.z) for v in verts])
    t = pts[np.array(tris)]
    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    ln = np.linalg.norm(n, axis=1)
    keep = ln > 1e-12
    t, n, ln = t[keep], n[keep] / ln[keep][:, None], ln[keep]
    vol = np.einsum("ij,ij->i", t[:, 0], np.cross(t[:, 1], t[:, 2])) / 6.0
    com = (vol[:, None] * t.sum(axis=1) / 4.0).sum(axis=0) / vol.sum()
    return t, n, ln / 2.0, com


def hull(pts):
    """Convex hull of 2D points (monotone chain), counter-clockwise."""
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def print_stats(mesh, d):
    """Lying with the side d down: mm2 resting on the bed, mm2 looking down more than
    45 degrees (what needs support), and whether it stands (its centre of mass falls
    at least 1 mm inside what rests on the bed)."""
    t, n, area, com = mesh
    h = t @ d                               # height of each corner, measured downwards
    down = n @ d                            # 1 = the face looks straight at the bed
    on_bed = (down > 0.99) & ((h.max() - h).max(axis=1) < 0.05)
    hang = (down > 0.7072) & ~on_bed
    stands = False
    if on_bed.any():
        e1 = np.cross(d, [1.0, 0.0, 0.0] if abs(d[0]) < 0.9 else [0.0, 1.0, 0.0])
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(d, e1)
        foot = t[on_bed].reshape(-1, 3)
        poly = hull([(round(float(p @ e1), 3), round(float(p @ e2), 3)) for p in foot])
        c = (float(com @ e1), float(com @ e2))
        if len(poly) >= 3:
            m = len(poly)
            dist = []
            for i in range(m):
                (ax, ay), (bx, by) = poly[i], poly[(i + 1) % m]
                ln = math.hypot(bx - ax, by - ay) or 1.0
                dist.append(((bx - ax) * (c[1] - ay) - (by - ay) * (c[0] - ax)) / ln)
            stands = min(dist) > 1.0
    return area[on_bed].sum(), area[hang].sum(), stands


def candidates(shape):
    """Its six sides, its big flat faces, and directions spread over the whole sphere."""
    named = [(name, n) for name, n in AXES]
    flats = sorted((f for f in shape.Faces if "Plane" in str(type(f.Surface))), key=lambda f: -f.Area)
    for f in flats[:16]:
        if f.Area < 30.0:
            break
        u0, u1, v0, v1 = f.ParameterRange
        n = f.normalAt((u0 + u1) / 2, (v0 + v1) / 2)       # already follows the face orientation
        if all(n.getAngle(m) > 0.001 for _, m in named):
            named.append(("face %.0f mm2" % f.Area, n))
    spread = []
    k = 600
    for i in range(k):                      # Fibonacci sphere
        z = 1.0 - 2.0 * (i + 0.5) / k
        r = math.sqrt(1.0 - z * z)
        a = i * math.pi * (3.0 - math.sqrt(5.0))
        spread.append(("tilted", V(r * math.cos(a), r * math.sin(a), z)))
    return named, spread


def best_lie(shape):
    """The way of lying with the least surface over the air, among those that stand on the
    bed with at least 25 mm2 (ties: the most on the bed). A tilted position is only taken
    when it saves a good part of the supports of the best side or flat face."""
    mesh = mesh_arrays(shape)
    named, spread = candidates(shape)

    def pick(cands, show):
        best = None
        for name, n in cands:
            bed, hang, stands = print_stats(mesh, np.array([n.x, n.y, n.z]))
            if show and DEBUG_LIE:
                print("      %-16s bed %6.0f  air %6.0f  %s" % (name, bed, hang, "stands" if stands else "falls"))
            key = (not stands or bed < 25.0, round(hang / 25.0), -bed)
            if best is None or key < best[0]:
                best = (key, name, n, bed, hang)
        return best

    a = pick(named, True)
    b = pick(spread, False)
    if not b[0][0] and (a[0][0] or b[4] < 0.8 * a[4] - 30.0):
        a = b
        if DEBUG_LIE:
            print("      tilted: %s, bed %.0f  air %.0f" % (tuple(round(c, 3) for c in a[2]), a[3], a[4]))
    rot = FreeCAD.Rotation(a[2], V(0, 0, -1))
    return a[1], rot, a[3], a[4]


def overlap(a, b):
    if not a.BoundBox.intersect(b.BoundBox):
        return 0.0
    try:
        return a.common(b).Volume
    except Exception:
        return -1.0


# --------------------------------------------------------------------------------------
def main():
    os.makedirs(OUT_STL, exist_ok=True)
    os.makedirs(OUT_ASM, exist_ok=True)
    for d in (OUT_STL, OUT_ASM):
        for f in os.listdir(d):
            if f.endswith(".stl"):
                os.remove(os.path.join(d, f))

    doc = FreeCAD.openDocument(SRC)
    print("model: %s" % os.path.relpath(SRC, ROOT))
    parts = []              # (file, shape in place, copies, what)
    for fname, obj, copies, what in PRINTED:
        parts.append((fname, doc.getObject(obj).Shape.copy(), copies, what))
    ears = {s: make_ear(s) for s in (1, -1)}
    parts.append(("oreja_derecha", ears[1], 1, "oreja derecha, a presion en el eje del servo"))
    parts.append(("oreja_izquierda", ears[-1], 1, "oreja izquierda"))
    elec = [(n, doc.getObject(o).Shape.copy()) for n, o in ELECTRONICS]
    plug2 = doc.getObject("Fusion034").Shape.copy()

    print("\ncheck 1: every printed part is one closed, valid solid")
    problems = 0
    for fname, shape, _, _ in parts:
        ok = shape.isValid() and len(shape.Solids) == 1 and shape.Solids[0].isClosed()
        problems += 0 if ok else 1
        print("  %-16s %s" % (fname, "ok" if ok else "PROBLEM (%d solids, valid %s)" % (len(shape.Solids), shape.isValid())))

    print("\ncheck 2: where the parts of the model touch each other (mm3 shared)")
    placed = [(f, s) for f, s, _, _ in parts] + [("tapon_frontal_2", plug2)]
    for i, (fa, sa) in enumerate(placed):
        for fb, sb in placed[i + 1:] + elec:
            v = overlap(sa, sb)
            if abs(v) > 0.5:
                print("  %-16s x %-16s %7.1f" % (fa, fb, v))

    print("\ncheck 3: the ears turn from 0 (lying back) to 180 (front) without hitting anything")
    others = [(f, s) for f, s in placed if not f.startswith("oreja")] + elec
    for s, fname in ((1, "oreja_derecha"), (-1, "oreja_izquierda")):
        own = "servo_derecho" if s == 1 else "servo_izquierdo"
        hits = []
        for ang in range(0, 181, 15):
            e = ear_at(ears[s], ang)
            for fo, so in others:
                v = overlap(e, so)
                limit = 2.5 if fo == own else 0.05      # its own servo: the press fit on the spline
                if abs(v) > limit:
                    hits.append("%d deg: %s %.1f mm3" % (ang, fo, v))
        problems += len(hits)
        own_fit = overlap(ears[s], dict(elec)[own])
        print("  %-16s %s   (press fit on the spline: %.1f mm3)" % (
            fname, "clear" if not hits else "; ".join(hits), own_fit))

    print("\nprint files (cad/stl), each lying on its side with the least surface over the air:")
    print("  %-16s %-14s %9s %9s  %-22s %s" % ("part", "on the bed", "bed mm2", "air mm2", "size mm", "PLA"))
    grams = 0.0
    for fname, shape, copies, what in parts:
        if DEBUG_LIE:
            print("    %s:" % fname)
        side, rot, bed, hang = best_lie(shape)
        flat = turned(shape, rot)
        write_stl(flat, os.path.join(OUT_STL, fname + ".stl"))
        write_stl(shape, os.path.join(OUT_ASM, fname + ".stl"), tol=0.05)
        bb = flat.BoundBox
        g = shape.Volume / 1000.0 * PLA * copies
        grams += g
        print("  %-16s %-14s %9.0f %9.0f  %5.1f x %5.1f x %5.1f  ~%.0f g%s" % (
            fname, side, bed, hang, bb.XLength, bb.YLength, bb.ZLength, g, "  (x%d)" % copies if copies > 1 else ""))
    write_stl(plug2, os.path.join(OUT_ASM, "tapon_frontal_2.stl"), tol=0.05)
    print("  about %.0f g of PLA if printed solid (less with infill)" % grams)

    out = FreeCAD.newDocument("Esp32Eyes")
    for fname, shape, _, _ in parts:
        out.addObject("Part::Feature", fname).Shape = shape
    out.addObject("Part::Feature", "tapon_frontal_2").Shape = plug2
    for n, shape in elec:
        out.addObject("Part::Feature", n).Shape = shape
    out.recompute()
    Part.export(out.Objects, os.path.join(ROOT, "cad", "esp32_eyes.step"))
    print("\nleft out of the model: %s" % ", ".join(LEFT_OUT))
    print("\n%s" % ("ALL CHECKS PASSED" if problems == 0 else "%d PROBLEMS" % problems))


main()
_log.close()
