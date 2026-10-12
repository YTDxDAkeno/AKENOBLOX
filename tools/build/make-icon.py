#!/usr/bin/env python3
# AkenoBlox PS5 - draw the home-screen icon (512x512 PNG), from nothing.
# Copyright (C) 2026 AkenoBlox contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   make-icon.py <out.png>
#
# Generated here so the title ships art of known origin (PS5_VulkanTemplate
# title-packaging.md: "Every asset has a known origin and licence"): a dark
# blue field, an isometric block outlined in the launcher's accent blue, and
# "AKENO" in the launcher's font. No Roblox or PlayStation marks. Standard
# library only, so a build host needs no imaging package.
import pathlib
import re
import struct
import sys
import zlib

SIZE = 512
ROOT = pathlib.Path(__file__).resolve().parents[2]


def font() -> dict:
    """font8x8's basic Latin table (public domain), parsed from the vendored header."""
    text = (ROOT / "third_party/font8x8/font8x8_basic.h").read_text()
    rows = re.findall(r"\{\s*((?:0x[0-9A-Fa-f]{2},?\s*){8})\}", text)
    return {i: [int(b, 16) for b in re.findall(r"0x[0-9A-Fa-f]{2}", r)] for i, r in enumerate(rows)}


def main() -> int:
    out = pathlib.Path(sys.argv[1])
    px = [[(0, 0, 0)] * SIZE for _ in range(SIZE)]
    top, bottom = (5, 9, 20), (14, 30, 66)
    for y in range(SIZE):
        t = y / (SIZE - 1)
        c = tuple(round(a + (b - a) * t) for a, b in zip(top, bottom))
        for x in range(SIZE):
            px[y][x] = c

    accent, light, white = (46, 125, 255), (124, 178, 255), (232, 238, 249)

    def dot(x, y, c, r=5):
        for yy in range(y - r, y + r + 1):
            for xx in range(x - r, x + r + 1):
                if 0 <= xx < SIZE and 0 <= yy < SIZE and (xx - x) ** 2 + (yy - y) ** 2 <= r * r:
                    px[yy][xx] = c

    def line(a, b, c, r=5):
        steps = max(abs(b[0] - a[0]), abs(b[1] - a[1]))
        for i in range(steps + 1):
            dot(round(a[0] + (b[0] - a[0]) * i / steps), round(a[1] + (b[1] - a[1]) * i / steps), c, r)

    # An isometric block: top face, then the two visible sides.
    cx, cy, w, h = 256, 172, 130, 75
    top_face = [(cx, cy - h), (cx + w, cy), (cx, cy + h), (cx - w, cy)]
    for i in range(4):
        line(top_face[i], top_face[(i + 1) % 4], light)
    depth = 140
    line((cx - w, cy), (cx - w, cy + depth), accent)
    line((cx + w, cy), (cx + w, cy + depth), accent)
    line((cx, cy + h), (cx, cy + h + depth), accent)
    line((cx - w, cy + depth), (cx, cy + h + depth), accent)
    line((cx + w, cy + depth), (cx, cy + h + depth), accent)

    glyphs = font()
    word, scale = "AKENO", 9
    x0 = (SIZE - len(word) * 8 * scale) // 2
    y0 = 418
    for n, ch in enumerate(word):
        rows = glyphs[ord(ch)]
        for r, bits in enumerate(rows):
            for col in range(8):
                if bits >> col & 1:
                    for yy in range(scale):
                        for xx in range(scale):
                            px[y0 + r * scale + yy][x0 + (n * 8 + col) * scale + xx] = white

    raw = b"".join(b"\x00" + bytes(v for p in row for v in p) for row in px)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(png)
    return 0


if __name__ == "__main__":
    sys.exit(main())
