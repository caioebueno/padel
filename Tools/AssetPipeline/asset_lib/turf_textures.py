"""Procedural textures for the playing surface (GDD 10: blue artificial grass with
visible sand, white lines). Tuning comes from config/court_surface.json.

  T_Turf_N              tangent-space normal map of one tile of tufted fibres (DirectX green, like Unreal)
  T_Turf_Masks          same tile, linear: R = fibre cover (0 = sand shows), G = cavity AO, B = per-blade tint
  T_CourtSurface_Masks  the whole 20 x 10 playing area, linear: R = wear, G = loose sand, B = turf roll tint

Pure Python and seeded, like textures.py, so a build is the same on every machine
and needs nothing beyond the standard library.
"""

from __future__ import annotations

import json
import math
import random
from pathlib import Path
from typing import Sequence

from . import CONFIG_DIR
from .textures import write_png

SURFACE_CONFIG_PATH = CONFIG_DIR / "court_surface.json"


def load_config(path: Path = SURFACE_CONFIG_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


# --- Noise ------------------------------------------------------------------------

class TileNoise:
    """Value noise on a lattice that wraps, so anything built from it tiles."""

    def __init__(self, rng: random.Random, cells_x: int, cells_y: int) -> None:
        self.cx, self.cy = cells_x, cells_y
        self.values = [rng.random() for _ in range(cells_x * cells_y)]

    def at(self, u: float, v: float) -> float:
        """u, v in tiles (0..1 covers the lattice once). Returns 0..1."""
        x, y = u * self.cx, v * self.cy
        x0, y0 = math.floor(x), math.floor(y)
        fx, fy = x - x0, y - y0
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        x0 %= self.cx
        y0 %= self.cy
        x1, y1 = (x0 + 1) % self.cx, (y0 + 1) % self.cy
        v00 = self.values[y0 * self.cx + x0]
        v10 = self.values[y0 * self.cx + x1]
        v01 = self.values[y1 * self.cx + x0]
        v11 = self.values[y1 * self.cx + x1]
        top = v00 + (v10 - v00) * fx
        return top + (v01 + (v11 - v01) * fx - top) * fy


def fbm(rng: random.Random, cells_x: int, cells_y: int, octaves: int) -> list[TileNoise]:
    return [TileNoise(rng, cells_x << i, cells_y << i) for i in range(octaves)]


def fbm_at(layers: Sequence[TileNoise], u: float, v: float) -> float:
    total, amp, norm = 0.0, 1.0, 0.0
    for layer in layers:
        total += layer.at(u, v) * amp
        norm += amp
        amp *= 0.5
    return total / norm


def _clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v


def _byte(v: float) -> int:
    return int(round(255 * _clamp01(v)))


# --- Fibre tile -------------------------------------------------------------------

def fibre_fields(cfg: dict, seed: int) -> dict[str, list[float]]:
    """Height (mm), fibre cover and blade tint for one tile, row-major, size x size."""
    f = cfg["fibre"]
    size = int(f["texture_px"])
    rows, stitches = int(f["rows_per_tile"]), int(f["stitches_per_row"])
    tile_mm = rows * f["gauge_mm"]
    px_per_mm = size / tile_mm
    pile, sand = f["pile_height_mm"], f["sand_level_mm"]
    rng = random.Random(seed)

    # Sand: fine grain plus a few millimetres of heap and hollow between tufts.
    grain = fbm(rng, max(1, int(tile_mm / f["sand_grain_mm"] / 4)), max(1, int(tile_mm / f["sand_grain_mm"] / 4)), 3)
    heap = fbm(rng, 6, 6, 2)
    height = [0.0] * (size * size)
    cover = [0.0] * (size * size)
    tint = [0.5] * (size * size)
    for y in range(size):
        v = (y + 0.5) / size
        for x in range(size):
            u = (x + 0.5) / size
            height[y * size + x] = sand + 0.8 * (fbm_at(grain, u, v) - 0.5) + 1.4 * (fbm_at(heap, u, v) - 0.5)

    # Blades: from each tuft root (buried at sand level) to a tip at pile height,
    # leaning outward. Seen from above, the height rises along the blade.
    half_w = max(0.5, f["blade_width_mm"] * px_per_mm / 2)
    lo_n, hi_n = f["blades_per_tuft"]
    lo_r, hi_r = f["blade_reach_mm"]
    lean = rng.uniform(0, 2 * math.pi)  # brushing direction of this turf
    for r in range(rows):
        for s in range(stitches):
            # Alternate rows are offset half a stitch, like a real tufting machine.
            cx = (s + 0.5 * (r % 2) + rng.uniform(-0.2, 0.2)) * size / stitches
            cy = (r + 0.5 + rng.uniform(-0.15, 0.15)) * size / rows
            for _ in range(rng.randint(lo_n, hi_n)):
                ang = lean + rng.gauss(0, 1.1)
                reach = rng.uniform(lo_r, hi_r) * px_per_mm
                tip = pile * rng.uniform(0.85, 1.05)
                shade = rng.random()
                steps = max(2, int(reach * 1.5))
                dx, dy = math.cos(ang) * reach, math.sin(ang) * reach
                for k in range(steps + 1):
                    t = k / steps
                    h = sand - 0.5 + (tip - sand + 0.5) * math.sqrt(t)
                    px, py = cx + dx * t, cy + dy * t
                    w = half_w * (1.0 - 0.45 * t)  # blades taper toward the tip
                    for oy in range(int(math.floor(py - w)), int(math.ceil(py + w)) + 1):
                        for ox in range(int(math.floor(px - w)), int(math.ceil(px + w)) + 1):
                            d = math.hypot(ox + 0.5 - px, oy + 0.5 - py)
                            if d > w:
                                continue
                            # Rounded blade cross-section: highest along its middle.
                            hh = h - 0.6 * (d / w) ** 2
                            i = (oy % size) * size + (ox % size)
                            if hh > height[i]:
                                height[i] = hh
                                cover[i] = 1.0 if hh > sand else 0.5
                                tint[i] = shade
    return {"size": size, "px_per_mm": px_per_mm, "height": height, "cover": cover, "tint": tint}


def write_fibre_textures(cfg: dict, out_dir: Path, seed: int) -> list[Path]:
    fields = fibre_fields(cfg, seed)
    size, h = fields["size"], fields["height"]
    # Slope in mm of height per mm across, times a strength: the same look at any resolution.
    gain = cfg["fibre"]["normal_slope_scale"] * fields["px_per_mm"]
    pile, sand = cfg["fibre"]["pile_height_mm"], cfg["fibre"]["sand_level_mm"]

    def hat(x: int, y: int) -> float:
        return h[(y % size) * size + (x % size)]

    def normal(x: int, y: int) -> tuple[int, int, int]:
        # Central differences, wrapped. y grows down the image; Unreal reads
        # green as DirectX (+Y down), so G = -dh/dy_image.
        ddx = (hat(x + 1, y) - hat(x - 1, y)) * 0.5 * gain
        ddy = (hat(x, y + 1) - hat(x, y - 1)) * 0.5 * gain
        nx, ny, nz = -ddx, -ddy, 1.0
        n = math.sqrt(nx * nx + ny * ny + nz * nz)
        return (_byte(nx / n * 0.5 + 0.5), _byte(ny / n * 0.5 + 0.5), _byte(nz / n * 0.5 + 0.5))

    def masks(x: int, y: int) -> tuple[int, int, int]:
        i = y * size + x
        # Cavity: deep sand between tall blades is darker; tips are fully lit.
        ao = 0.6 + 0.4 * _clamp01((h[i] - (sand - 2.0)) / (pile - sand + 2.0))
        return (_byte(fields["cover"][i]), _byte(ao), _byte(fields["tint"][i]))

    return [write_png(out_dir / "T_Turf_N.png", size, size, 3, normal),
            write_png(out_dir / "T_Turf_Masks.png", size, size, 3, masks)]


# --- Whole-court masks -----------------------------------------------------------------

def _zones(cfg: dict, half_length: float) -> list[tuple[float, float, float, float, float]]:
    """Every wear zone in court coordinates (x, y, rx, ry, strength), mirrored."""
    out = []
    for z in cfg["court_masks"]["wear_zones"]:
        for sx in (-1, 1):
            for sy in ((-1, 1) if z.get("mirror_y") else (1,)):
                x = sx * min(z["d"], half_length)
                out.append((x, sy * z["y"], z["rd"], z["ry"], z["strength"]))
    return out


def court_fields(cfg: dict, spec: dict, seed: int) -> dict:
    """Wear, drift and roll tint over the playing area. Pixel (0, 0) is the corner
    at -X, +Y (image rows run from +Y to -Y, so Blender's V = (y + HalfWidth) / Width)."""
    m = cfg["court_masks"]
    w, hgt = (int(v) for v in m["texture_px"])
    length, width = spec["Length"], spec["Width"]
    hl, hw = spec["HalfLength"], spec["HalfWidth"]
    rng = random.Random(seed + 1)
    wear_noise = fbm(rng, 20, 10, 4)
    drift_noise = fbm(rng, 40, 20, 3)
    wear = [0.0] * (w * hgt)
    drift = [0.0] * (w * hgt)
    roll = [0.0] * (w * hgt)
    mpp = length / w  # metres per pixel (square pixels: width / hgt matches)

    def px_to_court(px: float, py: float) -> tuple[float, float]:
        return -hl + (px + 0.5) * mpp, hw - (py + 0.5) * mpp

    def stamp(xc: float, yc: float, rx: float, ry: float, ang: float, strength: float, falloff: float) -> None:
        reach = max(rx, ry) * 2.2
        ca, sa = math.cos(ang), math.sin(ang)
        x0 = max(0, int((xc - reach + hl) / mpp))
        x1 = min(w - 1, int((xc + reach + hl) / mpp))
        y0 = max(0, int((hw - (yc + reach)) / mpp))
        y1 = min(hgt - 1, int((hw - (yc - reach)) / mpp))
        for py in range(y0, y1 + 1):
            for px in range(x0, x1 + 1):
                cx, cy = px_to_court(px, py)
                lx, ly = cx - xc, cy - yc
                a, b = (lx * ca + ly * sa) / rx, (-lx * sa + ly * ca) / ry
                d2 = a * a + b * b
                if d2 < 4.0:
                    i = py * w + px
                    wear[i] += strength * math.exp(-d2 * falloff)

    zones = _zones(cfg, hl)
    for xc, yc, rx, ry, strength in zones:
        stamp(xc, yc, rx, ry, 0.0, strength, 1.0)
    sc = m["scuffs"]
    for xc, yc, rx, ry, _ in zones:
        for _ in range(int(sc["per_zone"])):
            x = xc + rng.gauss(0, rx * 0.6)
            y = yc + rng.gauss(0, ry * 0.6)
            if abs(x) > hl - 0.05 or abs(y) > hw - 0.05:
                continue
            sideways = rng.random() < sc["sideways_bias"]
            ang = (math.pi / 2 if sideways else 0.0) + rng.gauss(0, 0.5)
            stamp(x, y, rng.uniform(*sc["length"]) / 2, rng.uniform(*sc["width"]) / 2, ang,
                  rng.uniform(*sc["strength"]), 2.5)

    roll_w = m["roll_width"]
    roll_values = [rng.uniform(-1, 1) for _ in range(int(math.ceil(length / roll_w)) + 2)]
    for py in range(hgt):
        for px in range(w):
            x, y = px_to_court(px, py)
            i = py * w + px
            u, v = (x + hl) / length, (y + hw) / width
            n = fbm_at(wear_noise, u, v)
            wear[i] = _clamp01(wear[i] * (1.0 - m["wear_noise"] + 2 * m["wear_noise"] * n))
            # Sand drifts against the walls and into the corners, and gathers
            # under the net where nobody stands (fibres stay upright there too).
            to_side, to_back = hw - abs(y), hl - abs(x)
            dw = max(math.exp(-to_side / m["drift_wall_width"]), math.exp(-to_back / m["drift_wall_width"]))
            corner = math.hypot(max(0.0, m["drift_corner_radius"] - to_side),
                                max(0.0, m["drift_corner_radius"] - to_back)) / m["drift_corner_radius"]
            dn = math.exp(-abs(x) / m["drift_net_width"]) * 0.5
            dn_noise = fbm_at(drift_noise, u, v)
            drift[i] = _clamp01(max(dw, corner, dn) * (1.0 - m["drift_noise"] + 2 * m["drift_noise"] * dn_noise))
            # Rolls are laid across the court; each roll's fibres lean a slightly
            # different way, which reads as a faint band of tint from the camera.
            k = int((x + hl) // roll_w)
            # The material scales this by RollTint (palette.json).
            roll[i] = 0.5 + 0.5 * roll_values[k]
    return {"size": (w, hgt), "metres_per_px": mpp, "wear": wear, "drift": drift, "roll": roll}


def wear_at(fields: dict, spec: dict, x: float, y: float) -> float:
    """Wear value at a court position (metres), for tests and tools."""
    w, hgt = fields["size"]
    px = min(w - 1, max(0, int((x + spec["HalfLength"]) / fields["metres_per_px"])))
    py = min(hgt - 1, max(0, int((spec["HalfWidth"] - y) / fields["metres_per_px"])))
    return fields["wear"][py * w + px]


def write_court_masks(cfg: dict, spec: dict, out_dir: Path, seed: int) -> Path:
    fields = court_fields(cfg, spec, seed)
    w, hgt = fields["size"]

    def pixel(x: int, y: int) -> tuple[int, int, int]:
        i = y * w + x
        return (_byte(fields["wear"][i]), _byte(fields["drift"][i]), _byte(fields["roll"][i]))

    return write_png(out_dir / "T_CourtSurface_Masks.png", w, hgt, 3, pixel)


def generate_all(spec: dict, out_dir: Path, cfg: dict | None = None) -> list[Path]:
    cfg = cfg or load_config()
    seed = int(cfg["seed"])
    return write_fibre_textures(cfg, out_dir, seed) + [write_court_masks(cfg, spec, out_dir, seed)]
