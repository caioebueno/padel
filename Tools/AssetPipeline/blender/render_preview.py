"""Renders a preview image of the procedural assets from the broadcast camera
(GDD 9: behind and above Team A's back glass), so changes can be reviewed
without opening Unreal. Not used by the game.

Job keys: spec, palette, assets (catalogue entries of the procedural assets),
out (PNG path), samples, resolution, report.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402
from mathutils import Euler, Vector  # noqa: E402

import bl_common as bc  # noqa: E402
import build_character  # noqa: E402
import build_court  # noqa: E402
import build_court_surface  # noqa: E402
import build_equipment  # noqa: E402


def _job(entry: dict, job: dict, part: str | None = None) -> dict:
    out = dict(entry)
    out.update({"id": entry["id"], "spec": job["spec"], "palette": job["palette"],
                "surface": job.get("surface"), "textures_dir": job.get("textures_dir"),
                "palette_group": entry.get("palette", ""), "part": part or entry.get("part", "solid")})
    out.setdefault("size", {})
    out.setdefault("body", {})
    return out


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    entries = {a["id"]: a for a in job["assets"]}
    objects = []
    for entry in entries.values():
        if entry["builder"] == "court":
            objects += build_court.build(_job(entry, job))
        elif entry["builder"] == "court_surface":
            objects += build_court_surface.build(_job(entry, job))

    # Two players per team in a typical rally shape: A at the back, B at the net.
    players = [e for e in entries.values() if e["builder"] == "character"]
    racket_entry = next(e for e in entries.values() if e["builder"] == "racket")
    spots = [((7.5, 2.4), math.pi / 2, 0), ((7.0, -2.6), math.pi / 2, 0),
             ((-3.0, 2.2), -math.pi / 2, 1), ((-2.6, -2.4), -math.pi / 2, 1)]
    for (x, y), yaw, team in spots:
        entry = players[team % len(players)]
        builder = build_character.Builder(_job(entry, job))
        body = builder.mesh(f"{entry['id']}_{x}")
        # Lower the arms out of the T-pose for the picture: rotate the arm verts
        # about each shoulder (the real game poses the skeleton instead).
        h = builder.h
        for v in body.data.vertices:
            groups = {body.vertex_groups[g.group].name for g in v.groups if g.weight > 0.3}
            if groups & {"upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r"}:
                side = 1 if v.co.x > 0 else -1
                pivot = Vector((side * 0.19 * h * builder.w, 0, 1.42 * h))
                rel = v.co - pivot
                rel.rotate(Euler((0, side * math.radians(70), 0)))
                v.co = pivot + rel
        body.rotation_euler = (0, 0, yaw + math.pi / 2)
        body.location = (x, y, 0)
        racket = build_equipment.build_racket(_job(racket_entry, job))[0]
        racket.rotation_euler = (math.radians(20), 0, yaw + math.pi / 2)
        racket.location = body.matrix_world @ (Vector((-0.42 * h, -0.12, 0.95 * h)))
        objects += [body, racket]
    ball_entry = next(e for e in entries.values() if e["builder"] == "ball")
    ball = build_equipment.build_ball(_job(ball_entry, job))[0]
    ball.location = (2.0, 0.8, 1.4)
    ball.scale = (3, 3, 3)  # exaggerated so it reads at preview resolution

    scene = bpy.context.scene
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.02, 0.022, 0.026, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
    scene.world = world
    for x in (-6.0, -2.0, 2.0, 6.0):
        for y in (-3.2, 3.2):
            light = bpy.data.objects.new(f"L{x}{y}", bpy.data.lights.new(f"L{x}{y}", "AREA"))
            light.data.energy = 900
            light.data.size = 1.4
            light.data.color = (1.0, 0.95, 0.88)
            light.location = (x, y, 7.5)
            scene.collection.objects.link(light)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = 26
    cam.location = (17.0, 0.0, 8.0)
    direction = Vector((1.0, 0.0, 0.0)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = int(job.get("samples", 24))
    scene.cycles.use_denoising = True
    scene.view_settings.view_transform = "AgX"
    scene.render.resolution_x, scene.render.resolution_y = job.get("resolution", [1280, 720])
    scene.render.filepath = job["out"]
    bpy.ops.render.render(write_still=True)
    Path(job["report"]).write_text(json.dumps({"id": "preview", "out": job["out"]}), encoding="utf-8")


if __name__ == "__main__":
    main()
