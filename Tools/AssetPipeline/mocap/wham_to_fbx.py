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
  2. drop TRIM_S from each end of a track and resample it to OUT_FPS (UE 5.8's FBX import
     misreads 29.97 fps as 29)
  3. low-pass the rotations and the path (one camera's jitter): the torso hardest, the legs least
  4. pin the lowest foot to the floor (WHAM's height drifts by up to ~0.7 m over a rally)
  5. find foot contacts and plant them: during a contact the ankle holds one position and
     orientation, reached with two-bone leg IK and blended in and out over PLANT_BLEND_S.
     Unplanted, WHAM's planted feet slide 7-24 cm/s, which the motion matching quality test
     (Padel.Animation.MotionMatching) can't pass
  6. put a root bone under the pelvis, on the floor, facing the pelvis's smoothed heading

The output carries the SMPL body (its own licence, non-commercial unless licensed from
Meshcapade), so it stays in the git-ignored SourceArt/_incoming/Footwork/.
"""
import argparse, glob, json, os, pickle, sys
import numpy as np
from scipy.signal import butter, filtfilt
from scipy.spatial.transform import Rotation as R, Slerp

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from footwork_moves import find_moves, labelled_moves, mirror_track, slice_rig  # noqa: E402

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
OUT_FPS = 30.0               # every clip is written at this rate
FOOTWORK_CONFIG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "config", "footwork.json")
# Rotation low-pass cutoffs (Hz) per joint group. WHAM's single-camera jitter is worst on the
# torso, which moves slowly anyway; the legs keep the most detail for quick steps.
TORSO = {0, 3, 6, 9, 12, 15}
LEGS = {1, 2, 4, 5, 7, 8, 10, 11}
CUTOFF_HZ = {"torso": 3.0, "legs": 6.0, "arms": 4.0}
PATH_CUTOFF_HZ = 2.5
# A foot is planted while its lowest joint is under CONTACT_HEIGHT_M and its ball moves slower
# than CONTACT_SPEED_M_S; gaps shorter than CONTACT_GAP_S are filled, plants shorter than
# CONTACT_MIN_S dropped.
CONTACT_HEIGHT_M = 0.12
CONTACT_SPEED_M_S = 0.5
CONTACT_GAP_S = 0.1
CONTACT_MIN_S = 0.13
PLANT_BLEND_S = 0.13
# The root's heading follows the pelvis slowly. In the game the body stays square to the net and
# the motion matching query keeps one facing, so a root that swings with every hip turn (2 Hz)
# matched it badly; the hips' quicker turns stay in the pose.
ROOT_HEADING_CUTOFF_HZ = 0.5
SOFT_REACH = 0.96            # share of the leg's length past which leg IK eases off its target
LEG_CHAINS = {"L": (1, 4, 7), "R": (2, 5, 8)}   # hip, knee, ankle


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


def sign_align(q):
    """q: T x J x 4. Flip quaternions so neighbouring frames are in the same hemisphere."""
    for t in range(1, len(q)):
        flip = (q[t] * q[t - 1]).sum(-1) < 0
        q[t, flip] *= -1
    return q


def smooth_rotations(q, fps):
    """q: T x 24 x 4 (x y z w). Low-pass in quaternion space, per joint group (CUTOFF_HZ)."""
    q = sign_align(q.copy())
    for j in range(q.shape[1]):
        group = "torso" if j in TORSO else "legs" if j in LEGS else "arms"
        q[:, j] = lowpass(q[:, j], CUTOFF_HZ[group], fps)
    return q / np.linalg.norm(q, axis=-1, keepdims=True)


def resample(aa, trans, src_fps, dst_fps):
    """aa: T x 24 x 3 axis-angle, trans: T x 3, at src_fps. Returns quaternions and trans at dst_fps."""
    T = len(aa)
    t_src = np.arange(T) / src_fps
    t_dst = np.arange(int(np.floor(t_src[-1] * dst_fps)) + 1) / dst_fps
    q = np.zeros((len(t_dst), 24, 4))
    for j in range(24):
        q[:, j] = Slerp(t_src, R.from_rotvec(aa[:, j]))(t_dst).as_quat()
    tr = np.stack([np.interp(t_dst, t_src, trans[:, i]) for i in range(3)], 1)
    return q, tr


def forward_kinematics(q, root_t, J_rest, with_rotations=False):
    """q: T x 24 x 4 local rotations, root_t: T x 3, J_rest: 24 x 3.
    Returns joint positions T x 24 x 3 (and global rotation matrices T x 24 x 3 x 3)."""
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
    return (P, G) if with_rotations else P


def rotation_between(a, b):
    """Smallest rotation matrix taking direction a to direction b (both 3-vectors)."""
    a = a / np.linalg.norm(a); b = b / np.linalg.norm(b)
    axis = np.cross(a, b); s = np.linalg.norm(axis); c = float(np.clip(a @ b, -1, 1))
    if s < 1e-9:
        return np.eye(3)
    return R.from_rotvec(axis / s * np.arctan2(s, c)).as_matrix()


def contact_mask(h, speed, fps):
    """Planted frames: low and slow, short gaps filled, short plants dropped."""
    c = (h < CONTACT_HEIGHT_M) & (speed < CONTACT_SPEED_M_S)
    gap, shortest = int(round(CONTACT_GAP_S * fps)), int(round(CONTACT_MIN_S * fps))
    segments = segments_of(c)
    for (s0, e0), (s1, _) in zip(segments, segments[1:]):
        if s1 - e0 <= gap:
            c[e0:s1] = True
    for s0, e0 in segments_of(c):
        if e0 - s0 < shortest:
            c[s0:e0] = False
    return c


def segments_of(mask):
    """[(start, end)) runs of True."""
    d = np.diff(np.concatenate([[0], mask.astype(int), [0]]))
    return list(zip(np.nonzero(d == 1)[0], np.nonzero(d == -1)[0]))


def plant_feet(q, trans, J_rest, contacts, fps):
    """Hold each planted ankle at one position and orientation, by two-bone leg IK.

    q: T x 24 x 4 local rotations (changed in place for the hips, knees and ankles).
    The plant blends in and out over PLANT_BLEND_S, so the step itself is untouched.
    """
    T = len(q)
    blend = max(1, int(round(PLANT_BLEND_S * fps)))
    P, G = forward_kinematics(q, trans, J_rest, with_rotations=True)
    for side, (hip, knee, ankle) in LEG_CHAINS.items():
        # The plant holds the foot's position and its yaw; the heel and toe still roll, which
        # is how a foot really leaves the floor and keeps the release from snapping the ankle.
        foot_yaw = yaw_of(G[:, ankle])
        weight = np.zeros(T); target_p = P[:, ankle].copy(); target_yaw = foot_yaw.copy()
        for s0, e0 in segments_of(contacts[side]):
            p = np.median(P[s0:e0, ankle], 0)
            yaw = np.unwrap(foot_yaw[s0:e0]).mean()
            yaw = foot_yaw[s0] + np.angle(np.exp(1j * (yaw - foot_yaw[s0])))
            for t in range(max(0, s0 - blend), min(T, e0 + blend)):
                w = 1.0 if s0 <= t < e0 else (1 - (s0 - t) / (blend + 1) if t < s0 else 1 - (t - e0 + 1) / (blend + 1))
                w = 0.5 - 0.5 * np.cos(np.pi * w)               # ease in and out
                # Blend over what earlier plants left, so overlapping ramps (two quick steps)
                # hand over smoothly instead of jumping between the two targets.
                weight[t] = max(weight[t], w)
                target_p[t] = (1 - w) * target_p[t] + w * p
                target_yaw[t] += w * np.angle(np.exp(1j * (yaw - target_yaw[t])))
        a = np.linalg.norm(J_rest[knee] - J_rest[hip]); b = np.linalg.norm(J_rest[ankle] - J_rest[knee])
        for t in np.nonzero(weight > 0)[0]:
            H, K, A, Tg = P[t, hip], P[t, knee], P[t, ankle], target_p[t]
            # Soft reach: past SOFT_REACH of the leg's length the ankle eases off its target
            # instead of snapping the knee straight (the body has moved off a plant it is lifting).
            d = soft_reach(np.linalg.norm(Tg - H), a + b)
            d = max(d, abs(a - b) + 1e-4)
            u = (Tg - H) / max(np.linalg.norm(Tg - H), 1e-9)
            # Bend toward where the knee points (SMPL's knee faces +z at rest), which stays
            # steady when the leg is nearly straight, unlike the knee's offset from the hip-ankle line.
            pole = G[t, knee] @ np.array([0.0, 0.0, 1.0])
            v = pole - (pole @ u) * u
            v = v / max(np.linalg.norm(v), 1e-9)
            cos_a = (a * a + d * d - b * b) / (2 * a * d)
            K_new = H + a * (cos_a * u + np.sqrt(max(0.0, 1 - cos_a * cos_a)) * v)
            D1 = rotation_between(K - H, K_new - H)
            G_hip = D1 @ G[t, hip]
            D2 = rotation_between(D1 @ (A - K), H + u * d - K_new)
            G_knee = D2 @ D1 @ G[t, knee]
            G_parent = G[t, PARENTS[hip]]
            turn = R.from_euler("y", target_yaw[t] - foot_yaw[t]).as_matrix()
            solved = R.from_matrix([G_parent.T @ G_hip, G_hip.T @ G_knee, G_knee.T @ turn @ G[t, ankle]])
            # The solve puts the knee in the pole's plane, a little off WHAM's knee even with the
            # target on the ankle, so the result fades in with the plant's weight too.
            for i, joint in enumerate((hip, knee, ankle)):
                before = R.from_quat(q[t, joint])
                q[t, joint] = Slerp([0, 1], R.concatenate([before, solved[i]]))(weight[t]).as_quat()
    return sign_align(q)


def yaw_of(G):
    """Heading (radians about +y) of each frame's +z axis; SMPL's feet point +z at rest."""
    fwd = G[..., :, 2]
    return np.arctan2(fwd[..., 0], fwd[..., 2])


def soft_reach(d, length):
    """Distance the IK actually reaches for a target d away, never quite straightening the leg."""
    soft = SOFT_REACH * length
    if d <= soft:
        return d
    return soft + (length - soft) * (1 - np.exp(-(d - soft) / (length - soft)))


def planted_slide(P, contacts, fps):
    """Mean of each foot's median ball speed (m/s) over its planted frames."""
    out = []
    for side, (a, b) in FOOT_JOINTS.items():
        c = contacts[side]
        if c.any():
            vel = np.linalg.norm(np.gradient(P[:, b][:, [0, 2]], axis=0), axis=1) * fps
            out.append(np.median(vel[c]))
    return float(np.mean(out)) if out else None


def find_contacts(P, fps):
    contacts = {}
    for side, (a, b) in FOOT_JOINTS.items():
        h = P[:, [a, b], 1].min(1)
        speed = np.linalg.norm(np.gradient(P[:, b][:, [0, 2]], axis=0), axis=1) * fps
        contacts[side] = contact_mask(h, speed, fps)
    return contacts


def rolling_min(x, w):
    pad = np.pad(x, (w // 2, w - w // 2 - 1), mode="edge")
    return np.lib.stride_tricks.sliding_window_view(pad, w).min(1)


# --- per track ----------------------------------------------------------------
# The near court's box in the image, as shares of its height and width. The defaults fit the
# broadcast camera (0012): the net at about 0.5 H, the side glass at about 0.1 W and 0.9 W.
# A training match's end and side cameras put near players from about 0.42 H down and out to
# the glass at the frame's edge, with the far pair at 0.25-0.39 H: there use --net-y 0.42
# --side-x 0.0.
NET_Y = 0.5
SIDE_X = 0.1
MIN_SHARED = 0.3   # the two near players share at least this much of the shorter track's frames


def pick_near_players(res, n_frames, width=1920, height=1080, net_y=NET_Y, side_x=SIDE_X):
    """The two longest on-court tracks that are on screen together, named L and R by position.

    A track counts when its median position is below net_y * height and between side_x and
    1 - side_x of the width. Umpire, ball kids and the bench are outside that box or barely
    move, so they drop out. Both near players can stand right of the middle (a side camera,
    one partner by the glass), so the pair isn't one per half; a second track that never
    shares the screen with the first is the same player tracked twice, so it isn't taken.
    """
    f = np.hypot(width, height)
    found = []
    for k, v in res.items():
        t = v["trans"]
        u = f * t[:, 0] / t[:, 2] + width / 2
        y = f * t[:, 1] / t[:, 2] + height / 2
        cover = len(v["frame_ids"]) / n_frames
        on_court = (np.median(y) > net_y * height) and (side_x * width <= np.median(u) <= (1 - side_x) * width)
        if cover < 0.4 or not on_court or np.ptp(u) < 30:
            continue
        found.append((len(v["frame_ids"]), k, float(np.median(u)), set(np.asarray(v["frame_ids"]).tolist())))
    found.sort(key=lambda c: -c[0])
    pair = found[:1]
    for c in found[1:]:
        if len(pair) == 2: break
        shared = len(c[3] & pair[0][3]) / min(len(c[3]), len(pair[0][3]))
        if shared >= MIN_SHARED:
            pair.append(c)
    pair.sort(key=lambda c: c[2])
    if len(pair) == 1:
        return {"L" if pair[0][2] < width / 2 else "R": pair[0][1]}
    return {side: c[1] for side, c in zip("LR", pair)}


def clean_track(v, smpl, src_fps, fps=OUT_FPS):
    betas = np.median(v["betas"], 0)
    verts = smpl["v_template"] + smpl["shapedirs"][:, :, :len(betas)] @ betas
    J_rest = smpl["J_regressor"] @ verts
    # Rest pose standing on the floor (y = 0) with the pelvis over the origin, where the
    # root bone sits.
    offset = np.array([J_rest[0, 0], verts[:, 1].min(), J_rest[0, 2]])
    verts = verts - offset
    J_rest = J_rest - offset

    trim = int(round(TRIM_S * src_fps))                         # track starts and ends wobble
    sl = slice(trim, len(v["pose_world"]) - trim)
    q, trans = resample(v["pose_world"][sl].reshape(-1, 24, 3), v["trans_world"][sl], src_fps, fps)
    q = smooth_rotations(q, fps)
    trans = lowpass(trans, cutoff=PATH_CUTOFF_HZ, fps=fps)

    P = forward_kinematics(q, trans, J_rest)                    # y up (WHAM)
    feet = P[:, [7, 8, 10, 11], 1]
    floor = rolling_min(feet.min(1), int(fps * 1.5))           # lowest foot over ~1.5 s
    floor = lowpass(floor, cutoff=0.5, fps=fps) - FOOT_JOINT_HEIGHT_M
    trans[:, 1] -= floor
    P[:, :, 1] -= floor[:, None]

    contacts = find_contacts(P, fps)
    slide_before = planted_slide(P, contacts, fps)
    q = plant_feet(q, trans, J_rest, contacts, fps)
    P = forward_kinematics(q, trans, J_rest)

    # Root bone: the pelvis on the floor, facing the pelvis's smoothed heading (SMPL faces +z).
    pelvis_rot = R.from_quat(q[:, 0])
    fwd = pelvis_rot.apply([0.0, 0.0, 1.0])
    heading = lowpass(np.unwrap(np.arctan2(fwd[:, 0], fwd[:, 2])), cutoff=ROOT_HEADING_CUTOFF_HZ, fps=fps)
    root_rot = R.from_euler("y", heading[:, None])
    root_pos = np.stack([P[:, 0, 0], np.zeros(len(P)), P[:, 0, 2]], 1)
    pelvis_local_rot = (root_rot.inv() * pelvis_rot).as_quat()
    pelvis_local_pos = root_rot.inv().apply(P[:, 0] - root_pos) - J_rest[0]
    rig = {"q": q.copy(), "root_q": root_rot.as_quat(), "root_pos": root_pos, "pelvis_pos": pelvis_local_pos}
    rig["q"][:, 0] = pelvis_local_rot

    slide = planted_slide(P, contacts, fps)
    cm = lambda x: round(x * 100, 1) if x is not None else None
    quality = {
        "frames": int(len(q)), "fps": fps, "first_frame_trimmed": trim, "body_height_m": round(float(np.ptp(verts[:, 1])), 2),
        "travel_m": round(float(np.linalg.norm(np.diff(root_pos[:, [0, 2]], axis=0), axis=1).sum()), 1),
        "planted_slide_cm_s": cm(slide), "planted_slide_before_ik_cm_s": cm(slide_before),
        "planted_share": round(float(np.mean([c.mean() for c in contacts.values()])), 2),
    }
    return rig, J_rest, verts, {k: c.astype(int).tolist() for k, c in contacts.items()}, quality, P


# --- Blender ------------------------------------------------------------------
def write_fbx(path, rig, J_rest, verts, smpl, fps=OUT_FPS):
    """SMPL skeleton under a `root` bone (pelvis on the floor, the pelvis's heading), skinned body, baked keys.

    The armature object is named "Armature" so Unreal's FBX import doesn't add it as an extra bone.
    """
    import bpy
    from mathutils import Matrix, Quaternion, Vector
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    # Built in centimetres in a centimetre scene, like bl_common.export_fbx: in metres, UE 5.8
    # drops the "Armature" node and moves its 100x unit scale onto `root`, which the retargeter
    # turns into a body tens of metres off.
    sc.unit_settings.scale_length = 0.01
    cm = 100.0
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
    # The root bone points +Y with no roll, so its rest frame is the world's, like the
    # mannequin's root: Unreal then sees an unrotated root. (Pointing +Z like the others gave
    # it a 90-degree roll, and the retargeter scrambled the pelvis by tens of metres.)
    root = arm.edit_bones.new("root")
    root.head = (0, 0, 0); root.tail = (0, 10, 0); root.roll = 0
    names = [UE_NAMES[n] for n in SMPL_NAMES]
    for j, name in enumerate(names):
        eb = arm.edit_bones.new(name)
        head = Y2Z @ Vector(J_rest[j] * cm)
        eb.head = head; eb.tail = head + Vector((0, 0, 8))
        eb.parent = arm.edit_bones[names[PARENTS[j]]] if PARENTS[j] >= 0 else root
    bpy.ops.object.mode_set(mode="OBJECT")

    me = bpy.data.meshes.new("SMPL_Body")
    me.from_pydata([tuple(Y2Z @ Vector(p * cm)) for p in verts], [], [tuple(int(i) for i in f) for f in smpl["f"]])
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
    # The root's keys are in Blender's (z-up) axes; every other bone's rest frame is Y2Z, so its
    # keys stay in WHAM's axes.
    y2z = Y2Z.to_quaternion()
    for t in range(n):
        rb.rotation_quaternion = y2z @ quat(rig["root_q"][t]) @ y2z.inverted()
        rb.location = Y2Z @ Vector(rig["root_pos"][t] * cm)
        rb.keyframe_insert("rotation_quaternion", frame=t); rb.keyframe_insert("location", frame=t)
        for j, pb in enumerate(pbs):
            pb.rotation_quaternion = quat(rig["q"][t, j])
            pb.keyframe_insert("rotation_quaternion", frame=t)
        pelvis.location = Vector(rig["pelvis_pos"][t] * cm)
        pelvis.keyframe_insert("location", frame=t)

    bpy.ops.export_scene.fbx(filepath=path, use_selection=False, object_types={"ARMATURE", "MESH"},
                             add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0,
                             axis_forward="-Y", axis_up="Z", apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("results"); ap.add_argument("out")
    ap.add_argument("--fps", type=float, default=29.97, help="the source video's frame rate")
    ap.add_argument("--moves", choices=("shuffles", "labelled"), default="shuffles",
                    help="shuffles: cut the side shuffles out of whole rallies; labelled: each clip is "
                         "already one move, named <label>-<n>_..., cut into its moving windows")
    ap.add_argument("--net-y", type=float, default=NET_Y, help="near players are below this share of the height")
    ap.add_argument("--side-x", type=float, default=SIDE_X, help="near players are this share of the width in from each side")
    ap.add_argument("--config", default=FOOTWORK_CONFIG, help="labelled_windows: how --moves labelled cuts each clip")
    a = ap.parse_args()
    with open(a.config) as f:
        windows = {k: v for k, v in json.load(f)["labelled_windows"].items() if not k.startswith("_")}
    import joblib
    smpl = load_smpl(os.path.join(a.results, "SMPL_NEUTRAL.pkl"))
    os.makedirs(a.out, exist_ok=True)
    report = {}
    for pkl in sorted(glob.glob(os.path.join(a.results, "*", "wham_slim.pkl"))):
        clip = os.path.basename(os.path.dirname(pkl))
        res = joblib.load(pkl)
        n = max(int(v["frame_ids"][-1]) + 1 for v in res.values())
        picked = pick_near_players(res, n, net_y=a.net_y, side_x=a.side_x)
        if not picked:
            print(clip, "no near player found", flush=True)
        for side, k in picked.items():
            for mirrored in (False, True):
                track = mirror_track(res[k]) if mirrored else res[k]
                rig, J_rest, verts, contacts, quality, _ = clean_track(track, smpl, a.fps)
                track_name = f"{clip}_near{side}" + ("_mirror" if mirrored else "")
                quality.update(track=int(k), first_frame=int(res[k]["frame_ids"][0]), mirrored=mirrored)
                moves = labelled_moves(rig, clip, OUT_FPS, windows) if a.moves == "labelled" else find_moves(rig, OUT_FPS)
                for i, (s, e, label) in enumerate(moves):
                    name = f"{track_name}_{label}_{i:02d}"
                    write_fbx(os.path.join(a.out, name + ".fbx"), slice_rig(rig, s, e, J_rest[0]), J_rest, verts, smpl)
                    move = dict(quality, frames=int(e - s), from_frame=int(s), label=label)
                    with open(os.path.join(a.out, name + ".json"), "w") as f:
                        json.dump({"quality": move, "contacts": {f_: c[s:e] for f_, c in contacts.items()}}, f)
                    report[name] = move
                    print(name, move["frames"], label, flush=True)
    with open(os.path.join(a.out, "report.json"), "w") as f:
        json.dump(report, f, indent=1)


if __name__ == "__main__":
    main()
