"""Builds the indoor slice court (GDD 2, GDD 10) from CourtSpec.h measurements.

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
POST_SIZE = 0.08          # square aluminium post
RAIL_RADIUS = 0.025
FLOOR_THICKNESS = 0.02
NET_POST_SIZE = 0.08
NET_TAPE = 0.05


def net_height_at(spec: dict, y: float) -> float:
    t = min(max(abs(y) / spec["HalfWidth"], 0.0), 1.0)
    return spec["NetHeightCentre"] + (spec["NetHeightPosts"] - spec["NetHeightCentre"]) * t * t


def build(job: dict) -> list:
    spec, pal = job["spec"], job["palette"]["court"]
    names = ["Turf", "Line", "Runoff", "Frame", "Mesh", "Net", "NetTape", "Glass"]
    mats = [bc.material(f"MI_Court_{n}", pal[n]) for n in names]
    idx = {n: i for i, n in enumerate(names)}
    hl, hw = spec["HalfLength"], spec["HalfWidth"]
    solid = bmesh.new()
    glass = bmesh.new()

    # --- Floor: turf, lines, run-off ---
    bc.add_box(solid, (0, 0, -FLOOR_THICKNESS / 2), (spec["Length"], spec["Width"], FLOOR_THICKNESS), idx["Turf"])
    outer_l = spec["Length"] + 2 * spec["RunoffBack"]
    for s in (-1, 1):
        bc.add_box(solid, (0, s * (hw + spec["RunoffSide"] / 2), -FLOOR_THICKNESS / 2 - 0.001),
                   (outer_l, spec["RunoffSide"], FLOOR_THICKNESS), idx["Runoff"])
        bc.add_box(solid, (s * (hl + spec["RunoffBack"] / 2), 0, -FLOOR_THICKNESS / 2 - 0.001),
                   (spec["RunoffBack"], spec["Width"], FLOOR_THICKNESS), idx["Runoff"])
    lw, lift = spec["LineWidth"], 0.002
    centre_len = spec["ServiceLineX"] + spec["CentreLineExtension"]
    for s in (-1, 1):
        bc.add_box(solid, (s * spec["ServiceLineX"], 0, lift / 2), (lw, spec["Width"], lift), idx["Line"])
        bc.add_box(solid, (s * centre_len / 2, 0, lift / 2), (centre_len, lw, lift), idx["Line"])

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

    # --- Frame: posts behind every panel joint, rails along the top ---
    off = t + POST_SIZE / 2
    for sx in (-1, 1):
        for i in range(6):
            y = -hw + i * spec["Width"] / 5
            bc.add_box(solid, (sx * (hl + off), y, spec["FenceTotalHeight"] / 2),
                       (POST_SIZE, POST_SIZE, spec["FenceTotalHeight"]), idx["Frame"])
        bc.add_cylinder(solid, (sx * (hl + off), -hw - off, spec["FenceTotalHeight"]),
                        (sx * (hl + off), hw + off, spec["FenceTotalHeight"]), RAIL_RADIUS, 8, idx["Frame"])
        for sy in (-1, 1):
            y = sy * (hw + off)
            for x in (hl, spec["SideGlassStepStartX"], spec["SideGlassEndX"], spec["ExitNearX"], spec["ExitNearX"] / 2, 0.0):
                h = spec["FenceTotalHeight"] if x >= spec["SideGlassStepStartX"] else spec["SideMeshHeight"]
                if x == 0.0 and sx < 0:
                    continue  # the post on the net line is shared by both halves
                bc.add_box(solid, (sx * x, y, h / 2), (POST_SIZE, POST_SIZE, h), idx["Frame"])
            bc.add_cylinder(solid, (sx * spec["SideGlassStepStartX"], y, spec["FenceTotalHeight"]),
                            (sx * (hl + off), y, spec["FenceTotalHeight"]), RAIL_RADIUS, 8, idx["Frame"])
            for x0, x1 in ((spec["SideGlassEndX"], spec["SideGlassStepStartX"]), (0.0, spec["ExitNearX"])):
                bc.add_cylinder(solid, (sx * x0, y, spec["SideMeshHeight"]), (sx * x1, y, spec["SideMeshHeight"]),
                                RAIL_RADIUS, 8, idx["Frame"])

    # --- Welded mesh panels (masked material; UVs in metres for real cell size) ---
    tile = job["palette"]["court"]["Mesh"].get("tiling_m", 0.05)
    mesh_off = t + 0.004
    for sx in (-1, 1):
        x = sx * (hl + mesh_off)
        bc.add_quad(solid, [(x, -hw, spec["BackGlassHeight"]), (x, hw, spec["BackGlassHeight"]),
                            (x, hw, spec["FenceTotalHeight"]), (x, -hw, spec["FenceTotalHeight"])], idx["Mesh"], tile)
        for sy in (-1, 1):
            y = sy * (hw + mesh_off)
            for x0, x1, z0, z1 in ((spec["SideGlassStepStartX"], hl, spec["SideGlassFullHeight"], spec["FenceTotalHeight"]),
                                   (spec["SideGlassEndX"], spec["SideGlassStepStartX"], spec["SideGlassStepHeight"], spec["SideMeshHeight"]),
                                   (0.0, spec["ExitNearX"], 0.0, spec["SideMeshHeight"])):
                bc.add_quad(solid, [(sx * x0, y, z0), (sx * x1, y, z0), (sx * x1, y, z1), (sx * x0, y, z1)],
                            idx["Mesh"], tile)

    # --- Net: sagging mesh, tape and posts (GDD 2: 0.88 centre, 0.92 posts) ---
    net_tile = job["palette"]["court"]["Net"].get("tiling_m", 0.045)
    steps = 40
    for i in range(steps):
        y0 = -hw + spec["Width"] * i / steps
        y1 = -hw + spec["Width"] * (i + 1) / steps
        h0, h1 = net_height_at(spec, y0) - NET_TAPE, net_height_at(spec, y1) - NET_TAPE
        bc.add_quad(solid, [(0, y0, 0), (0, y1, 0), (0, y1, h1), (0, y0, h0)], idx["Net"], net_tile)
        top0, top1 = net_height_at(spec, y0), net_height_at(spec, y1)
        verts = [solid.verts.new(v) for v in ((0.006, y0, top0 - NET_TAPE), (0.006, y1, top1 - NET_TAPE),
                                              (0.006, y1, top1), (0.006, y0, top0))]
        solid.faces.new(verts).material_index = idx["NetTape"]
        verts = [solid.verts.new(v) for v in ((-0.006, y0, top0), (-0.006, y1, top1),
                                              (-0.006, y1, top1 - NET_TAPE), (-0.006, y0, top0 - NET_TAPE))]
        solid.faces.new(verts).material_index = idx["NetTape"]
    for sy in (-1, 1):
        bc.add_box(solid, (0, sy * (hw - NET_POST_SIZE / 2), spec["NetPostHeight"] / 2),
                   (NET_POST_SIZE, NET_POST_SIZE, spec["NetPostHeight"]), idx["Frame"])

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
