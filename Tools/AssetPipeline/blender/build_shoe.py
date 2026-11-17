"""Builds the players' padel shoe with its low sock (docs/decisions/0006-metahuman-players.md):
a court trainer with a thick gum-free sole, a wrap-up toe guard, a padded collar and a
white crew sock cuff above it. UE 5.8's MetaHuman wardrobe has no footwear, so the game
attaches this to each MetaHuman's feet (UPadelPlayerAnimComponent) and hides the toes.

Job keys: id, size, palette, palette_group, budget_tris, out, lod_outs, report.

Orientation (Unreal axes, metres): the origin is on the ground under the ankle joint, +X
points to the toe, +Z up, symmetric about the XZ plane so one mesh serves both feet.
size.ankle_to_ball_m is the authored distance from the ankle to the ball of the foot;
the game scales the shoe so it matches each MetaHuman's own foot.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402  (the bpy module must load before bmesh)
import bmesh  # noqa: E402

import bl_common as bc  # noqa: E402

SLOTS = ("Sole", "Upper", "Accent", "Sock")
SOLE, UPPER, ACCENT, SOCK = range(len(SLOTS))


def _materials(job: dict) -> list:
    pal = job["palette"][job["palette_group"]]
    group = "".join(p.capitalize() for p in job["palette_group"].split("_"))
    return [bc.material(f"MI_{group}_{name}", pal[name]) for name in SLOTS]


# Foot outline along X (heel -> toe): half width and the upper's height at each station.
# Metres, for a 28 cm shoe whose ankle sits 6 cm in front of the heel.
STATIONS = [
    # x,     half width, sole top, upper top
    (-0.060, 0.030, 0.028, 0.085),
    (-0.050, 0.038, 0.028, 0.092),
    (-0.030, 0.043, 0.027, 0.095),
    (0.000, 0.045, 0.026, 0.092),
    (0.040, 0.046, 0.024, 0.080),
    (0.080, 0.050, 0.022, 0.068),
    (0.120, 0.053, 0.020, 0.060),
    (0.155, 0.054, 0.019, 0.055),
    (0.185, 0.050, 0.018, 0.050),
    (0.205, 0.041, 0.018, 0.044),
    (0.218, 0.028, 0.018, 0.038),
    (0.224, 0.012, 0.018, 0.032),
]
SOLE_BOTTOM = 0.0
RING = 16  # vertices around each upper section


def _section(x: float, half_w: float, z0: float, z1: float) -> list[tuple[float, float, float]]:
    """A rounded cross-section: flat underside at z0, a dome up to z1."""
    pts = []
    for i in range(RING):
        a = 2 * math.pi * i / RING
        y = half_w * math.cos(a)
        s = math.sin(a)
        z = z0 + (z1 - z0) * (s if s > 0 else 0.0) ** 0.8
        pts.append((x, y, z))
    return pts


def _loft(bm, sections, mat, cap_start=True, cap_end=True) -> None:
    rings = [[bm.verts.new(p) for p in sec] for sec in sections]
    n = len(rings[0])
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            f = bm.faces.new((a[i], b[i], b[j], a[j]))
            f.material_index = mat
            f.smooth = True
    if cap_start:
        f = bm.faces.new(list(reversed(rings[0])))
        f.material_index = mat
    if cap_end:
        f = bm.faces.new(rings[-1])
        f.material_index = mat


def build_shoe(job: dict):
    mats = _materials(job)
    bm = bmesh.new()

    # Sole: a slab following the outline, slightly wider than the upper, rounded at both ends.
    sole = []
    for x, hw, top, _ in STATIONS:
        w = hw + 0.004
        sole.append([(x, w * math.cos(2 * math.pi * i / RING), SOLE_BOTTOM if math.sin(2 * math.pi * i / RING) <= 0 else top)
                     for i in range(RING)])
    _loft(bm, sole, SOLE)

    # Upper: sits on the sole, domed, lower and narrower towards the toe.
    upper = [_section(x, hw, top - 0.002, ut) for x, hw, top, ut in STATIONS]
    _loft(bm, upper, UPPER)

    # Toe guard and heel counter in the accent colour: thin shells over the ends.
    toe = [_section(x, hw + 0.002, top, top + 0.018) for x, hw, top, _ in STATIONS[-5:]]
    _loft(bm, toe, ACCENT, cap_start=False, cap_end=False)
    heel = [_section(x - 0.002, hw + 0.002, top, ut - 0.035) for x, hw, top, ut in STATIONS[:3]]
    _loft(bm, heel, ACCENT, cap_start=False, cap_end=False)

    # Padded collar and the sock: a white cuff from inside the collar to 6 cm above it.
    cx, r = -0.008, 0.038
    sock = []
    for k, z in enumerate((0.075, 0.098, 0.125, 0.150)):
        rr = r - 0.003 * k
        sock.append([(cx + rr * 0.95 * math.cos(2 * math.pi * i / RING), rr * math.sin(2 * math.pi * i / RING), z)
                     for i in range(RING)])
    _loft(bm, sock, SOCK, cap_start=False, cap_end=False)

    # Box-projected UVs in metres, like bl_common.add_box: the materials are plain colours.
    uv_layer = bm.loops.layers.uv.verify()
    for face in bm.faces:
        n = face.normal
        axis = max(range(3), key=lambda i: abs(n[i]))
        u_axis, v_axis = [(1, 2), (0, 2), (0, 1)][axis]
        for loop in face.loops:
            loop[uv_layer].uv = (loop.vert.co[u_axis], loop.vert.co[v_axis])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

    obj = bc.mesh_object(job["id"], bm, mats)
    lods = []
    for i, _ in enumerate(job.get("lod_outs", [])):
        lod = obj.copy()
        lod.data = obj.data.copy()
        lod.name = f"{job['id']}_LOD{i + 1}"
        bpy.context.scene.collection.objects.link(lod)
        bc.decimate_to(lod, max(60, int(bc.triangle_count(obj) * (0.4 if i == 0 else 0.15))))
        lods.append(lod)
    return [obj], lods


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects, lods = build_shoe(job)
    bc.write_report(job, objects, {"lod_triangles": [bc.triangle_count(o) for o in lods]})
    bc.export_fbx(objects, Path(job["out"]))
    for obj, out in zip(lods, job.get("lod_outs", [])):
        bc.export_fbx([obj], Path(out))


if __name__ == "__main__":
    main()
