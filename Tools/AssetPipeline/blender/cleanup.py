"""Cleans up an AI-generated or hand-off model so it is game-ready for Unreal.

Steps: import (GLB, glTF or FBX), drop cameras, lights and empties, join the
meshes, merge duplicate vertices and remove loose geometry, decimate to the
triangle budget, scale to the real-world size from the catalogue, put the
origin at the feet (characters and props) or the centre (round things), bake
LODs for skeletal meshes, and export FBX in Unreal units with textures embedded.

Job keys: id, kind ("static" or "skeletal"), source_file, size, budget_tris,
lods, out, lod_outs, report.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import bl_common as bc  # noqa: E402


def import_any(path: Path) -> None:
    suffix = path.suffix.lower()
    if suffix in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=str(path))
    elif suffix == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=False)
    else:
        raise SystemExit(f"cleanup: unsupported file type {suffix}")


def target_scale(size: dict, lo: Vector, hi: Vector) -> float:
    extent = hi - lo
    if "height_m" in size:
        return size["height_m"] / extent.z
    if "diameter_m" in size:
        return size["diameter_m"] / max(extent)
    if "length_m" in size:
        return size["length_m"] / max(extent.x, extent.y)
    raise SystemExit("cleanup: size needs height_m, diameter_m or length_m")


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    import_any(Path(job["source_file"]))

    for obj in list(bpy.context.scene.objects):
        if obj.type not in ("MESH", "ARMATURE"):
            # Keep the hierarchy's transforms: unparent children first.
            for child in obj.children:
                world = child.matrix_world.copy()
                child.parent = None
                child.matrix_world = world
            bpy.data.objects.remove(obj)
    armatures = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    # Bone display shapes (the glTF importer adds an icosphere) are not part of the model.
    shapes = {pb.custom_shape for arm in armatures for pb in arm.pose.bones if pb.custom_shape}
    for shape in shapes:
        bpy.data.objects.remove(shape)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        raise SystemExit("cleanup: no meshes in the source file")
    if job["kind"] == "skeletal" and len(armatures) != 1:
        raise SystemExit(f"cleanup: skeletal assets need exactly one armature, found {len(armatures)}")
    if job["kind"] == "static":
        for arm in armatures:
            bpy.data.objects.remove(arm)
        armatures = []
        for mesh in meshes:
            for mod in list(mesh.modifiers):
                if mod.type == "ARMATURE":
                    mesh.modifiers.remove(mod)

    # Bake every transform into the data first. Generated files arrive with all
    # sorts of object scales (0.01 armatures, scaled children); unparenting and
    # applying each object on its own avoids double-scaling the skeleton.
    for mesh in meshes:
        world = mesh.matrix_world.copy()
        mesh.parent = None
        mesh.matrix_world = world
    for obj in armatures + meshes:
        bc.select_only([obj])
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Join into one mesh (one asset, one draw per material).
    bc.select_only(meshes)
    if len(meshes) > 1:
        bpy.ops.object.join()
    body = bpy.context.view_layer.objects.active
    body.name = job["id"]
    with bpy.context.temp_override(object=body, active_object=body, selected_objects=[body], edit_object=body):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.remove_doubles(threshold=0.0001)
        bpy.ops.mesh.delete_loose()
        bpy.ops.object.mode_set(mode="OBJECT")
    bc.decimate_to(body, job["budget_tris"])

    # Real-world size, and the origin at the feet (or the centre for round things).
    lo, hi = bc.world_bounds([body])
    scale = target_scale(job["size"], lo, hi)
    centre_z = (lo.z + hi.z) / 2 if "diameter_m" in job["size"] else lo.z
    pivot = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, centre_z))
    fix = Matrix.Scale(scale, 4) @ Matrix.Translation(-pivot)
    for obj in armatures + [body]:
        obj.matrix_world = fix @ obj.matrix_world
        bc.select_only([obj])
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if armatures:
        # Unreal drops an armature object named "Armature"; any other name becomes an extra root bone.
        armatures[0].name = "Armature"
        body.parent = armatures[0]
        if not any(m.type == "ARMATURE" for m in body.modifiers):
            body.modifiers.new("Armature", "ARMATURE").object = armatures[0]
    root = armatures[0] if armatures else body
    objs = [root] + ([body] if root is not body else [])

    lod_tris = []
    lod_objects = []
    for i, lod in enumerate(job.get("lods", []), start=1):
        copy = body.copy()
        copy.data = body.data.copy()
        copy.name = f"{job['id']}_LOD{i}"
        bpy.context.scene.collection.objects.link(copy)
        bc.decimate_to(copy, max(200, int(bc.triangle_count(body) * lod["ratio"])))
        lod_tris.append(bc.triangle_count(copy))
        lod_objects.append(copy)
    bc.write_report(job, [body], {"lod_triangles": lod_tris,
                                  "bones": [b.name for b in armatures[0].data.bones] if armatures else []})
    for obj in lod_objects:
        obj.hide_set(True)
    bc.export_fbx(objs, Path(job["out"]), embed_textures=True)
    for i, obj in enumerate(lod_objects):
        obj.hide_set(False)
        bc.export_fbx([root, obj] if armatures else [obj], Path(job["lod_outs"][i]), embed_textures=True)
        obj.hide_set(True)


if __name__ == "__main__":
    main()
