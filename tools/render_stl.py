"""Pictures of the printed parts for the README, made from the STL of cad/ (no FreeCAD needed).

    python tools/render_stl.py

doc/cad-ensamblado.png   the robot put together (ears up)
doc/cad-despiece.png     the same, pulled apart
doc/cad-piezas.png       every part as it lies on the printer bed
"""

import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STL = os.path.join(ROOT, "cad", "stl")
ASM = os.path.join(ROOT, "cad", "ensamblado")
DOC = os.path.join(ROOT, "doc")

COLORS = {"carcasa": (236, 236, 232), "chasis": (40, 40, 44), "pata": (196, 198, 204),
          "tapon": (110, 210, 120), "oreja": (90, 220, 100)}

# how far each part moves in the exploded view (mm, in the frame of the CAD)
EXPLODE = {"chasis": (0, 62, 0), "pata_derecha": (38, 0, -8), "pata_izquierda": (-38, 0, -8),
           "tapon_frontal": (0, -34, 0), "tapon_frontal_2": (0, -34, 0),
           "oreja_derecha": (24, 0, 22), "oreja_izquierda": (-24, 0, 22)}


def read_stl(path):
    with open(path, "rb") as fh:
        data = fh.read()
    n = int.from_bytes(data[80:84], "little")
    dt = np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
    return np.frombuffer(data, dtype=dt, count=n, offset=84)["v"].astype(np.float64)


def color_of(name):
    for k, c in COLORS.items():
        if name.startswith(k):
            return c
    return (200, 120, 200)


def rot(yaw, pitch):
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    rz = np.array([[cy, -sy, 0], [sy, cy, 0], [0, 0, 1]])
    rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
    return rx @ rz


def render(parts, yaw, pitch, size=(1000, 760), title=None, labels=None, ss=2):
    """parts: [(name, triangles)]. The camera looks along +y after turning the scene."""
    W, H = size[0] * ss, size[1] * ss
    R = rot(yaw, pitch)
    t = np.concatenate([v @ R.T for _, v in parts])
    c = np.concatenate([np.repeat([color_of(n)], len(v), axis=0) for n, v in parts]).astype(np.float64)
    allp = t.reshape(-1, 3)
    umin, umax = allp[:, 0].min(), allp[:, 0].max()
    vmin, vmax = allp[:, 2].min(), allp[:, 2].max()
    top = 0.12 if title else 0.06
    scale = min(W * 0.88 / (umax - umin), H * (0.94 - top) / (vmax - vmin))
    ox = W / 2 - (umin + umax) / 2 * scale
    oy = H * (top + 0.94) / 2 + (vmin + vmax) / 2 * scale

    def to_screen(p):
        return np.stack([ox + p[..., 0] * scale, oy - p[..., 2] * scale], axis=-1)

    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    ln = np.linalg.norm(n, axis=1)
    keep = ln > 1e-12
    t, c, n = t[keep], c[keep], n[keep] / ln[keep][:, None]
    facing = n[:, 1] < 0.02
    t, c, n = t[facing], c[facing], n[facing]
    light = np.array([-0.45, -0.75, 0.55])
    light /= np.linalg.norm(light)
    lam = np.clip(-n[:, 1] * 0.3 + np.clip(n @ light, 0, 1) * 0.7, 0, 1)
    rgb = np.clip(c * (0.30 + 0.70 * lam)[:, None] + 10, 0, 255).astype(np.uint8)
    scr = to_screen(t)

    zbuf = np.full((H, W), np.inf)
    img = np.zeros((H, W, 3), dtype=np.uint8)
    for y in range(H):
        k = y / H
        img[y, :] = (int(44 - 18 * k), int(48 - 18 * k), int(58 - 20 * k))
    for i in range(len(t)):
        p = scr[i]
        x0 = max(int(math.floor(p[:, 0].min())), 0)
        x1 = min(int(math.ceil(p[:, 0].max())), W - 1)
        y0 = max(int(math.floor(p[:, 1].min())), 0)
        y1 = min(int(math.ceil(p[:, 1].max())), H - 1)
        if x1 < x0 or y1 < y0:
            continue
        (ax, ay), (bx, by), (cx, cy) = p
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-9:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
        w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not inside.any():
            continue
        z = w0 * t[i, 0, 1] + w1 * t[i, 1, 1] + w2 * t[i, 2, 1]
        sub = zbuf[y0:y1 + 1, x0:x1 + 1]
        win = inside & (z < sub)
        if win.any():
            sub[win] = z[win]
            img[y0:y1 + 1, x0:x1 + 1][win] = rgb[i]
    out = Image.fromarray(img, "RGB").resize(size, Image.LANCZOS)
    d = ImageDraw.Draw(out)
    try:
        font = ImageFont.truetype("segoeui.ttf", 22)
        small = ImageFont.truetype("segoeui.ttf", 17)
    except OSError:
        font = small = ImageFont.load_default()
    if title:
        d.text((20, 14), title, fill=(214, 220, 232), font=font)
    for text, (x, y, z) in labels or []:
        q = to_screen(np.array([x, y, z]) @ R.T) / ss
        w = d.textlength(text, font=small)
        d.text((q[0] - w / 2, q[1]), text, fill=(200, 208, 222), font=small)
    return out


def load(folder):
    return [(f[:-4], read_stl(os.path.join(folder, f))) for f in sorted(os.listdir(folder)) if f.endswith(".stl")]


def main():
    asm = load(ASM)
    render(asm, -32, 16, title="Las piezas impresas, armadas").save(os.path.join(DOC, "cad-ensamblado.png"))
    moved = [(n, v + np.array(EXPLODE.get(n, (0, 0, 0)))) for n, v in asm]
    render(moved, -48, 18, title="Despiece: el chasis entra en la carcasa por detrás").save(os.path.join(DOC, "cad-despiece.png"))

    # the print files side by side, as they lie on the bed
    parts = load(STL)
    order = ["carcasa", "chasis", "pata_derecha", "pata_izquierda", "oreja_derecha", "oreja_izquierda", "tapon_frontal"]
    parts.sort(key=lambda p: order.index(p[0]) if p[0] in order else 99)
    placed, labels = [], []
    x = 0.0
    for name, v in parts:
        lo, hi = v.reshape(-1, 3).min(axis=0), v.reshape(-1, 3).max(axis=0)
        w = hi[0] - lo[0]
        placed.append((name, v + np.array([x - lo[0], 0, 0])))
        labels.append((name.replace("_", " ") + (" x2" if name == "tapon_frontal" else ""), (x + w / 2, lo[1] - 4, -6)))
        x += w + 12.0
    render(placed, 0, 24, size=(1500, 520), title="Como van en la cama de la impresora", labels=labels).save(
        os.path.join(DOC, "cad-piezas.png"))
    print("doc/cad-ensamblado.png, doc/cad-despiece.png, doc/cad-piezas.png")


if __name__ == "__main__":
    main()
