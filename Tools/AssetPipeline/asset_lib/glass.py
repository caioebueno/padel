"""The court's tempered glass: panel layout, bolt positions and the procedural
dirt textures (GDD 2 sizes, GDD 10 look). See docs/decisions/0006-court-glass.md.

Pure Python, standard library only, so it runs in the pipeline, in the tests and
inside Blender (blender/build_court_glass.py imports it), and the layout is
tested without Blender. Panel sizes come from CourtSpec.h through court_spec;
config/glass.json holds the build details that are not regulation numbers.

A panel is described by its inner (court-side) face: `origin` is its bottom
corner, `u` the unit vector along its width, `normal` the unit vector pointing
out of the court. The slab runs from the inner face outward by the thickness,
so the inner faces sit exactly on the playing-area limits like the colliders
in CourtLayout.h.
"""

from __future__ import annotations

import json
import math
import random
from pathlib import Path
from typing import Any, Callable

from . import CONFIG_DIR, SOURCE_ART_DIR
from .textures import write_png

GLASS_CONFIG_PATH = CONFIG_DIR / "glass.json"
TEXTURE_DIR = SOURCE_ART_DIR / "Textures"

SMUDGE_TEXTURE = "T_Glass_Smudge_Mask"
BALL_MARK_TEXTURE = "T_Glass_BallMarks_Mask"
ROLLER_WAVE_TEXTURE = "T_Glass_RollerWave_N"


def load_config(path: Path = GLASS_CONFIG_PATH) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    panel, fix = data["panel"], data["fixings"]
    if not 0.008 <= panel["thickness_m"] <= 0.015:
        raise ValueError("glass.json: tempered court glass is 10-12 mm; thickness_m out of range")
    if panel["arris_m"] * 2 >= panel["thickness_m"] or panel["arris_m"] >= panel["corner_radius_m"]:
        raise ValueError("glass.json: arris_m must be smaller than half the thickness and the corner radius")
    # Tempered glass can't be drilled close to an edge: hole edge at least 2x thickness from it.
    hole_edge = fix["edge_inset_m"] - fix["bolt_radius_m"] - 0.001
    if hole_edge < 2 * panel["thickness_m"]:
        raise ValueError("glass.json: bolt holes must be at least twice the glass thickness from the edge")
    return data


def _height_key(height: float) -> str:
    return f"{height:.1f}"


def panels(spec: dict[str, float], config: dict[str, Any]) -> list[dict[str, Any]]:
    """Every glass panel of the court, in metres on Unreal's axes (GDD 2).

    Back walls: `back_wall_panels` panels across the full width at |X| = HalfLength.
    Side walls: per side and per half, the full-height panel next to the back
    wall and the lower "step" panel after it. Each panel gives half a joint at
    each vertical edge, so joints are centred on the post lines.
    """
    hl, hw = spec["HalfLength"], spec["HalfWidth"]
    joint = config["panel"]["joint_m"]
    count = config["panel"]["back_wall_panels"]
    rng = random.Random(config["seed"])
    out: list[dict[str, Any]] = []

    def add(name: str, wall: str, origin: tuple, u: tuple, normal: tuple, width: float, height: float) -> None:
        # A random window into the dirt textures, so no two panels carry the same marks.
        uv_offset = (round(rng.uniform(0.0, 8.0), 3), round(rng.uniform(0.0, 8.0), 3))
        out.append({"name": name, "wall": wall, "origin": origin, "u": u, "normal": normal,
                    "width": width, "height": height, "uv_offset": uv_offset})

    pitch = spec["Width"] / count
    for sx, half in ((1, "A"), (-1, "B")):
        for i in range(count):
            # u x up points out of the court (+X on half A), so u runs +Y on A and -Y on B.
            y0 = -hw + i * pitch + joint / 2 if sx > 0 else hw - i * pitch - joint / 2
            add(f"Back{half}{i + 1}", "back", (sx * hl, y0, 0.0), (0.0, float(sx), 0.0), (float(sx), 0.0, 0.0),
                pitch - joint, spec["BackGlassHeight"])
        for sy, side in ((1, "L"), (-1, "R")):
            for kind, x_near, x_far, height in (
                    ("Full", spec["SideGlassStepStartX"], hl, spec["SideGlassFullHeight"]),
                    ("Step", spec["SideGlassEndX"], spec["SideGlassStepStartX"], spec["SideGlassStepHeight"])):
                # u x up must point out of the court, which fixes u's direction on each wall.
                direction = -sy
                lo, hi = sorted((sx * x_near, sx * x_far))
                start_x = lo + joint / 2 if direction > 0 else hi - joint / 2
                add(f"Side{half}{side}{kind}", "side", (start_x, sy * hw, 0.0), (float(direction), 0.0, 0.0),
                    (0.0, float(sy), 0.0), x_far - x_near - joint, height)
    return out


def fixings(panel: dict[str, Any], config: dict[str, Any]) -> list[tuple[float, float]]:
    """Bolt positions on a panel as (u, z) in metres on its inner face: one row
    on each vertical edge, at the heights glass.json gives for that panel height."""
    fix = config["fixings"]
    key = _height_key(panel["height"])
    if key not in fix["heights_m"]:
        raise ValueError(f"glass.json: no fixing heights for {key} m panels")
    inset = fix["edge_inset_m"]
    return [(u, z) for u in (inset, panel["width"] - inset) for z in fix["heights_m"][key]]


def point_on(panel: dict[str, Any], u: float, z: float, depth: float = 0.0) -> tuple[float, float, float]:
    """World point at (u along the width, z up) on the inner face, pushed `depth` outward."""
    o, d, n = panel["origin"], panel["u"], panel["normal"]
    return tuple(o[i] + d[i] * u + n[i] * depth + (z if i == 2 else 0.0) for i in range(3))  # type: ignore[return-value]


# --- Procedural dirt textures ------------------------------------------------------


def _canvas(size: int) -> list[float]:
    return [0.0] * (size * size)


def _stamp(canvas: list[float], size: int, cx: float, cy: float, rx: float, ry: float, angle: float,
           profile: Callable[[float], float], strength: float) -> None:
    """Adds a rotated elliptical stamp, wrapping at the edges so the texture tiles."""
    reach = int(math.ceil(max(rx, ry))) + 1
    ca, sa = math.cos(angle), math.sin(angle)
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            px, py = dx + 0.5 - (cx % 1.0), dy + 0.5 - (cy % 1.0)
            lx, ly = (px * ca + py * sa) / rx, (-px * sa + py * ca) / ry
            r = math.sqrt(lx * lx + ly * ly)
            if r >= 1.0:
                continue
            x, y = (int(cx) + dx) % size, (int(cy) + dy) % size
            canvas[y * size + x] = max(canvas[y * size + x], strength * profile(r))


def _soft(r: float) -> float:
    return (1.0 - r * r) ** 2


def _to_grey(canvas: list[float], size: int) -> Callable[[int, int], tuple[int]]:
    return lambda x, y: (int(round(255 * min(max(canvas[y * size + x], 0.0), 1.0))),)


def smudge_mask(path: Path, cfg: dict[str, Any], rng: random.Random) -> Path:
    """Greasy hand and towel smears plus a few curved squeegee wipes. Drives the
    roughness of the glass (smeared reflections), barely its opacity."""
    size = cfg["size_px"]
    canvas = _canvas(size)
    for _ in range(cfg["smudges"]):
        r = rng.uniform(0.02, 0.09) * size
        _stamp(canvas, size, rng.uniform(0, size), rng.uniform(0, size), r, r * rng.uniform(0.4, 1.0),
               rng.uniform(0, math.pi), _soft, rng.uniform(0.15, 0.55))
        # Fingertip prints near some smears.
        if rng.random() < 0.4:
            for _ in range(rng.randint(2, 4)):
                fr = rng.uniform(0.006, 0.01) * size
                _stamp(canvas, size, rng.uniform(0, size), rng.uniform(0, size), fr, fr * 1.3,
                       rng.uniform(0, math.pi), _soft, rng.uniform(0.3, 0.7))
    for _ in range(cfg["wipes"]):
        # An arc of overlapping thin ellipses: the trace a cloth leaves.
        cx, cy = rng.uniform(0, size), rng.uniform(0, size)
        radius = rng.uniform(0.08, 0.25) * size
        start, sweep = rng.uniform(0, 2 * math.pi), rng.uniform(0.6, 1.6)
        width = rng.uniform(0.004, 0.012) * size
        strength = rng.uniform(0.1, 0.3)
        steps = int(radius * sweep / max(width, 1.0)) + 1
        for i in range(steps):
            t = start + sweep * i / steps
            _stamp(canvas, size, cx + radius * math.cos(t), cy + radius * math.sin(t), width * 2.5, width,
                   t + math.pi / 2, _soft, strength)
    # Fine haze so clean areas aren't perfectly uniform.
    for i in range(size * size):
        canvas[i] = min(1.0, canvas[i] + rng.uniform(0.0, 0.05))
    return write_png(path, size, size, 1, _to_grey(canvas, size))


def ball_mark_mask(path: Path, cfg: dict[str, Any], rng: random.Random) -> Path:
    """Felt scuffs where balls hit the glass: a faint disc the size of the ball
    with a stronger rim and a spray of fibre specks, some smeared sideways by spin."""
    size = cfg["size_px"]
    canvas = _canvas(size)
    px_per_m = size / cfg["tile_m"]
    base = cfg["mark_radius_m"] * px_per_m

    def ring(r: float) -> float:
        return 0.55 * _soft(r) + 0.45 * math.exp(-((r - 0.7) / 0.22) ** 2)

    for _ in range(cfg["marks"]):
        cx, cy = rng.uniform(0, size), rng.uniform(0, size)
        r = base * rng.uniform(0.75, 1.15)
        skid = rng.uniform(1.0, 1.9)
        angle = rng.uniform(0, math.pi)
        strength = rng.uniform(0.25, 1.0)
        _stamp(canvas, size, cx, cy, r * skid, r, angle, ring, strength)
        for _ in range(rng.randint(4, 12)):
            d = r * rng.uniform(0.6, 1.6)
            t = rng.uniform(0, 2 * math.pi)
            sr = rng.uniform(0.6, 1.6)
            _stamp(canvas, size, cx + d * math.cos(t), cy + d * math.sin(t), sr, sr, 0.0, _soft, strength * rng.uniform(0.4, 0.9))
    return write_png(path, size, size, 1, _to_grey(canvas, size))


def roller_wave_normal(path: Path, cfg: dict[str, Any]) -> Path:
    """Tangent-space normal map of tempering roller wave: one sinusoidal ripple
    period along V (the panel's height). This is what makes reflections in real
    court glass bend slightly; the material scales it down."""
    width, height = cfg["size_px"]
    max_slope = cfg["max_slope"]

    def pixel(x: int, y: int) -> tuple[int, int, int]:
        slope = max_slope * math.cos(2 * math.pi * (y + 0.5) / height)
        n = (0.0, -slope, 1.0)
        length = math.sqrt(sum(c * c for c in n))
        return tuple(int(round(255 * (c / length * 0.5 + 0.5))) for c in n)  # type: ignore[return-value]

    return write_png(path, width, height, 3, pixel)


def generate_textures(config: dict[str, Any] | None = None, out_dir: Path = TEXTURE_DIR) -> list[Path]:
    config = config or load_config()
    tex = config["textures"]
    rng = random.Random(config["seed"])
    return [
        smudge_mask(out_dir / f"{SMUDGE_TEXTURE}.png", tex["smudge"], rng),
        ball_mark_mask(out_dir / f"{BALL_MARK_TEXTURE}.png", tex["ball_marks"], rng),
        roller_wave_normal(out_dir / f"{ROLLER_WAVE_TEXTURE}.png", tex["roller_wave"]),
    ]
