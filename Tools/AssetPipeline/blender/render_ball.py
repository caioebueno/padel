"""Close-up preview of the ball (pipeline.py preview-ball): three balls on the court
turf under a soft key light, so the felt, the seam groove, the fuzz and the printed
mark can be judged without Unreal. Uses the same textures as the game
(asset_lib/ball_textures.py) and the Blender stand-ins for its materials
(build_equipment.apply_ball_preview_materials). Not used by the game.

Job keys: ball (the catalogue entry), palette, textures_dir, out, samples, resolution, report.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import bl_common as bc  # noqa: E402
import build_equipment  # noqa: E402


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    entry = dict(job["ball"], id="Ball", palette=job["palette"])
    radius = entry["size"]["diameter_m"] / 2.0
    textures = Path(job["textures_dir"])
    # Hero ball, one turned to show the printed mark, one turned to show the seam's neck.
    poses = [((0.0, 0.0), (0.0, 0.0, math.radians(200))),
             ((-0.05, 0.085), (math.radians(90), 0.0, math.radians(40))),
             ((0.075, 0.07), (0.0, math.radians(35), math.radians(-60)))]
    for i, ((x, y), rot) in enumerate(poses):
        balls, lods = build_equipment.build_ball(dict(entry, id=f"Ball{i}"))
        for lod in lods:
            bpy.data.objects.remove(lod)
        ball = balls[0]
        build_equipment.apply_ball_preview_materials(ball, job["palette"], textures)
        ball.location = (x, y, radius)
        ball.rotation_euler = rot

    # The court turf (palette court.Turf), as a rough blue floor.
    turf = job["palette"]["court"]["Turf"]
    bm = bmesh.new()
    bc.add_box(bm, (0, 0, -0.005), (2.0, 2.0, 0.01))
    floor = bc.mesh_object("Floor", bm, [bc.material("MI_Court_Turf", turf)])
    floor.data.materials[0].node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.9

    scene = bpy.context.scene
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.05, 0.055, 0.065, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    scene.world = world
    for name, loc, energy, size in (("Key", (0.5, -0.6, 0.9), 60.0, 0.5), ("Fill", (-0.8, 0.3, 0.5), 12.0, 1.0),
                                    ("Rim", (-0.3, 0.9, 0.35), 25.0, 0.3)):
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, "AREA"))
        light.data.energy = energy
        light.data.size = size
        light.location = loc
        light.rotation_mode = "QUATERNION"
        light.rotation_quaternion = (Vector((0, 0, radius)) - Vector(loc)).to_track_quat("-Z", "Y")
        scene.collection.objects.link(light)

    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 85.0
    cam.data.dof.use_dof = True
    cam.data.dof.focus_distance = 0.42
    cam.data.dof.aperture_fstop = 8.0
    cam.location = (0.3, -0.3, 0.15)
    cam.rotation_euler = (Vector((0.01, 0.03, radius)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = int(job.get("samples", 64))
    scene.cycles.use_denoising = True
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.view_settings.exposure = -1.6
    scene.render.film_transparent = False
    scene.render.resolution_x, scene.render.resolution_y = job.get("resolution", [1600, 900])
    scene.render.filepath = job["out"]
    bpy.ops.render.render(write_still=True)
    Path(job["report"]).write_text(json.dumps({"id": "preview-ball", "out": job["out"]}), encoding="utf-8")


if __name__ == "__main__":
    main()
