"""Builds the indoor slice court (GDD 2, GDD 10) from CourtSpec.h measurements.
The net is its own asset (build_net.py). The floor, lines and run-off are SM_CourtSurface
(build_court_surface.py).

Run by pipeline.py. Job keys: id, part ("solid" or "glass"), spec (CourtSpec.h
constants in metres), palette, budget_tris, out (FBX path), report.

Layout (Unreal axes, metres): length along X (-10..10), width along Y (-5..5),
Z up, ground at Z = 0, net on X = 0. Glass inner faces sit exactly on the
playing-area limits, like the colliders in CourtLayout.h.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402,F401  (the bpy module must load before bmesh)
import bmesh  # noqa: E402

import bl_common as bc  # noqa: E402

GLASS_THICKNESS = 0.012   # tempered panels are 10-12 mm
PANEL_GAP = 0.008         # joint between glass panels
LIGHT_HEIGHT = 7.6


def build(job: dict) -> list:
    spec, pal = job["spec"], job["palette"]["court"]
    # The net is SM_CourtNet (build_net.py); the floor, lines and run-off are SM_CourtSurface
    # (build_court_surface.py).
    # The fence posts, rails and welded mesh are SM_CourtFrame and SM_CourtMesh (build_court_frame.py).
    names = ["LightPanel", "Glass"]
    mats = [bc.material(f"MI_Court_{n}", pal[n]) for n in names]
    idx = {n: i for i, n in enumerate(names)}
    hl, hw = spec["HalfLength"], spec["HalfWidth"]
    solid = bmesh.new()
    glass = bmesh.new()

    # --- Glass panels (inner face on the court limit) ---
    t = GLASS_THICKNESS
    for sx in (-1, 1):
        # Back wall: five 2 m panels across the width.
        panels = 5
        pw = spec["Width"] / panels
        for i in range(panels):
            y0 = -hw + i * pw
            bc.add_box(glass, (sx * (hl + t / 2), y0 + pw / 2, spec["BackGlassHeight"] / 2),
                       (t, pw - PANEL_GAP, spec["BackGlassHeight"]), idx["Glass"])
        for sy in (-1, 1):
            for x0, x1, h in ((spec["SideGlassStepStartX"], hl, spec["SideGlassFullHeight"]),
                              (spec["SideGlassEndX"], spec["SideGlassStepStartX"], spec["SideGlassStepHeight"])):
                bc.add_box(glass, (sx * (x0 + x1) / 2, sy * (hw + t / 2), h / 2),
                           (x1 - x0 - PANEL_GAP, t, h), idx["Glass"])

    # The fence posts, rails and welded mesh are built by build_court_frame.py (CourtFrame, CourtMesh).

    # The net, its tape and posts are SM_CourtNet (build_net.py), so the hit reaction
    # can move them with their own materials.

    # --- Arena light panels over the court ---
    for x in (-6.0, -2.0, 2.0, 6.0):
        for y in (-3.2, 3.2):
            bc.add_box(solid, (x, y, LIGHT_HEIGHT), (1.4, 0.35, 0.08), idx["LightPanel"])

    keep, drop = (glass, solid) if job["part"] == "glass" else (solid, glass)
    drop.free()
    # Only the materials this part uses become slots, so Unreal sees only real sections.
    used = sorted({f.material_index for f in keep.faces})
    remap = {old: new for new, old in enumerate(used)}
    for face in keep.faces:
        face.material_index = remap[face.material_index]
    obj = bc.mesh_object(job["id"], keep, [mats[i] for i in used])
    bc.decimate_to(obj, job["budget_tris"])
    return [obj]


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects = build(job)
    bc.write_report(job, objects)
    bc.export_fbx(objects, Path(job["out"]))


if __name__ == "__main__":
    main()
