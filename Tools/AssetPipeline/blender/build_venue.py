"""Builds the venue around the court (GDD 10, decision 0009): the indoor club hall, the
light trusses and outdoor masts that carry the floodlights, the stands, the perimeter
boards, the outdoor ground and the floodlight head the game places on every fixture.

Run by pipeline.py. Job keys: id, part (hall, truss, masts, ground, stands, boards,
floodlight), spec (CourtSpec.h in metres), palette, rigs (fixture positions per venue
from Data/Lighting.json, asset_lib/lighting.py), budget_tris, out, report.

Everything is original and sponsor-free: the boards carry only a club colour stripe.
Unreal axes in metres: X along the court, Y across, Z up, court floor top at Z = 0.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402,F401  (the bpy module must load before bmesh)
import bmesh  # noqa: E402

import bl_common as bc  # noqa: E402

# The hall, after docs/art/ARENA_SPEC.md section 2 (inner faces): room for the broadcast
# camera 7 m behind each back glass (GDD 9) and for stands on both long sides. A stand-in
# until the arena's parametric builder lands; the light rig below is the lighting's own.
HALL_HALF_LENGTH = 25.0
HALL_HALF_WIDTH = 23.0
HALL_WALL_HEIGHT = 15.0
HALL_WALL_THICKNESS = 0.3
DADO_HEIGHT = 2.6
ROOF_TRUSS_BOTTOM = 12.0
ROOF_TRUSS_DEPTH = 2.8
ROOF_TRUSS_SPACING = 6.0
FLOOR_TOP = -0.03          # just under the court's run-off, so the two never z-fight

# Light rig (ARENA_SPEC 6.1): a 0.4 m box-truss rectangle round the fixture rows plus a
# cross truss over the net, its underside exactly on the 8 m ceiling plane (CourtSpec.h),
# so a lob that hits "the ceiling" visibly hits the rig. Chain hoists up to the roof.
TRUSS_SIZE = 0.4
TRUSS_CHORD_RADIUS = 0.025
TRUSS_LACE_RADIUS = 0.012
TRUSS_LACE_PITCH = 0.5
HOIST_SPACING = 5.5

# Outdoor masts: a tube behind each fixture with a short arm out to it.
MAST_RADIUS = 0.08
MAST_SETBACK = 0.35

# Stands along both long sides (ARENA_SPEC 5.1: S1 and S2; the end stands are an open
# decision for Caio, so they're left out here).
STAND_FIRST_Y = 10.8
STAND_HALF_LENGTH = 14.0
STAND_TIERS = 10
STAND_FIRST_FLOOR = 0.5
STAND_RISE = 0.4
STAND_DEPTH = 0.85
SEAT_PITCH = 0.5
STAND_AISLES = (-7.0, 0.0, 7.0)
AISLE_WIDTH = 1.2

# Perimeter boards round the run-off (court floor is 24 x 18 m with the run-off).
BOARD_HEIGHT = 0.9
BOARD_THICKNESS = 0.06
BOARD_GAP = 0.3

# Floodlight head (lens centre at the origin, shining along +X).
HEAD_WIDTH = 0.56
HEAD_HEIGHT = 0.42
HEAD_DEPTH = 0.11


def _mats(job: dict, names: list[str]):
    pal = job["palette"]["venue"]
    return [bc.material(f"MI_Venue_{n}", pal[n]) for n in names], {n: i for i, n in enumerate(names)}


def _finish(job: dict, bm: bmesh.types.BMesh, mats: list) -> list:
    used = sorted({f.material_index for f in bm.faces})
    remap = {old: new for new, old in enumerate(used)}
    for face in bm.faces:
        face.material_index = remap[face.material_index]
    obj = bc.mesh_object(job["id"], bm, [mats[i] for i in used])
    bc.decimate_to(obj, job["budget_tris"])
    return [obj]


def _lattice(bm, start, end, z0, z1, pitch, chord_r, lace_r, mat):
    """A flat truss in the vertical plane from start to end (x, y pairs), chords at z0 and z1."""
    (xa, ya), (xb, yb) = start, end
    length = math.hypot(xb - xa, yb - ya)
    for z in (z0, z1):
        bc.add_cylinder(bm, (xa, ya, z), (xb, yb, z), chord_r, 6, mat)
    bays = max(1, round(length / pitch))
    for i in range(bays + 1):
        t = i / bays
        x, y = xa + (xb - xa) * t, ya + (yb - ya) * t
        bc.add_cylinder(bm, (x, y, z0), (x, y, z1), lace_r, 4, mat)
        if i < bays:
            t2 = (i + 1) / bays
            x2, y2 = xa + (xb - xa) * t2, ya + (yb - ya) * t2
            lo, hi = (z0, z1) if i % 2 == 0 else (z1, z0)
            bc.add_cylinder(bm, (x, y, lo), (x2, y2, hi), lace_r, 4, mat)


def build_hall(job: dict) -> list:
    mats, idx = _mats(job, ["Concrete", "WallLower", "WallUpper", "Roof", "Steel"])
    bm = bmesh.new()
    hl, hw, t = HALL_HALF_LENGTH, HALL_HALF_WIDTH, HALL_WALL_THICKNESS
    # Dark polished concrete floor round the court.
    bc.add_box(bm, (0, 0, FLOOR_TOP - 0.1), (2 * hl, 2 * hw, 0.2), idx["Concrete"])
    # Walls: painted block to 2.6 m, dark acoustic panels above with vertical seams every
    # 1.2 m, so the room recedes behind the bright court (ARENA_SPEC 2).
    for sx in (-1, 1):
        x = sx * (hl + t / 2)
        bc.add_box(bm, (x, 0, DADO_HEIGHT / 2), (t, 2 * hw + 2 * t, DADO_HEIGHT), idx["WallLower"])
        bc.add_box(bm, (x, 0, (DADO_HEIGHT + HALL_WALL_HEIGHT) / 2), (t, 2 * hw + 2 * t, HALL_WALL_HEIGHT - DADO_HEIGHT),
                   idx["WallUpper"])
    for sy in (-1, 1):
        y = sy * (hw + t / 2)
        bc.add_box(bm, (0, y, DADO_HEIGHT / 2), (2 * hl, t, DADO_HEIGHT), idx["WallLower"])
        bc.add_box(bm, (0, y, (DADO_HEIGHT + HALL_WALL_HEIGHT) / 2), (2 * hl, t, HALL_WALL_HEIGHT - DADO_HEIGHT),
                   idx["WallUpper"])
    seam_h = HALL_WALL_HEIGHT - DADO_HEIGHT - 2.5
    seam_z = DADO_HEIGHT + seam_h / 2
    for i in range(int(2 * hl / 1.2) + 1):
        x = -hl + i * 1.2
        for sy in (-1, 1):
            bc.add_box(bm, (x, sy * (hw - 0.01), seam_z), (0.02, 0.02, seam_h), idx["Steel"])
    for i in range(int(2 * hw / 1.2) + 1):
        y = -hw + i * 1.2
        for sx in (-1, 1):
            bc.add_box(bm, (sx * (hl - 0.01), y, seam_z), (0.02, 0.02, seam_h), idx["Steel"])
    # Roof deck, steel roof trusses across the width every 6 m, columns under each end.
    top = ROOF_TRUSS_BOTTOM + ROOF_TRUSS_DEPTH
    bc.add_box(bm, (0, 0, HALL_WALL_HEIGHT + 0.1), (2 * hl + 2 * t, 2 * hw + 2 * t, 0.2), idx["Roof"])
    x = -hl + 1.0
    while x <= hl - 1.0 + 1e-6:
        _lattice(bm, (x, -hw), (x, hw), ROOF_TRUSS_BOTTOM, top, 1.5, 0.08, 0.04, idx["Steel"])
        for sy in (-1, 1):
            bc.add_box(bm, (x, sy * (hw - 0.25), HALL_WALL_HEIGHT / 2), (0.35, 0.5, HALL_WALL_HEIGHT), idx["Steel"])
        x += ROOF_TRUSS_SPACING
    # Purlins along the length.
    for j in range(-7, 8):
        bc.add_box(bm, (0, j * 3.0, top + 0.1), (2 * hl, 0.1, 0.2), idx["Steel"])
    return _finish(job, bm, mats)


def _box_truss(bm, start, end, z0, mat):
    """A square box truss from start to end (x, y pairs) with its underside at z0."""
    (xa, ya), (xb, yb) = start, end
    length = math.hypot(xb - xa, yb - ya)
    ux, uy = (xb - xa) / length, (yb - ya) / length
    nx, ny = -uy * TRUSS_SIZE / 2, ux * TRUSS_SIZE / 2
    z1 = z0 + TRUSS_SIZE
    for s in (-1, 1):
        _lattice(bm, (xa + s * nx, ya + s * ny), (xb + s * nx, yb + s * ny), z0 + TRUSS_CHORD_RADIUS,
                 z1 - TRUSS_CHORD_RADIUS, TRUSS_LACE_PITCH, TRUSS_CHORD_RADIUS, TRUSS_LACE_RADIUS, mat)
    bays = max(1, round(length / TRUSS_LACE_PITCH))
    for i in range(bays + 1):
        x, y = xa + (xb - xa) * i / bays, ya + (yb - ya) * i / bays
        for zz in (z0 + TRUSS_CHORD_RADIUS, z1 - TRUSS_CHORD_RADIUS):
            bc.add_cylinder(bm, (x - nx, y - ny, zz), (x + nx, y + ny, zz), TRUSS_LACE_RADIUS, 4, mat)


def build_truss(job: dict) -> list:
    """The indoor light rig: a box-truss rectangle through the court fixture rows, a cross
    truss over the net, hoists to the roof, and the house-light bars on the roof trusses."""
    mats, idx = _mats(job, ["Truss", "Steel"])  # black powder-coated truss (ARENA_SPEC 6.1)
    bm = bmesh.new()
    rig = job["rigs"].get("Indoor", [])
    ceiling = job["spec"]["CeilingHeight"]
    court = [(x, y, z) for x, y, z, role in rig if role == "Court"]
    house = [(x, y, z) for x, y, z, role in rig if role == "House"]
    if court:
        hx = max(abs(x) for x, _, _ in court)
        hy = max(abs(y) for _, y, _ in court)
        for sy in (-1, 1):
            _box_truss(bm, (-hx - TRUSS_SIZE / 2, sy * hy), (hx + TRUSS_SIZE / 2, sy * hy), ceiling, idx["Truss"])
        for x in (-hx, 0.0, hx):
            _box_truss(bm, (x, -hy + TRUSS_SIZE / 2), (x, hy - TRUSS_SIZE / 2), ceiling, idx["Truss"])
        # Chain hoists from the top chords to the roof trusses.
        top = ceiling + TRUSS_SIZE
        n = max(1, round(2 * hx / HOIST_SPACING))
        for i in range(n + 1):
            x = -hx + i * 2 * hx / n
            for sy in (-1, 1):
                bc.add_cylinder(bm, (x, sy * hy, top), (x, sy * hy, ROOF_TRUSS_BOTTOM), 0.008, 4, idx["Steel"])
                bc.add_box(bm, (x, sy * hy, top + 0.5), (0.18, 0.14, 0.3), idx["Steel"])
    # House lights hang on short bars under the roof trusses.
    for x, y, z in house:
        bc.add_box(bm, (x, y, z + 0.12), (0.08, 1.2, 0.08), idx["Steel"])
        bc.add_cylinder(bm, (x, y, z + 0.16), (x, y, ROOF_TRUSS_BOTTOM), 0.02, 6, idx["Steel"])
    return _finish(job, bm, mats)


def build_masts(job: dict) -> list:
    """A galvanised mast behind each outdoor fixture, with an arm out to it."""
    mats, idx = _mats(job, ["Steel", "Concrete"])
    bm = bmesh.new()
    for x, y, z, _ in job["rigs"].get("Outdoor", []):
        side = 1.0 if y > 0 else -1.0
        my = y + side * MAST_SETBACK
        top = z + 0.35
        bc.add_box(bm, (x, my, 0.05), (0.5, 0.5, 0.1), idx["Concrete"])
        bc.add_cylinder(bm, (x, my, 0.1), (x, my, top), MAST_RADIUS, 12, idx["Steel"])
        bc.add_cylinder(bm, (x, my, top - 0.1), (x, y, top - 0.1), 0.04, 8, idx["Steel"])
        bc.add_cylinder(bm, (x, y, top - 0.1), (x, y, z + HEAD_HEIGHT / 2 + 0.17), 0.03, 8, idx["Steel"])
    return _finish(job, bm, mats)


def build_ground(job: dict) -> list:
    """Outdoor club: a concrete pad round the court, lawn beyond, hedges closing the view."""
    mats, idx = _mats(job, ["PadConcrete", "Grass", "Hedge"])
    bm = bmesh.new()
    bc.add_box(bm, (0, 0, FLOOR_TOP - 0.1), (60.0, 44.0, 0.2), idx["PadConcrete"])
    bc.add_box(bm, (0, 0, FLOOR_TOP - 0.12), (180.0, 140.0, 0.2), idx["Grass"])
    for sx in (-1, 1):
        bc.add_box(bm, (sx * 31.0, 0, 1.0), (1.2, 46.0, 2.0), idx["Hedge"])
    for sy in (-1, 1):
        bc.add_box(bm, (0, sy * 23.0, 1.0), (63.0, 1.2, 2.0), idx["Hedge"])
    return _finish(job, bm, mats)


def build_stands(job: dict) -> list:
    """Temporary grandstands along each long side (ARENA_SPEC 5.1): ten rows on a scaffold
    frame, three stair aisles, dark slate seats so empty ones read dark, front and back rails."""
    mats, idx = _mats(job, ["Aluminium", "SeatShell", "Steel"])
    bm = bmesh.new()
    seats = int(2 * STAND_HALF_LENGTH / SEAT_PITCH)
    back_y = STAND_FIRST_Y + STAND_TIERS * STAND_DEPTH
    top_z = STAND_FIRST_FLOOR + (STAND_TIERS - 1) * STAND_RISE
    for sy in (-1, 1):
        for tier in range(STAND_TIERS):
            y0 = STAND_FIRST_Y + tier * STAND_DEPTH
            zt = STAND_FIRST_FLOOR + tier * STAND_RISE
            bc.add_box(bm, (0, sy * (y0 + STAND_DEPTH / 2), zt - 0.025), (2 * STAND_HALF_LENGTH, STAND_DEPTH, 0.05),
                       idx["Aluminium"])
            for i in range(seats):
                x = -STAND_HALF_LENGTH + (i + 0.5) * SEAT_PITCH
                if any(abs(x - a) < AISLE_WIDTH / 2 for a in STAND_AISLES):
                    continue
                yc = sy * (y0 + 0.4)
                bc.add_box(bm, (x, yc, zt + 0.43), (0.44, 0.40, 0.05), idx["SeatShell"])
                bc.add_box(bm, (x, yc + sy * 0.2, zt + 0.66), (0.44, 0.04, 0.40), idx["SeatShell"])
        # Scaffold frame underneath, visible at the sides.
        for x in (-STAND_HALF_LENGTH, -7.0, 0.0, 7.0, STAND_HALF_LENGTH):
            for tier in range(0, STAND_TIERS, 3):
                y = sy * (STAND_FIRST_Y + tier * STAND_DEPTH + 0.1)
                z = STAND_FIRST_FLOOR + tier * STAND_RISE
                bc.add_cylinder(bm, (x, y, 0.0), (x, y, z), 0.024, 6, idx["Steel"])
        # Front fascia and rail (1.0 m), back rail (1.1 m).
        bc.add_box(bm, (0, sy * (STAND_FIRST_Y - 0.02), STAND_FIRST_FLOOR / 2), (2 * STAND_HALF_LENGTH, 0.04, STAND_FIRST_FLOOR),
                   idx["Steel"])
        for y, z0, h in ((STAND_FIRST_Y + 0.05, STAND_FIRST_FLOOR, 1.0), (back_y - 0.05, top_z, 1.1)):
            bc.add_cylinder(bm, (-STAND_HALF_LENGTH, sy * y, z0 + h), (STAND_HALF_LENGTH, sy * y, z0 + h), 0.024, 8,
                            idx["Steel"])
            for i in range(15):
                x = -STAND_HALF_LENGTH + i * (2 * STAND_HALF_LENGTH / 14)
                bc.add_cylinder(bm, (x, sy * y, z0), (x, sy * y, z0 + h), 0.02, 6, idx["Steel"])
    return _finish(job, bm, mats)


def build_boards(job: dict) -> list:
    """Low perimeter boards round the run-off: club colours only, no sponsors or text."""
    mats, idx = _mats(job, ["Board", "BoardStripe", "Steel"])
    spec = job["spec"]
    bm = bmesh.new()
    bx = spec["HalfLength"] + spec["RunoffBack"] + BOARD_GAP
    by = spec["HalfWidth"] + spec["RunoffSide"] + BOARD_GAP
    stripe_z = BOARD_HEIGHT - 0.12

    def board(cx, cy, length, along_x):
        size = (length, BOARD_THICKNESS, BOARD_HEIGHT) if along_x else (BOARD_THICKNESS, length, BOARD_HEIGHT)
        bc.add_box(bm, (cx, cy, BOARD_HEIGHT / 2), size, idx["Board"])
        stripe = (length, BOARD_THICKNESS + 0.004, 0.06) if along_x else (BOARD_THICKNESS + 0.004, length, 0.06)
        bc.add_box(bm, (cx, cy, stripe_z), stripe, idx["BoardStripe"])

    panel = 3.0
    for sy in (-1, 1):
        n = int(round(2 * bx / panel))
        for i in range(n):
            board(-bx + (i + 0.5) * (2 * bx / n), sy * by, 2 * bx / n - 0.02, True)
    for sx in (-1, 1):
        n = int(round(2 * by / panel))
        for i in range(n):
            board(sx * bx, -by + (i + 0.5) * (2 * by / n), 2 * by / n - 0.02, False)
    # Feet behind every joint.
    for sy in (-1, 1):
        for i in range(int(round(2 * bx / panel)) + 1):
            x = -bx + i * (2 * bx / round(2 * bx / panel))
            bc.add_box(bm, (x, sy * (by + 0.15), 0.02), (0.05, 0.3, 0.04), idx["Steel"])
    return _finish(job, bm, mats)


def build_floodlight(job: dict) -> list:
    """A sports LED floodlight: finned housing, glare visor, lens and yoke. The lens centre
    is at the origin facing +X, like the engine's spot lights, so the game can place one on
    each fixture with the light's own transform."""
    mats, idx = _mats(job, ["FixtureHousing", "FixtureLens", "Aluminium"])
    bm = bmesh.new()
    w, h, d = HEAD_WIDTH, HEAD_HEIGHT, HEAD_DEPTH
    bc.add_box(bm, (-d / 2 - 0.004, 0, 0), (d, w, h), idx["FixtureHousing"])
    # Cooling fins on the back.
    fins = 11
    for i in range(fins):
        y = -w / 2 + 0.03 + i * (w - 0.06) / (fins - 1)
        bc.add_box(bm, (-d - 0.045, y, 0), (0.09, 0.008, h * 0.92), idx["FixtureHousing"])
    # Glare visor over the lens (keeps light off the sky and out of the camera).
    bc.add_box(bm, (0.07, 0, h / 2 + 0.006), (0.15, w + 0.02, 0.012), idx["FixtureHousing"])
    for sy in (-1, 1):
        bc.add_box(bm, (0.05, sy * (w / 2 + 0.006), 0.04), (0.11, 0.012, h - 0.08), idx["FixtureHousing"])
    # The lens, a hair proud of the housing so its emissive face is the front.
    bc.add_quad(bm, [(0.0, -w / 2 + 0.03, -h / 2 + 0.03), (0.0, w / 2 - 0.03, -h / 2 + 0.03),
                     (0.0, w / 2 - 0.03, h / 2 - 0.03), (0.0, -w / 2 + 0.03, h / 2 - 0.03)], idx["FixtureLens"], 1.0)
    # Yoke: arms on both sides, pivots, and a top plate.
    for sy in (-1, 1):
        bc.add_box(bm, (-d / 2, sy * (w / 2 + 0.04), 0.06), (0.05, 0.012, h / 2 + 0.24), idx["Aluminium"])
        bc.add_cylinder(bm, (-d / 2, sy * (w / 2 + 0.002), 0), (-d / 2, sy * (w / 2 + 0.05), 0), 0.03, 10,
                        idx["Aluminium"])
    bc.add_box(bm, (-d / 2, 0, h / 2 + 0.17), (0.05, w + 0.092, 0.012), idx["Aluminium"])
    return _finish(job, bm, mats)


BUILDERS = {"hall": build_hall, "truss": build_truss, "masts": build_masts, "ground": build_ground,
            "stands": build_stands, "boards": build_boards, "floodlight": build_floodlight}


def build(job: dict) -> list:
    return BUILDERS[job["part"]](job)


def clearance(job: dict, objects: list) -> dict:
    """How far the mesh keeps from the court, for the tests: the smallest |Y| of any vertex
    (side walls are at 5 m) and the smallest distance outside the run-off rectangle
    (negative when a vertex is inside it)."""
    spec = job["spec"]
    rx, ry = spec["HalfLength"] + spec["RunoffBack"], spec["HalfWidth"] + spec["RunoffSide"]
    min_abs_y, outside = math.inf, math.inf
    for obj in objects:
        for v in obj.data.vertices:
            co = obj.matrix_world @ v.co
            min_abs_y = min(min_abs_y, abs(co.y))
            outside = min(outside, max(abs(co.x) - rx, abs(co.y) - ry))
    return {"min_abs_y_m": min_abs_y, "outside_runoff_m": outside}


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    objects = build(job)
    bc.write_report(job, objects, clearance(job, objects))
    bc.export_fbx(objects, Path(job["out"]))


if __name__ == "__main__":
    main()
