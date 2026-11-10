#!/usr/bin/env python3
"""Convert GIF animations into compressed clips for the face engine.

Every GIF is scaled to 128x64, turned into 1 bit per pixel and stored as
key / XOR-delta frames with run-length encoding (format described in
src/clips/Clip.h). The result is a single C++ file with all the clips.

    python tools/gif2clip.py "../robot ojos" -o src/clips/ClipData.cpp

Requires Pillow (pip install pillow).
"""
import argparse
import glob
import os
import re
import sys

from PIL import Image, ImageSequence

W, H = 128, 64
FRAME_BYTES = W * H // 8

# Mood that likes to play each clip (matched on the file name, first hit wins).
# 0 Normal, 1 Happy, 2 Sad, 3 Angry, 4 Alert, 5 Sleepy
MOOD_RULES = [
    (r"sleep", 5),
    (r"heart|wink|boba|hula|nyan|switch|reveal|rila|wein|plant", 1),
    (r"attack|autobot", 3),
    (r"dizzy|escape|twitch", 4),
]


def frame_bits(img, invert):
    """PIL frame -> 1024 bytes, row-major, LSB first. Lit pixel = 1."""
    rgba = img.convert("RGBA")
    if rgba.size != (W, H):
        rgba = rgba.resize((W, H), getattr(Image, "Resampling", Image).NEAREST)
    px = rgba.load()
    out = bytearray(FRAME_BYTES)
    for y in range(H):
        for x in range(W):
            r, g, b, a = px[x, y]
            # The drawings are dark ink on a clear (or white) background:
            # the ink is what lights up on the OLED.
            ink = a > 127 and (r + g + b) < 384
            if invert:
                ink = not ink
            if ink:
                out[y * (W // 8) + (x >> 3)] |= 1 << (x & 7)
    return bytes(out)


def rle(data):
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        # run of equal bytes
        j = i
        while j < n and data[j] == data[i]:
            j += 1
        run = j - i
        if data[i] == 0:
            while run > 0:
                if run >= 64:
                    k = min(run, 319)
                    out += bytes([0xBF, k - 64])
                else:
                    k = run
                    out.append(0x80 + k - 1)
                run -= k
            i = j
            continue
        if run >= 3:
            while run >= 2:
                k = min(run, 65)
                out += bytes([0xC0 + k - 2, data[i]])
                run -= k
            i = j - run
            continue
        # literal run: until a zero byte or a run of 3 equal bytes starts
        j = i
        while j < n and j - i < 128:
            if data[j] == 0:
                break
            if j + 2 < n and data[j] == data[j + 1] == data[j + 2]:
                break
            j += 1
        if j == i:
            j = i + 1
        out.append(j - i - 1)
        out += data[i:j]
        i = j
    return bytes(out)


def encode(frames):
    out = bytearray()
    prev = None
    for f in frames:
        key = bytes([0]) + rle(f)
        best = key
        if prev is not None:
            delta = bytes([1]) + rle(bytes(a ^ b for a, b in zip(f, prev)))
            if len(delta) < len(key):
                best = delta
        out += best
        prev = f
    return bytes(out)


def decode(data, count):
    """Reference decoder, used to verify the encoder."""
    frames, pos = [], 0
    cur = bytearray(FRAME_BYTES)
    for _ in range(count):
        t = data[pos]
        pos += 1
        if t == 0:
            cur = bytearray(FRAME_BYTES)
        o = 0
        while o < FRAME_BYTES:
            op = data[pos]
            pos += 1
            if op < 0x80:
                for _ in range(op + 1):
                    cur[o] ^= data[pos]
                    pos += 1
                    o += 1
            elif op < 0xBF:
                o += op - 0x7F
            elif op == 0xBF:
                o += data[pos] + 64
                pos += 1
            else:
                v = data[pos]
                pos += 1
                for _ in range(op - 0xBE):
                    cur[o] ^= v
                    o += 1
        frames.append(bytes(cur))
    assert pos == len(data)
    return frames


def c_ident(name):
    s = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_")
    return s if not s[0].isdigit() else "_" + s


def pretty(name):
    s = re.sub(r"(?i)ortomi[_ ]*", "", name)
    s = re.sub(r"[^A-Za-z0-9]+", " ", s).strip()
    return s.title().replace(" ", "") or name


def c_array(data, per_line=24):
    lines = []
    for i in range(0, len(data), per_line):
        lines.append("  " + ",".join(str(b) for b in data[i:i + per_line]) + ",")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src", nargs="+", help="GIF files or folders")
    ap.add_argument("-o", "--out", default="src/clips/ClipData.cpp")
    ap.add_argument("--invert", action="store_true", help="light the background instead of the ink")
    ap.add_argument("--max-frames", type=int, default=0, help="drop clips longer than this (0 = keep all)")
    ap.add_argument("--skip", default="", help="regex of file names to leave out")
    args = ap.parse_args()

    files = []
    for s in args.src:
        if os.path.isdir(s):
            files += sorted(glob.glob(os.path.join(s, "*.gif")))
        else:
            files.append(s)
    if not files:
        sys.exit("no GIF found")

    clips, total_raw, total_enc, names = [], 0, 0, set()
    for path in files:
        base = os.path.splitext(os.path.basename(path))[0]
        if args.skip and re.search(args.skip, base, re.I):
            print(f"  skip   {base}")
            continue
        im = Image.open(path)
        frames, delays = [], []
        for fr in ImageSequence.Iterator(im):
            frames.append(frame_bits(fr, args.invert))
            delays.append(max(2, min(255, round(fr.info.get("duration", 80) / 10))))
        # A frame that is mostly lit is a drawing on a white page: flip it.
        lit = sum(bin(b).count("1") for b in frames[0]) / (W * H)
        if lit > 0.5:
            frames = [bytes(b ^ 0xFF for b in f) for f in frames]
        if args.max_frames and len(frames) > args.max_frames:
            print(f"  drop   {base} ({len(frames)} frames)")
            continue
        data = encode(frames)
        assert decode(data, len(frames)) == frames, base
        name = pretty(base)
        while name in names:
            name += "2"
        names.add(name)
        mood = 0
        for rx, m in MOOD_RULES:
            if re.search(rx, base, re.I):
                mood = m
                break
        clips.append((name, c_ident(base), frames, delays, data, mood))
        total_raw += len(frames) * FRAME_BYTES
        total_enc += len(data) + len(delays)
        print(f"  {name:22s} {len(frames):4d} frames  {len(frames) * FRAME_BYTES:7d} -> {len(data):6d} bytes")

    with open(args.out, "w", newline="\n") as f:
        f.write("// Generated by tools/gif2clip.py - do not edit.\n")
        f.write('#include "Clip.h"\n\n')
        for name, ident, frames, delays, data, mood in clips:
            f.write(f"static const uint8_t {ident}_data[] = {{\n{c_array(data)}\n}};\n")
            f.write(f"static const uint8_t {ident}_delays[] = {{\n{c_array(bytes(delays))}\n}};\n\n")
        f.write("static const ClipDef CLIPS[] = {\n")
        for name, ident, frames, delays, data, mood in clips:
            f.write(f'  {{"{name}", {len(frames)}, {len(data)}, {ident}_data, {ident}_delays, {mood}}},\n')
        f.write("};\n\n")
        f.write("int Clips::count() { return (int)(sizeof(CLIPS) / sizeof(CLIPS[0])); }\n")
        f.write("const ClipDef &Clips::get(int i) { return CLIPS[i]; }\n")

    print(f"{len(clips)} clips, {total_raw} -> {total_enc} bytes "
          f"({100.0 * total_enc / max(1, total_raw):.1f}%)  ->  {args.out}")


if __name__ == "__main__":
    main()
