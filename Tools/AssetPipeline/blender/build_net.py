"""Builds the net (GDD 2: 0.88 m at the centre, 0.92 m at the posts) as SM_CourtNet.

Run by pipeline.py. Job keys: id, spec (CourtSpec.h constants in metres), palette,
budget_tris, net (the asset's "net" block from assets.json: build sizes), out, report.

What a real padel net is made of, and what this builds:

- A knotted square mesh of dark polyethylene cord, hung from a steel cable. The
  cloth is a two-sided card cut out by T_NetGrid_Mask (one UV tile = one cell). It is
  a dense grid, not one quad, because the net materials move its vertices when the
  ball hits it (MPC_NetReaction, see Source/Padel/Public/Presentation/Model/PadelNetReaction.h).
  Cell rows follow the sag at the top and the flat bottom cord, the way a net hangs.
- The white headband tape folded over the cable, with a rounded top.
- A dark border tape along the bottom and white side bands where the net is laced
  to the posts, with the lacing cord.
- The steel cable showing between the tape and each post.
- Two square steel posts on bolted base plates with caps, a cable guide on each,
  a ratchet winch with a crank on one post and an eye bolt with a turnbuckle on the other.

Layout (Unreal axes, metres): net in the plane X = 0, across Y, Z up, ground at Z = 0.
Posts sit where CourtLayout's NetPost colliders are, so the art and the collision agree.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402,F401  (the bpy module must load before bmesh)
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import bl_common as bc  # noqa: E402

SLOTS = ["Net", "NetTape", "NetBorder", "NetCable", "NetPost", "NetHardware", "NetCord"]


def net_height_at(spec: dict, y: float) -> float:
    """Mirror of Padel::Court::NetHeightAt (CourtSpec.h)."""
    t = min(max(abs(y) / spec["HalfWidth"], 0.0), 1.0)
    return spec["NetHeightCentre"] + (spec["NetHeightPosts"] - spec["NetHeightCentre"]) * t * t


class NetBuilder:
    def __init__(self, job: dict) -> None:
        self.spec = job["spec"]
        self.n = job["net"]
        pal = job["palette"]["court"]
        self.mats = [bc.material(f"MI_Court_{name}", pal[name]) for name in SLOTS]
        self.idx = {name: i for i, name in enumerate(SLOTS)}
        self.cell = pal["Net"].get("tiling_m", 0.045)
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.verify()
        hw = self.spec["HalfWidth"]
        post = self.n["post_size_m"]
        # Post centres match CourtLayout's NetPost colliders.
        self.post_y = hw - post / 2
        self.post_inner = hw - post
        # The cloth ends at the side bands, which stop short of the posts for the lacing.
        self.span = self.post_inner - self.n["lacing_gap_m"]

    # --- small helpers ------------------------------------------------------------

    def face(self, coords, uvs, slot: str, smooth: bool = True) -> None:
        verts = [self.bm.verts.new(c) for c in coords]
        f = self.bm.faces.new(verts)
        f.material_index = self.idx[slot]
        f.smooth = smooth
        for loop, uv in zip(f.loops, uvs):
            loop[self.uv].uv = uv

    def grid(self, rows: list[list[tuple]], uvs: list[list[tuple]], slot: str, smooth: bool = True) -> None:
        """Quads between consecutive rows of points (each row the same length)."""
        verts = [[self.bm.verts.new(p) for p in row] for row in rows]
        for r in range(len(rows) - 1):
            for c in range(len(rows[r]) - 1):
                f = self.bm.faces.new((verts[r][c], verts[r][c + 1], verts[r + 1][c + 1], verts[r + 1][c]))
                f.material_index = self.idx[slot]
                f.smooth = smooth
                corner = ((r, c), (r, c + 1), (r + 1, c + 1), (r + 1, c))
                for loop, (rr, cc) in zip(f.loops, corner):
                    loop[self.uv].uv = uvs[rr][cc]

    def box(self, centre, size, slot: str) -> None:
        bc.add_box(self.bm, centre, size, self.idx[slot])

    def cylinder(self, start, end, radius: float, segments: int, slot: str) -> None:
        bc.add_cylinder(self.bm, start, end, radius, segments, self.idx[slot])

    def columns(self, y0: float, y1: float, step: float) -> list[float]:
        count = max(1, round((y1 - y0) / step))
        return [y0 + (y1 - y0) * i / count for i in range(count + 1)]

    def hang(self, y: float, z: float) -> float:
        """A few millimetres of slack waviness, so the cloth isn't a perfect sheet.
        Deterministic (no RNG) and zero at the posts, the cable and the ground."""
        span = self.span
        edge = math.sin(math.pi * min(max((y + span) / (2 * span), 0.0), 1.0))
        top = net_height_at(self.spec, y) - self.n["tape_height_m"]
        vertical = math.sin(math.pi * min(max(z / top, 0.0), 1.0))
        wave = math.sin(y * 2.1 + 0.4) * 0.6 + math.sin(y * 5.3 + 1.7) * 0.3 + math.sin(z * 9.0 + y) * 0.1
        return self.n["slack_m"] * edge * vertical * wave

    # --- parts --------------------------------------------------------------------

    def cloth(self) -> None:
        n, cell = self.n, self.cell
        bottom = n["border_top_m"]
        ys = self.columns(-self.span + n["side_band_m"], self.span - n["side_band_m"], n["column_step_m"])
        # Rows of cells: as many as fit at the centre; they stretch a little towards the posts.
        tape_bottom = [net_height_at(self.spec, y) - n["tape_height_m"] for y in ys]
        cell_rows = max(1, round((min(tape_bottom) - bottom) / cell))
        grid_rows = max(2, round((min(tape_bottom) - bottom) / n["row_step_m"]))
        rows, uvs = [], []
        for r in range(grid_rows + 1):
            t = r / grid_rows
            row, uv_row = [], []
            for y, top in zip(ys, tape_bottom):
                z = bottom + (top - bottom) * t
                row.append((self.hang(y, z), y, z))
                # One tile per cell: u along the net, v counts cells up from the bottom cord.
                uv_row.append(((y + self.span) / cell, t * cell_rows))
            rows.append(row)
            uvs.append(uv_row)
        self.grid(rows, uvs, "Net")

    def tape(self) -> None:
        """The headband folded over the cable: front, rounded top, back. Same columns
        as the cloth so the hit reaction bends it smoothly."""
        n = self.n
        half_t = n["tape_thickness_m"] / 2
        h = n["tape_height_m"]
        ys = self.columns(-self.span, self.span, n["column_step_m"])
        # Profile around the fold, as (x, dz below the top) from the front hem to the back hem.
        profile = [(half_t, h)]
        arc = 6
        for i in range(arc + 1):
            a = math.pi * i / arc
            profile.append((half_t * math.cos(a), half_t - half_t * math.sin(a)))
        profile.append((-half_t, h))
        rows, uvs = [], []
        dist = 0.0
        for i, (x, dz) in enumerate(profile):
            if i:
                px, pdz = profile[i - 1]
                dist += math.hypot(x - px, dz - pdz)
            rows.append([(x, y, net_height_at(self.spec, y) - dz) for y in ys])
            uvs.append([((y + self.span), dist) for y in ys])
        self.grid(rows, uvs, "NetTape")
        # Close the tape ends against the side bands.
        for s in (-1, 1):
            y = s * self.span
            top = net_height_at(self.spec, y)
            self.box((0, y - s * 0.001, top - h / 2), (n["tape_thickness_m"], 0.002, h), "NetTape")

    def bands(self) -> None:
        """Dark border tape along the bottom, white side bands at the posts."""
        n = self.n
        t = n["band_thickness_m"]
        ys = self.columns(-self.span, self.span, n["column_step_m"])
        z0, z1 = n["border_bottom_m"], n["border_top_m"]
        for x in (t / 2, -t / 2):
            rows = [[(x, y, z0) for y in ys], [(x, y, z1) for y in ys]]
            if x < 0:
                rows.reverse()
            self.grid(rows, [[(y, 0.0) for y in ys], [(y, z1 - z0) for y in ys]], "NetBorder")
        self.box((0, 0, z0 + 0.001), (t, 2 * self.span, 0.002), "NetBorder")
        for s in (-1, 1):
            y_out = s * self.span
            y_in = s * (self.span - n["side_band_m"])
            top = net_height_at(self.spec, y_out) - n["tape_height_m"]
            self.box((0, (y_out + y_in) / 2, (z1 + top) / 2), (t, n["side_band_m"], top - z1), "NetTape")

    def lacing(self) -> None:
        """Cord laced from each side band round the post, every lacing_step_m."""
        n = self.n
        r = n["cord_radius_m"]
        half_post = n["post_size_m"] / 2
        for s in (-1, 1):
            band = s * self.span
            post_far = s * (self.post_y + half_post * 0.6)
            top = net_height_at(self.spec, band) - n["tape_height_m"]
            z = n["border_top_m"] + 0.03
            while z < top - 0.02:
                for x in (half_post + 0.002, -half_post - 0.002):
                    self.cylinder((0, band, z), (x, s * self.post_inner, z + 0.012), r, 5, "NetCord")
                    self.cylinder((x, s * self.post_inner, z + 0.012), (x * 0.7, post_far, z + 0.02), r, 5, "NetCord")
                z += n["lacing_step_m"]

    def cable(self) -> None:
        """The steel cable between the tape ends and the guides on the posts."""
        n = self.n
        r = n["cable_radius_m"]
        for s in (-1, 1):
            y0 = s * (self.span - 0.01)
            y1 = s * self.post_inner
            z0 = net_height_at(self.spec, y0) - n["tape_thickness_m"] / 2
            z1 = n["cable_guide_height_m"]
            self.cylinder((0, y0, z0), (0, y1 + s * 0.012, z1), r, 8, "NetCable")
            # Guide bushing where the cable enters the post.
            self.cylinder((0, y1 - s * 0.002, z1), (0, y1 + s * 0.014, z1), r * 2.6, 10, "NetHardware")

    def rounded_tube(self, y: float, size: float, z0: float, z1: float, corner: float, slot: str) -> None:
        """A square tube with rounded corners (the extruded profile real posts have)."""
        ring = []
        half = size / 2 - corner
        for cx, cy, a0 in ((half, half, 0.0), (-half, half, 90.0), (-half, -half, 180.0), (half, -half, 270.0)):
            for i in range(4):
                a = math.radians(a0 + 90.0 * i / 3)
                ring.append((cx + corner * math.cos(a), y + cy + corner * math.sin(a)))
        bottom = [self.bm.verts.new((x, yy, z0)) for x, yy in ring]
        top = [self.bm.verts.new((x, yy, z1)) for x, yy in ring]
        perimeter = 0.0
        for i in range(len(ring)):
            j = (i + 1) % len(ring)
            f = self.bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
            f.material_index = self.idx[slot]
            f.smooth = True
            step = math.dist(ring[i], ring[j])
            for loop, (u, v) in zip(f.loops, ((perimeter, z0), (perimeter + step, z0), (perimeter + step, z1), (perimeter, z1))):
                loop[self.uv].uv = (u, v)
            perimeter += step
        cap = self.bm.faces.new(top)
        cap.material_index = self.idx[slot]
        bottom_cap = self.bm.faces.new(list(reversed(bottom)))
        bottom_cap.material_index = self.idx[slot]
        for f in (cap, bottom_cap):
            for loop in f.loops:
                loop[self.uv].uv = (loop.vert.co.x, loop.vert.co.y)

    def hex_bolt(self, x: float, y: float, z: float) -> None:
        n = self.n
        self.cylinder((x, y, z), (x, y, z + 0.0025), n["bolt_head_m"] * 0.62, 12, "NetHardware")  # washer
        self.cylinder((x, y, z + 0.0025), (x, y, z + 0.0025 + n["bolt_height_m"]), n["bolt_head_m"] / math.sqrt(3), 6,
                      "NetHardware")

    def posts(self) -> None:
        n = self.n
        size, height = n["post_size_m"], self.spec["NetPostHeight"]
        plate, plate_t = n["base_plate_m"], n["base_plate_thickness_m"]
        cap_t = n["cap_height_m"]
        for s in (-1, 1):
            y = s * self.post_y
            self.box((0, y, plate_t / 2), (plate, plate, plate_t), "NetPost")
            for dx in (-1, 1):
                for dy in (-1, 1):
                    self.hex_bolt(dx * plate * 0.36, y + dy * plate * 0.36, plate_t)
            self.rounded_tube(y, size, plate_t, height - cap_t, n["post_corner_m"], "NetPost")
            # Plastic cap, a touch wider than the tube.
            self.rounded_tube(y, size + 0.006, height - cap_t, height, n["post_corner_m"] + 0.003, "NetBorder")
        self.winch(+1)
        self.anchor(-1)

    def winch(self, s: int) -> None:
        """Ratchet winch on the outside face of the +Y post: housing, drum, pawl and crank."""
        n = self.n
        face = s * (self.post_y + n["post_size_m"] / 2)
        z = n["winch_height_m"]
        self.box((0, face + s * 0.004, z), (0.07, 0.008, 0.16), "NetHardware")  # mounting plate
        for dz in (-0.06, 0.06):
            self.hex_bolt_side(face + s * 0.008, z + dz, s)
        # Two cheeks either side of the drum.
        for x in (-0.028, 0.028):
            self.box((x, face + s * 0.03, z), (0.005, 0.045, 0.075), "NetHardware")
        self.cylinder((-0.026, face + s * 0.03, z), (0.026, face + s * 0.03, z), 0.016, 14, "NetCable")  # wound cable
        self.cylinder((-0.035, face + s * 0.03, z), (0.035, face + s * 0.03, z), 0.006, 10, "NetHardware")  # axle
        self.cylinder((0.029, face + s * 0.03, z), (0.033, face + s * 0.03, z), 0.03, 16, "NetHardware")  # ratchet wheel
        self.box((0.031, face + s * 0.03, z + 0.034), (0.004, 0.012, 0.022), "NetHardware")  # pawl
        # Crank: arm off the axle, then a knob.
        arm_end = (0.045, face + s * 0.03, z - 0.09)
        self.cylinder((0.04, face + s * 0.03, z), arm_end, 0.006, 8, "NetHardware")
        self.cylinder(arm_end, (0.085, arm_end[1], arm_end[2]), 0.011, 12, "NetBorder")
        # Cable from the guide down the post into the drum.
        self.cylinder((0, s * self.post_y, n["cable_guide_height_m"]), (0, face + s * 0.03, z + 0.016),
                      n["cable_radius_m"], 6, "NetCable")

    def hex_bolt_side(self, y: float, z: float, s: int) -> None:
        r = self.n["bolt_head_m"] / math.sqrt(3)
        self.cylinder((0, y, z), (0, y + s * self.n["bolt_height_m"] * 0.7, z), r * 0.8, 6, "NetHardware")

    def anchor(self, s: int) -> None:
        """The fixed end: an eye bolt through the post with a small turnbuckle."""
        n = self.n
        face = s * (self.post_y + n["post_size_m"] / 2)
        z = n["cable_guide_height_m"]
        self.cylinder((0, face, z), (0, face + s * 0.025, z), 0.007, 6, "NetHardware")  # nut
        y = face + s * 0.035
        self.cylinder((0, y - s * 0.01, z), (0, y + s * 0.01, z), 0.009, 12, "NetHardware")  # eye
        self.cylinder((0, y + s * 0.01, z), (0, y + s * 0.07, z), 0.0055, 8, "NetHardware")  # turnbuckle body
        for dy in (0.012, 0.068):
            self.cylinder((0, y + s * (dy - 0.003), z), (0, y + s * (dy + 0.003), z), 0.008, 6, "NetHardware")

    def build(self) -> bmesh.types.BMesh:
        self.cloth()
        self.tape()
        self.bands()
        self.lacing()
        self.cable()
        self.posts()
        return self.bm


def build(job: dict) -> list:
    builder = NetBuilder(job)
    bm = builder.build()
    used = sorted({f.material_index for f in bm.faces})
    remap = {old: new for new, old in enumerate(used)}
    for f in bm.faces:
        f.material_index = remap[f.material_index]
    obj = bc.mesh_object(job["id"], bm, [builder.mats[i] for i in used])
    # The cloth is a deliberate grid for the hit reaction; decimating it would break
    # that, so the budget is a hard check in the tests rather than a decimation target.
    return [obj]


def tape_tops(obj) -> dict:
    """Highest tape point at the centre and next to a post, for the tests."""
    mesh = obj.data
    slot = next(i for i, s in enumerate(obj.material_slots) if s.material and s.material.name == "MI_Court_NetTape")
    points = [mesh.vertices[v].co for p in mesh.polygons if p.material_index == slot for v in p.vertices]
    centre = max(p.z for p in points if abs(p.y) < 0.03)
    edge_y = max(abs(p.y) for p in points)
    near = max(p.z for p in points if abs(p.y) > edge_y - 0.06)
    return {"centre": centre, "near_post": near, "near_post_y": edge_y - 0.03}


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects = build(job)
    bc.write_report(job, objects, {"net_tape_top_m": tape_tops(objects[0])})
    bc.export_fbx(objects, Path(job["out"]))


if __name__ == "__main__":
    main()
