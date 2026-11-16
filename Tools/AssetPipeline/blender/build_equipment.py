"""Builds the ball, its speed trail and the placeholder racket (GDD 2: ball 0.065
diameter; racket solid, perforated face, at most 0.455 long and 0.26 wide). Ball
details (LOD spheres, fuzz shell) come from config/ball.json; see docs/decisions/0007-ball.md.

Job keys: id, builder ("ball", "ball_trail" or "racket"), size, palette, palette_group,
budget_tris, out (FBX), lod_outs (ball LOD1+), report.

Orientation (Unreal axes): the ball is centred on the origin. The racket's grip
centre is on the origin, its head points up +Z and its hitting faces look along
+/-Y, so a hand socket can hold it without extra offsets.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402  (the bpy module must load before bmesh)
import bmesh  # noqa: E402

import bl_common as bc  # noqa: E402


BALL_CONFIG = Path(__file__).resolve().parent.parent / "config" / "ball.json"


def _ball_sphere(bm: bmesh.types.BMesh, radius: float, segments: int, rings: int, mat_index: int) -> None:
    """A UV sphere with the equirectangular UVs asset_lib/ball_textures.py draws in:
    U = longitude / 2 pi from +X towards +Y, V = 0.5 + latitude / pi. Built by hand
    rather than with create_uvsphere so the texture convention is explicit, the U wrap
    is exact (the last column of faces runs to U = 1, not back to 0), and each pole
    triangle takes the U of its own column."""
    uv_layer = bm.loops.layers.uv.verify()
    north = bm.verts.new((0.0, 0.0, radius))
    south = bm.verts.new((0.0, 0.0, -radius))
    grid = []  # grid[r][j]: ring r (1 .. rings - 1 from the north), column j
    for r in range(1, rings):
        lat = math.pi / 2 - math.pi * r / rings
        grid.append([bm.verts.new((radius * math.cos(lat) * math.cos(2 * math.pi * j / segments),
                                   radius * math.cos(lat) * math.sin(2 * math.pi * j / segments),
                                   radius * math.sin(lat))) for j in range(segments)])

    def uv(r: int, j: float) -> tuple[float, float]:
        return (j / segments, 1.0 - r / rings)

    def face(verts, uvs) -> None:
        f = bm.faces.new(verts)
        f.material_index = mat_index
        f.smooth = True
        for loop, coord in zip(f.loops, uvs):
            loop[uv_layer].uv = coord

    for j in range(segments):
        k = (j + 1) % segments
        # Counter-clockwise seen from outside, so the normals point out.
        face((north, grid[0][j], grid[0][k]), (uv(0, j + 0.5), uv(1, j), uv(1, j + 1)))
        for r in range(rings - 2):
            face((grid[r][j], grid[r + 1][j], grid[r + 1][k], grid[r][k]),
                 (uv(r + 1, j), uv(r + 2, j), uv(r + 2, j + 1), uv(r + 1, j + 1)))
        face((south, grid[-1][k], grid[-1][j]), (uv(rings, j + 0.5), uv(rings - 1, j + 1), uv(rings - 1, j)))


def build_ball(job: dict) -> tuple[list, list]:
    """LOD0 (close-ups and replays): the felt sphere plus a fuzz shell a fraction of a
    millimetre outside it, whose masked material draws the loose fibres that make a
    felt ball's outline soft. Lower LODs are plain spheres with fewer segments, built
    directly rather than decimated so their UVs stay exact. Returns (LOD0, other LODs)."""
    cfg = json.loads(BALL_CONFIG.read_text(encoding="utf-8"))["mesh"]
    radius = job["size"]["diameter_m"] / 2.0
    felt = bc.material("MI_Ball_Felt", job["palette"]["ball"]["Felt"])
    fuzz = bc.material("MI_Ball_Fuzz", job["palette"]["ball"]["Fuzz"])
    lods = []
    for i, (segments, rings) in enumerate(cfg["lod_segments"]):
        bm = bmesh.new()
        _ball_sphere(bm, radius, segments, rings, 0)
        if i == 0:
            shell_segments, shell_rings = cfg["shell_segments"]
            _ball_sphere(bm, radius + cfg["shell_offset_mm"] / 1000.0, shell_segments, shell_rings, 1)
        lods.append(bc.mesh_object(job["id"] if i == 0 else f"{job['id']}_LOD{i}", bm, [felt, fuzz] if i == 0 else [felt]))
    return lods[:1], lods[1:]


def apply_ball_preview_materials(obj, palette: dict, textures_dir: Path) -> None:
    """Blender versions of M_PadelBallFelt and M_PadelBallFuzz (unreal/ball_material.py)
    for previews only: the FBX carries slot names, and Unreal builds the real materials.
    Same textures and the same palette values, so a Cycles render is a fair check."""
    cfg = json.loads(BALL_CONFIG.read_text(encoding="utf-8"))
    felt_cfg, ball = cfg["felt"], palette["ball"]
    radius_mm = max(obj.dimensions) * 500.0 / max(obj.scale)
    sdf_range_mm = cfg["textures"]["sdf_range_rad"] * radius_mm
    tiles = (round(2 * math.pi * radius_mm / felt_cfg["tile_mm"]), round(math.pi * radius_mm / felt_cfg["tile_mm"]))

    def image(nodes, name: str):
        node = nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(str(textures_dir / f"{name}.png"), check_existing=True)
        node.image.colorspace_settings.name = "Non-Color"
        node.interpolation = "Cubic"
        return node

    def rgb(nodes, hex_color: str):
        node = nodes.new("ShaderNodeRGB")
        node.outputs[0].default_value = bc.srgb_to_linear(hex_color)
        return node

    def tiled_uv(nodes, links, scale):
        uv = nodes.new("ShaderNodeUVMap")
        mapping = nodes.new("ShaderNodeMapping")
        mapping.inputs["Scale"].default_value = (scale[0], scale[1], 1.0)
        links.new(uv.outputs["UV"], mapping.inputs["Vector"])
        return mapping.outputs["Vector"]

    def seam_mask(nodes, links, masks):
        # 1 on the rubber strip, 0 on the felt, with a short soft edge.
        half = ball["Felt"].get("seam_half_width_mm", cfg["seam"]["half_width_mm"]) / sdf_range_mm
        edge = 0.15 / sdf_range_mm
        ramp = nodes.new("ShaderNodeMapRange")
        ramp.interpolation_type = "SMOOTHSTEP"
        ramp.inputs["From Min"].default_value = half + edge
        ramp.inputs["From Max"].default_value = half - edge
        links.new(masks.outputs["Red"], ramp.inputs["Value"])
        return ramp.outputs["Result"]

    felt = ball["Felt"]
    mat = bpy.data.materials.new("MI_Ball_Felt_Preview")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    split = nodes.new("ShaderNodeSeparateColor")
    links.new(image(nodes, "T_Ball_Masks").outputs["Color"], split.inputs["Color"])
    seam = seam_mask(nodes, links, split)
    # Mottle: felt * (1 + amount * (G - 0.5) * 2).
    mottle = nodes.new("ShaderNodeMath")
    mottle.operation = "MULTIPLY_ADD"
    links.new(split.outputs["Green"], mottle.inputs[0])
    mottle.inputs[1].default_value = 2.0 * felt["mottle_amount"]
    mottle.inputs[2].default_value = 1.0 - felt["mottle_amount"]
    tinted = nodes.new("ShaderNodeMix")
    tinted.data_type = "RGBA"
    tinted.blend_type = "MULTIPLY"
    tinted.inputs["Factor"].default_value = 1.0
    links.new(rgb(nodes, felt["color"]).outputs[0], tinted.inputs[6])
    links.new(mottle.outputs[0], tinted.inputs[7])
    inked = nodes.new("ShaderNodeMix")
    inked.data_type = "RGBA"
    links.new(split.outputs["Blue"], inked.inputs["Factor"])
    links.new(tinted.outputs[2], inked.inputs[6])
    links.new(rgb(nodes, felt["ink_color"]).outputs[0], inked.inputs[7])
    base = nodes.new("ShaderNodeMix")
    base.data_type = "RGBA"
    links.new(seam, base.inputs["Factor"])
    links.new(inked.outputs[2], base.inputs[6])
    links.new(rgb(nodes, felt["seam_color"]).outputs[0], base.inputs[7])
    links.new(base.outputs[2], bsdf.inputs["Base Color"])
    rough = nodes.new("ShaderNodeMapRange")
    links.new(seam, rough.inputs["Value"])
    rough.inputs["To Min"].default_value = felt["roughness"]
    rough.inputs["To Max"].default_value = felt["seam_roughness"]
    links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    no_fuzz = nodes.new("ShaderNodeMath")
    no_fuzz.operation = "SUBTRACT"
    no_fuzz.inputs[0].default_value = 1.0
    links.new(seam, no_fuzz.inputs[1])
    links.new(no_fuzz.outputs[0], bsdf.inputs["Sheen Weight"])
    bsdf.inputs["Sheen Roughness"].default_value = 0.45
    bsdf.inputs["Sheen Tint"].default_value = bc.srgb_to_linear(felt["fuzz_color"])
    # Seam groove normal (stored DirectX: flip green for Blender), then the fibres as a bump.
    normal_img = image(nodes, "T_Ball_N")
    flip = nodes.new("ShaderNodeSeparateColor")
    links.new(normal_img.outputs["Color"], flip.inputs["Color"])
    inv = nodes.new("ShaderNodeMath")
    inv.operation = "SUBTRACT"
    inv.inputs[0].default_value = 1.0
    links.new(flip.outputs["Green"], inv.inputs[1])
    gl = nodes.new("ShaderNodeCombineColor")
    links.new(flip.outputs["Red"], gl.inputs["Red"])
    links.new(inv.outputs[0], gl.inputs["Green"])
    links.new(flip.outputs["Blue"], gl.inputs["Blue"])
    nmap = nodes.new("ShaderNodeNormalMap")
    links.new(gl.outputs["Color"], nmap.inputs["Color"])
    fibres = image(nodes, "T_Felt_Mask")
    links.new(tiled_uv(nodes, links, tiles), fibres.inputs["Vector"])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 1.0
    bump.inputs["Distance"].default_value = 0.0006 * max(obj.scale)
    links.new(fibres.outputs["Color"], bump.inputs["Height"])
    links.new(nmap.outputs["Normal"], bump.inputs["Normal"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    fuzz = ball["Fuzz"]
    shell = bpy.data.materials.new("MI_Ball_Fuzz_Preview")
    shell.use_nodes = True
    nodes, links = shell.node_tree.nodes, shell.node_tree.links
    sbsdf = nodes.get("Principled BSDF")
    sbsdf.inputs["Base Color"].default_value = bc.srgb_to_linear(fuzz["color"])
    sbsdf.inputs["Roughness"].default_value = 1.0
    sbsdf.inputs["Sheen Weight"].default_value = 1.0
    sbsdf.inputs["Sheen Tint"].default_value = bc.srgb_to_linear(felt["fuzz_color"])
    strands = image(nodes, "T_Felt_Mask")
    links.new(tiled_uv(nodes, links, (tiles[0] * 1.7, tiles[1] * 1.7)), strands.inputs["Vector"])
    facing = nodes.new("ShaderNodeLayerWeight")
    rim = nodes.new("ShaderNodeMath")
    rim.operation = "MULTIPLY_ADD"
    links.new(facing.outputs["Facing"], rim.inputs[0])
    rim.inputs[1].default_value = fuzz["rim"]
    rim.inputs[2].default_value = fuzz["density"]
    alpha = nodes.new("ShaderNodeMath")
    alpha.operation = "MULTIPLY"
    links.new(strands.outputs["Color"], alpha.inputs[0])
    links.new(rim.outputs[0], alpha.inputs[1])
    split = nodes.new("ShaderNodeSeparateColor")
    links.new(image(nodes, "T_Ball_Masks").outputs["Color"], split.inputs["Color"])
    off_seam = nodes.new("ShaderNodeMath")
    off_seam.operation = "SUBTRACT"
    off_seam.inputs[0].default_value = 1.0
    links.new(seam_mask(nodes, links, split), off_seam.inputs[1])
    masked = nodes.new("ShaderNodeMath")
    masked.operation = "MULTIPLY"
    masked.use_clamp = True
    links.new(alpha.outputs[0], masked.inputs[0])
    links.new(off_seam.outputs[0], masked.inputs[1])
    links.new(masked.outputs[0], sbsdf.inputs["Alpha"])

    for slot in obj.material_slots:
        if slot.material and slot.material.name == "MI_Ball_Felt":
            slot.material = mat
        elif slot.material and slot.material.name == "MI_Ball_Fuzz":
            slot.material = shell


def build_ball_trail(job: dict) -> list:
    """The speed trail (GDD 10: a subtle motion trail at high speed): a tapered tube
    1 m long from the origin back along -X. APadelBall's trail component stretches it
    along X to the trail's length and points it back along the ball's path. UV.x runs
    0 at the head to 1 at the tail, UV.y once around, for the fade in M_PadelBallTrail."""
    radius = job["size"]["diameter_m"] / 2.0
    mat = bc.material("MI_Ball_Trail", job["palette"]["ball"]["Trail"])
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()
    sides, rings = 12, 10
    loops = []
    for r in range(rings + 1):
        s = r / rings
        # Full ball width at the head, narrowing to a fifth at the tail.
        rr = radius * (1.0 - 0.8 * s)
        loops.append([bm.verts.new((-s, rr * math.cos(2 * math.pi * k / sides), rr * math.sin(2 * math.pi * k / sides)))
                      for k in range(sides)])
    for r in range(rings):
        for k in range(sides):
            n = (k + 1) % sides
            f = bm.faces.new((loops[r][k], loops[r][n], loops[r + 1][n], loops[r + 1][k]))
            f.smooth = True
            for loop, coord in zip(f.loops, ((r / rings, k / sides), (r / rings, (k + 1) / sides),
                                              ((r + 1) / rings, (k + 1) / sides), ((r + 1) / rings, k / sides))):
                loop[uv_layer].uv = coord
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return [bc.mesh_object(job["id"], bm, [mat])]


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
    builder = job["builder"]
    if builder == "ball":
        objects, lods = build_ball(job)
        bc.write_report(job, objects, {"lod_triangles": [bc.triangle_count(o) for o in lods]})
        bc.export_fbx(objects, Path(job["out"]))
        for obj, out in zip(lods, job.get("lod_outs", [])):
            bc.export_fbx([obj], Path(out))
        return
    objects = build_ball_trail(job) if builder == "ball_trail" else build_racket(job)
    bc.write_report(job, objects)
    bc.export_fbx(objects, Path(job["out"]))


if __name__ == "__main__":
    main()
