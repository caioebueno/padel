"""Helpers shared by the Blender scripts. Runs inside Blender (bpy), either the
Blender app (`blender -b --python ...`) or the `bpy` Python module.

Every script builds in metres with Z up and the court's length along X (GDD 2),
then export_fbx() converts to Unreal centimetres in one place.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any, Iterable, Sequence

import bpy  # with the bpy module, bpy must load before bmesh
import bmesh
from mathutils import Vector

# Unreal works in centimetres (CLAUDE.md "Coordinates and units").
UNREAL_UNITS_PER_METRE = 100.0


def job_from_argv() -> dict[str, Any]:
    """Scripts get one argument after `--`: the path of a JSON job file."""
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if not argv:
        raise SystemExit("usage: <script> -- <job.json>")
    return json.loads(Path(argv[-1]).read_text(encoding="utf-8"))


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0


def srgb_to_linear(hex_color: str) -> tuple[float, float, float, float]:
    value = hex_color.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(value[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return (out[0], out[1], out[2], 1.0)


def material(slot_name: str, settings: dict[str, Any]) -> bpy.types.Material:
    """A Principled material whose name is the Unreal material instance it maps to.

    Unreal ignores these node values (unreal/import_assets.py assigns real
    material instances by slot name); they only make Blender previews look right.
    """
    existing = bpy.data.materials.get(slot_name)
    if existing:
        return existing
    mat = bpy.data.materials.new(slot_name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    color = srgb_to_linear(settings["color"])
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Roughness"].default_value = float(settings.get("roughness", 0.5))
    bsdf.inputs["Metallic"].default_value = float(settings.get("metallic", 0.0))
    if settings.get("master") == "Glass":
        bsdf.inputs["Transmission Weight"].default_value = 1.0
        bsdf.inputs["IOR"].default_value = 1.5
    if settings.get("master") == "Masked":
        # Unreal cuts the mesh and net out with a mask texture; a see-through
        # tint is enough for a Blender preview.
        bsdf.inputs["Alpha"].default_value = 0.3
    if settings.get("master") == "Emissive" or settings.get("emission"):
        bsdf.inputs["Emission Color"].default_value = color
        bsdf.inputs["Emission Strength"].default_value = float(settings.get("emission", 1.0)) * 0.05
    mat.diffuse_color = color
    return mat


def mesh_object(name: str, bm: bmesh.types.BMesh, materials: Sequence[bpy.types.Material]) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for mat in materials:
        mesh.materials.append(mat)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def add_box(bm: bmesh.types.BMesh, centre: Sequence[float], size: Sequence[float], mat_index: int = 0) -> None:
    sx, sy, sz = (s / 2.0 for s in size)
    cx, cy, cz = centre
    verts = [bm.verts.new((cx + dx * sx, cy + dy * sy, cz + dz * sz))
             for dx in (-1, 1) for dy in (-1, 1) for dz in (-1, 1)]
    # Index = 4*ix + 2*iy + iz.
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    for f in faces:
        face = bm.faces.new([verts[i] for i in f])
        face.material_index = mat_index
        face.smooth = False
    # Box UVs in metres, so tiling textures have a real-world scale.
    uv_layer = bm.loops.layers.uv.verify()
    for face in bm.faces[-6:]:
        normal = face.normal
        axis = max(range(3), key=lambda i: abs(normal[i]))
        u_axis, v_axis = [(1, 2), (0, 2), (0, 1)][axis]
        for loop in face.loops:
            loop[uv_layer].uv = (loop.vert.co[u_axis], loop.vert.co[v_axis])


def add_quad(bm: bmesh.types.BMesh, corners: Sequence[Sequence[float]], mat_index: int, uv_scale: float = 1.0) -> None:
    """One quad with UVs in metres along its two edges (for mesh and net textures)."""
    verts = [bm.verts.new(c) for c in corners]
    face = bm.faces.new(verts)
    face.material_index = mat_index
    uv_layer = bm.loops.layers.uv.verify()
    a, b, _, d = (Vector(c) for c in corners)
    u_dir, v_dir = (b - a), (d - a)
    for loop in face.loops:
        rel = loop.vert.co - a
        u = rel.dot(u_dir.normalized()) / uv_scale
        v = rel.dot(v_dir.normalized()) / uv_scale
        loop[uv_layer].uv = (u, v)


def add_cylinder(bm: bmesh.types.BMesh, start: Sequence[float], end: Sequence[float], radius: float,
                 segments: int, mat_index: int = 0) -> None:
    """Capped cylinder between two points (posts, rails, the handle)."""
    a, b = Vector(start), Vector(end)
    axis = (b - a).normalized()
    helper = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
    u = axis.cross(helper).normalized()
    v = axis.cross(u).normalized()
    rings = []
    for centre in (a, b):
        rings.append([bm.verts.new(centre + radius * (math.cos(t) * u + math.sin(t) * v))
                      for t in (2 * math.pi * i / segments for i in range(segments))])
    for i in range(segments):
        j = (i + 1) % segments
        face = bm.faces.new((rings[0][i], rings[0][j], rings[1][j], rings[1][i]))
        face.material_index = mat_index
        face.smooth = True
    for ring, reverse in ((rings[0], True), (rings[1], False)):
        face = bm.faces.new(list(reversed(ring)) if reverse else ring)
        face.material_index = mat_index


def triangle_count(obj: bpy.types.Object) -> int:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    mesh = obj.evaluated_get(depsgraph).to_mesh()
    count = sum(len(p.vertices) - 2 for p in mesh.polygons)
    obj.evaluated_get(depsgraph).to_mesh_clear()
    return count


def decimate_to(obj: bpy.types.Object, budget: int) -> None:
    """Collapse-decimates a mesh to at most `budget` triangles, then applies it."""
    tris = triangle_count(obj)
    if tris <= budget:
        return
    mod = obj.modifiers.new("Budget", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = budget / tris
    mod.use_collapse_triangulate = True
    apply_modifier(obj, mod.name)
    # Collapse can land a few faces over; nudge until it fits.
    for _ in range(4):
        tris = triangle_count(obj)
        if tris <= budget:
            return
        mod = obj.modifiers.new("Budget", "DECIMATE")
        mod.ratio = 0.98 * budget / tris
        apply_modifier(obj, mod.name)


def apply_modifier(obj: bpy.types.Object, name: str) -> None:
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj]):
        bpy.ops.object.modifier_apply(modifier=name)


def select_only(objects: Iterable[bpy.types.Object]) -> None:
    objects = list(objects)
    for obj in bpy.context.scene.objects:
        obj.select_set(False)
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def world_bounds(objects: Iterable[bpy.types.Object]) -> tuple[Vector, Vector]:
    lo = Vector((math.inf,) * 3)
    hi = Vector((-math.inf,) * 3)
    for obj in objects:
        if obj.type != "MESH":
            continue
        for corner in obj.bound_box:
            p = obj.matrix_world @ Vector(corner)
            lo = Vector(map(min, lo, p))
            hi = Vector(map(max, hi, p))
    return lo, hi


def export_fbx(objects: Sequence[bpy.types.Object], path: Path, embed_textures: bool = False) -> None:
    """Exports to FBX in Unreal units.

    Unreal's FBX importer is happiest when the file is in centimetres and every
    object has unit scale (otherwise skeletons arrive with a 100x root bone), so
    the scene is scaled up and the scale applied here, once, rather than
    modelling in centimetres.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    scene = bpy.context.scene
    roots = [o for o in objects if o.parent is None]
    for obj in roots:
        obj.location *= UNREAL_UNITS_PER_METRE
        obj.scale *= UNREAL_UNITS_PER_METRE
    select_only(objects)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    scene.unit_settings.scale_length = 1.0 / UNREAL_UNITS_PER_METRE
    try:
        bpy.ops.export_scene.fbx(
            filepath=str(path),
            use_selection=True,
            object_types={"MESH", "ARMATURE"},
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_NONE",
            use_mesh_modifiers=True,
            mesh_smooth_type="FACE",
            use_tspace=True,
            add_leaf_bones=False,
            use_armature_deform_only=True,
            armature_nodetype="NULL",
            bake_anim=False,
            # Procedural assets get their materials in Unreal; AI models keep their own textures.
            path_mode="COPY" if embed_textures else "STRIP",
            embed_textures=embed_textures,
        )
    finally:
        # Put the scene back in metres so later steps (LODs, reports) still make sense.
        for obj in roots:
            obj.location /= UNREAL_UNITS_PER_METRE
            obj.scale /= UNREAL_UNITS_PER_METRE
        select_only(objects)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        scene.unit_settings.scale_length = 1.0


def write_report(job: dict[str, Any], objects: Sequence[bpy.types.Object], extra: dict[str, Any] | None = None) -> None:
    """Writes a small JSON report next to the job, read by pipeline.py and the tests."""
    lo, hi = world_bounds(objects)
    report = {
        "id": job["id"],
        "triangles": sum(triangle_count(o) for o in objects if o.type == "MESH"),
        "bounds_min_m": list(lo),
        "bounds_max_m": list(hi),
        "materials": sorted({s.material.name for o in objects if o.type == "MESH" for s in o.material_slots if s.material}),
    }
    report.update(extra or {})
    Path(job["report"]).write_text(json.dumps(report, indent=2), encoding="utf-8")
