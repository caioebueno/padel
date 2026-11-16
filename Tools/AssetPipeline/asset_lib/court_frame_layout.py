"""Where every piece of the court's steel structure goes (GDD 2, GDD 10).

Pure Python, so the layout is tested without Blender. blender/build_court_frame.py
turns it into geometry. Everything is in metres on Unreal's axes (X along the
court, Y across it, Z up), mirrored into all four corners.

The rules come from how a real court is put together:
- The glass and the mesh panels sit in the wall plane, inner face on the court
  limit (like the colliders in CourtLayout.h). The posts stand behind them,
  centred on every joint line, with their court-side face one glass thickness
  plus a gasket outside the limit, so the glass bolts pass through glass, gasket
  and post.
- The mesh zones are exactly the "mesh" surfaces of CourtSpec.h
  (BackWallSurfaceAt and SideWallSurfaceAt), split into panels of at most 2 m.
- Each corner is one L-shaped post that backs the end of both the back glass
  and the side glass.
- Base plates, anchors and gussets stay outside the court limit, on the run-off.
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from . import CONFIG_DIR

CONFIG_PATH = CONFIG_DIR / "court_frame.json"

Vec3 = tuple[float, float, float]
Vec2 = tuple[float, float]


@dataclass
class Post:
    """A vertical post: an XY outline (counter-clockwise) extruded from z0 to z1."""
    name: str
    outline: list[Vec2]
    z0: float
    z1: float


@dataclass
class Panel:
    """A welded mesh panel in a wall plane.

    origin is the panel's lower corner on the court limit, `along` and `up` span
    the panel, `out` points away from the court (into the post). Size in metres.
    """
    name: str
    origin: Vec3
    along: Vec3
    up: Vec3
    out: Vec3
    length: float
    height: float
    mid_rails: list[float] = field(default_factory=list)  # heights above the panel bottom


@dataclass
class Box:
    name: str
    lo: Vec3
    hi: Vec3


@dataclass
class Gusset:
    """A triangular stiffener: right angle at `root` (post face, plate top)."""
    name: str
    root: Vec3
    out: Vec3        # along the plate, away from the post
    side: Vec3       # thickness direction
    length: float
    height: float
    thickness: float


@dataclass
class Fastener:
    """A nut, bolt head or washer stack: `axis` points out of the surface it sits on."""
    name: str
    base: Vec3
    axis: Vec3
    kind: str  # "anchor" (M16 stud, washer, nut) or "panel" (M10 hex head and washer)


@dataclass
class FrameLayout:
    posts: list[Post]
    panels: list[Panel]
    plates: list[Box]
    gussets: list[Gusset]
    fasteners: list[Fastener]
    post_face_offset: float  # court limit to post court-side face

    def summary(self) -> dict[str, Any]:
        return {"posts": len(self.posts), "panels": len(self.panels), "plates": len(self.plates),
                "gussets": len(self.gussets), "fasteners": len(self.fasteners),
                "mesh_area_m2": round(sum(p.length * p.height for p in self.panels), 4)}


def load_config(path: Path = CONFIG_PATH) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _split(a0: float, a1: float, max_len: float) -> list[tuple[float, float]]:
    count = max(1, math.ceil((a1 - a0) / max_len - 1e-9))
    step = (a1 - a0) / count
    return [(a0 + i * step, a0 + (i + 1) * step) for i in range(count)]


def mesh_zones(spec: dict[str, float]) -> list[dict[str, Any]]:
    """The mesh areas of one half (+X) and one side (+Y), from CourtSpec.h.

    Back zones run along Y, side zones along X. Mirrors BackWallSurfaceAt and
    SideWallSurfaceAt: mesh above the back glass, above both side glass panels,
    and full height from the exit to the net.
    """
    hl, hw = spec["HalfLength"], spec["HalfWidth"]
    return [
        {"wall": "back", "a0": -hw, "a1": hw, "z0": spec["BackGlassHeight"], "z1": spec["FenceTotalHeight"]},
        {"wall": "side", "a0": spec["SideGlassStepStartX"], "a1": hl,
         "z0": spec["SideGlassFullHeight"], "z1": spec["FenceTotalHeight"]},
        {"wall": "side", "a0": spec["SideGlassEndX"], "a1": spec["SideGlassStepStartX"],
         "z0": spec["SideGlassStepHeight"], "z1": spec["SideMeshHeight"]},
        {"wall": "side", "a0": 0.0, "a1": spec["ExitNearX"], "z0": 0.0, "z1": spec["SideMeshHeight"]},
    ]


def _rect(x0: float, x1: float, y0: float, y1: float) -> list[Vec2]:
    xa, xb = sorted((x0, x1))
    ya, yb = sorted((y0, y1))
    return [(xa, ya), (xb, ya), (xb, yb), (xa, yb)]


def _ccw(points: list[Vec2]) -> list[Vec2]:
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(points, points[1:] + points[:1]))
    return points if area > 0 else list(reversed(points))


def build(spec: dict[str, float], cfg: dict[str, Any] | None = None) -> FrameLayout:
    cfg = cfg or load_config()
    hl, hw = spec["HalfLength"], spec["HalfWidth"]
    post_a, post_d = cfg["post"]["across_m"], cfg["post"]["depth_m"]
    off = cfg["glass"]["thickness_m"] + cfg["glass"]["gasket_m"]
    pf = cfg["panel_frame"]
    bp = cfg["base_plate"]
    plate_t = bp["thickness_m"]
    gus = cfg["gusset"]

    posts: list[Post] = []
    panels: list[Panel] = []
    plates: list[Box] = []
    gussets: list[Gusset] = []
    fasteners: list[Fastener] = []

    # --- Mesh panels, and the post heights they need ---
    # Joint lines along each wall, with the height the post there must reach.
    back_joints: dict[float, float] = {}
    side_joints: dict[float, float] = {}
    for zone in mesh_zones(spec):
        for a0, a1 in _split(zone["a0"], zone["a1"], pf["max_panel_length_m"]):
            joints = back_joints if zone["wall"] == "back" else side_joints
            for a in (a0, a1):
                joints[round(a, 6)] = max(joints.get(round(a, 6), 0.0), zone["z1"])
            height = zone["z1"] - zone["z0"]
            rails = [pf["mid_rail_z_m"] - zone["z0"]] if height >= pf["mid_rail_min_height_m"] else []
            for sx in (-1, 1):
                if zone["wall"] == "back":
                    panels.append(Panel(f"Back{sx:+d}_{a0:+.1f}", (sx * hl, a0, zone["z0"]),
                                        (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (float(sx), 0.0, 0.0),
                                        a1 - a0, height, rails))
                    continue
                for sy in (-1, 1):
                    panels.append(Panel(f"Side{sx:+d}{sy:+d}_{a0:.1f}", (sx * a0, sy * hw, zone["z0"]),
                                        (float(sx), 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, float(sy), 0.0),
                                        a1 - a0, height, rails))
    # The glass joints need posts too (the side glass step at SideGlassStepStartX is
    # also a mesh edge, so this only matters if the zones ever change).
    for x in (spec["SideGlassStepStartX"], spec["SideGlassEndX"]):
        side_joints.setdefault(round(x, 6), spec["SideMeshHeight"])
    for y in (-hw + i * spec["Width"] / 5 for i in range(6)):
        back_joints.setdefault(round(y, 6), spec["FenceTotalHeight"])

    z_post0 = plate_t
    # --- Back wall posts (corners are handled below) ---
    for y, top in sorted(back_joints.items()):
        if abs(abs(y) - hw) < 1e-6:
            continue
        for sx in (-1, 1):
            x0 = sx * (hl + off)
            posts.append(Post(f"PostBack{sx:+d}_{y:+.1f}", _rect(x0, x0 + sx * post_d, y - post_a / 2, y + post_a / 2),
                              z_post0, top))
            _foot(f"Back{sx:+d}_{y:+.1f}", (sx, 0), y, hl, hw, off, post_a, post_d, bp, gus, plates, gussets, fasteners)
    # --- Side wall posts; the post on the net line is shared by both halves ---
    for x, top in sorted(side_joints.items()):
        if abs(x - hl) < 1e-6:
            continue
        for sx in ((1,) if abs(x) < 1e-6 else (-1, 1)):
            for sy in (-1, 1):
                y0 = sy * (hw + off)
                cx = sx * x
                posts.append(Post(f"PostSide{sx:+d}{sy:+d}_{x:.1f}",
                                  _rect(cx - post_a / 2, cx + post_a / 2, y0, y0 + sy * post_d), z_post0, top))
                _foot(f"Side{sx:+d}{sy:+d}_{x:.1f}", (0, sy), cx, hl, hw, off, post_a, post_d, bp, gus, plates,
                      gussets, fasteners)
    # --- Corner L posts, backing the end of the back glass and of the side glass ---
    for sx in (-1, 1):
        for sy in (-1, 1):
            xi, yi = hl + off, hw + off          # court-side faces
            xo, yo = xi + post_d, yi + post_d    # outside faces
            pts = [(xi, hw - post_a / 2), (xo, hw - post_a / 2), (xo, yo), (hl - post_a / 2, yo),
                   (hl - post_a / 2, yi), (xi, yi)]
            outline = _ccw([(sx * px, sy * py) for px, py in pts])
            posts.append(Post(f"PostCorner{sx:+d}{sy:+d}", outline, z_post0,
                              max(spec["FenceTotalHeight"], back_joints.get(round(sy * hw, 6), 0.0))))
            out = bp["out_m"]
            for name, (ax0, ax1, ay0, ay1) in (("A", (xi, xi + out, hw - bp["across_m"] / 2, yi + out)),
                                               ("B", (hl - bp["across_m"] / 2, xi, yi, yi + out))):
                lo = (min(sx * ax0, sx * ax1), min(sy * ay0, sy * ay1), 0.0)
                hi = (max(sx * ax0, sx * ax1), max(sy * ay0, sy * ay1), plate_t)
                plates.append(Box(f"PlateCorner{sx:+d}{sy:+d}{name}", lo, hi))
            ins = bp["anchor_inset_m"]
            for i, (px, py) in enumerate(((xi + out - ins, hw - bp["across_m"] / 2 + ins),
                                          (xi + out - ins, yi + out - ins),
                                          (hl - bp["across_m"] / 2 + ins, yi + out - ins))):
                fasteners.append(Fastener(f"AnchorCorner{sx:+d}{sy:+d}_{i}", (sx * px, sy * py, plate_t),
                                          (0.0, 0.0, 1.0), "anchor"))

    # --- Panel bracket bolts: two per vertical frame tube, into the post behind ---
    for panel in panels:
        for a in (pf["width_m"] / 2, panel.length - pf["width_m"] / 2):
            for z in (pf["bolt_inset_m"], panel.height - pf["bolt_inset_m"]):
                if panel.height < 2 * pf["bolt_inset_m"] + 0.05:
                    continue
                base = tuple(o + a * al + z * u for o, al, u in zip(panel.origin, panel.along, panel.up))
                fasteners.append(Fastener(f"Bolt{panel.name}_{a:.2f}_{z:.2f}", base,
                                          tuple(-c for c in panel.out), "panel"))

    return FrameLayout(posts, panels, plates, gussets, fasteners, off)


def _foot(name: str, wall: tuple[int, int], along: float, hl: float, hw: float, off: float, post_a: float,
          post_d: float, bp: dict, gus: dict, plates: list[Box], gussets: list[Gusset],
          fasteners: list[Fastener]) -> None:
    """Base plate, four anchors and a rear gusset for a straight post.

    wall is (sx, 0) for a back-wall post at Y = along, (0, sy) for a side-wall
    post at X = along. The plate starts at the post's court-side face and runs
    outward, so it never reaches inside the court.
    """
    sx, sy = wall
    out, half, t, ins = bp["out_m"], bp["across_m"] / 2, bp["thickness_m"], bp["anchor_inset_m"]
    if sx:
        n0 = hl + off
        lo, hi = (min(sx * n0, sx * (n0 + out)), along - half, 0.0), (max(sx * n0, sx * (n0 + out)), along + half, t)
        normal = (float(sx), 0.0, 0.0)
        side = (0.0, 1.0, 0.0)
        point = lambda n, a: (sx * n, a)  # noqa: E731
    else:
        n0 = hw + off
        lo, hi = (along - half, min(sy * n0, sy * (n0 + out)), 0.0), (along + half, max(sy * n0, sy * (n0 + out)), t)
        normal = (0.0, float(sy), 0.0)
        side = (1.0, 0.0, 0.0)
        point = lambda n, a: (a, sy * n)  # noqa: E731
    plates.append(Box(f"Plate{name}", lo, hi))
    for n in (n0 + ins, n0 + out - ins):
        for a in (along - half + ins, along + half - ins):
            x, y = point(n, a)
            fasteners.append(Fastener(f"Anchor{name}_{n:.2f}_{a:.2f}", (x, y, t), (0.0, 0.0, 1.0), "anchor"))
    x, y = point(n0 + post_d, along - gus["thickness_m"] / 2)
    gussets.append(Gusset(f"Gusset{name}", (x, y, t), normal, side, out - post_d - 0.008, gus["height_m"],
                          gus["thickness_m"]))
