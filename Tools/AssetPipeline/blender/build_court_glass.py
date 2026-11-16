"""Builds the court's tempered glass walls (GDD 2 sizes, GDD 10 look) as one
static mesh, SM_CourtGlass: 18 panels with polished arrised edges and dubbed
corners, plus the bolt fixings that hold them to the posts.

Run by pipeline.py (builder "court_glass"). Job keys: id, spec (CourtSpec.h
constants in metres), palette, budget_tris, out (FBX path), report.

The panel layout and bolt positions come from asset_lib/glass.py (tested
without Blender); this script only turns them into geometry. Material slots:

  MI_Court_Glass        the two large faces (translucent, smudges, ball marks)
  MI_Court_GlassEdge    the polished edges and arrises, where soda-lime glass shows green
  MI_Court_GlassBolt    stainless countersunk heads, shafts, washers and nuts
  MI_Court_GlassGasket  the black nylon bush around each head and the EPDM washer behind the glass

The fixings are opaque sections of the same mesh, so the venue places one asset.
The mesh is not Nanite: Nanite can't render translucent materials, and at about
25k triangles it doesn't need it. See docs/decisions/0006-court-glass.md.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import bpy  # noqa: E402,F401  (the bpy module must load before bmesh)
import bmesh  # noqa: E402

import bl_common as bc  # noqa: E402
from asset_lib import glass  # noqa: E402

SLOTS = ("Glass", "GlassEdge", "GlassBolt", "GlassGasket")


def rounded_rect(half_u: float, half_z: float, radius: float, segments: int) -> list[tuple[float, float]]:
    """Outline of a rectangle with rounded corners, counter-clockwise from the
    bottom-right corner, centred on the origin."""
    pts = []
    corners = ((half_u - radius, -half_z + radius, -math.pi / 2), (half_u - radius, half_z - radius, 0.0),
               (-half_u + radius, half_z - radius, math.pi / 2), (-half_u + radius, -half_z + radius, math.pi))
    for cx, cz, start in corners:
        for i in range(segments + 1):
            t = start + (math.pi / 2) * i / segments
            pts.append((cx + radius * math.cos(t), cz + radius * math.sin(t)))
    return pts


def add_panel(bm: bmesh.types.BMesh, panel: dict, cfg: dict, face_mat: int, edge_mat: int) -> None:
    """One slab: an inset ring on each face and two full-size rings between them
    make the 45-degree arris on all four edges of both faces."""
    p = cfg["panel"]
    t, arris, radius, segs = p["thickness_m"], p["arris_m"], p["corner_radius_m"], p["corner_segments"]
    cu, cz = panel["width"] / 2, panel["height"] / 2
    uv = bm.loops.layers.uv.verify()
    rings = []
    for inset, depth in ((arris, 0.0), (0.0, arris), (0.0, t - arris), (arris, t)):
        outline = rounded_rect(cu - inset, cz - inset, radius - inset, segs)
        rings.append([bm.verts.new(glass.point_on(panel, cu + du, cz + dz, depth)) for du, dz in outline])
    count = len(rings[0])

    # Faces: UVs in metres plus a per-panel offset, so the dirt textures land differently on each panel.
    off_u, off_v = panel["uv_offset"]
    for ring, reverse in ((rings[0], False), (rings[3], True)):
        face = bm.faces.new(list(reversed(ring)) if reverse else ring)
        face.material_index = face_mat
        _face_uvs(face, uv, panel, off_u, off_v)

    # Edges: U runs along the perimeter, V across the thickness.
    perimeter = [0.0]
    outline = rounded_rect(cu, cz, radius, segs)
    for i in range(1, count + 1):
        a, b = outline[i - 1], outline[i % count]
        perimeter.append(perimeter[-1] + math.dist(a, b))
    depths = (0.0, arris, t - arris, t)
    for k in range(3):
        for i in range(count):
            j = (i + 1) % count
            face = bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]))
            face.material_index = edge_mat
            for loop, (s, d) in zip(face.loops, ((perimeter[i], depths[k]), (perimeter[i + 1], depths[k]),
                                                (perimeter[i + 1], depths[k + 1]), (perimeter[i], depths[k + 1]))):
                loop[uv].uv = (s, d)


def _face_uvs(face, uv, panel: dict, off_u: float, off_v: float) -> None:
    origin, u_dir = panel["origin"], panel["u"]
    for loop in face.loops:
        co = loop.vert.co
        u = sum((co[i] - origin[i]) * u_dir[i] for i in range(3))
        loop[uv].uv = (u + off_u, co[2] + off_v)


def add_fixing(bm: bmesh.types.BMesh, panel: dict, u: float, z: float, cfg: dict, bolt: int, gasket: int) -> None:
    """A countersunk through-bolt, flush on the court side (nothing may stick out
    into play), with its gasket, washer and nut on the back of the post."""
    f, t = cfg["fixings"], cfg["panel"]["thickness_m"]
    seg = f["segments"]

    def cyl(d0: float, d1: float, radius: float, mat: int, segments: int = seg) -> None:
        bc.add_cylinder(bm, glass.point_on(panel, u, z, d0), glass.point_on(panel, u, z, d1), radius, segments, mat)

    # Court side: the stainless head sits just proud of its nylon bush, both a hair proud of the
    # glass so the opaque parts never z-fight the translucent face.
    cyl(-f["bush_proud_m"], t * 0.3, f["bush_radius_m"], gasket)
    cyl(-f["head_proud_m"], t * 0.35, f["head_radius_m"], bolt)
    back = t + f["gasket_thickness_m"] + f["post_depth_m"]
    end = back + f["washer_thickness_m"] + f["nut_height_m"] + f["thread_overhang_m"]
    cyl(0.0, end, f["bolt_radius_m"], bolt, 8)
    cyl(t, t + f["gasket_thickness_m"], f["gasket_radius_m"], gasket)
    cyl(back, back + f["washer_thickness_m"], f["washer_radius_m"], bolt)
    nut0 = back + f["washer_thickness_m"]
    cyl(nut0, nut0 + f["nut_height_m"], f["nut_across_flats_m"] / math.sqrt(3.0), bolt, 6)


def build(job: dict) -> list:
    cfg = glass.load_config()
    pal = job["palette"]["court"]
    mats = [bc.material(f"MI_Court_{name}", pal[name]) for name in SLOTS]
    idx = {name: i for i, name in enumerate(SLOTS)}
    bm = bmesh.new()
    for panel in glass.panels(job["spec"], cfg):
        add_panel(bm, panel, cfg, idx["Glass"], idx["GlassEdge"])
        for u, z in glass.fixings(panel, cfg):
            add_fixing(bm, panel, u, z, cfg, idx["GlassBolt"], idx["GlassGasket"])
    # Panels are flat-shaded (bmesh's default); the bolt cylinders keep their smooth sides.
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = bc.mesh_object(job["id"], bm, mats)
    # No decimation: collapsing a 12 mm slab would wreck the edges. Over budget is a build error.
    return [obj]


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects = build(job)
    bc.write_report(job, objects, {"panels": len(glass.panels(job["spec"], glass.load_config()))})
    bc.export_fbx(objects, Path(job["out"]))


if __name__ == "__main__":
    main()
