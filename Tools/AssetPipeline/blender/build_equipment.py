"""Builds the ball and the placeholder racket (GDD 2: ball 0.065 diameter;
racket solid, perforated face, at most 0.455 long and 0.26 wide).

Job keys: id, builder ("ball" or "racket"), size, palette, palette_group,
budget_tris, out (FBX), report.

Orientation (Unreal axes): the ball is centred on the origin. The racket's grip
centre is on the origin, its head points up +Z and its hitting faces look along
+/-Y, so a hand socket can hold it without extra offsets.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402  (the bpy module must load before bmesh)
import bmesh  # noqa: E402

import bl_common as bc  # noqa: E402


def build_ball(job: dict) -> list:
    radius = job["size"]["diameter_m"] / 2.0
    felt = bc.material("MI_Ball_Felt", job["palette"]["ball"]["Felt"])
    bm = bmesh.new()
    # A UV sphere so the felt texture (with the seam that shows spin) maps cleanly.
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=16, radius=radius, calc_uvs=True)
    for face in bm.faces:
        face.smooth = True
    obj = bc.mesh_object(job["id"], bm, [felt])
    bc.decimate_to(obj, job["budget_tris"])
    return [obj]


def _head_outline(cx: float, cz: float, half_w: float, half_h: float, n: int = 72) -> list[tuple[float, float]]:
    # A superellipse: squarer shoulders than an ellipse, like a modern round-head racket.
    p = 2.4
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        c, s = math.cos(t), math.sin(t)
        x = half_w * math.copysign(abs(c) ** (2 / p), c)
        z = half_h * math.copysign(abs(s) ** (2 / p), s)
        pts.append((cx + x, cz + z))
    return pts


def _prism(bm: bmesh.types.BMesh, outline: list[tuple[float, float]], thickness: float,
           face_mat: int, rim_mat: int) -> None:
    front = [bm.verts.new((x, -thickness / 2, z)) for x, z in outline]
    back = [bm.verts.new((x, thickness / 2, z)) for x, z in outline]
    f = bm.faces.new(front)
    f.material_index = face_mat
    b = bm.faces.new(list(reversed(back)))
    b.material_index = face_mat
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        side = bm.faces.new((front[j], front[i], back[i], back[j]))
        side.material_index = rim_mat
        side.smooth = True
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])


def build_racket(job: dict) -> list:
    size = job["size"]
    pal = job["palette"][job["palette_group"]]
    group = "".join(p.capitalize() for p in job["palette_group"].split("_"))
    names = ["Face", "Frame", "Accent", "Grip"]
    mats = [bc.material(f"MI_{group}_{n}", pal[n]) for n in names]
    face_i, frame_i, accent_i, grip_i = range(4)
    length, width, thick = size["length_m"], size["width_m"], size["thickness_m"]

    # Grip centre at the origin: handle 0.15 long, the rest is throat and head.
    butt_z = -0.075
    tip_z = butt_z + length
    head_half_h = 0.135
    head_cz = tip_z - head_half_h
    head = bmesh.new()
    _prism(head, _head_outline(0.0, head_cz, width / 2, head_half_h), thick, face_i, frame_i)
    head_obj = bc.mesh_object(job["id"], head, mats)

    # Perforations: a staggered grid inside the head, kept clear of the rim.
    holes = bmesh.new()
    hole_r, pitch, margin = 0.0065, 0.024, 0.024
    row = 0
    z = head_cz - head_half_h + margin + 0.02
    while z < tip_z - margin:
        offset = (pitch / 2) if row % 2 else 0.0
        x = -width / 2 + margin + offset
        while x < width / 2 - margin:
            nx, nz = x / (width / 2 - margin), (z - head_cz) / (head_half_h - margin)
            if abs(nx) ** 2.4 + abs(nz) ** 2.4 < 1.0:
                bc.add_cylinder(holes, (x, -thick, z), (x, thick, z), hole_r, 10)
            x += pitch
        z += pitch * 0.87
        row += 1
    holes_obj = bc.mesh_object("Holes", holes, [])
    mod = head_obj.modifiers.new("Holes", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = holes_obj
    mod.solver = "EXACT"
    bc.apply_modifier(head_obj, mod.name)
    bpy.data.objects.remove(holes_obj)

    # Invented two-tone design: a diagonal accent band across both faces.
    for poly in head_obj.data.polygons:
        if poly.material_index != face_i:
            continue
        cx, _, cz = poly.center
        if 0.0 < (cz - head_cz + 0.03) + 0.55 * cx < 0.03:
            poly.material_index = accent_i

    # Throat (a bridge with an open heart) and handle.
    rest = bmesh.new()
    throat_top = head_cz - head_half_h + 0.03
    throat_bottom = butt_z + 0.15
    for side in (-1, 1):
        bc.add_cylinder(rest, (side * 0.016, 0, throat_bottom), (side * 0.07, 0, throat_top), thick * 0.45, 10, frame_i)
    bc.add_cylinder(rest, (0, 0, butt_z), (0, 0, throat_bottom + 0.01), 0.0175, 8, grip_i)
    bc.add_cylinder(rest, (0, 0, butt_z), (0, 0, butt_z + 0.01), thick / 2, 12, frame_i)
    rest_obj = bc.mesh_object("Throat", rest, mats)
    bc.select_only([head_obj, rest_obj])
    bpy.ops.object.join()
    for poly in head_obj.data.polygons:
        poly.use_smooth = poly.material_index in (frame_i, grip_i)
    bc.decimate_to(head_obj, job["budget_tris"])
    return [head_obj]


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects = build_ball(job) if job["builder"] == "ball" else build_racket(job)
    bc.write_report(job, objects)
    bc.export_fbx(objects, Path(job["out"]))


if __name__ == "__main__":
    main()
