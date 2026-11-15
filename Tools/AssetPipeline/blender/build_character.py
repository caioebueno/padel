"""Builds a rigged placeholder player (GDD 6.4, GDD 10).

This is a stand-in until realistic characters arrive (MetaHuman, or an AI model
rigged by the provider). It exists so gameplay, animation and the camera can be
built against a correctly sized, correctly rigged athlete in the team's kit.

The skeleton uses Unreal mannequin bone names (root, pelvis, spine_01..03,
neck_01, head, clavicle/upperarm/lowerarm/hand, thigh/calf/foot/ball, _l/_r), in a
T-pose, so the engine's IK Retargeter maps mannequin animations onto it with no
manual chain setup. The character faces Blender -Y, which arrives facing +Y in
Unreal like the engine's own mannequins.

Job keys: id, size.height_m, body.build ("tall" or "compact"), body.hair
("short" or "bun"), palette, palette_group, budget_tris, lods, out, lod_outs, report.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy  # noqa: E402  (the bpy module must load before bmesh)
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402

import bl_common as bc  # noqa: E402

# Reference proportions for a 1.80 m athlete; everything scales with height.
REFERENCE_HEIGHT = 1.80
SEGMENTS = 16

# name: (head, tail, parent). Left side is +X because the character faces -Y.
BONES = {
    "root": ((0, 0, 0), (0, 0.2, 0), None),
    "pelvis": ((0, 0, 0.95), (0, 0, 1.02), "root"),
    "spine_01": ((0, 0, 1.02), (0, 0, 1.15), "pelvis"),
    "spine_02": ((0, 0, 1.15), (0, 0, 1.28), "spine_01"),
    "spine_03": ((0, 0, 1.28), (0, 0, 1.44), "spine_02"),
    "neck_01": ((0, 0, 1.46), (0, 0, 1.56), "spine_03"),
    "head": ((0, 0, 1.56), (0, 0, 1.80), "neck_01"),
    "clavicle_l": ((0.03, 0, 1.42), (0.17, 0, 1.43), "spine_03"),
    "upperarm_l": ((0.19, 0, 1.42), (0.46, 0, 1.42), "clavicle_l"),
    "lowerarm_l": ((0.46, 0, 1.42), (0.71, 0, 1.42), "upperarm_l"),
    "hand_l": ((0.71, 0, 1.42), (0.88, 0, 1.42), "lowerarm_l"),
    "thigh_l": ((0.095, 0, 0.92), (0.095, 0, 0.50), "pelvis"),
    "calf_l": ((0.095, 0, 0.50), (0.095, 0, 0.09), "thigh_l"),
    "foot_l": ((0.095, 0, 0.09), (0.095, -0.12, 0.03), "calf_l"),
    "ball_l": ((0.095, -0.12, 0.03), (0.095, -0.20, 0.03), "foot_l"),
}


def _mirror(name: str) -> str:
    return name[:-2] + "_r" if name.endswith("_l") else name


def bones() -> dict:
    out = dict(BONES)
    for name, (head, tail, parent) in BONES.items():
        if name.endswith("_l"):
            out[_mirror(name)] = ((-head[0], head[1], head[2]), (-tail[0], tail[1], tail[2]), _mirror(parent))
    return out


class Builder:
    def __init__(self, job: dict) -> None:
        self.h = job["size"]["height_m"] / REFERENCE_HEIGHT
        build = job["body"].get("build", "tall")
        # Girth relative to height: a lean right-side defender vs a compact touch player.
        self.w = 0.96 if build == "tall" else 1.07
        self.hair = job["body"].get("hair", "short")
        pal = job["palette"][job["palette_group"]]
        group = "".join(p.capitalize() for p in job["palette_group"].split("_"))
        self.mat_names = ["Skin", "Shirt", "Shorts", "Trim", "Socks", "Shoes", "Hair"]
        self.mats = [bc.material(f"MI_{group}_{n}", pal[n]) for n in self.mat_names]
        self.bone_names = list(bones().keys())
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()

    def m(self, name: str) -> int:
        return self.mat_names.index(name)

    def p(self, x: float, y: float, z: float) -> Vector:
        return Vector((x * self.h * self.w, y * self.h * self.w, z * self.h))

    def _weight(self, vert, weights: dict) -> None:
        for bone, w in weights.items():
            vert[self.deform][self.bone_names.index(bone)] = w

    def tube(self, rings: list, mirror: bool = False) -> None:
        """rings: (centre, (ra, rb), weights, material of the segment that starts here)."""
        sign = -1.0 if mirror else 1.0
        centres = [self.p(sign * c[0], c[1], c[2]) for c, *_ in rings]
        loops = []
        for i, (_, (ra, rb), weights, _) in enumerate(rings):
            nxt = centres[min(i + 1, len(rings) - 1)]
            prv = centres[max(i - 1, 0)]
            axis = (nxt - prv).normalized()
            if abs(axis.z) > 0.7:
                u = Vector((0, 1, 0)).cross(axis).normalized()
            else:
                u = axis.cross(Vector((0, 0, 1))).normalized()
            v = axis.cross(u).normalized()
            if mirror:
                weights = {_mirror(b): w for b, w in weights.items()}
            loop = []
            for s in range(SEGMENTS):
                t = 2 * math.pi * s / SEGMENTS
                co = centres[i] + (ra * self.h * self.w) * math.cos(t) * u + (rb * self.h * self.w) * math.sin(t) * v
                vert = self.bm.verts.new(co)
                self._weight(vert, weights)
                loop.append(vert)
            loops.append(loop)
        for i in range(len(loops) - 1):
            mat = self.m(rings[i][3])
            for s in range(SEGMENTS):
                n = (s + 1) % SEGMENTS
                face = self.bm.faces.new((loops[i][s], loops[i][n], loops[i + 1][n], loops[i + 1][s]))
                face.material_index = mat
                face.smooth = True
        for loop, mat in ((loops[0], rings[0][3]), (loops[-1], rings[-2][3])):
            face = self.bm.faces.new(loop)
            face.material_index = self.m(mat)
            face.smooth = True

    def ellipsoid(self, centre, radii, weights: dict, mat: str, keep=None, segments=(20, 14)) -> None:
        tmp = bmesh.new()
        bmesh.ops.create_uvsphere(tmp, u_segments=segments[0], v_segments=segments[1], radius=1.0)
        if keep:
            bmesh.ops.delete(tmp, geom=[v for v in tmp.verts if not keep(v.co)], context="VERTS")
        c = self.p(*centre)
        mapping = {}
        for v in tmp.verts:
            co = Vector((c.x + v.co.x * radii[0] * self.h, c.y + v.co.y * radii[1] * self.h, c.z + v.co.z * radii[2] * self.h))
            nv = self.bm.verts.new(co)
            self._weight(nv, weights)
            mapping[v] = nv
        for f in tmp.faces:
            face = self.bm.faces.new([mapping[v] for v in f.verts])
            face.material_index = self.m(mat)
            face.smooth = True
        tmp.free()

    def body(self) -> None:
        sp1, sp2, sp3 = "spine_01", "spine_02", "spine_03"
        self.tube([
            ((0, 0, 0.84), (0.155, 0.10), {"pelvis": 1}, "Shorts"),
            ((0, 0, 0.92), (0.165, 0.105), {"pelvis": 1}, "Shorts"),
            ((0, 0, 1.00), (0.155, 0.10), {"pelvis": 0.5, sp1: 0.5}, "Trim"),
            ((0, 0, 1.03), (0.15, 0.10), {sp1: 1}, "Shirt"),
            ((0, 0, 1.15), (0.155, 0.105), {sp1: 0.5, sp2: 0.5}, "Shirt"),
            ((0, 0, 1.28), (0.175, 0.115), {sp2: 0.5, sp3: 0.5}, "Shirt"),
            ((0, 0, 1.38), (0.19, 0.12), {sp3: 1}, "Shirt"),
            ((0, 0, 1.44), (0.16, 0.105), {sp3: 0.7, "neck_01": 0.3}, "Trim"),
            ((0, 0, 1.47), (0.07, 0.065), {"neck_01": 1}, "Skin"),
            ((0, 0, 1.50), (0.055, 0.055), {"neck_01": 1}, "Skin"),
            ((0, 0, 1.60), (0.05, 0.05), {"neck_01": 0.5, "head": 0.5}, "Skin"),
        ])
        for mirror in (False, True):
            self.tube([
                ((0.14, 0, 1.415), (0.06, 0.06), {"clavicle_l": 1}, "Shirt"),
                ((0.21, 0, 1.42), (0.058, 0.055), {"upperarm_l": 0.7, "clavicle_l": 0.3}, "Shirt"),
                ((0.33, 0, 1.42), (0.053, 0.05), {"upperarm_l": 1}, "Trim"),
                ((0.35, 0, 1.42), (0.05, 0.048), {"upperarm_l": 1}, "Skin"),
                ((0.46, 0, 1.42), (0.04, 0.04), {"upperarm_l": 0.5, "lowerarm_l": 0.5}, "Skin"),
                ((0.58, 0, 1.42), (0.042, 0.038), {"lowerarm_l": 1}, "Skin"),
                ((0.70, 0, 1.42), (0.03, 0.025), {"lowerarm_l": 0.6, "hand_l": 0.4}, "Skin"),
                ((0.73, 0, 1.42), (0.032, 0.02), {"hand_l": 1}, "Skin"),
                ((0.80, 0, 1.42), (0.045, 0.016), {"hand_l": 1}, "Skin"),
                ((0.88, 0, 1.42), (0.03, 0.012), {"hand_l": 1}, "Skin"),
            ], mirror)
            self.tube([
                ((0.09, 0, 0.95), (0.09, 0.09), {"thigh_l": 0.7, "pelvis": 0.3}, "Shorts"),
                ((0.095, 0, 0.80), (0.088, 0.088), {"thigh_l": 1}, "Shorts"),
                ((0.095, 0, 0.68), (0.078, 0.078), {"thigh_l": 1}, "Trim"),
                ((0.095, 0, 0.66), (0.072, 0.072), {"thigh_l": 1}, "Skin"),
                ((0.095, 0, 0.50), (0.053, 0.056), {"thigh_l": 0.5, "calf_l": 0.5}, "Skin"),
                ((0.095, 0.01, 0.38), (0.058, 0.064), {"calf_l": 1}, "Skin"),
                ((0.095, 0, 0.20), (0.04, 0.042), {"calf_l": 1}, "Socks"),
                ((0.095, 0, 0.10), (0.036, 0.038), {"calf_l": 0.5, "foot_l": 0.5}, "Socks"),
            ], mirror)
            self.tube([
                ((0.095, 0.06, 0.035), (0.042, 0.03), {"foot_l": 1}, "Shoes"),
                ((0.095, 0.03, 0.05), (0.05, 0.045), {"foot_l": 1}, "Shoes"),
                ((0.095, -0.05, 0.045), (0.05, 0.04), {"foot_l": 1}, "Shoes"),
                ((0.095, -0.12, 0.035), (0.048, 0.03), {"foot_l": 0.5, "ball_l": 0.5}, "Shoes"),
                ((0.095, -0.18, 0.027), (0.04, 0.022), {"ball_l": 1}, "Shoes"),
                ((0.095, -0.21, 0.017), (0.02, 0.012), {"ball_l": 1}, "Shoes"),
            ], mirror)

    def head(self) -> None:
        hw = {"head": 1}
        self.ellipsoid((0, -0.01, 1.67), (0.085, 0.10, 0.115), hw, "Skin")
        self.ellipsoid((0, -0.108, 1.655), (0.014, 0.02, 0.026), hw, "Skin", segments=(8, 6))  # nose
        for x in (-0.087, 0.087):
            self.ellipsoid((x, 0.0, 1.665), (0.012, 0.022, 0.03), hw, "Skin", segments=(8, 6))  # ears
        for x in (-0.034, 0.034):
            self.ellipsoid((x, -0.093, 1.69), (0.012, 0.008, 0.009), hw, "Hair", segments=(8, 6))  # eyes
        # Hair: the top of a slightly larger shell, lower at the back than the front.
        self.ellipsoid((0, -0.005, 1.675), (0.092, 0.108, 0.122), hw, "Hair",
                       keep=lambda co: co.z > 0.05 + 0.45 * (-co.y) - 0.2)
        if self.hair == "bun":
            self.ellipsoid((0, 0.115, 1.73), (0.042, 0.042, 0.042), hw, "Hair", segments=(12, 8))

    def mesh(self, name: str) -> bpy.types.Object:
        self.body()
        self.head()
        obj = bc.mesh_object(name, self.bm, self.mats)
        for bone in self.bone_names:
            obj.vertex_groups.new(name=bone)
        return obj

    def armature(self, name: str) -> bpy.types.Object:
        data = bpy.data.armatures.new(name)
        arm = bpy.data.objects.new(name, data)
        bpy.context.scene.collection.objects.link(arm)
        bc.select_only([arm])
        bpy.ops.object.mode_set(mode="EDIT")
        edit = {}
        for bone, (head, tail, _) in bones().items():
            eb = data.edit_bones.new(bone)
            eb.head = self.p(*head) if bone != "root" else Vector(head)
            eb.tail = self.p(*tail) if bone != "root" else Vector(tail)
            edit[bone] = eb
        for bone, (_, _, parent) in bones().items():
            if parent:
                edit[bone].parent = edit[parent]
                # Children that start where the parent ends are connected joints.
                edit[bone].use_connect = (edit[bone].head - edit[parent].tail).length < 1e-4
        bpy.ops.object.mode_set(mode="OBJECT")
        return arm


def main() -> None:
    job = bc.job_from_argv()
    bc.reset_scene()
    builder = Builder(job)
    # Unreal drops an armature object named "Armature"; any other name becomes an extra root bone.
    arm = builder.armature("Armature")
    body = builder.mesh(job["id"])
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    bc.decimate_to(body, job["budget_tris"])
    bc.write_report(job, [body], {"bones": builder.bone_names, "lod_triangles": []})
    report_lods = []
    lod_objects = []
    for i, lod in enumerate(job.get("lods", []), start=1):
        copy = body.copy()
        copy.data = body.data.copy()
        copy.name = f"{job['id']}_LOD{i}"
        bpy.context.scene.collection.objects.link(copy)
        copy.modifiers.clear()
        mod = copy.modifiers.new("Armature", "ARMATURE")
        mod.object = arm
        bc.decimate_to(copy, max(200, int(bc.triangle_count(body) * lod["ratio"])))
        report_lods.append(bc.triangle_count(copy))
        lod_objects.append(copy)
    # Rewrite the report with the LOD counts (write_report replaces the file).
    bc.write_report(job, [body], {"bones": builder.bone_names, "lod_triangles": report_lods})
    for obj in lod_objects:
        obj.hide_set(True)
    bc.export_fbx([arm, body], Path(job["out"]))
    for i, obj in enumerate(lod_objects):
        obj.hide_set(False)
        bc.export_fbx([arm, obj], Path(job["lod_outs"][i]))


if __name__ == "__main__":
    main()
