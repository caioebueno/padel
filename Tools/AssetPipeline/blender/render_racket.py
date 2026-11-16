"""Studio preview of the racket (pipeline.py preview-racket): the same mesh the game
imports, with Blender stand-ins for its materials (M_PadelRacketFace is rebuilt here from
the same textures and palette values; the rest are the plain Opaque slots), so the shape,
the face graphic, the carbon, the sand finish and the hardware can be judged without
Unreal. Not used by the game.

Job keys: racket (the catalogue entry), palette, textures_dir, out, samples, resolution,
lods, shot ("hero": both faces; "closeup": the sand finish, holes and cartridge; "lods":
LOD0, LOD1 and LOD2 side by side), report.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import bpy  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Euler, Vector  # noqa: E402

import bl_common as bc  # noqa: E402
import build_racket  # noqa: E402
from asset_lib import racket_layout as layout  # noqa: E402


def face_preview_material(palette: dict, textures: Path, cfg: dict) -> bpy.types.Material:
    """M_PadelRacketFace (unreal/racket_material.py) in Cycles nodes: the same layering."""
    face = palette["racket"]["Face"]
    mat = bpy.data.materials.new("MI_Racket_Face_Preview")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")

    def image(name: str, uv_scale: float = 1.0):
        node = nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(str(textures / f"{name}.png"), check_existing=True)
        node.image.colorspace_settings.name = "Non-Color"
        node.image.alpha_mode = "CHANNEL_PACKED"
        node.interpolation = "Cubic"
        if uv_scale != 1.0:
            uv = nodes.new("ShaderNodeUVMap")
            mapping = nodes.new("ShaderNodeMapping")
            mapping.inputs["Scale"].default_value = (uv_scale, uv_scale, 1.0)
            links.new(uv.outputs["UV"], mapping.inputs["Vector"])
            links.new(mapping.outputs["Vector"], node.inputs["Vector"])
        return node

    def rgb(hex_color: str):
        node = nodes.new("ShaderNodeRGB")
        node.outputs[0].default_value = bc.srgb_to_linear(hex_color)
        return node.outputs[0]

    def mix(a, b, factor):
        node = nodes.new("ShaderNodeMix")
        node.data_type = "RGBA"
        links.new(factor, node.inputs["Factor"])
        links.new(a, node.inputs[6])
        links.new(b, node.inputs[7])
        return node.outputs[2]

    def math_node(op, *inputs):
        node = nodes.new("ShaderNodeMath")
        node.operation = op
        for i, v in enumerate(inputs):
            if isinstance(v, (int, float)):
                node.inputs[i].default_value = v
            else:
                links.new(v, node.inputs[i])
        return node.outputs[0]

    t = cfg["textures"]
    masks = image("T_Racket_Masks")
    split = nodes.new("ShaderNodeSeparateColor")
    links.new(masks.outputs["Color"], split.inputs["Color"])
    paint, accent, ink, holes = split.outputs["Red"], split.outputs["Green"], split.outputs["Blue"], masks.outputs["Alpha"]
    weave = image("T_CarbonWeave_Mask", t["size_m"] / t["weave_tile_m"]).outputs["Color"]
    # Carbon: the colour scaled by the tow brightness around 1.
    tows = math_node("MULTIPLY_ADD", weave, 2.0 * face["weave_contrast"], 1.0 - face["weave_contrast"])
    carbon = nodes.new("ShaderNodeMix")
    carbon.data_type = "RGBA"
    carbon.blend_type = "MULTIPLY"
    carbon.inputs["Factor"].default_value = 1.0
    links.new(rgb(face["color"]), carbon.inputs[6])
    links.new(tows, carbon.inputs[7])
    base = mix(carbon.outputs[2], rgb(face["paint_color"]), paint)
    base = mix(base, rgb(face["accent_color"]), accent)
    base = mix(base, rgb(face["ink_color"]), ink)
    base = mix(base, rgb(face["hole_color"]), holes)
    links.new(base, bsdf.inputs["Base Color"])
    printed = math_node("MAXIMUM", math_node("MAXIMUM", paint, accent), ink)
    rough = math_node("ADD", math_node("MULTIPLY", printed, face["paint_roughness"] - face["roughness"]), face["roughness"])
    rough = math_node("ADD", math_node("MULTIPLY", rough, 1.0 - face["grit_amount"]), face["grit_roughness"] * face["grit_amount"])
    links.new(rough, bsdf.inputs["Roughness"])
    # The sand finish, stored DirectX: flip green for Blender.
    grit = image("T_RacketGrit_N", t["size_m"] / t["grit_tile_m"])
    flip = nodes.new("ShaderNodeSeparateColor")
    links.new(grit.outputs["Color"], flip.inputs["Color"])
    gl = nodes.new("ShaderNodeCombineColor")
    links.new(flip.outputs["Red"], gl.inputs["Red"])
    links.new(math_node("SUBTRACT", 1.0, flip.outputs["Green"]), gl.inputs["Green"])
    links.new(flip.outputs["Blue"], gl.inputs["Blue"])
    nmap = nodes.new("ShaderNodeNormalMap")
    nmap.inputs["Strength"].default_value = face["grit_normal_strength"]
    links.new(gl.outputs["Color"], nmap.inputs["Color"])
    links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    return mat


def _studio(scene, target: Vector, key_from: Vector) -> None:
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.045, 0.05, 0.06, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.25
    scene.world = world
    for name, offset, energy, size in (("Key", key_from, 70.0, 0.9), ("Fill", Vector((-1.2, -0.6, 0.4)), 12.0, 1.5),
                                       ("Rim", Vector((0.2, 1.3, 0.9)), 50.0, 0.5), ("Top", Vector((0.0, 0.0, 1.6)), 15.0, 1.2)):
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, "AREA"))
        light.data.energy = energy
        light.data.size = size
        light.location = target + offset
        light.rotation_mode = "QUATERNION"
        light.rotation_quaternion = (target - light.location).to_track_quat("-Z", "Y")
        scene.collection.objects.link(light)
    bm = bmesh.new()
    bc.add_box(bm, (0, 0, -0.01), (6.0, 6.0, 0.02))
    floor = bc.mesh_object("Floor", bm, [bc.material("Studio_Floor", {"color": "#2b2e33", "roughness": 0.55})])
    floor.data.materials[0].node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.55


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    cfg = layout.load_config()
    entry = dict(job["racket"], id="Racket", palette=job["palette"], palette_group=job["racket"]["palette"])
    face_mat = face_preview_material(job["palette"], Path(job["textures_dir"]), cfg)
    shot = job.get("shot", "hero")
    mats = build_racket._materials(entry)

    def racket(lod: int, loc, rot) -> bpy.types.Object:
        obj = build_racket.build_lod(entry, cfg, lod, mats, f"Racket{lod}_{len(bpy.data.objects)}")
        obj.location = loc
        obj.rotation_euler = Euler(rot)
        return obj

    scene = bpy.context.scene
    if shot == "lods":
        rackets = [racket(lod, (0.32 * (lod - 1), 0.0, 0.48), (math.radians(-12), 0.0, math.radians(-8)))
                   for lod in range(1 + job.get("lods", 2))]
        target, cam_loc, lens = Vector((0.0, 0.0, 0.62)), Vector((0.0, -1.85, 0.75)), 50.0
    elif shot == "closeup":
        rackets = [racket(0, (0.0, 0.0, 0.3), (math.radians(-35), 0.0, math.radians(-25)))]
        target, cam_loc, lens = Vector((0.03, 0.0, 0.42)), Vector((0.2, -0.38, 0.6)), 85.0
    else:
        # Leaning back to show the -Y face, and another turned to show the +Y face and the edge.
        rackets = [racket(0, (-0.13, 0.0, 0.1), (math.radians(-22), math.radians(-6), math.radians(-14))),
                   racket(0, (0.17, 0.12, 0.1), (math.radians(-16), math.radians(10), math.radians(200)))]
        target, cam_loc, lens = Vector((0.02, 0.05, 0.36)), Vector((0.1, -1.45, 0.62)), 55.0
    for obj in rackets:
        for slot in obj.material_slots:
            if slot.material and slot.material.name == "MI_Racket_Face":
                slot.material = face_mat
    # Stand the rackets on the floor (the butt rests on it) and aim at what's there.
    bpy.context.view_layer.update()
    for obj in rackets:
        obj.location.z -= min((obj.matrix_world @ v.co).z for v in obj.data.vertices)
    bpy.context.view_layer.update()
    if shot == "closeup":
        # The shoulder of the -Y face: holes, the keystone, the sand finish and a weight port.
        m = rackets[0].matrix_world
        target = m @ Vector((0.075, -0.019, 0.29))
        cam_loc = target + m.to_3x3() @ Vector((0.16, -0.3, 0.07))
    else:
        lo, hi = bc.world_bounds(rackets)
        centre = (lo + hi) / 2
        cam_loc = cam_loc + (centre - target)
        target = centre
    _studio(scene, target, Vector((0.9, -1.0, 1.1)))

    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    cam.data.lens = lens
    cam.location = cam_loc
    cam.rotation_euler = (target - cam_loc).to_track_quat("-Z", "Y").to_euler()
    if shot == "closeup":
        cam.data.dof.use_dof = True
        cam.data.dof.focus_distance = (target - cam_loc).length
        cam.data.dof.aperture_fstop = 5.6
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = int(job.get("samples", 64))
    scene.cycles.use_denoising = True
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.view_settings.exposure = -0.5
    scene.render.resolution_x, scene.render.resolution_y = job.get("resolution", [1600, 1000])
    scene.render.filepath = job["out"]
    bpy.ops.render.render(write_still=True)
    Path(job["report"]).write_text(json.dumps({"id": "preview-racket", "out": job["out"]}), encoding="utf-8")


if __name__ == "__main__":
    main()
