"""Builds the playing surface, SM_CourtSurface (GDD 2, GDD 10): the blue turf over
the 20 x 10 playing area, the inlaid white lines and the run-off around the cage.

Run by pipeline.py. Job keys: id, spec (CourtSpec.h constants in metres),
palette, surface (config/court_surface.json), budget_tris, out (FBX path),
report, and optionally textures_dir (for the Blender preview material only).

UV0 is in metres (x, y), so the tiling fibre textures have a real-world size.
UV1 ("CourtUV") maps the playing area to 0..1, for T_CourtSurface_Masks (wear,
sand drift, roll tint). Unreal's FBX import flips V, so image row 0 lands on +Y
in both Blender and Unreal.

The turf is a fine grid with a millimetre-scale undulation that fades to zero at
the court edge. It only catches light at grazing angles; the ball bounces on the
flat Z = 0 of CourtSpec.h, never on this mesh.
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402
import bmesh  # noqa: E402

import bl_common as bc  # noqa: E402

SLOTS = ("Turf", "Line", "Runoff")
COURT_UV = "CourtUV"


class Undulation:
    """A few long, seeded waves; zero on the edge of the playing area."""

    def __init__(self, cfg: dict, spec: dict, seed: int) -> None:
        rng = random.Random(seed + 7)
        self.amp = cfg["undulation_mm"] / 1000.0
        k = 2 * math.pi / cfg["undulation_wavelength"]
        self.waves = []
        for _ in range(4):
            ang = rng.uniform(0, math.pi)
            scale = rng.uniform(0.6, 1.6)
            self.waves.append((k * scale * math.cos(ang), k * scale * math.sin(ang), rng.uniform(0, 2 * math.pi)))
        self.hl, self.hw = spec["HalfLength"], spec["HalfWidth"]

    def __call__(self, x: float, y: float) -> float:
        edge = min(self.hl - abs(x), self.hw - abs(y))
        fade = min(max(edge / 0.5, 0.0), 1.0)
        s = sum(math.sin(kx * x + ky * y + p) for kx, ky, p in self.waves) / len(self.waves)
        return self.amp * s * fade


def _uv_layers(bm: bmesh.types.BMesh):
    uv0 = bm.loops.layers.uv.get("UVMap") or bm.loops.layers.uv.new("UVMap")
    uv1 = bm.loops.layers.uv.get(COURT_UV) or bm.loops.layers.uv.new(COURT_UV)
    return uv0, uv1


def _face(bm, corners, mat: int, spec: dict) -> None:
    verts = [bm.verts.new(c) for c in corners]
    face = bm.faces.new(verts)
    face.material_index = mat
    face.smooth = True
    uv0, uv1 = _uv_layers(bm)
    for loop in face.loops:
        x, y = loop.vert.co.x, loop.vert.co.y
        loop[uv0].uv = (x, y)
        loop[uv1].uv = ((x + spec["HalfLength"]) / spec["Length"], (y + spec["HalfWidth"]) / spec["Width"])


def _grid(bm, x0: float, x1: float, y0: float, y1: float, cell: float, height, lift: float, mat: int, spec: dict) -> None:
    """A grid of quads over a rectangle, sharing vertices, following height(x, y)."""
    nx, ny = max(1, round((x1 - x0) / cell)), max(1, round((y1 - y0) / cell))
    rows = []
    for j in range(ny + 1):
        y = y0 + (y1 - y0) * j / ny
        rows.append([bm.verts.new((x0 + (x1 - x0) * i / nx, y, height(x0 + (x1 - x0) * i / nx, y) + lift))
                     for i in range(nx + 1)])
    uv0, uv1 = _uv_layers(bm)
    for j in range(ny):
        for i in range(nx):
            face = bm.faces.new((rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i]))
            face.material_index = mat
            face.smooth = True
            for loop in face.loops:
                x, y = loop.vert.co.x, loop.vert.co.y
                loop[uv0].uv = (x, y)
                loop[uv1].uv = ((x + spec["HalfLength"]) / spec["Length"], (y + spec["HalfWidth"]) / spec["Width"])


def build_mesh(job: dict) -> bmesh.types.BMesh:
    spec, cfg = job["spec"], job["surface"]["mesh"]
    hl, hw = spec["HalfLength"], spec["HalfWidth"]
    idx = {n: i for i, n in enumerate(SLOTS)}
    height = Undulation(cfg, spec, int(job["surface"]["seed"]))
    flat = lambda x, y: 0.0  # noqa: E731
    cell = cfg["cell"]
    bm = bmesh.new()

    # Turf over the playing area.
    _grid(bm, -hl, hl, -hw, hw, cell, height, 0.0, idx["Turf"], spec)

    # Inlaid lines (GDD 2): the service line across the court at |X| = ServiceLineX,
    # and the centre line from the net to 0.20 past it. White fibres tufted into
    # the carpet sit flush, so the strips only ride a millimetre above the turf.
    lw, lift = spec["LineWidth"], cfg["line_lift_mm"] / 1000.0
    centre_end = spec["ServiceLineX"] + spec["CentreLineExtension"]
    for s in (-1, 1):
        x = s * spec["ServiceLineX"]
        _grid(bm, x - lw / 2, x + lw / 2, -hw, hw, max(lw, cell), height, lift, idx["Line"], spec)
    # One strip for both halves' centre lines: they meet under the net.
    _grid(bm, -centre_end, centre_end, -lw / 2, lw / 2, max(lw, cell), height, lift, idx["Line"], spec)

    # Run-off outside the cage: flat, coarse, same top height as the turf edge.
    ol, ow = hl + spec["RunoffBack"], hw + spec["RunoffSide"]
    for x0, x1, y0, y1 in ((-ol, ol, hw, ow), (-ol, ol, -ow, -hw), (-ol, -hl, -hw, hw), (hl, ol, -hw, hw)):
        _grid(bm, x0, x1, y0, y1, 1.0, flat, 0.0, idx["Runoff"], spec)

    # Closed slab underneath, so Lumen's distance fields see a solid floor, not a sheet.
    t = cfg["slab_thickness"]
    for corners in (
        [(-ol, -ow, -t), (-ol, ow, -t), (ol, ow, -t), (ol, -ow, -t)],
        [(-ol, -ow, -t), (ol, -ow, -t), (ol, -ow, 0), (-ol, -ow, 0)],
        [(ol, ow, -t), (-ol, ow, -t), (-ol, ow, 0), (ol, ow, 0)],
        [(-ol, ow, -t), (-ol, -ow, -t), (-ol, -ow, 0), (-ol, ow, 0)],
        [(ol, -ow, -t), (ol, ow, -t), (ol, ow, 0), (ol, -ow, 0)],
    ):
        _face(bm, corners, idx["Runoff"], spec)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    return bm


# --- Blender preview material ---------------------------------------------------------
# Unreal never sees these node trees (import_assets.py assigns MI_Court_* by slot
# name). They follow M_PadelTurf closely so `pipeline.py preview` shows the look.

def _image(path: Path, non_color: bool):
    img = bpy.data.images.load(str(path), check_existing=True)
    if non_color:
        img.colorspace_settings.name = "Non-Color"
    return img


def preview_material(slot: str, settings: dict, textures_dir: Path | None) -> bpy.types.Material:
    mat = bc.material(slot, settings)
    if textures_dir is None or not (textures_dir / "T_Turf_N.png").is_file():
        return mat
    nt = mat.node_tree
    nodes, links = nt.nodes, nt.links
    bsdf = nodes.get("Principled BSDF")
    tile = float(settings.get("fibre_tile_m", 0.2375))

    def node(kind: str, x: int, y: int, **inputs):
        n = nodes.new(kind)
        n.location = (x, y)
        for k, v in inputs.items():
            n.inputs[k].default_value = v
        return n

    def math_node(op: str, x: int, y: int, a=None, b=None, c=None):
        n = nodes.new("ShaderNodeMath")
        n.operation = op
        n.location = (x, y)
        for i, v in enumerate((a, b, c)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                links.new(v, n.inputs[i])
        return n

    uv0 = nodes.new("ShaderNodeUVMap")
    uv0.uv_map = "UVMap"
    uv0.location = (-1600, 0)
    uv1 = nodes.new("ShaderNodeUVMap")
    uv1.uv_map = COURT_UV
    uv1.location = (-1600, -400)
    scale = node("ShaderNodeVectorMath", -1400, 0)
    scale.operation = "SCALE"
    scale.inputs["Scale"].default_value = 1.0 / tile
    links.new(uv0.outputs["UV"], scale.inputs[0])

    tex_n = nodes.new("ShaderNodeTexImage")
    tex_n.image = _image(textures_dir / "T_Turf_N.png", True)
    tex_n.location = (-1200, 200)
    tex_m = nodes.new("ShaderNodeTexImage")
    tex_m.image = _image(textures_dir / "T_Turf_Masks.png", True)
    tex_m.location = (-1200, -100)
    tex_c = nodes.new("ShaderNodeTexImage")
    tex_c.image = _image(textures_dir / "T_CourtSurface_Masks.png", True)
    tex_c.extension = "EXTEND"
    tex_c.location = (-1200, -400)
    for t in (tex_n, tex_m):
        links.new(scale.outputs["Vector"], t.inputs["Vector"])
    links.new(uv1.outputs["UV"], tex_c.inputs["Vector"])
    sep_m = nodes.new("ShaderNodeSeparateColor")
    sep_m.location = (-900, -100)
    links.new(tex_m.outputs["Color"], sep_m.inputs["Color"])
    sep_c = nodes.new("ShaderNodeSeparateColor")
    sep_c.location = (-900, -400)
    links.new(tex_c.outputs["Color"], sep_c.inputs["Color"])

    wear = math_node("MULTIPLY", -700, -350, sep_c.outputs["Red"], float(settings.get("wear_strength", 0.0)))
    drift = math_node("MULTIPLY", -700, -500, sep_c.outputs["Green"], float(settings.get("drift_strength", 0.0)))
    # Sand shows where no blade covers it, more where play has flattened the fibres.
    gaps = math_node("SUBTRACT", -700, -150, 1.0, sep_m.outputs["Red"])
    sand = math_node("MULTIPLY", -500, -150, gaps.outputs[0], float(settings.get("sand_amount", 0.2)) * 2.0)
    sand = math_node("ADD", -350, -200, sand.outputs[0], math_node("MULTIPLY", -500, -300, wear.outputs[0], 0.35).outputs[0])
    sand = math_node("ADD", -200, -250, sand.outputs[0], drift.outputs[0])
    sand.use_clamp = True

    fibre = bc.srgb_to_linear(settings["color"])
    worn = bc.srgb_to_linear(settings.get("worn_color", settings["color"]))
    sand_rgb = bc.srgb_to_linear(settings.get("sand_color", "#c9b48a"))
    tint = math_node("MULTIPLY_ADD", -700, 100, sep_m.outputs["Blue"], 2 * float(settings.get("blade_tint", 0.1)),
                     1.0 - float(settings.get("blade_tint", 0.1)))
    fibre_col = node("ShaderNodeMix", -450, 150)
    fibre_col.data_type = "RGBA"
    links.new(wear.outputs[0], fibre_col.inputs["Factor"])
    fibre_col.inputs["A"].default_value = fibre
    fibre_col.inputs["B"].default_value = worn
    tinted = node("ShaderNodeMix", -250, 150)
    tinted.data_type = "RGBA"
    tinted.blend_type = "MULTIPLY"
    tinted.inputs["Factor"].default_value = 1.0
    links.new(fibre_col.outputs["Result"], tinted.inputs["A"])
    links.new(tint.outputs[0], tinted.inputs["B"])
    base = node("ShaderNodeMix", -50, 150)
    base.data_type = "RGBA"
    links.new(sand.outputs[0], base.inputs["Factor"])
    links.new(tinted.outputs["Result"], base.inputs["A"])
    base.inputs["B"].default_value = sand_rgb
    ao = node("ShaderNodeMix", 100, 150)
    ao.data_type = "RGBA"
    ao.blend_type = "MULTIPLY"
    ao.inputs["Factor"].default_value = 1.0
    links.new(base.outputs["Result"], ao.inputs["A"])
    links.new(sep_m.outputs["Green"], ao.inputs["B"])
    links.new(ao.outputs["Result"], bsdf.inputs["Base Color"])

    rough = node("ShaderNodeMix", 100, -150)
    links.new(sand.outputs[0], rough.inputs["Factor"])
    rough.inputs["A"].default_value = float(settings.get("roughness", 0.8))
    rough.inputs["B"].default_value = float(settings.get("sand_roughness", 0.95))
    links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])

    nmap = nodes.new("ShaderNodeNormalMap")
    nmap.uv_map = "UVMap"
    nmap.location = (-200, 400)
    nmap.inputs["Strength"].default_value = float(settings.get("normal_strength", 1.0))
    links.new(tex_n.outputs["Color"], nmap.inputs["Color"])
    links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def build(job: dict) -> list:
    pal = job["palette"]["court"]
    textures_dir = Path(job["textures_dir"]) if job.get("textures_dir") else None
    mats = [preview_material(f"MI_Court_{n}", pal[n], textures_dir) for n in SLOTS]
    bm = build_mesh(job)
    obj = bc.mesh_object(job["id"], bm, mats)
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
