"""Builds the match racket, the invented Ostrel Keystone (docs/decisions/0008-racket.md):
a diamond-shaped power racket with a carbon face, a raised sand finish, a drilled hole
pattern, an open heart with a bridge, an alloy weight cartridge and shoulder ports, a
rubber bumper and an overgrip-wrapped handle. Layout from asset_lib/racket_layout.py
and config/racket.json; colours from palette.json 'racket'.

Job keys: id, size, palette, palette_group, budget_tris, lods, out, lod_outs, report.

Orientation (Unreal axes, metres): grip centre on the origin, head up +Z, the faces look
along -Y and +Y, X across the face, so a hand socket holds it without extra offsets.

LODs are built, not decimated, so the face UVs stay exact: LOD0 for close-ups and replays
(bevelled edges, every hole drilled, the overgrip's helix, the hardware), LOD1 for the
near player, LOD2 for the broadcast camera (no holes: T_Racket_Masks.A draws them).
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import bpy  # noqa: E402  (the bpy module must load before bmesh)
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import bl_common as bc  # noqa: E402
from asset_lib import racket_layout as layout  # noqa: E402

# Material slots, in order. MI_Racket_<Name> in Unreal.
SLOTS = ("Face", "Frame", "Core", "Bumper", "Metal", "Accent", "Grip", "Cap")
FACE, FRAME, CORE, BUMPER, METAL, ACCENT, GRIP, CAP = range(len(SLOTS))


def _materials(job: dict) -> list:
    pal = job["palette"][job["palette_group"]]
    group = "".join(p.capitalize() for p in job["palette_group"].split("_"))
    return [bc.material(f"MI_{group}_{name}", pal[name]) for name in SLOTS]


def _curve_spline(curve, points) -> None:
    spline = curve.splines.new("POLY")
    spline.points.add(len(points) - 1)
    for p, (x, z) in zip(spline.points, points):
        p.co = (x, z, 0.0, 1.0)
    spline.use_cyclic_u = True


def _frame_body(cfg: dict, lod: int) -> bpy.types.Object:
    """The frame, face and throat as one solid: the silhouette with the heart windows cut
    out, extruded to the full thickness and rounded on every edge by a curve bevel. The
    bevel grows a 2D curve by its radius, so the outline goes in inset and the windows
    grown, and the finished edges land where racket_layout says."""
    frame = cfg["frame"]
    radius = frame["edge_radius_m"]
    count = cfg["outline_lod_points"][lod]
    curve = bpy.data.curves.new("RacketFrame", "CURVE")
    curve.dimensions = "2D"
    curve.fill_mode = "BOTH"
    curve.extrude = frame["thickness_m"] / 2 - radius
    curve.bevel_depth = radius
    curve.bevel_resolution = cfg["edge_lod_resolution"][lod]
    _curve_spline(curve, layout.offset(layout.outline(cfg, count), radius))
    for window in layout.heart_windows(cfg):
        window = layout._resample(window, max(16, count // 3), closed=True)
        _curve_spline(curve, layout.offset(window, -radius))
    obj = bpy.data.objects.new("RacketFrame", curve)
    bpy.context.scene.collection.objects.link(obj)
    mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    bpy.data.objects.remove(obj)
    bpy.data.curves.remove(curve)
    # The curve is drawn in its XY plane: its Y is the racket's Z, its Z (the extrusion)
    # is through the face. (x, y, z) -> (x, -z, y) is a proper rotation, so normals hold.
    for v in mesh.vertices:
        x, y, z = v.co
        v.co = (x, -z, y)
    body = bpy.data.objects.new("RacketBody", mesh)
    bpy.context.scene.collection.objects.link(body)
    return body


def _drill(body: bpy.types.Object, cfg: dict, lod: int) -> None:
    segments = cfg["holes"]["lod_segments"][lod]
    if segments <= 0:
        return
    r = cfg["holes"]["diameter_m"] / 2
    t = cfg["frame"]["thickness_m"]
    bm = bmesh.new()
    for x, z in layout.hole_centres(cfg):
        bc.add_cylinder(bm, (x, -t, z), (x, t, z), r, segments)
    cutter = bc.mesh_object("HoleCutter", bm, [])
    mod = body.modifiers.new("Holes", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = cutter
    mod.solver = "EXACT"
    bc.apply_modifier(body, mod.name)
    bpy.data.objects.remove(cutter)


def _paint_body(body: bpy.types.Object, cfg: dict, mats: list) -> None:
    """Material slots and UVs. The two flat faces take the face material and the face
    UVs (U mirrored on +Y so the wordmark reads from both sides), and so does the front
    of every rounded edge, where the face skin wraps over the frame; the walls of the
    drilled holes show the foam core; everything else is frame."""
    # The curve leaves an empty slot behind; start from none so the indices match SLOTS.
    body.data.materials.clear()
    for mat in mats:
        body.data.materials.append(mat)
    half_t = cfg["frame"]["thickness_m"] / 2
    holes = layout.hole_centres(cfg)
    r = cfg["holes"]["diameter_m"] / 2
    bm = bmesh.new()
    bm.from_mesh(body.data)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        c = f.calc_center_median()
        n = f.normal
        # The flat faces and the front of the rounded edges: the face skin wraps over them.
        if abs(n.y) > 0.7 and abs(c.y) > half_t - cfg["frame"]["edge_radius_m"]:
            f.material_index = FACE
            f.smooth = False
        elif abs(n.y) < 0.2 and any(math.hypot(c.x - x, c.z - z) < r + 5e-4 for x, z in holes):
            f.material_index = CORE
            f.smooth = True
        else:
            f.material_index = FRAME
            f.smooth = True
        mirror = c.y > 0
        for loop in f.loops:
            u, v = layout.face_uv(cfg, loop.vert.co.x, loop.vert.co.z)
            loop[uv].uv = (1.0 - u if mirror else u, 1.0 - v)
    bm.to_mesh(body.data)
    bm.free()


def _edge_frame(cfg: dict, point_index: int, outline: list) -> tuple[Vector, Vector]:
    """Position on the finished edge and the outward normal there (in X-Z)."""
    n = len(outline)
    a, b = outline[(point_index - 1) % n], outline[(point_index + 1) % n]
    tx, tz = b[0] - a[0], b[1] - a[1]
    length = math.hypot(tx, tz) or 1.0
    # Outline is CCW in (x, z), so outward is to the right of the direction of travel.
    normal = Vector((tz / length, 0.0, -tx / length))
    p = outline[point_index]
    return Vector((p[0], 0.0, p[1])), normal


def _bumper(bm: bmesh.types.BMesh, cfg: dict, lod: int) -> None:
    """Rubber protector over the top of the frame: the frame's rounded edge profile,
    grown by proud_m, swept along the edge from the tip to start_deg either side."""
    bump = cfg["bumper"]
    t = cfg["frame"]["thickness_m"]
    rad = cfg["frame"]["edge_radius_m"]
    proud, wrap = bump["proud_m"], bump["wrap_m"]
    outline = layout.outline(cfg, cfg["outline_lod_points"][lod] * 2)
    centre_z = layout.widest(cfg)[1] - 0.04
    limit = math.radians(bump["start_deg"])
    run = [i for i, (x, z) in enumerate(outline) if z > centre_z and abs(math.atan2(x, z - centre_z)) <= limit]
    # Ordered along the edge (the run may wrap round the start of the list).
    run.sort(key=lambda i: math.atan2(outline[i][0], outline[i][1] - centre_z))
    arc_steps = 4 if lod == 0 else 2
    profile = [(-rad - wrap, t / 2 + proud)]
    for k in range(arc_steps + 1):
        a = math.pi / 2 * (1 - k / arc_steps)
        profile.append((-rad + (rad + proud) * math.cos(a), t / 2 - rad + (rad + proud) * math.sin(a)))
    for k in range(arc_steps + 1):
        a = -math.pi / 2 * k / arc_steps
        profile.append((-rad + (rad + proud) * math.cos(a), -(t / 2 - rad) + (rad + proud) * math.sin(a)))
    profile.append((-rad - wrap, -(t / 2 + proud)))
    rings = []
    for i in run:
        p, n = _edge_frame(cfg, i, outline)
        rings.append([bm.verts.new(p + n * d + Vector((0.0, y, 0.0))) for d, y in profile])
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(len(profile) - 1):
            f = bm.faces.new((r0[k], r0[k + 1], r1[k + 1], r1[k]))
            f.material_index = BUMPER
            f.smooth = True


def _rounded_rect(bm, centre, half_x, half_z, y0, y1, mat, corner_steps=4) -> None:
    """A slab in X-Z with rounded ends (the weight cartridge), from y0 to y1."""
    r = min(half_x, half_z)
    pts = []
    for cx, cz, start in ((half_x - r, half_z - r, 0), (-(half_x - r), half_z - r, 90),
                          (-(half_x - r), -(half_z - r), 180), (half_x - r, -(half_z - r), 270)):
        for k in range(corner_steps + 1):
            a = math.radians(start + 90 * k / corner_steps)
            pts.append((centre[0] + cx + r * math.cos(a), centre[1] + cz + r * math.sin(a)))
    lo = [bm.verts.new((x, y0, z)) for x, z in pts]
    hi = [bm.verts.new((x, y1, z)) for x, z in pts]
    top = bm.faces.new(hi if y1 > y0 else list(reversed(hi)))
    top.material_index = mat
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        f = bm.faces.new((lo[i], lo[j], hi[j], hi[i]) if y1 > y0 else (lo[j], lo[i], hi[i], hi[j]))
        f.material_index = mat
        f.smooth = True


def _hardware(bm: bmesh.types.BMesh, cfg: dict, lod: int) -> None:
    """The weight cartridge on the bridge (both faces, two hex screws each) and an alloy
    port with an accent ring in each shoulder of the frame."""
    w = cfg["weights"]
    half_t = cfg["frame"]["thickness_m"] / 2
    hx, hz = w["plate_size_m"][0] / 2, w["plate_size_m"][1] / 2
    zc = w["plate_centre_z_m"]
    for side in (-1, 1):
        y0, y1 = side * (half_t - 0.0003), side * (half_t + w["plate_proud_m"])
        _rounded_rect(bm, (0.0, zc), hx, hz, y0, y1, METAL, 4 if lod == 0 else 2)
        if lod == 0:
            for dz in (-w["screw_offset_m"], w["screw_offset_m"]):
                bc.add_cylinder(bm, (0.0, y1 - side * 0.0002, zc + dz), (0.0, y1 + side * 0.0005, zc + dz),
                                w["screw_radius_m"], 6, CAP)
    outline = layout.outline(cfg, 720)
    for side in (-1, 1):
        i = min(range(len(outline)), key=lambda k: math.hypot(outline[k][0] - side * 0.2, outline[k][1] - w["port_z_m"]))
        p, n = _edge_frame(cfg, i, outline)
        segments = 16 if lod == 0 else 8
        # Set into the edge, never proud of it: the regulation width is measured over them.
        bc.add_cylinder(bm, p - n * 0.003, p - n * 0.00005, w["port_radius_m"] + w["port_ring_m"], segments, ACCENT)
        bc.add_cylinder(bm, p - n * 0.002, p, w["port_radius_m"], segments, METAL)


def _ellipse_ring(bm, z: float, ax: float, ay: float, segments: int, bump=None) -> list:
    ring = []
    for k in range(segments):
        a = 2 * math.pi * k / segments
        extra = bump(a, z) if bump else 0.0
        ring.append(bm.verts.new(((ax + extra) * math.cos(a), (ay + extra) * math.sin(a), z)))
    return ring


def _bridge_rings(bm, rings: list, mat: int, smooth: bool = True) -> None:
    for r0, r1 in zip(rings, rings[1:]):
        n = len(r0)
        for k in range(n):
            f = bm.faces.new((r0[k], r0[(k + 1) % n], r1[(k + 1) % n], r1[k]))
            f.material_index = mat
            f.smooth = smooth


def _grip(bm: bmesh.types.BMesh, cfg: dict, lod: int) -> None:
    """Overgrip, finishing tape and butt cap. At LOD0 the overgrip is a helix of
    overlapping bands: each turn rises step_m over the one below it."""
    g = cfg["grip"]
    ax, ay = g["semi_axes_m"]
    around, along = g["lod_segments"][lod]
    z0, z1, z2 = g["wrap_bottom_z_m"], g["wrap_top_z_m"], g["tape_top_z_m"]
    butt = cfg["frame"]["butt_z_m"]

    def helix(a: float, z: float) -> float:
        if lod != 0:
            return 0.0
        phase = (a / (2 * math.pi) + (z - z0) / g["pitch_m"]) % 1.0
        # A ramp up to the next band's edge, eased over the last tenth: the overlap step.
        return g["step_m"] * (phase if phase < 0.9 else (1.0 - phase) * 9.0)

    wrap = [_ellipse_ring(bm, z0 + (z1 - z0) * i / along, ax, ay, around, helix) for i in range(along + 1)]
    _bridge_rings(bm, wrap, GRIP)
    # Finishing tape: a band a little proud of the wrap, chamfered into it.
    tape_ax, tape_ay = ax + g["step_m"] + 0.0006, ay + g["step_m"] + 0.0006
    tape = [_ellipse_ring(bm, z1 - 0.002, ax, ay, around),
            _ellipse_ring(bm, z1, tape_ax, tape_ay, around),
            _ellipse_ring(bm, z2 - 0.0015, tape_ax, tape_ay, around),
            _ellipse_ring(bm, z2, tape_ax - 0.0012, tape_ay - 0.0012, around)]
    _bridge_rings(bm, tape, ACCENT)
    # Butt cap: flares out from the wrap, a rounded bottom, an accent ring at its top.
    cx, cy = g["cap_semi_axes_m"]
    cap_profile = [(z0 + 0.003, ax, ay, ACCENT), (z0 - 0.001, cx, cy, ACCENT), (z0 - 0.003, cx, cy, CAP),
                   (butt + 0.003, cx, cy, CAP), (butt + 0.0008, cx - 0.0015, cy - 0.0015, CAP),
                   (butt, cx - 0.005, cy - 0.005, CAP)]
    rings = [(_ellipse_ring(bm, z, rx, ry, around), mat) for z, rx, ry, mat in cap_profile]
    for (r0, _), (r1, mat) in zip(rings, rings[1:]):
        _bridge_rings(bm, [r0, r1], mat)
    bottom = bm.faces.new(list(reversed(rings[-1][0])))
    bottom.material_index = CAP


def _face_away_from(bm: bmesh.types.BMesh, centre_of) -> None:
    """Points every face of an open surface (the bumper, the grip's tubes) away from a
    reference point: Blender can't tell inside from outside on a surface that isn't
    closed. Flat ends (the butt) face down."""
    # New faces have no normal until it is computed: without this every test below read
    # zero and nothing was flipped, so the butt cap rendered inside out in Unreal.
    bm.normal_update()
    for f in bm.faces:
        c = f.calc_center_median()
        n = f.normal
        if abs(n.z) > 0.95 and abs(c.x) < 1e-6 and abs(c.y) < 1e-6:
            out = n.z < 0
        else:
            out = n.dot(c - centre_of(c)) >= 0
        if not out:
            f.normal_flip()
    bm.normal_update()


def build_lod(job: dict, cfg: dict, lod: int, mats: list, name: str) -> bpy.types.Object:
    body = _frame_body(cfg, lod)
    _drill(body, cfg, lod)
    _paint_body(body, cfg, mats)
    parts = []
    if lod < 2:
        bm = bmesh.new()
        _bumper(bm, cfg, lod)
        _face_away_from(bm, lambda c: Vector((0.0, 0.0, layout.widest(cfg)[1] - 0.04)))
        parts.append(bc.mesh_object("RacketBumper", bm, mats))
        bm = bmesh.new()
        _hardware(bm, cfg, lod)
        # Closed solids: Blender can orient them itself.
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        parts.append(bc.mesh_object("RacketHardware", bm, mats))
    bm = bmesh.new()
    _grip(bm, cfg, lod)
    _face_away_from(bm, lambda c: Vector((0.0, 0.0, c.z)))
    parts.append(bc.mesh_object("RacketGrip", bm, mats))
    bc.select_only([body] + parts)
    bpy.ops.object.join()
    body.name = name
    body.data.name = name
    return body


def build_racket(job: dict) -> tuple[list, list]:
    cfg = layout.load_config()
    mats = _materials(job)
    objects = [build_lod(job, cfg, lod, mats, job["id"] if lod == 0 else f"{job['id']}_LOD{lod}")
               for lod in range(1 + len(job.get("lod_outs", [])))]
    return objects[:1], objects[1:]


def inward_faces(obj: bpy.types.Object, cfg: dict) -> int:
    """Faces of the open surfaces that point into the racket: Unreal culls back faces, so
    each one is a hole in the shell (the butt cap was inside out once)."""
    names = [m.name if m else "" for m in obj.data.materials]
    centre = Vector((0.0, 0.0, layout.widest(cfg)[1] - 0.04))
    handle_top = cfg["grip"]["tape_top_z_m"] + 0.002
    count = 0
    for p in obj.data.polygons:
        name, c, n = names[p.material_index], p.center, p.normal
        if name == "MI_Racket_Bumper":
            count += n.dot(c - centre) < -1e-6
        elif name in ("MI_Racket_Grip", "MI_Racket_Cap", "MI_Racket_Accent") and c.z < handle_top:
            radial = Vector((c.x, c.y, 0.0))
            count += (n.z > 0.0) if radial.length < 0.01 and abs(n.z) > 0.95 else n.dot(radial) < -1e-6
    return count


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects, lods = build_racket(job)
    cfg = layout.load_config()
    bc.write_report(job, objects, {"lod_triangles": [bc.triangle_count(o) for o in lods],
                                   "holes": len(layout.hole_centres(cfg)),
                                   "inward_faces": [inward_faces(o, cfg) for o in objects + lods]})
    bc.export_fbx(objects, Path(job["out"]))
    for obj, out in zip(lods, job.get("lod_outs", [])):
        bc.export_fbx([obj], Path(out))


if __name__ == "__main__":
    main()
