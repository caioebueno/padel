"""Builds the court's steel structure and welded mesh (GDD 2, GDD 10).

Two parts, picked by the job's "part":
  frame  SM_CourtFrame  posts, corner L-posts, mesh panel frames, mid rails, base
                        plates, gussets, anchors and bracket bolts. Opaque, Nanite.
  mesh   SM_CourtMesh   one quad per mesh opening. The wires are drawn by the
                        WireMesh material (an analytic, box-filtered grid), so the
                        mesh stays sharp up close and turns into an even haze far
                        away instead of shimmering. Masked, two-sided, not Nanite.

Where everything goes comes from asset_lib/court_frame_layout.py (tested without
Blender); this script only turns it into geometry. Sections are modelled with
rounded corners so the edges catch the arena lights like real tube steel.
Job keys: id, part, spec, palette, budget_tris, out, report.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import bpy  # noqa: E402  (the bpy module must load before bmesh)
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import bl_common as bc  # noqa: E402
from asset_lib import court_frame_layout as layout  # noqa: E402

FRAME, STEEL, CAP = 0, 1, 2


# --- Profiles ---------------------------------------------------------------------

def fillet(points: list[tuple[float, float]], radius: float, segments: int) -> list[tuple[Vector, int | None]]:
    """Rounds every corner of a closed 2D outline (convex or concave).

    Returns (point, corner id) pairs; faces between two points of the same corner
    are smooth-shaded, the straight runs between corners stay flat.
    """
    if radius <= 0 or segments <= 0:
        return [(Vector(p), None) for p in points]
    out: list[tuple[Vector, int | None]] = []
    n = len(points)
    for i in range(n):
        p, a, b = Vector(points[i]), Vector(points[i - 1]), Vector(points[(i + 1) % n])
        d1, d2 = (a - p).normalized(), (b - p).normalized()
        half = d1.angle(d2) / 2
        r = min(radius, 0.45 * min((a - p).length, (b - p).length) * math.tan(half))
        t = r / math.tan(half)
        t1, t2 = p + d1 * t, p + d2 * t
        centre = p + (d1 + d2).normalized() * (r / math.sin(half))
        a1 = math.atan2(*(reversed(tuple(t1 - centre))))
        a2 = math.atan2(*(reversed(tuple(t2 - centre))))
        sweep = (a2 - a1 + math.pi) % (2 * math.pi) - math.pi  # shortest way round
        for k in range(segments + 1):
            ang = a1 + sweep * k / segments
            out.append((centre + r * Vector((math.cos(ang), math.sin(ang))), i))
    return out


def circle(radius: float, segments: int) -> list[tuple[Vector, int | None]]:
    return [(Vector((radius * math.cos(2 * math.pi * i / segments), radius * math.sin(2 * math.pi * i / segments))), 0)
            for i in range(segments)]


def rounded_rect(w: float, h: float, r: float, segments: int) -> list[tuple[Vector, int | None]]:
    return fillet([(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)], r, segments)


def hexagon(across_flats: float, chamfer: float) -> list[tuple[Vector, int | None]]:
    r = across_flats / math.sqrt(3)
    return fillet([(r * math.cos(math.pi / 3 * i), r * math.sin(math.pi / 3 * i)) for i in range(6)], chamfer, 2)


def offset_outline(points: list[tuple[float, float]], d: float) -> list[tuple[float, float]]:
    """Grows a counter-clockwise outline by d (each edge moves out, corners re-meet)."""
    n = len(points)
    lines = []
    for i in range(n):
        a, b = Vector(points[i]), Vector(points[(i + 1) % n])
        e = (b - a).normalized()
        normal = Vector((e.y, -e.x))
        lines.append((a + normal * d, e))
    out = []
    for i in range(n):
        (p1, e1), (p2, e2) = lines[i - 1], lines[i]
        cross = e1.x * e2.y - e1.y * e2.x
        t = ((p2 - p1).x * e2.y - (p2 - p1).y * e2.x) / cross
        out.append(tuple(p1 + e1 * t))
    return out


# --- Geometry ---------------------------------------------------------------------

def prism(bm, profile, origin, u, v, w, length: float, mat: int) -> None:
    """Extrudes a 2D profile (in the u, v plane at origin) along w, with caps.

    UVs are in metres (round the perimeter, and along the length), so the
    powder-coat normal map keeps its real scale on every piece.
    """
    origin, u, v, w = Vector(origin), Vector(u).normalized(), Vector(v).normalized(), Vector(w).normalized()
    pts = [p for p, _ in profile]
    ids = [c for _, c in profile]
    area = sum(a.x * b.y - b.x * a.y for a, b in zip(pts, pts[1:] + pts[:1]))
    if (area > 0) != (u.cross(v).dot(w) > 0):
        pts, ids = pts[::-1], ids[::-1]
    rings = [[bm.verts.new(origin + p.x * u + p.y * v + t * w) for p in pts] for t in (0.0, length)]
    uv_layer = bm.loops.layers.uv.verify()
    n = len(pts)
    run = 0.0
    for i in range(n):
        j = (i + 1) % n
        seg = (pts[j] - pts[i]).length
        face = bm.faces.new((rings[0][i], rings[0][j], rings[1][j], rings[1][i]))
        face.material_index = mat
        face.smooth = ids[i] is not None and ids[i] == ids[j]
        for loop, uv in zip(face.loops, ((run, 0.0), (run + seg, 0.0), (run + seg, length), (run, length))):
            loop[uv_layer].uv = uv
        run += seg
    for ring, flip in ((rings[0], True), (rings[1], False)):
        verts = list(reversed(ring)) if flip else ring
        face = bm.faces.new(verts)
        face.material_index = mat
        face.smooth = False
        for loop in face.loops:
            rel = loop.vert.co - origin
            loop[uv_layer].uv = (rel.dot(u), rel.dot(v))


def perpendicular(axis: Vector) -> Vector:
    helper = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
    return axis.cross(helper).normalized()


def tube(bm, start, end, width_axis, width: float, depth: float, radius: float, segments: int, mat: int) -> None:
    """Rectangular hollow section between two points; width along width_axis."""
    start, end = Vector(start), Vector(end)
    w = (end - start).normalized()
    u = Vector(width_axis).normalized()
    v = w.cross(u).normalized()
    prism(bm, rounded_rect(width, depth, radius, segments), start, u, v, w, (end - start).length, mat)


def post(bm, item: layout.Post, cfg: dict) -> None:
    p = cfg["post"]
    profile = fillet(item.outline, p["corner_radius_m"], p["corner_segments"])
    prism(bm, profile, (0, 0, item.z0), (1, 0, 0), (0, 1, 0), (0, 0, 1), item.z1 - item.z0, FRAME)
    cap = cfg["cap"]
    grown = offset_outline(item.outline, cap["overhang_m"])
    prism(bm, fillet(grown, p["corner_radius_m"] + cap["overhang_m"], p["corner_segments"]),
          (0, 0, item.z1), (1, 0, 0), (0, 1, 0), (0, 0, 1), cap["height_m"], CAP)


def panel_frame(bm, item: layout.Panel, cfg: dict) -> None:
    pf = cfg["panel_frame"]
    fw, fd, r, seg = pf["width_m"], pf["depth_m"], pf["corner_radius_m"], pf["corner_segments"]
    o, a, up, n = (Vector(x) for x in (item.origin, item.along, item.up, item.out))
    mid = n * (fd / 2)

    def at(s: float, z: float) -> Vector:
        return o + a * s + up * z + mid

    # Top and bottom run the full length; the uprights fit between them, like a welded frame.
    for z in (fw / 2, item.height - fw / 2):
        tube(bm, at(0, z), at(item.length, z), up, fw, fd, r, seg, FRAME)
    for s in (fw / 2, item.length - fw / 2):
        tube(bm, at(s, fw), at(s, item.height - fw), a, fw, fd, r, seg, FRAME)
    for z in item.mid_rails:
        tube(bm, at(fw, z), at(item.length - fw, z), up, fw, fd, r, seg, FRAME)


def mesh_openings(bm, item: layout.Panel, cfg: dict, cell: float, mat: int) -> int:
    """One quad per opening in the panel frame, UVs in wire cells from the frame's inner corner."""
    fw, fd = cfg["panel_frame"]["width_m"], cfg["panel_frame"]["depth_m"]
    o, a, up, n = (Vector(x) for x in (item.origin, item.along, item.up, item.out))
    cuts = [fw] + [c for z in item.mid_rails for c in (z - fw / 2, z + fw / 2)] + [item.height - fw]
    count = 0
    for z0, z1 in zip(cuts[0::2], cuts[1::2]):
        s0, s1 = fw, item.length - fw
        corners = [o + a * s + up * z + n * (fd / 2) for s, z in ((s0, z0), (s1, z0), (s1, z1), (s0, z1))]
        bc.add_quad(bm, [tuple(c) for c in corners], mat, cell)
        count += 1
    return count


def base_plate(bm, item: layout.Box) -> None:
    (x0, y0, z0), (x1, y1, z1) = item.lo, item.hi
    prism(bm, fillet([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], 0.006, 2), (0, 0, z0), (1, 0, 0), (0, 1, 0),
          (0, 0, 1), z1 - z0, FRAME)


def gusset(bm, item: layout.Gusset) -> None:
    profile = [(Vector(p), None) for p in ((0.0, 0.0), (item.length, 0.0), (item.length, 0.012), (0.0, item.height))]
    prism(bm, profile, item.root, item.out, (0, 0, 1), item.side, item.thickness, FRAME)


def fastener(bm, item: layout.Fastener, cfg: dict) -> None:
    b = cfg["bolts"]
    seg = b["round_segments"]
    axis = Vector(item.axis).normalized()
    u = perpendicular(axis)
    v = axis.cross(u)
    base = Vector(item.base)
    if item.kind == "anchor":
        washer_h, nut_h = 0.003, 0.013
        prism(bm, circle(b["anchor_washer_m"] / 2, seg), base, u, v, axis, washer_h, STEEL)
        prism(bm, hexagon(b["anchor_nut_m"], 0.0015), base + axis * washer_h, u, v, axis, nut_h, STEEL)
        # Threaded stud end standing proud of the nut.
        prism(bm, circle(b["anchor_stud_m"] / 2, seg), base + axis * (washer_h + nut_h), u, v, axis, 0.012, STEEL)
    else:
        washer_h, head_h = 0.002, 0.0064
        prism(bm, circle(b["panel_washer_m"] / 2, seg), base, u, v, axis, washer_h, STEEL)
        prism(bm, hexagon(b["panel_head_m"], 0.001), base + axis * washer_h, u, v, axis, head_h, STEEL)


def preview_wires(mat, fraction: float) -> None:
    """Cuts the wire grid out of the Blender material, so previews show the mesh
    as Unreal's WireMesh material draws it (Unreal ignores these nodes)."""
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    uv = nodes.new("ShaderNodeUVMap")
    split = nodes.new("ShaderNodeSeparateXYZ")
    links.new(uv.outputs["UV"], split.inputs["Vector"])
    wires = []
    for axis in ("X", "Y"):
        # Distance to the nearest whole cell, compared with the wire's half width.
        shifted = nodes.new("ShaderNodeMath")
        shifted.operation = "ADD"
        shifted.inputs[1].default_value = 0.5
        links.new(split.outputs[axis], shifted.inputs[0])
        wrapped = nodes.new("ShaderNodeMath")
        wrapped.operation = "FRACT"
        links.new(shifted.outputs[0], wrapped.inputs[0])
        centred = nodes.new("ShaderNodeMath")
        centred.operation = "SUBTRACT"
        centred.inputs[1].default_value = 0.5
        links.new(wrapped.outputs[0], centred.inputs[0])
        dist = nodes.new("ShaderNodeMath")
        dist.operation = "ABSOLUTE"
        links.new(centred.outputs[0], dist.inputs[0])
        on_wire = nodes.new("ShaderNodeMath")
        on_wire.operation = "LESS_THAN"
        on_wire.inputs[1].default_value = fraction / 2
        links.new(dist.outputs[0], on_wire.inputs[0])
        wires.append(on_wire)
    either = nodes.new("ShaderNodeMath")
    either.operation = "MAXIMUM"
    links.new(wires[0].outputs[0], either.inputs[0])
    links.new(wires[1].outputs[0], either.inputs[1])
    links.new(either.outputs[0], bsdf.inputs["Alpha"])


# --- Build ------------------------------------------------------------------------

def build(job: dict) -> tuple[list, dict]:
    spec, palette = job["spec"], job["palette"]["court"]
    cfg = layout.load_config()
    lay = layout.build(spec, cfg)
    bm = bmesh.new()
    extra = {"layout": lay.summary(), "post_face_offset_m": lay.post_face_offset}
    if job["part"] == "mesh":
        mesh_settings = palette["Mesh"]
        mat = bc.material("MI_Court_Mesh", mesh_settings)
        preview_wires(mat, mesh_settings["wire_m"] / mesh_settings["cell_m"])
        openings = sum(mesh_openings(bm, p, cfg, mesh_settings["cell_m"], 0) for p in lay.panels)
        extra["openings"] = openings
        obj = bc.mesh_object(job["id"], bm, [mat])
    else:
        mats = [bc.material(f"MI_Court_{n}", palette[n]) for n in ("Frame", "Steel", "Cap")]
        for item in lay.posts:
            post(bm, item, cfg)
        for item in lay.panels:
            panel_frame(bm, item, cfg)
        for item in lay.plates:
            base_plate(bm, item)
        for item in lay.gussets:
            gusset(bm, item)
        for item in lay.fasteners:
            fastener(bm, item, cfg)
        obj = bc.mesh_object(job["id"], bm, mats)
    bc.decimate_to(obj, job["budget_tris"])
    return [obj], extra


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects, extra = build(job)
    bc.write_report(job, objects, extra)
    bc.export_fbx(objects, Path(job["out"]))


if __name__ == "__main__":
    main()
