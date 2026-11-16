"""Procedural textures for the padel ball (GDD 2, GDD 10). Tuning in config/ball.json.

  T_Ball_Masks  equirectangular over the ball's UV0, linear:
                R = angular distance from the seam's centre line / sdf_range (a distance field,
                    so the material can draw the seam at any width, and widen it to stay
                    readable when the ball is a few pixels across)
                G = mottle: slow dye and nap variation over the whole felt (0.5 = none)
                B = printed mark coverage (an original court-from-above mark, no text)
  T_Ball_N      the same mapping, tangent-space normal (DirectX green): the seam's rubber
                groove and its raised felt shoulders
  T_Felt_N      one tile of felt fibres, tiled by the material every felt.tile_mm
  T_Felt_Mask   the same tile's fibre height, linear (the fuzz shell's mask)

UV convention, shared with blender/build_equipment.py: U = longitude / 2 pi, measured from
+X towards +Y; V = 0.5 + latitude / pi, so image row 0 is +Z. Pure Python and seeded, like
textures.py, so a build is identical everywhere and needs only the standard library.
"""

from __future__ import annotations

import json
import math
import random
from pathlib import Path
from typing import Sequence

from . import CONFIG_DIR
from .textures import write_png

BALL_CONFIG_PATH = CONFIG_DIR / "ball.json"
Vec = tuple[float, float, float]


def load_config(path: Path = BALL_CONFIG_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v


def _byte(v: float) -> int:
    return int(round(255 * _clamp01(v)))


def _norm(p: Sequence[float]) -> Vec:
    n = math.sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2])
    return (p[0] / n, p[1] / n, p[2] / n)


def _dot(a: Sequence[float], b: Sequence[float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a: Sequence[float], b: Sequence[float]) -> Vec:
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


# --- Seam --------------------------------------------------------------------------

def seam_point(t: float, b: float) -> Vec:
    """Point of the seam's centre line on the unit sphere (config seam._doc)."""
    a = 1.0 - b
    c = 2.0 * math.sqrt(a * b)
    return _norm((a * math.cos(t) + b * math.cos(3 * t), a * math.sin(t) - b * math.sin(3 * t), c * math.sin(2 * t)))


def seam_curve(b: float, samples: int) -> list[Vec]:
    return [seam_point(2 * math.pi * i / samples, b) for i in range(samples)]


def uv_direction(u: float, v: float) -> Vec:
    """Unit direction for a UV (the convention in the module doc)."""
    lon, lat = 2 * math.pi * u, math.pi * (v - 0.5)
    return (math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat))


def pixel_direction(x: float, y: float, width: int, height: int) -> Vec:
    """Direction at the centre of pixel (x, y); row 0 is the top of the image (+Z)."""
    return uv_direction((x + 0.5) / width, 1.0 - (y + 0.5) / height)


class SeamField:
    """Angular distance (radians) from a direction to the seam's centre line.

    Brute force over the whole curve is too slow in pure Python at texture size, so a
    coarse grid stores the nearest curve parameter t, and each lookup refines t locally
    by golden-section search. That is exact where it matters (near the seam, where the
    nearest point is unique); far away the distance is clamped by the caller anyway.
    """

    def __init__(self, b: float, samples: int, coarse: tuple[int, int]) -> None:
        self.b = b
        self.samples = samples
        self.curve = seam_curve(b, samples)
        self.cw, self.ch = coarse
        self.coarse_t: list[float] = []
        for y in range(self.ch):
            for x in range(self.cw):
                d = pixel_direction(x, y, self.cw, self.ch)
                best = max(range(samples), key=lambda i: _dot(d, self.curve[i]))
                self.coarse_t.append(2 * math.pi * best / samples)

    def _refine(self, d: Vec, t0: float, span: float) -> float:
        lo, hi = t0 - span, t0 + span
        g = (math.sqrt(5) - 1) / 2
        c1, c2 = hi - g * (hi - lo), lo + g * (hi - lo)
        f1, f2 = _dot(d, seam_point(c1, self.b)), _dot(d, seam_point(c2, self.b))
        for _ in range(13):
            if f1 > f2:
                hi, c2, f2 = c2, c1, f1
                c1 = hi - g * (hi - lo)
                f1 = _dot(d, seam_point(c1, self.b))
            else:
                lo, c1, f1 = c1, c2, f2
                c2 = lo + g * (hi - lo)
                f2 = _dot(d, seam_point(c2, self.b))
        return max(f1, f2)

    def distance(self, d: Vec, u: float, v: float, far: float = math.pi) -> float:
        """Angular distance, exact up to `far`; anything farther may come back as `far`.
        The nearest t only jumps between coarse cells on the medial axis between two
        arms of the seam, which is further from the seam (0.41 rad at the necks) than
        the distances the texture stores, so one cell's t is enough."""
        cx = min(self.cw - 1, max(0, int(u * self.cw)))
        cy = min(self.ch - 1, max(0, int((1.0 - v) * self.ch)))
        t0 = self.coarse_t[cy * self.cw + cx]
        rough = math.acos(max(-1.0, min(1.0, _dot(d, seam_point(t0, self.b)))))
        # The coarse point is within a cell (and half a curve sample) of the true nearest one.
        if rough - 2.5 * math.pi / self.ch > far:
            return far
        best = self._refine(d, t0, 2 * math.pi / self.samples * 24)
        return min(far, math.acos(max(-1.0, min(1.0, best))))


# --- Printed mark -------------------------------------------------------------------

def _round_rect_sdf(px: float, py: float, hx: float, hy: float, r: float) -> float:
    qx, qy = abs(px) - hx + r, abs(py) - hy + r
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    return outside + min(max(qx, qy), 0.0) - r


def mark_coverage(xm: float, ym: float, mark: dict, pixel_mm: float) -> float:
    """Ink coverage of the mark at (xm, ym) mm from its centre on the tangent plane.
    A court seen from above: rounded outline, the net across the middle, a ball dot."""
    hx, hy = mark["length_mm"] / 2, mark["width_mm"] / 2
    stroke = mark["stroke_mm"] / 2
    outline = abs(_round_rect_sdf(xm, ym, hx, hy, mark["corner_mm"])) - stroke
    net = max(abs(xm) - stroke * 0.8, abs(ym) - hy)
    dot = math.hypot(xm - hx * 0.45, ym + hy * 0.3) - mark["dot_mm"]
    sdf = min(outline, net, dot)
    return _clamp01(0.5 - sdf / max(pixel_mm, 1e-6))


# --- Mottle (3D value noise, so it is seamless on the sphere) ------------------------------

class SphereNoise:
    def __init__(self, rng: random.Random, cells: int, octaves: int) -> None:
        self.layers = []
        for o in range(octaves):
            n = cells << o
            self.layers.append((n, [rng.random() for _ in range((n + 1) ** 3)]))

    def at(self, d: Vec) -> float:
        total, amp, norm = 0.0, 1.0, 0.0
        for n, values in self.layers:
            x, y, z = ((c + 1.0) * 0.5 * (n - 1e-9) for c in d)
            x0, y0, z0 = int(x), int(y), int(z)
            fx, fy, fz = x - x0, y - y0, z - z0
            fx, fy, fz = (f * f * (3 - 2 * f) for f in (fx, fy, fz))
            s = n + 1

            def val(i: int, j: int, k: int) -> float:
                return values[(k * s + j) * s + i]
            c00 = val(x0, y0, z0) + (val(x0 + 1, y0, z0) - val(x0, y0, z0)) * fx
            c10 = val(x0, y0 + 1, z0) + (val(x0 + 1, y0 + 1, z0) - val(x0, y0 + 1, z0)) * fx
            c01 = val(x0, y0, z0 + 1) + (val(x0 + 1, y0, z0 + 1) - val(x0, y0, z0 + 1)) * fx
            c11 = val(x0, y0 + 1, z0 + 1) + (val(x0 + 1, y0 + 1, z0 + 1) - val(x0, y0 + 1, z0 + 1)) * fx
            c0 = c00 + (c10 - c00) * fy
            c1 = c01 + (c11 - c01) * fy
            total += (c0 + (c1 - c0) * fz) * amp
            norm += amp
            amp *= 0.5
        return total / norm


# --- Ball masks and seam normal ------------------------------------------------------

def groove_height(dist_mm: float, seam: dict) -> float:
    """Surface height in mm across the seam: the rubber strip sits below the felt, and
    the felt rises in a soft shoulder where it is glued down to it."""
    half, depth, shoulder = seam["half_width_mm"], seam["groove_depth_mm"], seam["shoulder_mm"]
    if dist_mm < half:
        # Slightly domed rubber at the bottom of the groove.
        k = dist_mm / half
        return -depth + 0.08 * depth * (1 - k * k)
    k = min(1.0, (dist_mm - half) / shoulder)
    # Rises from the groove to the felt with an ease-out: a felt edge, not a step.
    return -depth * (1 - k) ** 2


def ball_fields(cfg: dict, radius_mm: float) -> dict[str, list[float]]:
    """Distance (mm), mottle and ink over the equirectangular grid, row-major."""
    width, height = cfg["textures"]["masks_px"]
    cw, ch = cfg["textures"]["coarse_px"]
    seam = cfg["seam"]
    field = SeamField(seam["b"], seam["curve_samples"], (cw, ch))
    rng = random.Random(cfg["seed"])
    mottle = SphereNoise(rng, cfg["felt"]["mottle_cells"], cfg["felt"]["mottle_octaves"])
    mark = cfg["mark"]
    centres = []
    if mark.get("enabled", True):
        for c in mark["centres"]:
            n = _norm(c)
            # Tangent frame: e1 along the equator, e2 towards +Z, so the mark sits level.
            e1 = _norm(_cross((0.0, 0.0, 1.0), n))
            e2 = _cross(n, e1)
            centres.append((n, e1, e2))
    reach = math.hypot(mark["length_mm"], mark["width_mm"]) / radius_mm
    range_rad = cfg["textures"]["sdf_range_rad"]

    dist, mot, ink = [], [], []
    for y in range(height):
        v = 1.0 - (y + 0.5) / height
        lat = math.pi * (v - 0.5)
        pixel_mm = max(2 * math.pi * radius_mm * math.cos(lat) / width, math.pi * radius_mm / height)
        for x in range(width):
            u = (x + 0.5) / width
            d = uv_direction(u, v)
            dist.append(field.distance(d, u, v, range_rad) * radius_mm)
            mot.append(mottle.at(d))
            cover = 0.0
            for n, e1, e2 in centres:
                if _dot(d, n) > math.cos(reach):
                    # Gnomonic projection onto the tangent plane, in mm.
                    k = radius_mm / _dot(d, n)
                    cover = max(cover, mark_coverage(_dot(d, e1) * k, _dot(d, e2) * k, mark, pixel_mm))
            ink.append(cover)
    # Mottle: stretch to use the whole range around 0.5.
    lo, hi = min(mot), max(mot)
    mot = [(m - lo) / (hi - lo) if hi > lo else 0.5 for m in mot]
    return {"width": width, "height": height, "distance_mm": dist, "mottle": mot, "ink": ink}


def write_ball_textures(cfg: dict, radius_mm: float, out_dir: Path) -> list[Path]:
    f = ball_fields(cfg, radius_mm)
    width, height = f["width"], f["height"]
    far_mm = cfg["textures"]["sdf_range_rad"] * radius_mm
    dist, mot, ink = f["distance_mm"], f["mottle"], f["ink"]
    seam = cfg["seam"]
    h = [groove_height(dm, seam) for dm in dist]

    def masks(x: int, y: int) -> tuple[int, int, int]:
        i = y * width + x
        return (_byte(dist[i] / far_mm), _byte(mot[i]), _byte(ink[i]))

    def at(x: int, y: int) -> float:
        # Wraps in longitude; clamps at the poles.
        return h[min(height - 1, max(0, y)) * width + x % width]

    def normal(x: int, y: int) -> tuple[int, int, int]:
        lat = math.pi * (0.5 - (y + 0.5) / height)
        # Millimetres per pixel along U (shrinks towards the poles) and along V.
        du_mm = max(2 * math.pi * radius_mm * math.cos(lat) / width, 1e-3)
        dv_mm = math.pi * radius_mm / height
        ddx = (at(x + 1, y) - at(x - 1, y)) / (2 * du_mm)
        ddy = (at(x, y + 1) - at(x, y - 1)) / (2 * dv_mm)
        # y grows down the image; Unreal reads green as DirectX (+Y down): G = -dh/dy_image.
        nx, ny, nz = -ddx, -ddy, 1.0
        n = math.sqrt(nx * nx + ny * ny + nz * nz)
        return (_byte(nx / n * 0.5 + 0.5), _byte(ny / n * 0.5 + 0.5), _byte(nz / n * 0.5 + 0.5))

    return [write_png(out_dir / "T_Ball_Masks.png", width, height, 3, masks),
            write_png(out_dir / "T_Ball_N.png", width, height, 3, normal)]


# --- Felt fibre tile --------------------------------------------------------------------

def felt_height(cfg: dict) -> tuple[int, list[float]]:
    """One tiling tile of felt: short, curly fibres lying every which way over a matted
    base. Height 0..1, row-major."""
    felt = cfg["felt"]
    size = int(felt["fibre_px"])
    rng = random.Random(cfg["seed"] + 1)
    h = [0.0] * (size * size)
    # Matted base: fine wrapped value noise.
    cells = 32
    lattice = [rng.random() for _ in range(cells * cells)]
    for y in range(size):
        for x in range(size):
            gx, gy = x / size * cells, y / size * cells
            x0, y0 = int(gx), int(gy)
            fx, fy = gx - x0, gy - y0
            fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
            a = lattice[(y0 % cells) * cells + x0 % cells]
            b = lattice[(y0 % cells) * cells + (x0 + 1) % cells]
            c = lattice[((y0 + 1) % cells) * cells + x0 % cells]
            d = lattice[((y0 + 1) % cells) * cells + (x0 + 1) % cells]
            top = a + (b - a) * fx
            h[y * size + x] = (top + (c + (d - c) * fx - top) * fy) * felt["base_noise"]
    lo_len, hi_len = felt["fibre_length_px"]
    width = felt["fibre_width_px"]
    reach = int(math.ceil(width * 2))
    for _ in range(int(felt["fibres"])):
        x, y = rng.uniform(0, size), rng.uniform(0, size)
        angle = rng.uniform(0, 2 * math.pi)
        curl = rng.uniform(-0.25, 0.25)
        length = rng.uniform(lo_len, hi_len)
        top = felt["fibre_height"] * rng.uniform(0.55, 1.0)
        steps = int(length * 2)
        for s in range(steps):
            # Each fibre arches: highest in the middle, tucked into the mat at both ends.
            arch = math.sin(math.pi * (s + 0.5) / steps)
            angle += curl / 2
            x += math.cos(angle) * 0.5
            y += math.sin(angle) * 0.5
            for oy in range(-reach, reach + 1):
                for ox in range(-reach, reach + 1):
                    px, py = int(x) + ox, int(y) + oy
                    dd = math.hypot(px + 0.5 - x, py + 0.5 - y) / width
                    if dd >= 1.0:
                        continue
                    value = top * arch * math.sqrt(1 - dd * dd)
                    i = (py % size) * size + px % size
                    if value > h[i]:
                        h[i] = value
    peak = max(h)
    return size, [v / peak for v in h]


def write_felt_textures(cfg: dict, out_dir: Path) -> list[Path]:
    size, h = felt_height(cfg)
    # Fibres are ~0.15 mm thick and lie in a ~0.8 mm deep nap; this gain gives a
    # normal map that reads as fuzz without looking like grooves.
    gain = 2.2

    def at(x: int, y: int) -> float:
        return h[(y % size) * size + x % size]

    def normal(x: int, y: int) -> tuple[int, int, int]:
        ddx = (at(x + 1, y) - at(x - 1, y)) * 0.5 * gain
        ddy = (at(x, y + 1) - at(x, y - 1)) * 0.5 * gain
        nx, ny, nz = -ddx, -ddy, 1.0
        n = math.sqrt(nx * nx + ny * ny + nz * nz)
        return (_byte(nx / n * 0.5 + 0.5), _byte(ny / n * 0.5 + 0.5), _byte(nz / n * 0.5 + 0.5))

    return [write_png(out_dir / "T_Felt_N.png", size, size, 3, normal),
            write_png(out_dir / "T_Felt_Mask.png", size, size, 1, lambda x, y: (_byte(at(x, y)),))]


def generate_all(radius_mm: float, out_dir: Path, cfg: dict | None = None) -> list[Path]:
    cfg = cfg or load_config()
    return [*write_ball_textures(cfg, radius_mm, out_dir), *write_felt_textures(cfg, out_dir)]
