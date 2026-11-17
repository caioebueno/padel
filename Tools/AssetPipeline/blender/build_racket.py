"""Builds the match racket, the invented Ostrel Keystone (docs/decisions/0008-racket.md):
a pro-style diamond power racket with a carbon face, a raised sand finish, a drilled
hole pattern, one open triangular heart, a protector band with an alloy weight slot at
the tip, a lettered frame, an overgrip-wrapped handle with a ribbed collar, and a wrist
strap. Layout from asset_lib/racket_layout.py
and config/racket.json; colours from palette.json 'racket'.

Job keys: id, size, palette, palette_group, budget_tris, lods, out, lod_outs, report.

Orientation (Unreal axes, metres): grip centre on the origin, head up +Z, the faces look
along -Y and +Y, X across the face, so a hand socket holds it without extra offsets.

LODs are built, not decimated, so the UVs stay exact: LOD0 for close-ups and replays
(bevelled edges, every hole drilled, the overgrip's helix, the collar's ribs, the
hardware), LOD1 for the near player, LOD2 for the broadcast camera (no holes:
T_Racket_Masks.A draws them; no strap).

UV0: the face square of racket_layout.face_uv on the faces; the edge unrolled
(racket_layout.edge_u, V across the thickness) on the frame; the wrap unrolled (U
round, V along) on the overgrip.
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
SLOTS = ("Face", "Frame", "Core", "Bumper", "Metal", "Accent", "Grip", "Cap", "Strap")
FACE, FRAME, CORE, BUMPER, METAL, ACCENT, GRIP, CAP, STRAP = range(len(SLOTS))


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
    drilled holes show the foam core; everything else is frame, which takes the edge
    unrolled (U round the outline from the neck, V across the thickness) for its
    lettering."""
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
    param = layout.edge_param(cfg)
    edge_u: dict[int, float] = {}
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
        if f.material_index == FRAME:
            us = []
            for loop in f.loops:
                i = loop.vert.index
                if i not in edge_u:
                    edge_u[i] = layout.edge_u(cfg, loop.vert.co.x, loop.vert.co.z, param)
                us.append(edge_u[i])
            # A face across the seam (inside the grip) takes the far side's U past 1.
            if max(us) - min(us) > 0.5:
                us = [u + 1.0 if u < 0.5 else u for u in us]
            for loop, u in zip(f.loops, us):
                v = (loop.vert.co.y + half_t) / (2 * half_t)
                loop[uv].uv = (u, 1.0 - v)
            continue
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


def _top_edge_z(cfg: dict, x: float) -> float:
    """Height of the finished edge at x across the top of the head."""
    right = layout.outline_half(cfg)
    for a, b in zip(right, right[1:]):
        if (a[0] - abs(x)) * (b[0] - abs(x)) <= 0 and a[0] != b[0]:
            return a[1] + (b[1] - a[1]) * (abs(x) - a[0]) / (b[0] - a[0])
    return layout.tip_z(cfg)


def _hardware(bm: bmesh.types.BMesh, cfg: dict, lod: int) -> None:
    """The weight slot at the tip: a rounded alloy plate across the edge, following its
    curve, set a fraction of a millimetre into the protector, with two screws."""
    w = cfg["weights"]
    proud = cfg["bumper"]["proud_m"]
    hx, hy = w["slot_size_m"][0] / 2, w["slot_size_m"][1] / 2
    r = w["slot_corner_m"]
    steps = 4 if lod == 0 else 2
    pts = []
    for cx, cy, start in ((hx - r, hy - r, 0), (-(hx - r), hy - r, 90), (-(hx - r), -(hy - r), 180), (hx - r, -(hy - r), 270)):
        for k in range(steps + 1):
            a = math.radians(start + 90 * k / steps)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    top = [bm.verts.new((x, y, _top_edge_z(cfg, x) + proud * 0.6)) for x, y in pts]
    low = [bm.verts.new((x, y, _top_edge_z(cfg, x) - w["slot_depth_m"])) for x, y in pts]
    n = len(pts)
    f = bm.faces.new(top)
    f.material_index = METAL
    f = bm.faces.new(list(reversed(low)))
    f.material_index = METAL
    for i in range(n):
        j = (i + 1) % n
        f = bm.faces.new((low[i], low[j], top[j], top[i]))
        f.material_index = METAL
        f.smooth = True
    if lod == 0:
        for dx in (-w["screw_offset_m"], w["screw_offset_m"]):
            z = _top_edge_z(cfg, dx) + proud * 0.6
            bc.add_cylinder(bm, (dx, 0.0, z - 0.0003), (dx, 0.0, z + 0.0004), w["screw_radius_m"], 6, CAP)


def _strap(bm: bmesh.types.BMesh, cfg: dict, lod: int) -> None:
    """The wrist strap: a flat cord (width through Y) from the butt cap down through a
    slider into a narrow loop and back, swept along a closed path in the X-Z plane, so it
    is a closed solid. Its two ends meet inside the cap."""
    st = cfg["strap"]
    steps = st["lod_steps"][lod]
    if steps <= 0:
        return
    butt = cfg["frame"]["butt_z_m"]
    gap = st["thickness_m"] * 1.2
    z_slider = butt - st["slider_z_m"]
    bottom = butt - st["loop_length_m"]
    half = st["loop_width_m"] / 2
    # Right half of the loop below the slider: out, down and round the bottom.
    side = []
    for k in range(steps + 1):
        t = k / steps
        z = z_slider + (bottom + half - z_slider) * t
        side.append((gap / 2 + (half - gap / 2) * math.sin(math.pi / 2 * min(1.0, t * 1.6)), z))
    arc = [(half * math.cos(a), bottom + half + half * math.sin(a))
           for a in (-math.pi * k / (steps // 2 + 2) for k in range(1, steps // 2 + 2))]
    right = [(gap / 2, butt + 0.004), (gap / 2, z_slider)] + side + arc
    left = [(-x, z) for x, z in reversed(right[:-len(arc)])]
    path = right + left
    w2, t2 = st["width_m"] / 2, st["thickness_m"] / 2
    rings = []
    n = len(path)
    for i, (x, z) in enumerate(path):
        a, b = path[i - 1], path[(i + 1) % n]
        tx, tz = b[0] - a[0], b[1] - a[1]
        length = math.hypot(tx, tz) or 1.0
        nx, nz = tz / length, -tx / length
        rings.append([bm.verts.new((x + nx * dn, dy, z + nz * dn)) for dn, dy in ((-t2, -w2), (t2, -w2), (t2, w2), (-t2, w2))])
    for i in range(n):
        r0, r1 = rings[i], rings[(i + 1) % n]
        for k in range(4):
            f = bm.faces.new((r0[k], r0[(k + 1) % 4], r1[(k + 1) % 4], r1[k]))
            f.material_index = STRAP
            f.smooth = False
    if lod == 0:
        bc.add_cylinder(bm, (0.0, 0.0, z_slider - 0.006), (0.0, 0.0, z_slider + 0.006), 0.0055, 10, STRAP)


def _ellipse_ring(bm, z: float, ax: float, ay: float, segments: int, bump=None) -> list:
    ring = []
    for k in range(segments):
        a = 2 * math.pi * k / segments
        extra = bump(a, z) if bump else 0.0
        ring.append(bm.verts.new(((ax + extra) * math.cos(a), (ay + extra) * math.sin(a), z)))
    return ring


def _bridge_rings(bm, rings: list, mat: int, smooth: bool = True, vs: list | None = None) -> None:
    """Quads between consecutive rings. With vs (one V per ring) the faces get UVs: U
    round the ring (0..1, the last quad ending at 1), V as given."""
    # Named like the body's layer, so the join keeps one UV0 (a bare verify() on a new
    # BMesh makes a generic float2 attribute instead).
    uv = (bm.loops.layers.uv.get("UVMap") or bm.loops.layers.uv.new("UVMap")) if vs is not None else None
    for i, (r0, r1) in enumerate(zip(rings, rings[1:])):
        n = len(r0)
        for k in range(n):
            f = bm.faces.new((r0[k], r0[(k + 1) % n], r1[(k + 1) % n], r1[k]))
            f.material_index = mat
            f.smooth = smooth
            if uv is not None:
                for loop, (du, dv) in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
                    loop[uv].uv = ((k + du) / n, vs[i + dv])


def _grip(bm: bmesh.types.BMesh, cfg: dict, lod: int) -> None:
    """Overgrip, ribbed collar and butt cap. At LOD0 the overgrip is a helix of
    overlapping bands (each turn rises step_m over the one below it) and the collar has
    its ribs. The overgrip carries the unrolled UVs its dots are printed in."""
    g = cfg["grip"]
    ax, ay = g["semi_axes_m"]
    around, along = g["lod_segments"][lod]
    z0, z1 = g["wrap_bottom_z_m"], g["wrap_top_z_m"]
    butt = cfg["frame"]["butt_z_m"]

    def helix(a: float, z: float) -> float:
        if lod != 0:
            return 0.0
        phase = (a / (2 * math.pi) + (z - z0) / g["pitch_m"]) % 1.0
        # A ramp up to the next band's edge, eased over the last tenth: the overlap step.
        return g["step_m"] * (phase if phase < 0.9 else (1.0 - phase) * 9.0)

    zs = [z0 + (z1 - z0) * i / along for i in range(along + 1)]
    wrap = [_ellipse_ring(bm, z, ax, ay, around, helix) for z in zs]
    _bridge_rings(bm, wrap, GRIP, vs=[1.0 - (z - z0) / (z1 - z0) for z in zs])
    # Collar: a moulded sleeve over the top of the wrap and the bottom of the throat,
    # ribbed round at LOD0.
    cax, cay = g["collar_semi_axes_m"]
    top = g["collar_top_z_m"]
    profile = [(z1 - 0.003, ax, ay), (z1, cax, cay)]
    ribs = g["collar_ribs"] if lod == 0 else 0
    if ribs:
        pitch = (top - 0.002 - z1) / ribs
        for k in range(ribs):
            zb = z1 + k * pitch
            profile += [(zb + pitch * 0.3, cax + g["collar_rib_m"], cay + g["collar_rib_m"]),
                        (zb + pitch * 0.7, cax + g["collar_rib_m"], cay + g["collar_rib_m"]),
                        (zb + pitch, cax, cay)]
    profile += [(top - 0.002, cax, cay), (top, cax - 0.0015, cay - 0.0015)]
    collar = [_ellipse_ring(bm, z, rx, ry, around) for z, rx, ry in profile]
    _bridge_rings(bm, collar, CAP)
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
    bm = bmesh.new()
    _strap(bm, cfg, lod)
    if bm.faces:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        parts.append(bc.mesh_object("RacketStrap", bm, mats))
    else:
        bm.free()
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
    handle_top = cfg["grip"]["collar_top_z_m"] + 0.002
    count = 0
    for p in obj.data.polygons:
        name, c, n = names[p.material_index], p.center, p.normal
        if name == "MI_Racket_Bumper":
            count += n.dot(c - centre) < -1e-6
        elif name in ("MI_Racket_Grip", "MI_Racket_Cap", "MI_Racket_Accent") and cfg["frame"]["butt_z_m"] - 1e-4 < c.z < handle_top:
            radial = Vector((c.x, c.y, 0.0))
            count += (n.z > 0.0) if radial.length < 0.01 and abs(n.z) > 0.95 else n.dot(radial) < -1e-6
    return count


def racket_bounds(obj: bpy.types.Object) -> tuple[list[float], list[float]]:
    """Bounds of everything but the strap: the regulation size is the racket's."""
    names = [m.name if m else "" for m in obj.data.materials]
    used = {v for p in obj.data.polygons if names[p.material_index] != "MI_Racket_Strap" for v in p.vertices}
    cos = [obj.data.vertices[i].co for i in used]
    return [min(c[k] for c in cos) for k in range(3)], [max(c[k] for c in cos) for k in range(3)]


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects, lods = build_racket(job)
    cfg = layout.load_config()
    bc.write_report(job, objects, {"lod_triangles": [bc.triangle_count(o) for o in lods],
                                   "holes": len(layout.hole_centres(cfg)),
                                   "inward_faces": [inward_faces(o, cfg) for o in objects + lods],
                                   "racket_bounds_m": [racket_bounds(o) for o in objects + lods]})
    bc.export_fbx(objects, Path(job["out"]))
    for obj, out in zip(lods, job.get("lod_outs", [])):
        bc.export_fbx([obj], Path(out))


if __name__ == "__main__":
    main()
