"""Procedural textures for the racket (docs/decisions/0008-racket.md). Tuning and the
graphic design are in config/racket.json; positions come from racket_layout.py.

  T_Racket_Masks      the face graphic over a square of the face (racket_layout.face_uv),
                      linear RGBA: R = the arch emblem's silver stones, G = accent (the
                      keystone, the chevron and the shoulder lines), B = ink (the OSTREL
                      wordmark, the model name and the fine print), A = holes (only drawn
                      by LOD2, which has no hole geometry)
  T_RacketFrame_Masks the frame's edge unrolled (U round the outline from the neck, V across
                      the thickness): OSTREL down each side in G
  T_RacketGrip_Masks  the overgrip unrolled (U round, V along): its perforation dots in G
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


def _fill_polygon(canvas: Canvas, ch: int, poly: list[Point]) -> None:
    xs, zs = [p[0] for p in poly], [p[1] for p in poly]

    def inside(x: float, z: float) -> float:
        d = layout.distance_to_polyline((x, z), poly)
        return d if layout.point_in_polygon((x, z), poly) else -d

    canvas.fill(ch, inside, (min(xs), min(zs), max(xs), max(zs)))


def draw_face(cfg: dict) -> Canvas:
    g = cfg["graphics"]
    canvas = Canvas(cfg)
    R, G, B, A = range(4)

    # The arch emblem: silver stones, the keystone in the accent colour.
    stones = layout.arch_stones(cfg)
    key = cfg["graphics"]["arch"]["stones"] // 2
    for i, stone in enumerate(stones):
        _fill_polygon(canvas, G if i == key else R, stone)

    # The chevron along the bottom of the face, and a short line on each shoulder.
    canvas.stroke(G, layout.chevron(cfg), g["chevron"]["stroke_m"])
    sh = g["shoulder"]
    inner = layout.offset(layout.outline(cfg, 360), sh["inset_m"])
    for side in (-1, 1):
        run = sorted((p for p in inner if p[0] * side > 0 and sh["z_m"][0] <= p[1] <= sh["z_m"][1]), key=lambda p: p[1])
        canvas.stroke(G, run, sh["stroke_m"])

    # The wordmark, the model name and the fine print at the tip, in ink.
    brand = cfg["brand"]
    for text, spec in ((brand["name"], g["wordmark"]), (brand["model"], g["model"]), (brand["tag"], g["tag"])):
        for stroke in text_strokes(text, (0.0, spec["centre_z_m"]), spec["cap_m"], spec["tracking"], spec.get("slant", 0.0)):
            canvas.stroke(B, stroke, spec["stroke_m"])

    # Holes, a little smaller than the drilled ones so LOD0 shows no dark ring round them.
    r = cfg["holes"]["diameter_m"] / 2 - 0.0004
    for c in layout.hole_centres(cfg):
        canvas.disc(A, c, r)
    return canvas


class StripCanvas:
    """Float coverage over an unrolled strip: w x h pixels covering width_m x height_m,
    wrapping in U. Points are (u_m, v_m) from the top-left corner."""

    def __init__(self, w: int, h: int, width_m: float, height_m: float) -> None:
        self.w, self.h = w, h
        self.width_m, self.height_m = width_m, height_m
        self.channels = [[0.0] * (w * h) for _ in range(4)]

    def stroke(self, ch: int, polyline: list[Point], width: float) -> None:
        half = width / 2
        px = self.width_m / self.w
        py = self.height_m / self.h
        for a, b in zip(polyline, polyline[1:]):
            c0 = int((min(a[0], b[0]) - half) / px) - 1
            c1 = int((max(a[0], b[0]) + half) / px) + 2
            r0 = max(0, int((min(a[1], b[1]) - half) / py) - 1)
            r1 = min(self.h, int((max(a[1], b[1]) + half) / py) + 2)
            for row in range(r0, r1):
                for col in range(c0, c1):
                    p = ((col + 0.5) * px, (row + 0.5) * py)
                    d = layout._segment_distance(p, a, b)
                    i = row * self.w + col % self.w
                    v = min(1.0, (half - d) / min(px, py) + 0.5)
                    if v > self.channels[ch][i]:
                        self.channels[ch][i] = v

    def disc(self, ch: int, centre: Point, radius: float) -> None:
        self.stroke(ch, [centre, (centre[0] + 1e-6, centre[1])], 2 * radius)

    def pixel(self, x: int, y: int) -> tuple[int, int, int, int]:
        i = y * self.w + x
        return tuple(int(round(255 * max(0.0, min(1.0, c[i])))) for c in self.channels)  # type: ignore[return-value]


def draw_frame(cfg: dict) -> StripCanvas:
    """OSTREL along each side of the frame, centred at frame_text.centre_z_m and read
    from the throat towards the tip on both sides (so on one side it runs with U and on
    the other against it). V runs across the thickness, the letters' height."""
    w, h = cfg["textures"]["frame_px"]
    length = layout.perimeter(cfg)
    thick = cfg["frame"]["thickness_m"]
    canvas = StripCanvas(w, h, length, thick)
    spec = cfg["graphics"]["frame_text"]
    param = layout.edge_param(cfg)
    zc = spec["centre_z_m"]
    for side in (1, -1):
        u = layout.edge_u(cfg, side * layout.half_width_at(cfg, zc), zc, param) * length
        # The neck is U = 0 and the outline runs CCW, so the right side (+X) climbs with U.
        sign = 1.0 if side > 0 else -1.0
        for stroke in text_strokes(cfg["brand"]["name"], (0.0, 0.0), spec["cap_m"], spec["tracking"], 0.12):
            canvas.stroke(1, [(u + sign * x, thick / 2 - sign * z) for x, z in stroke], spec["stroke_m"])
    return canvas


def draw_grip(cfg: dict) -> StripCanvas:
    """Perforation dots on the overgrip: staggered rows with a little seeded jitter."""
    n = int(cfg["textures"]["grip_px"])
    d = cfg["graphics"]["grip_dots"]
    gr = cfg["grip"]
    ax, ay = gr["semi_axes_m"]
    around = math.pi * (3 * (ax + ay) - math.sqrt((3 * ax + ay) * (ax + 3 * ay)))
    along = gr["wrap_top_z_m"] - gr["wrap_bottom_z_m"]
    canvas = StripCanvas(n, n, around, along)
    rng = random.Random(cfg["seed"] + 7)
    rows, per = int(d["rows"]), int(d["per_row"])
    for i in range(rows):
        for j in range(per):
            u = (j + (0.5 if i % 2 else 0.0) + rng.uniform(-d["jitter"], d["jitter"])) / per * around
            v = (i + 0.5 + rng.uniform(-d["jitter"], d["jitter"])) / rows * along
            canvas.disc(1, (u, v), d["radius_m"] * rng.uniform(0.8, 1.2))
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
    for name, strip in (("T_RacketFrame_Masks", draw_frame(cfg)), ("T_RacketGrip_Masks", draw_grip(cfg))):
        paths.append(write_png(out_dir / f"{name}.png", strip.w, strip.h, 4, strip.pixel))

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
