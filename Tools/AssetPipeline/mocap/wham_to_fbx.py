"""Turns WHAM results into cleaned FBX footwork clips, one per near player
(docs/decisions/0012-video-footwork.md). Step 2 of 3; run_wham.sh is step 1 and
Tools/AssetPipeline/unreal/footwork_setup.py step 3.

    python3.11 Tools/AssetPipeline/mocap/wham_to_fbx.py RESULTS_DIR SourceArt/_incoming/Footwork

Needs Python 3.11 with the `bpy` wheel (pip install bpy==4.2.0), numpy, scipy and joblib.
RESULTS_DIR is the unpacked wham_results.tar.gz: SMPL_NEUTRAL.pkl plus one folder per rally
holding wham_slim.pkl. For every rally it writes
    <rally>_nearL.fbx, <rally>_nearR.fbx   skinned SMPL body on a skeleton with the Unreal
                                           mannequin's bone names, root motion on `root`
    <rally>_nearL.json, ...                foot contacts and quality numbers
    report.json                            the quality numbers of every clip

Cleanup, in order:
  1. keep the two near players (the longest on-court track in each half of the near court)
  2. drop TRIM_S from each end of a track, low-pass the rotations and the path (one camera's jitter)
  3. pin the lowest foot to the floor (WHAM's height drifts by up to ~0.7 m over a rally)
  4. put a root bone under the pelvis, on the floor, facing the pelvis's smoothed heading
  5. find foot contacts; the game's foot locking pins planted feet at runtime

The output carries the SMPL body (its own licence, non-commercial unless licensed from
Meshcapade), so it stays in the git-ignored SourceArt/_incoming/Footwork/.
"""
import argparse, glob, json, os, pickle, sys
import numpy as np
from scipy.signal import butter, filtfilt
from scipy.spatial.transform import Rotation as R

SMPL_NAMES = ["Pelvis", "L_Hip", "R_Hip", "Spine1", "L_Knee", "R_Knee", "Spine2",
              "L_Ankle", "R_Ankle", "Spine3", "L_Foot", "R_Foot", "Neck",
              "L_Collar", "R_Collar", "Head", "L_Shoulder", "R_Shoulder",
              "L_Elbow", "R_Elbow", "L_Wrist", "R_Wrist", "L_Hand", "R_Hand"]
# The FBX uses the Unreal mannequin's bone names (like SK_PlayerPlaceholderA), so Unreal's
# auto-generated IK Rig chains match the placeholder's one to one. SMPL's L_/R_ are the body's
# left and right, like _l/_r.
UE_NAMES = {"Pelvis": "pelvis", "Spine1": "spine_01", "Spine2": "spine_02", "Spine3": "spine_03",
            "Neck": "neck_01", "Head": "head", "L_Collar": "clavicle_l", "R_Collar": "clavicle_r",
            "L_Shoulder": "upperarm_l", "R_Shoulder": "upperarm_r", "L_Elbow": "lowerarm_l",
            "R_Elbow": "lowerarm_r", "L_Wrist": "hand_l", "R_Wrist": "hand_r",
            "L_Hand": "middle_01_l", "R_Hand": "middle_01_r", "L_Hip": "thigh_l", "R_Hip": "thigh_r",
            "L_Knee": "calf_l", "R_Knee": "calf_r", "L_Ankle": "foot_l", "R_Ankle": "foot_r",
            "L_Foot": "ball_l", "R_Foot": "ball_r"}
PARENTS = [-1, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9, 12, 13, 14, 16, 17, 18, 19, 20, 21]
FOOT_JOINTS = {"L": (7, 10), "R": (8, 11)}   # ankle, ball of foot
FOOT_JOINT_HEIGHT_M = 0.03   # the ankle/ball joints sit about this far above the sole
TRIM_S = 0.3                 # dropped from each end of a track


# --- SMPL loading without chumpy --------------------------------------------
class _Ch:
    def __init__(self, *a, **k): self.x = None
    def __setstate__(self, s): self.x = np.asarray(s.get("x")) if isinstance(s, dict) else None


class _Unpickler(pickle.Unpickler):
    def find_class(self, module, name):
        if module.startswith("chumpy"):
            return _Ch
        if module == "scipy.sparse.csc":
            module = "scipy.sparse"
        return super().find_class(module, name)


def _arr(v):
    if isinstance(v, _Ch): return v.x
    if hasattr(v, "toarray"): return v.toarray()
    return np.asarray(v)


def load_smpl(path):
    with open(path, "rb") as f:
        d = _Unpickler(f, encoding="latin1").load()
    return {k: _arr(d[k]) for k in ("v_template", "shapedirs", "J_regressor", "weights", "f")}


# --- maths --------------------------------------------------------------------
def lowpass(x, cutoff, fps, axis=0):
    if x.shape[axis] < 16: return x
    b, a = butter(2, cutoff / (fps / 2))
    return filtfilt(b, a, x, axis=axis)


def smooth_rotations(aa, cutoff, fps):
    """aa: T x J x 3 axis-angle. Low-pass in quaternion space, sign-aligned."""
    T, J, _ = aa.shape
    q = R.from_rotvec(aa.reshape(-1, 3)).as_quat().reshape(T, J, 4)
    for t in range(1, T):
        flip = (q[t] * q[t - 1]).sum(-1) < 0
        q[t, flip] *= -1
    q = lowpass(q, cutoff, fps)
    q /= np.linalg.norm(q, axis=-1, keepdims=True)
    return q  # x y z w


def forward_kinematics(q, root_t, J_rest):
    """q: T x 24 x 4 local rotations, root_t: T x 3, J_rest: 24 x 3. Returns T x 24 x 3."""
    T = q.shape[0]
    rots = R.from_quat(q.reshape(-1, 4)).as_matrix().reshape(T, 24, 3, 3)
    G = np.zeros((T, 24, 3, 3)); P = np.zeros((T, 24, 3))
    for j in range(24):
        p = PARENTS[j]
        if p < 0:
            G[:, j] = rots[:, j]; P[:, j] = root_t + J_rest[0]
        else:
            G[:, j] = G[:, p] @ rots[:, j]
            P[:, j] = P[:, p] + np.einsum("tij,j->ti", G[:, p], J_rest[j] - J_rest[p])
    return P


def rolling_min(x, w):
    pad = np.pad(x, (w // 2, w - w // 2 - 1), mode="edge")
    return np.lib.stride_tricks.sliding_window_view(pad, w).min(1)


# --- per track ----------------------------------------------------------------
def pick_near_players(res, n_frames, width=1920, height=1080):
    """Longest on-court track in each half of the near court.

    Main camera, 1080p: the net sits at about y = 0.5 H, the near court's
    side glass at about x = 0.1 W and 0.9 W. Umpire, ball kids and the
    bench are outside that box or barely move, so they drop out.
    """
    f = np.hypot(width, height)
    best = {}
    for k, v in res.items():
        t = v["trans"]
        u = f * t[:, 0] / t[:, 2] + width / 2
        y = f * t[:, 1] / t[:, 2] + height / 2
        cover = len(v["frame_ids"]) / n_frames
        on_court = (np.median(y) > 0.5 * height) and (0.1 * width < np.median(u) < 0.9 * width)
        if cover < 0.4 or not on_court or np.ptp(u) < 30:
            continue
        side = "L" if np.median(u) < width / 2 else "R"
        if side not in best or len(v["frame_ids"]) > len(res[best[side]]["frame_ids"]):
            best[side] = k
    return best


def clean_track(v, smpl, fps):
    betas = np.median(v["betas"], 0)
    verts = smpl["v_template"] + smpl["shapedirs"][:, :, :len(betas)] @ betas
    J_rest = smpl["J_regressor"] @ verts
    # Rest pose standing on the floor (y = 0) with the pelvis over the origin, where the
    # root bone sits.
    offset = np.array([J_rest[0, 0], verts[:, 1].min(), J_rest[0, 2]])
    verts = verts - offset
    J_rest = J_rest - offset

    trim = int(round(TRIM_S * fps))                             # track starts and ends wobble
    sl = slice(trim, len(v["pose_world"]) - trim)
    aa = v["pose_world"][sl].reshape(-1, 24, 3)
    q = smooth_rotations(aa, cutoff=6.0, fps=fps)
    trans = lowpass(v["trans_world"][sl].copy(), cutoff=3.0, fps=fps)

    P = forward_kinematics(q, trans, J_rest)                    # y up (WHAM)
    feet = P[:, [7, 8, 10, 11], 1]
    floor = rolling_min(feet.min(1), int(fps * 1.5))           # lowest foot over ~1.5 s
    floor = lowpass(floor, cutoff=0.5, fps=fps) - FOOT_JOINT_HEIGHT_M
    trans[:, 1] -= floor
    P[:, :, 1] -= floor[:, None]

    # Root bone: the pelvis on the floor, facing the pelvis's smoothed heading (SMPL faces +z).
    pelvis_rot = R.from_quat(q[:, 0])
    fwd = pelvis_rot.apply([0.0, 0.0, 1.0])
    heading = lowpass(np.unwrap(np.arctan2(fwd[:, 0], fwd[:, 2])), cutoff=2.0, fps=fps)
    root_rot = R.from_euler("y", heading[:, None])
    root_pos = np.stack([P[:, 0, 0], np.zeros(len(P)), P[:, 0, 2]], 1)
    pelvis_local_rot = (root_rot.inv() * pelvis_rot).as_quat()
    pelvis_local_pos = root_rot.inv().apply(P[:, 0] - root_pos) - J_rest[0]
    rig = {"q": q.copy(), "root_q": root_rot.as_quat(), "root_pos": root_pos, "pelvis_pos": pelvis_local_pos}
    rig["q"][:, 0] = pelvis_local_rot

    contacts, slide = {}, []
    for side, (a, b) in FOOT_JOINTS.items():
        h = P[:, [a, b], 1].min(1)
        vel = np.linalg.norm(np.gradient(P[:, b][:, [0, 2]], axis=0), axis=1) * fps
        c = (h < 0.10) & (vel < 0.6)
        contacts[side] = c.astype(int).tolist()
        if c.any(): slide.append(np.median(vel[c]))
    quality = {
        "frames": int(len(q)), "first_frame_trimmed": trim, "body_height_m": round(float(np.ptp(verts[:, 1])), 2),
        "travel_m": round(float(np.linalg.norm(np.diff(root_pos[:, [0, 2]], axis=0), axis=1).sum()), 1),
        "planted_slide_cm_s": round(float(np.mean(slide)) * 100, 1) if slide else None,
    }
    return rig, J_rest, verts, contacts, quality, P


# --- Blender ------------------------------------------------------------------
def write_fbx(path, rig, J_rest, verts, smpl, fps):
    """SMPL skeleton under a `root` bone (pelvis on the floor, the pelvis's heading), skinned body, baked keys.

    The armature object is named "Armature" so Unreal's FBX import doesn't add it as an extra bone.
    """
    import bpy
    from mathutils import Matrix, Quaternion, Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    # 29.97 is NTSC (30/1.001), which FBX stores exactly; other rates as-is.
    if abs(fps - 29.97) < 0.01: sc.render.fps, sc.render.fps_base = 30, 1.001
    else: sc.render.fps, sc.render.fps_base = round(fps), round(fps) / fps
    n = len(rig["q"])
    sc.frame_start, sc.frame_end = 0, n - 1

    # WHAM is y-up; Blender is z-up: rotate +90 deg about X. Every bone points +Z with no
    # roll, so its rest frame is Y2Z: a pose rotation or location in that frame equals the
    # same rotation or vector in WHAM's axes, unchanged.
    Y2Z = Matrix.Rotation(np.pi / 2, 4, "X")
    arm = bpy.data.armatures.new("SMPL")
    obj = bpy.data.objects.new("Armature", arm)
    sc.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm.edit_bones.new("root")
    root.head = (0, 0, 0); root.tail = (0, 0, 0.1)
    names = [UE_NAMES[n] for n in SMPL_NAMES]
    for j, name in enumerate(names):
        eb = arm.edit_bones.new(name)
        head = Y2Z @ Vector(J_rest[j])
        eb.head = head; eb.tail = head + Vector((0, 0, 0.08))
        eb.parent = arm.edit_bones[names[PARENTS[j]]] if PARENTS[j] >= 0 else root
    bpy.ops.object.mode_set(mode="OBJECT")

    me = bpy.data.meshes.new("SMPL_Body")
    me.from_pydata([tuple(Y2Z @ Vector(p)) for p in verts], [], [tuple(int(i) for i in f) for f in smpl["f"]])
    body = bpy.data.objects.new("SMPL_Body", me)
    sc.collection.objects.link(body); body.parent = obj
    for j, name in enumerate(names):
        vg = body.vertex_groups.new(name=name)
        for i in np.nonzero(smpl["weights"][:, j] > 1e-4)[0]:
            vg.add([int(i)], float(smpl["weights"][i, j]), "REPLACE")
    body.modifiers.new("Armature", "ARMATURE").object = obj

    def quat(xyzw):
        x, y, z, w = xyzw
        return Quaternion((w, x, y, z))

    for b in obj.pose.bones: b.rotation_mode = "QUATERNION"
    pbs = [obj.pose.bones[name] for name in names]
    rb, pelvis = obj.pose.bones["root"], obj.pose.bones["pelvis"]
    for t in range(n):
        rb.rotation_quaternion = quat(rig["root_q"][t]); rb.location = Vector(rig["root_pos"][t])
        rb.keyframe_insert("rotation_quaternion", frame=t); rb.keyframe_insert("location", frame=t)
        for j, pb in enumerate(pbs):
            pb.rotation_quaternion = quat(rig["q"][t, j])
            pb.keyframe_insert("rotation_quaternion", frame=t)
        pelvis.location = Vector(rig["pelvis_pos"][t])
        pelvis.keyframe_insert("location", frame=t)

    bpy.ops.export_scene.fbx(filepath=path, use_selection=False, object_types={"ARMATURE", "MESH"},
                             add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0,
                             axis_forward="-Y", axis_up="Z")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("results"); ap.add_argument("out"); ap.add_argument("--fps", type=float, default=29.97)
    a = ap.parse_args()
    import joblib
    smpl = load_smpl(os.path.join(a.results, "SMPL_NEUTRAL.pkl"))
    os.makedirs(a.out, exist_ok=True)
    report = {}
    for pkl in sorted(glob.glob(os.path.join(a.results, "*", "wham_slim.pkl"))):
        clip = os.path.basename(os.path.dirname(pkl))
        res = joblib.load(pkl)
        n = max(int(v["frame_ids"][-1]) + 1 for v in res.values())
        for side, k in pick_near_players(res, n).items():
            rig, J_rest, verts, contacts, quality, _ = clean_track(res[k], smpl, a.fps)
            name = f"{clip}_near{side}"
            write_fbx(os.path.join(a.out, name + ".fbx"), rig, J_rest, verts, smpl, a.fps)
            quality.update(track=int(k), first_frame=int(res[k]["frame_ids"][0]))
            with open(os.path.join(a.out, name + ".json"), "w") as f:
                json.dump({"quality": quality, "contacts": contacts}, f)
            report[name] = quality
            print(name, quality, flush=True)
    with open(os.path.join(a.out, "report.json"), "w") as f:
        json.dump(report, f, indent=1)


if __name__ == "__main__":
    main()
