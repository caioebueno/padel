"""Renders a preview image of the procedural assets from the broadcast camera
(GDD 9: behind and above Team A's back glass), so changes can be reviewed
without opening Unreal. Not used by the game.

Job keys: spec, palette, assets (catalogue entries of the procedural assets),
out (PNG path), samples, resolution, report; and from Data/Lighting.json and
Data/LookDev.json (pipeline.py preview): preset_name, preset, fixtures, rigs, camera.
The lights use the game's physical values (candela, lux, kelvin, EV100), converted to
Blender's watts at 683 lm/W, so the picture is a fair preview of the Unreal lighting.
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
import build_court_frame  # noqa: E402
import build_court_glass  # noqa: E402
import build_court_surface  # noqa: E402
import build_equipment  # noqa: E402
import build_net  # noqa: E402
import build_venue  # noqa: E402


def kelvin_rgb(kelvin: float) -> tuple[float, float, float]:
    """Approximate linear RGB of a black body, normalised to its brightest channel."""
    t = kelvin / 100.0
    r = 255.0 if t <= 66 else 329.698727446 * (t - 60) ** -0.1332047592
    g = 99.4708025861 * math.log(t) - 161.1195681661 if t <= 66 else 288.1221695283 * (t - 60) ** -0.0755148492
    b = 255.0 if t >= 66 else (0.0 if t <= 19 else 138.5177312231 * math.log(t - 10) - 305.0447927307)
    srgb = [min(max(c, 0.0), 255.0) / 255.0 for c in (r, g, b)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb]
    top = max(lin) or 1.0
    return tuple(c / top for c in lin)


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
    preset = job.get("preset", {})
    venue = preset.get("Venue", "Indoor")
    venue_parts = {"Indoor": {"hall", "truss", "stands", "boards"}, "Outdoor": {"ground", "masts", "stands", "boards"}}
    for entry in entries.values():
        if entry["builder"] == "court_glass":
            objects += build_court_glass.build(_job(entry, job))
        elif entry["builder"] == "net":
            objects += build_net.build(_job(entry, job))
        elif entry["builder"] == "court_surface":
            objects += build_court_surface.build(_job(entry, job))
        elif entry["builder"] == "court_frame":
            objects += build_court_frame.build(_job(entry, job))[0]
        elif entry["builder"] == "venue" and entry.get("part") in venue_parts.get(venue, set()):
            objects += build_venue.build(dict(_job(entry, job), rigs=job.get("rigs", {})))
    head_entry = next((e for e in entries.values() if e.get("part") == "floodlight"), None)
    lights_on = preset.get("FloodlightsOn", False)
    if head_entry:
        for i, f in enumerate(job.get("fixtures", [])):
            head = build_venue.build(dict(_job(head_entry, job), id=f"Head{i}"))[0]
            head.location = f["position"]
            head.rotation_mode = "QUATERNION"
            head.rotation_quaternion = Vector(f["direction"]).to_track_quat("X", "Z")
            for slot in head.material_slots:
                if slot.material and slot.material.name.endswith("FixtureLens"):
                    lens = slot.material.copy()
                    bsdf = lens.node_tree.nodes.get("Principled BSDF")
                    # Emission strength is radiance in W/m^2/sr: nits / 683.
                    bsdf.inputs["Emission Strength"].default_value = (f["lens_nits"] / 683.0) if lights_on else 0.0
                    slot.material = lens

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
    balls, lods = build_equipment.build_ball(_job(ball_entry, job))
    for lod in lods:
        bpy.data.objects.remove(lod)
    ball = balls[0]
    build_equipment.apply_ball_preview_materials(ball, job["palette"], Path(job["textures_dir"]))
    ball.location = (2.0, 0.8, 1.4)
    ball.scale = (3, 3, 3)  # exaggerated so it reads at preview resolution

    scene = bpy.context.scene
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    background = world.node_tree.nodes["Background"]
    sky = preset.get("Sky", {})
    sky_lux = sky.get("SkyIlluminanceLux", 0.0)
    if sky.get("Atmosphere") and sky_lux > 0:
        # A uniform sky of luminance L gives pi * L lux on the ground.
        background.inputs["Color"].default_value = (0.42, 0.62, 1.0, 1)
        background.inputs["Strength"].default_value = sky_lux / math.pi / 683.0 / 0.6
    else:
        background.inputs["Color"].default_value = (0.02, 0.022, 0.026, 1)
        background.inputs["Strength"].default_value = 0.002
    scene.world = world
    sun = preset.get("Sun", {})
    if sun.get("Enabled"):
        lamp = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        lamp.data.energy = sun["IlluminanceLux"] / 683.0  # irradiance, W/m^2
        lamp.data.angle = math.radians(sun.get("SourceAngleDeg", 0.53))
        lamp.data.color = kelvin_rgb(sun.get("TemperatureK", 6000.0))
        el, az = math.radians(sun["ElevationDeg"]), math.radians(sun["AzimuthDeg"])
        travel = Vector((-math.cos(el) * math.cos(az), -math.cos(el) * math.sin(az), -math.sin(el)))
        lamp.rotation_mode = "QUATERNION"
        lamp.rotation_quaternion = travel.to_track_quat("-Z", "Y")
        scene.collection.objects.link(lamp)
    if lights_on:
        for i, f in enumerate(job.get("fixtures", [])):
            spot = bpy.data.objects.new(f"Flood{i}", bpy.data.lights.new(f"Flood{i}", "SPOT"))
            # Blender spreads a spot's watts like a point light: I = P / (4 pi) at 683 lm/W.
            spot.data.energy = f["intensity_cd"] * 4.0 * math.pi / 683.0
            spot.data.spot_size = math.radians(2.0 * f["outer_deg"])
            spot.data.spot_blend = max(0.0, 1.0 - f["inner_deg"] / f["outer_deg"])
            spot.data.shadow_soft_size = 0.22
            spot.data.color = kelvin_rgb(f["kelvin"])
            spot.location = Vector(f["position"]) + Vector(f["direction"]) * 0.05
            spot.rotation_mode = "QUATERNION"
            spot.rotation_quaternion = Vector(f["direction"]).to_track_quat("-Z", "Y")
            scene.collection.objects.link(spot)
    view = job.get("camera", {"LocationM": [17.0, 0.0, 8.0], "LookAtM": [-2.0, 0.0, 0.0], "FovDeg": 50.0})
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.sensor_fit = "HORIZONTAL"
    cam.data.angle = math.radians(view["FovDeg"])  # Unreal's FOV is horizontal too
    cam.data.clip_end = 400.0
    cam.location = Vector(view["LocationM"])
    direction = Vector(view["LookAtM"]) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    # The game's locked exposure: Unreal maps luminance L to L / (1.2 * 2^EV100), Blender's
    # pixels are L / 683, so the matching exposure is log2(683 / 1.2) - EV100 stops.
    ev100 = preset.get("Post", {}).get("ExposureEV100", 9.0)
    scene.view_settings.exposure = math.log2(683.0 / 1.2) - ev100
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = int(job.get("samples", 24))
    scene.cycles.use_denoising = True
    scene.view_settings.view_transform = "AgX"
    scene.cycles.max_bounces = 4
    scene.render.resolution_x, scene.render.resolution_y = job.get("resolution", [1280, 720])
    scene.render.filepath = job["out"]
    bpy.ops.render.render(write_still=True)
    Path(job["report"]).write_text(json.dumps({"id": "preview", "out": job["out"]}), encoding="utf-8")


if __name__ == "__main__":
    main()
