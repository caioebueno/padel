"""Procedural textures for the racket (docs/decisions/0008-racket.md). Tuning and the
graphic design are in config/racket.json; positions come from racket_layout.py.

  T_Racket_Masks      the face graphic over a square of the face (racket_layout.face_uv),
                      linear RGBA: R = paint panel, G = accent (the keystone, the pinstripe
                      and the panel's edge line), B = ink (the OSTREL wordmark, the model
                      name and the fine print), A = holes (only drawn by LOD2, which has
                      no hole geometry)
  T_CarbonWeave_Mask  one tile of 2x2 twill carbon: how bright each tow looks, so the bare
                      carbon catches the light tow by tow
  T_RacketGrit_N      one tile of the raised sand finish (the face's 3D texture),
                      tangent-space normal, DirectX green like the other normal maps

Pure Python and seeded, like textures.py and ball_textures.py: identical everywhere and
needs only the standard library. The wordmark uses the stroke letters below, not a font
file, so there is nothing to license.
"""

from __future__ import annotations

import math
import random
from pathlib import Path
from typing import Iterable

from . import racket_layout as layout
from .textures import write_png

Point = tuple[float, float]

# Stroke letters on a cap-height grid (x right, y up, cap height 1). Each glyph is
# (advance, strokes); a stroke is a polyline. Only the letters the design uses.
_O = [[(0.5 + 0.47 * math.copysign(abs(math.cos(t)) ** 0.7, math.cos(t)),
        0.5 + 0.5 * math.copysign(abs(math.sin(t)) ** 0.7, math.sin(t)))
       for t in (2 * math.pi * i / 40 for i in range(41))]]
_C = [[(0.52 + 0.47 * math.copysign(abs(math.cos(t)) ** 0.7, math.cos(t)),
        0.5 + 0.5 * math.copysign(abs(math.sin(t)) ** 0.7, math.sin(t)))
       for t in (math.radians(40 + 280 * i / 32) for i in range(33))]]
GLYPHS: dict[str, tuple[float, list[list[Point]]]] = {
    " ": (0.45, []),
    "O": (1.0, _O),
    "C": (0.92, _C),
    "S": (0.84, [[(0.82, 0.86), (0.66, 1.0), (0.22, 1.0), (0.04, 0.84), (0.06, 0.62), (0.24, 0.52),
                  (0.62, 0.47), (0.82, 0.36), (0.84, 0.14), (0.66, 0.0), (0.18, 0.0), (0.0, 0.15)]]),
    "T": (0.86, [[(0.0, 1.0), (0.86, 1.0)], [(0.43, 1.0), (0.43, 0.0)]]),
    "R": (0.86, [[(0.0, 0.0), (0.0, 1.0), (0.62, 1.0), (0.82, 0.88), (0.84, 0.64), (0.64, 0.5), (0.0, 0.5)],
                 [(0.5, 0.5), (0.86, 0.0)]]),
    "E": (0.78, [[(0.78, 1.0), (0.0, 1.0), (0.0, 0.0), (0.78, 0.0)], [(0.0, 0.5), (0.64, 0.5)]]),
    "L": (0.74, [[(0.0, 1.0), (0.0, 0.0), (0.74, 0.0)]]),
    "K": (0.86, [[(0.0, 0.0), (0.0, 1.0)], [(0.84, 1.0), (0.0, 0.38)], [(0.3, 0.6), (0.86, 0.0)]]),
    "Y": (0.9, [[(0.0, 1.0), (0.45, 0.48), (0.9, 1.0)], [(0.45, 0.48), (0.45, 0.0)]]),
    "N": (0.86, [[(0.0, 0.0), (0.0, 1.0), (0.86, 0.0), (0.86, 1.0)]]),
    "A": (0.94, [[(0.0, 0.0), (0.47, 1.0), (0.94, 0.0)], [(0.17, 0.36), (0.77, 0.36)]]),
    "B": (0.84, [[(0.0, 0.0), (0.0, 1.0), (0.6, 1.0), (0.78, 0.88), (0.78, 0.64), (0.6, 0.53), (0.0, 0.53)],
                 [(0.6, 0.53), (0.84, 0.4), (0.84, 0.13), (0.64, 0.0), (0.0, 0.0)]]),
    "1": (0.5, [[(0.08, 0.78), (0.36, 1.0), (0.36, 0.0)]]),
    "8": (0.82, [[(0.41 + 0.33 * math.cos(t), 0.76 + 0.24 * math.sin(t)) for t in (2 * math.pi * i / 28 for i in range(29))],
                 [(0.41 + 0.4 * math.cos(t), 0.27 + 0.27 * math.sin(t)) for t in (2 * math.pi * i / 28 for i in range(29))]]),
}


def text_strokes(text: str, centre: Point, cap: float, tracking: float, slant: float = 0.0) -> list[list[Point]]:
    """Strokes of a line of text centred on `centre` (face metres), cap height `cap`,
    letter spacing `tracking` caps, slanted by `slant` (x shift per unit of height)."""
    advances = [GLYPHS[c][0] for c in text]
    width = (sum(advances) + tracking * (len(text) - 1)) * cap
    x = centre[0] - width / 2
    out = []
    for char, adv in zip(text, advances):
        for stroke in GLYPHS[char][1]:
            out.append([(x + (gx + slant * (gy - 0.5)) * cap, centre[1] + (gy - 0.5) * cap) for gx, gy in stroke])
        x += (adv + tracking) * cap
    return out


def text_width(text: str, cap: float, tracking: float) -> float:
    return (sum(GLYPHS[c][0] for c in text) + tracking * (len(text) - 1)) * cap


class Canvas:
    """Float coverage channels over the face texture, with anti-aliased stamps that only
    visit the pixels near each shape (pure Python has to be frugal at 1024 x 1024)."""

    def __init__(self, cfg: dict) -> None:
        self.cfg = cfg
        self.n = int(cfg["textures"]["masks_px"])
        self.size = cfg["textures"]["size_m"]
        self.px = self.size / self.n
        self.channels = [[0.0] * (self.n * self.n) for _ in range(4)]

    def _bbox(self, xs: Iterable[float], zs: Iterable[float], pad: float) -> tuple[range, range]:
        xs, zs = list(xs), list(zs)
        tip = layout.tip_z(self.cfg)
        c0 = max(0, int((min(xs) - pad + self.size / 2) / self.px))
        c1 = min(self.n, int((max(xs) + pad + self.size / 2) / self.px) + 2)
        r0 = max(0, int((tip - max(zs) - pad) / self.px))
        r1 = min(self.n, int((tip - min(zs) + pad) / self.px) + 2)
        return range(r0, r1), range(c0, c1)

    def _put(self, ch: int, row: int, col: int, value: float) -> None:
        i = row * self.n + col
        if value > self.channels[ch][i]:
            self.channels[ch][i] = min(1.0, value)

    def stroke(self, ch: int, polyline: list[Point], width: float) -> None:
        half = width / 2
        for a, b in zip(polyline, polyline[1:]):
            rows, cols = self._bbox((a[0], b[0]), (a[1], b[1]), half + self.px)
            for row in rows:
                for col in cols:
                    p = layout.pixel_to_face(self.cfg, col, row)
                    d = layout._segment_distance(p, a, b)
                    self._put(ch, row, col, (half - d) / self.px + 0.5)

    def disc(self, ch: int, centre: Point, radius: float) -> None:
        rows, cols = self._bbox((centre[0],), (centre[1],), radius + self.px)
        for row in rows:
            for col in cols:
                d = math.dist(layout.pixel_to_face(self.cfg, col, row), centre)
                self._put(ch, row, col, (radius - d) / self.px + 0.5)

    def fill(self, ch: int, coverage, bounds: tuple[float, float, float, float]) -> None:
        """coverage(x, z) -> signed distance inside the shape in metres (> 0 inside)."""
        x0, z0, x1, z1 = bounds
        rows, cols = self._bbox((x0, x1), (z0, z1), self.px)
        for row in rows:
            for col in cols:
                self._put(ch, row, col, coverage(*layout.pixel_to_face(self.cfg, col, row)) / self.px + 0.5)

    def pixel(self, x: int, y: int) -> tuple[int, int, int, int]:
        i = y * self.n + x
        return tuple(int(round(255 * max(0.0, min(1.0, c[i])))) for c in self.channels)  # type: ignore[return-value]


def draw_face(cfg: dict) -> Canvas:
    g = cfg["graphics"]
    canvas = Canvas(cfg)
    R, G, B, A = range(4)
    half_w = layout.widest(cfg)[0] + 0.01

    # The paint panel: everything below a line rising from left to right across the
    # lower head (so the arms and the throat are painted too).
    panel = g["panel"]
    zl, zr = panel["left_top_z_m"], panel["right_top_z_m"]

    def panel_edge(x: float) -> float:
        return zl + (zr - zl) * (x + half_w) / (2 * half_w)

    slope = (zr - zl) / (2 * half_w)
    cos_s = 1 / math.sqrt(1 + slope * slope)
    canvas.fill(R, lambda x, z: min((panel_edge(x) - z) * cos_s, z - panel["bottom_z_m"]),
                (-half_w, panel["bottom_z_m"], half_w, max(zl, zr)))
    # A fine accent line just above the panel's edge.
    canvas.stroke(G, [(-half_w, zl + 0.0035), (half_w, zr + 0.0035)], g["pinstripe_m"])

    # The keystone round the sweet spot.
    k = g["keystone"]
    stone = layout.keystone(cfg)
    canvas.stroke(G, stone + stone[:1], k["stroke_m"])
    for p in stone:
        canvas.disc(G, p, k["stroke_m"] / 2)

    # Pinstripe inside the frame edge, round the head only (the arms are too narrow).
    inner = layout.offset(layout.outline(cfg, 360), g["pinstripe_inset_m"])
    run: list[Point] = []
    for p in inner + inner[:1]:
        if p[1] > panel_edge(p[0]) + 0.006:
            run.append(p)
        elif run:
            canvas.stroke(G, run, g["pinstripe_m"])
            run = []
    if run:
        canvas.stroke(G, run, g["pinstripe_m"])

    # The wordmark, the model name and the fine print at the tip, in ink.
    brand = cfg["brand"]
    for text, spec, slant in ((brand["name"], g["wordmark"], 0.16), (brand["model"], g["model"], 0.0),
                              (brand["tag"], g["tag"], 0.0)):
        for s in text_strokes(text, (0.0, spec["centre_z_m"]), spec["cap_m"], spec["tracking"], slant):
            canvas.stroke(B, s, spec["stroke_m"])

    # Holes, a little smaller than the drilled ones so LOD0 shows no dark ring round them.
    r = cfg["holes"]["diameter_m"] / 2 - 0.0004
    for c in layout.hole_centres(cfg):
        canvas.disc(A, c, r)
    return canvas


def weave_tile(cfg: dict) -> tuple[int, list[float]]:
    """2x2 twill: each tow passes over two and under two, shifted one tow per row, so the
    weave runs in diagonals. Brightness per pixel 0..1: tows along U and along V catch the
    light differently (the checker that makes carbon look like carbon), each tow is
    rounded across its width, and the gaps between tows are dark."""
    t = cfg["textures"]
    size, tows = int(t["weave_px"]), int(t["weave_tows"])
    cell = size / tows
    rng = random.Random(cfg["seed"])
    jitter = [rng.uniform(-0.06, 0.06) for _ in range(tows * tows)]
    out = []
    for y in range(size):
        for x in range(size):
            i, j = int(x / cell), int(y / cell)
            fx, fy = x / cell - i, y / cell - j
            warp_on_top = (i + j) % 4 < 2
            across = fy if warp_on_top else fx
            along = fx if warp_on_top else fy
            # Rounded tow, dark in the gap between neighbours.
            profile = math.sin(math.pi * min(max(across, 0.0), 1.0)) ** 0.6
            # Tows dip where they pass under their neighbours.
            dip = 0.85 + 0.15 * math.sin(math.pi * along)
            sheen = 0.78 if warp_on_top else 0.38
            out.append(max(0.0, min(1.0, (sheen + jitter[j * tows + i]) * profile * dip)))
    return size, out


def grit_height(cfg: dict) -> tuple[int, list[float]]:
    """The sand finish: rounded grains of varied size bonded onto the face, tiling."""
    t = cfg["textures"]
    size = int(t["grit_px"])
    rng = random.Random(cfg["seed"] + 1)
    h = [0.0] * (size * size)
    lo, hi = t["grit_radius_px"]
    for _ in range(int(t["grit_grains"])):
        cx, cy = rng.uniform(0, size), rng.uniform(0, size)
        r = rng.uniform(lo, hi)
        top = rng.uniform(0.5, 1.0)
        squash = rng.uniform(0.7, 1.0)
        reach = int(math.ceil(r)) + 1
        for oy in range(-reach, reach + 1):
            for ox in range(-reach, reach + 1):
                px, py = int(cx) + ox, int(cy) + oy
                dx = (px + 0.5 - cx) / r
                dy = (py + 0.5 - cy) / (r * squash)
                d2 = dx * dx + dy * dy
                if d2 >= 1.0:
                    continue
                v = top * math.sqrt(1.0 - d2)
                i = (py % size) * size + px % size
                if v > h[i]:
                    h[i] = v
    return size, h


def _byte(v: float) -> int:
    return int(round(255 * max(0.0, min(1.0, v))))


def generate_all(out_dir: Path, cfg: dict | None = None) -> list[Path]:
    cfg = cfg or layout.load_config()
    canvas = draw_face(cfg)
    n = canvas.n
    paths = [write_png(out_dir / "T_Racket_Masks.png", n, n, 4, canvas.pixel)]

    wsize, weave = weave_tile(cfg)
    paths.append(write_png(out_dir / "T_CarbonWeave_Mask.png", wsize, wsize, 1,
                           lambda x, y: (_byte(weave[y * wsize + x]),)))

    gsize, h = grit_height(cfg)
    # Grains about 0.15 mm proud on a 14 mm tile of 256 px: gain turns height per pixel
    # into slope.
    gain = 0.15 / (cfg["textures"]["grit_tile_m"] * 1000.0 / gsize) * 3.0

    def at(x: int, y: int) -> float:
        return h[(y % gsize) * gsize + x % gsize]

    def normal(x: int, y: int) -> tuple[int, int, int]:
        ddx = (at(x + 1, y) - at(x - 1, y)) * 0.5 * gain
        ddy = (at(x, y + 1) - at(x, y - 1)) * 0.5 * gain
        nx, ny, nz = -ddx, -ddy, 1.0
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        return (_byte(nx / length * 0.5 + 0.5), _byte(ny / length * 0.5 + 0.5), _byte(nz / length * 0.5 + 0.5))

    paths.append(write_png(out_dir / "T_RacketGrit_N.png", gsize, gsize, 3, normal))
    return paths
