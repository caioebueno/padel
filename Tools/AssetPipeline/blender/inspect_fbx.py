"""Re-imports an exported FBX and reports what Unreal will see: sizes in metres,
triangles, material slots and bones. Used by the tests to check the unit
conversion end to end. Job keys: file, report."""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402

import bl_common as bc  # noqa: E402


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    bpy.ops.import_scene.fbx(filepath=job["file"])
    scene = bpy.context.scene
    meshes = [o for o in scene.objects if o.type == "MESH"]
    armatures = [o for o in scene.objects if o.type == "ARMATURE"]
    lo, hi = bc.world_bounds(meshes)
    report = {
        "unit_scale": bpy.context.scene.unit_settings.scale_length,
        "meshes": len(meshes),
        "triangles": sum(bc.triangle_count(o) for o in meshes),
        "bounds_min_m": list(lo),
        "bounds_max_m": list(hi),
        "materials": sorted({s.material.name for o in meshes for s in o.material_slots if s.material}),
        "uv_layers": sorted({uv.name for o in meshes for uv in o.data.uv_layers}),
        "armatures": len(armatures),
        "bones": [b.name for a in armatures for b in a.data.bones],
        "root_bones": [b.name for a in armatures for b in a.data.bones if b.parent is None],
        "bone_heads_m": {b.name: list(a.matrix_world @ b.head_local) for a in armatures for b in a.data.bones},
    }
    Path(job["report"]).write_text(json.dumps(report), encoding="utf-8")


if __name__ == "__main__":
    main()
