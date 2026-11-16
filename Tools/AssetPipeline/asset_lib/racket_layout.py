"""Where every part of the racket goes (GDD 2: solid, perforated face, at most 455 mm long
and 260 mm wide, 38 mm thick; docs/decisions/0008-racket.md). Plain Python, so the
Blender builder (blender/build_racket.py), the textures (racket_textures.py) and the tests
(tests/test_racket.py) share one layout and the tests run without Blender.

All positions are (x, z) in metres in the racket's axes: X across the face, Z along the
racket (tip up), grip centre on the origin. Y is through the face and only the builder
uses it. Tuning in config/racket.json.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

from . import CONFIG_DIR

RACKET_CONFIG_PATH = CONFIG_DIR / "racket.json"
Point = tuple[float, float]

# FIP-style equipment limits (GDD 2 gives the length and width; thickness and hole sizes
# are the usual padel limits). The tests hold the layout to these.
MAX_LENGTH_M = 0.455
MAX_WIDTH_M = 0.26
MAX_THICKNESS_M = 0.038
HOLE_DIAMETER_RANGE_M = (0.009, 0.013)


def load_config(path: Path = RACKET_CONFIG_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


# --- Curves --------------------------------------------------------------------------------

def _catmull_rom(p0: Point, p1: Point, p2: Point, p3: Point, t: float) -> Point:
    t2, t3 = t * t, t * t * t
    return tuple(0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)
                 for a, b, c, d in zip(p0, p1, p2, p3))  # type: ignore[return-value]


def _resample(points: list[Point], count: int, closed: bool) -> list[Point]:
    """count points evenly spaced by arc length along a dense polyline."""
    pts = points + ([points[0]] if closed else [])
    lengths = [0.0]
    for a, b in zip(pts, pts[1:]):
        lengths.append(lengths[-1] + math.dist(a, b))
    total = lengths[-1]
    out = []
    j = 0
    steps = count if closed else count - 1
    for i in range(count):
        s = total * i / steps
        while j < len(lengths) - 2 and lengths[j + 1] < s:
            j += 1
        seg = lengths[j + 1] - lengths[j]
        f = (s - lengths[j]) / seg if seg > 0 else 0.0
        a, b = pts[j], pts[j + 1]
        out.append((a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f))
    return out


def signed_area(poly: list[Point]) -> float:
    return 0.5 * sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(poly, poly[1:] + poly[:1]))


def _ccw(poly: list[Point]) -> list[Point]:
    return poly if signed_area(poly) > 0 else list(reversed(poly))


def chaikin(poly: list[Point], passes: int) -> list[Point]:
    """Corner cutting: rounds a closed polygon's corners, keeping its shape."""
    for _ in range(passes):
        out = []
        for a, b in zip(poly, poly[1:] + poly[:1]):
            out.append((0.75 * a[0] + 0.25 * b[0], 0.75 * a[1] + 0.25 * b[1]))
            out.append((0.25 * a[0] + 0.75 * b[0], 0.25 * a[1] + 0.75 * b[1]))
        poly = out
    return poly


def _walk(poly: list[Point], start: int, step: int, distance: float) -> tuple[Point, int]:
    """The point `distance` along a closed polyline from vertex `start`, walking by `step`
    (+1 or -1), and the index of the last vertex passed."""
    n = len(poly)
    i, left = start, distance
    while True:
        j = (i + step) % n
        seg = math.dist(poly[i], poly[j])
        if seg >= left or j == start:
            f = left / seg if seg > 0 else 0.0
            return (poly[i][0] + (poly[j][0] - poly[i][0]) * f, poly[i][1] + (poly[j][1] - poly[i][1]) * f), i
        left -= seg
        i = j


def _turns(poly: list[Point]) -> list[float]:
    """How far the polygon turns at each vertex, in degrees."""
    out = []
    for i in range(len(poly)):
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % len(poly)]
        u = (b[0] - a[0], b[1] - a[1])
        v = (c[0] - b[0], c[1] - b[1])
        out.append(math.degrees(abs(math.atan2(u[0] * v[1] - u[1] * v[0], u[0] * v[0] + u[1] * v[1]))))
    return out


def fillet_corners(poly: list[Point], radius: float, min_turn_deg: float = 25.0) -> list[Point]:
    """Rounds every sharp corner of a closed polygon (one that turns by more than
    min_turn_deg) with a curve tangent to both sides, about `radius` round, sharpest
    first. Two corners close together share the room between them. Gentle bends (the
    sampled curves) are left alone."""
    for _ in range(64):
        n = len(poly)
        turns = _turns(poly)
        sharp = [i for i in range(n) if turns[i] > min_turn_deg]
        if not sharp:
            return poly
        i = max(sharp, key=lambda k: turns[k])

        def room(step: int) -> float:
            # Polyline length to the next sharp corner (or half way round).
            length, k = 0.0, i
            for _ in range(n // 2):
                j = (k + step) % n
                length += math.dist(poly[k], poly[j])
                k = j
                if turns[k] > min_turn_deg:
                    return 0.45 * length
            return length

        interior = math.radians(180.0 - turns[i])
        d = min(radius / math.tan(max(interior, 1e-3) / 2), room(-1), room(1))
        start, last_in = _walk(poly, i, -1, d)
        end, last_out = _walk(poly, i, 1, d)
        corner = poly[i]
        arc = [((1 - t) ** 2 * start[0] + 2 * (1 - t) * t * corner[0] + t * t * end[0],
                (1 - t) ** 2 * start[1] + 2 * (1 - t) * t * corner[1] + t * t * end[1])
               for t in (k / 12 for k in range(13))]
        # The curve runs start -> end; keep the vertices beyond both ends.
        kept, k = [], (last_out + 1) % n
        while k != last_in:
            kept.append(poly[k])
            k = (k + 1) % n
        poly = _dedupe(arc + kept)
    return poly


def offset(poly: list[Point], inward: float) -> list[Point]:
    """Moves each vertex of a closed CCW polygon along its averaged normal: inward > 0
    shrinks it, < 0 grows it. Good for the smooth outlines here, not for sharp corners."""
    poly = _ccw(poly)
    n = len(poly)
    out = []
    for i in range(n):
        a, b, c = poly[i - 1], poly[i], poly[(i + 1) % n]
        normals = []
        for p, q in ((a, b), (b, c)):
            dx, dz = q[0] - p[0], q[1] - p[1]
            length = math.hypot(dx, dz) or 1.0
            normals.append((-dz / length, dx / length))  # left of the edge = inside for CCW
        nx, nz = normals[0][0] + normals[1][0], normals[0][1] + normals[1][1]
        length = math.hypot(nx, nz) or 1.0
        nx, nz = nx / length, nz / length
        # Keep the edges (not the vertex) at the offset distance.
        cos_half = max(0.5, nx * normals[0][0] + nz * normals[0][1])
        d = inward / cos_half
        out.append((b[0] + nx * d, b[1] + nz * d))
    return out


def point_in_polygon(p: Point, poly: list[Point]) -> bool:
    x, z = p
    inside = False
    for a, b in zip(poly, poly[1:] + poly[:1]):
        if (a[1] > z) != (b[1] > z):
            if x < a[0] + (z - a[1]) * (b[0] - a[0]) / (b[1] - a[1]):
                inside = not inside
    return inside


def distance_to_polyline(p: Point, poly: list[Point], closed: bool = True) -> float:
    pts = poly + ([poly[0]] if closed else [])
    return min(_segment_distance(p, a, b) for a, b in zip(pts, pts[1:]))


def _segment_distance(p: Point, a: Point, b: Point) -> float:
    dx, dz = b[0] - a[0], b[1] - a[1]
    length2 = dx * dx + dz * dz
    t = 0.0 if length2 == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / length2))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dz)


# --- The frame -----------------------------------------------------------------------------

def outline_half(cfg: dict, samples_per_span: int = 24) -> list[Point]:
    """The right half of the finished edge, tip to neck, dense and smooth."""
    pts = [tuple(p) for p in cfg["frame"]["outline_half"]]
    # Phantom points: the mirror of the second point keeps the tip's tangent level; the
    # neck extends straight down.
    first = (-pts[1][0], pts[1][1])
    last = (2 * pts[-1][0] - pts[-2][0], 2 * pts[-1][1] - pts[-2][1])
    ext = [first] + pts + [last]
    out = []
    for i in range(1, len(ext) - 2):
        for s in range(samples_per_span):
            out.append(_catmull_rom(ext[i - 1], ext[i], ext[i + 1], ext[i + 2], s / samples_per_span))
    out.append(pts[-1])
    # The spline can bulge a fraction of a millimetre past its points: scale it back so
    # the widest point is exactly the widest configured point (the regulation width).
    widest_x = max(p[0] for p in pts)
    scale = widest_x / max(p[0] for p in out)
    return [(x * scale, z) for x, z in out]


def outline(cfg: dict, count: int) -> list[Point]:
    """The whole finished silhouette (head, throat and neck), closed, CCW, count points.
    The neck is closed by a straight edge at neck_z_m, inside the grip."""
    right = outline_half(cfg)
    left = [(-x, z) for x, z in reversed(right[1:])]
    neck = right[-1]
    bottom = [(neck[0] * (1 - 2 * i / 8), neck[1]) for i in range(1, 8)]
    loop = right + bottom + left[:-1]
    # Put the tip's point exactly on the centre line and resample evenly.
    return _ccw(_resample(loop, count, closed=True))


def half_width_at(cfg: dict, z: float) -> float:
    """x of the finished right edge at height z (0 outside the silhouette)."""
    right = outline_half(cfg)
    for a, b in zip(right, right[1:]):
        if (a[1] - z) * (b[1] - z) <= 0 and a[1] != b[1]:
            f = (z - a[1]) / (b[1] - a[1])
            return a[0] + (b[0] - a[0]) * f
    return 0.0


def widest(cfg: dict) -> Point:
    return max(outline_half(cfg), key=lambda p: p[0])


def tip_z(cfg: dict) -> float:
    return cfg["frame"]["outline_half"][0][1]


def face_outline(cfg: dict, count: int = 240) -> list[Point]:
    """Where the hitting face starts: the edge inset by the frame's visible width."""
    return offset(outline(cfg, count), cfg["frame"]["face_margin_m"])


# --- The open heart --------------------------------------------------------------------------

def _bisect(f, lo: float, hi: float, steps: int = 60) -> float:
    flo = f(lo)
    for _ in range(steps):
        mid = 0.5 * (lo + hi)
        if (f(mid) > 0) == (flo > 0):
            lo, flo = mid, f(mid)
        else:
            hi = mid
    return 0.5 * (lo + hi)


def heart_windows(cfg: dict) -> list[list[Point]]:
    """The two windows of the open heart, finished size, each a closed CCW polygon:
    bounded by the bridge, the inner edge of the throat arm and the bottom of the face."""
    h = cfg["heart"]
    b = h["bridge_width_m"] / 2
    arm = h["arm_width_m"]

    def inner(z: float) -> float:
        return half_width_at(cfg, z) - arm

    def top(x: float) -> float:
        return h["top_z_m"] + h["top_rise"] * x

    # Bottom: where the arm's inner edge meets the bridge, or the configured floor.
    z_meet = _bisect(lambda z: inner(z) - b, cfg["frame"]["neck_z_m"], h["top_z_m"])
    zb = max(z_meet, h["bottom_z_m"])
    # Top corner: where the face's bottom edge meets the arm.
    x_top = _bisect(lambda x: inner(top(x)) - x, b, 0.2)
    z_top = top(x_top)
    right = [(b, zb), (b, top(b))]
    right += [(b + (x_top - b) * i / 10, top(b + (x_top - b) * i / 10)) for i in range(1, 11)]
    right += [(inner(z_top + (zb - z_top) * i / 16), z_top + (zb - z_top) * i / 16) for i in range(1, 17)]
    if inner(zb) - b > 1e-4:
        right.append((b, zb))
    window = _ccw(fillet_corners(_dedupe(right), h["corner_radius_m"]))
    return [window, _ccw([(-x, z) for x, z in window])]


def _dedupe(poly: list[Point]) -> list[Point]:
    out = []
    for p in poly:
        if not out or math.dist(out[-1], p) > 1e-5:
            out.append(p)
    if len(out) > 1 and math.dist(out[0], out[-1]) < 1e-5:
        out.pop()
    return out


# --- The holes -------------------------------------------------------------------------------

def hole_centres(cfg: dict) -> list[Point]:
    """Hole centres, left-right symmetric. Rings follow the face outline (outermost first),
    alternate rings staggered by half a pitch, then the centre column; a hole is dropped
    if it would sit too close to one already placed, leave the face, or cut into the
    wordmark panel."""
    holes = cfg["holes"]
    r = holes["diameter_m"] / 2
    gap = holes["min_gap_m"]
    clear_lo, clear_hi = holes["keep_clear_z_m"]
    face = face_outline(cfg)
    placed: list[Point] = []

    def fits(p: Point) -> bool:
        if p[1] - r < max(clear_hi, panel_edge(cfg, p[0]) + holes["graphic_clear_m"]):
            return False
        if not point_in_polygon(p, face) or distance_to_polyline(p, face) < r + 0.002:
            return False
        return all(math.dist(p, q) >= 2 * r + gap for q in placed)

    def try_place(x: float, z: float) -> None:
        # Both sides or neither, so the pattern stays symmetric.
        pair = [(0.0, z)] if abs(x) < 1e-3 else [(abs(x), z), (-abs(x), z)]
        if all(fits(p) for p in pair) and (len(pair) == 1 or math.dist(*pair) >= 2 * r + gap):
            placed.extend(pair)

    # The outline rings first: they give the pattern its diamond shape.
    for k, inset in enumerate(holes["ring_insets_m"]):
        ring = offset(outline(cfg, 240), cfg["frame"]["face_margin_m"] + inset + r)
        dense = _resample(ring, 2000, closed=True)
        n = len(dense)
        # Walk the right half from the top of the ring (the tip side) until it crosses
        # the centre line again at the bottom.
        i = max(range(n), key=lambda j: dense[j][1])
        step = 1 if dense[(i + 1) % n][0] > dense[i][0] else -1
        walked, next_s = 0.0, holes["pitch_m"] / 2 if k % 2 else 0.0
        for _ in range(n):
            x, z = dense[i]
            if walked > 0.01 and x < 0:
                break
            if walked >= next_s:
                try_place(x, z)
                next_s += holes["pitch_m"]
            j = (i + step) % n
            walked += math.dist(dense[i], dense[j])
            i = j
    # Then a staggered grid fills the middle, from the centre column outwards.
    pitch = holes["grid_pitch_m"]
    row = 0
    z = holes["grid_top_z_m"]
    while z > clear_hi:
        shift = pitch / 2 if row % 2 else 0.0
        for c in range(0, 12):
            try_place(shift + c * pitch, z)
        z -= pitch * math.sqrt(3) / 2
        row += 1
    return placed


# --- The graphic (racket_textures.py draws it; the holes keep clear of its lines) -------------

def panel_edge(cfg: dict, x: float) -> float:
    """Height of the top of the paint panel at x: a straight line rising left to right
    across the head, a little above the accent line drawn along it."""
    panel = cfg["graphics"]["panel"]
    half_w = widest(cfg)[0] + 0.01
    zl, zr = panel["left_top_z_m"], panel["right_top_z_m"]
    return zl + (zr - zl) * (x + half_w) / (2 * half_w) + 0.0035


def keystone(cfg: dict) -> list[Point]:
    """The keystone round the sweet spot, a closed trapezoid, wide edge at the top."""
    k = cfg["graphics"]["keystone"]
    return [(-k["top_half_m"], k["top_z_m"]), (k["top_half_m"], k["top_z_m"]),
            (k["bottom_half_m"], k["bottom_z_m"]), (-k["bottom_half_m"], k["bottom_z_m"])]


# --- Texture space ---------------------------------------------------------------------------

def face_uv(cfg: dict, x: float, z: float) -> Point:
    """UV of a face point for T_Racket_Masks, in Unreal's convention (V down from the
    tip). The -Y face reads it as is; the +Y face mirrors U so the wordmark reads
    correctly from both sides (the builder does that)."""
    size = cfg["textures"]["size_m"]
    return ((x + size / 2) / size, (tip_z(cfg) - z) / size)


def pixel_to_face(cfg: dict, px: float, py: float) -> Point:
    """Face point at the centre of texture pixel (px, py); row 0 is the tip."""
    size = cfg["textures"]["size_m"]
    n = cfg["textures"]["masks_px"]
    return (-size / 2 + (px + 0.5) / n * size, tip_z(cfg) - (py + 0.5) / n * size)
