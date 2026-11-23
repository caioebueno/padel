"""Cuts a cleaned footwork track into short, labelled moves for motion matching, and mirrors
tracks (docs/decisions/0012-video-footwork.md). Used by wham_to_fbx.py.

Whole-rally tracks lost in the search: standing stretches won as an idle and slid, and the
moving stretches were buried in noise and turning. Motion matching wants short clean moves,
so only the side shuffles are kept (the sample has no strafing, and it is padel's commonest
footwork), each with a little lead-in and run-out, and every track is mirrored so both
directions come from both players.

Clips already cut around one labelled move (the reference tables in PadelData, cut with
cut_clips.py and named `<label>-<n>_<time>_<camera>`) keep that label instead: `--moves
labelled` in wham_to_fbx.py. The shuffle finder would drop most of them, because a lunge, split
step or crossover is not a sideways run. Those clips run about 5 s, mostly standing around the
move, and whole they were never picked (each root holds one heading, which over 5 s of turning
fits few frames). So each is cut into the short windows where the player moves
(`labelled_windows` in config/footwork.json).
"""
import re

import numpy as np

# SMPL left/right joint pairs (hips, knees, ankles, feet, collars, shoulders, elbows, wrists, hands).
MIRROR_PAIRS = [(1, 2), (4, 5), (7, 8), (10, 11), (13, 14), (16, 17), (18, 19), (20, 21), (22, 23)]

# A shuffle: sideways speed (in the root's frame) over SHUFFLE_MIN_MPS and at least
# SHUFFLE_SIDE_RATIO times the forward speed, for SHUFFLE_MIN_S; gaps under SHUFFLE_GAP_S
# are bridged. Each move keeps MOVE_PAD_S either side (the database never picks its first and
# last 0.3 s: config/footwork.json), and runs at most MOVE_MAX_S.
SHUFFLE_MIN_MPS = 0.7
SHUFFLE_SIDE_RATIO = 1.2
SHUFFLE_MIN_S = 0.4
SHUFFLE_GAP_S = 0.15
MOVE_PAD_S = 0.3   # = the database's exclusion, so the near-still lead-in is never picked
MOVE_MAX_S = 3.0


def mirror_track(v):
    """WHAM track mirrored left-right: x negated, left and right joints swapped.

    For a reflection M = diag(-1, 1, 1), a rotation R becomes M R M: an axis-angle (x, y, z)
    becomes (x, -y, -z).
    """
    out = dict(v)
    pose = np.asarray(v["pose_world"]).reshape(len(v["pose_world"]), 24, 3).copy()
    for a, b in MIRROR_PAIRS:
        pose[:, [a, b]] = pose[:, [b, a]]
    pose[..., 1:] *= -1
    out["pose_world"] = pose.reshape(len(pose), -1)
    trans = np.asarray(v["trans_world"]).copy()
    trans[:, 0] *= -1
    out["trans_world"] = trans
    return out


def _segments(mask):
    d = np.diff(np.concatenate([[0], mask.astype(int), [0]]))
    return list(zip(np.nonzero(d == 1)[0], np.nonzero(d == -1)[0]))


def find_moves(rig, fps):
    """[(start, end, label)] frame ranges of the side shuffles, padded, in rig frames."""
    from scipy.spatial.transform import Rotation as R
    T = len(rig["root_pos"])
    if T < 4:
        return []
    heading = R.from_quat(rig["root_q"]).as_rotvec()[:, 1]       # the root turns about +y only
    vel = np.gradient(rig["root_pos"], axis=0) * fps
    fwd = np.stack([np.sin(heading), np.zeros(T), np.cos(heading)], 1)
    left = np.stack([np.cos(heading), np.zeros(T), -np.sin(heading)], 1)   # SMPL's left is +x facing +z
    side = (vel * left).sum(1)
    ahead = (vel * fwd).sum(1)
    k = max(1, int(round(0.1 * fps)))
    side = np.convolve(side, np.ones(k) / k, mode="same")
    ahead = np.convolve(ahead, np.ones(k) / k, mode="same")
    moving = (np.abs(side) > SHUFFLE_MIN_MPS) & (np.abs(side) > SHUFFLE_SIDE_RATIO * np.abs(ahead))
    gap = int(round(SHUFFLE_GAP_S * fps))
    segs = _segments(moving)
    for (s0, e0), (s1, _) in zip(segs, segs[1:]):
        if s1 - e0 <= gap and np.sign(side[e0 - 1]) == np.sign(side[s1]):
            moving[e0:s1] = True
    pad, shortest, longest = int(round(MOVE_PAD_S * fps)), int(round(SHUFFLE_MIN_S * fps)), int(round(MOVE_MAX_S * fps))
    moves = []
    for s, e in _segments(moving):
        if e - s < shortest:
            continue
        label = "shuffle_left" if np.median(side[s:e]) > 0 else "shuffle_right"
        s, e = max(0, s - pad), min(T, e + pad)
        for a in range(s, e, longest):
            b = min(e, a + longest)
            if b - a >= shortest + pad:
                moves.append((a, b, label))
    return moves


def label_from_clip(clip):
    """The move label at the front of a cut clip's name: `lunge-01_6m49s_side` -> `lunge`."""
    m = re.match(r"([a-z][a-z_]*?)-\d+", clip)
    return m.group(1) if m else "move"


def active_windows(root_pos, fps, cfg):
    """[(start, end)] frame windows where the root moves, for motion matching.

    Moving: the root's ground speed, smoothed over 0.1 s, over cfg["min_mps"], for at least
    cfg["min_s"], gaps up to cfg["gap_s"] bridged. Each stretch keeps cfg["pad_s"] either side
    (the database never picks a clip's first and last 0.3 s), overlapping stretches merge, and a
    stretch longer than cfg["max_s"] is split into equal parts no longer than that. Windows under
    cfg["min_window_s"] are dropped.
    """
    T = len(root_pos)
    if T < 4:
        return []
    vel = np.gradient(np.asarray(root_pos, dtype=float), axis=0) * fps
    speed = np.hypot(vel[:, 0], vel[:, 2])           # y is up
    k = max(1, int(round(0.1 * fps)))
    speed = np.convolve(speed, np.ones(k) / k, mode="same")
    moving = speed > cfg["min_mps"]
    gap = int(round(cfg["gap_s"] * fps))
    segs = _segments(moving)
    for (_, e0), (s1, _) in zip(segs, segs[1:]):
        if s1 - e0 <= gap:
            moving[e0:s1] = True
    pad = int(round(cfg["pad_s"] * fps))
    shortest = int(round(cfg["min_s"] * fps))
    stretches = []
    for s, e in _segments(moving):
        if e - s < shortest:
            continue
        s, e = max(0, s - pad), min(T, e + pad)
        if stretches and s <= stretches[-1][1]:
            stretches[-1] = (stretches[-1][0], max(e, stretches[-1][1]))
        else:
            stretches.append((s, e))
    longest = max(1, int(round(cfg["max_s"] * fps)))
    least = int(round(cfg["min_window_s"] * fps))
    windows = []
    for s, e in stretches:
        parts = int(np.ceil((e - s) / longest))
        edges = np.linspace(s, e, parts + 1).round().astype(int)
        windows += [(int(a), int(b)) for a, b in zip(edges, edges[1:]) if b - a >= least]
    return windows


def median_speed(root_pos, fps):
    """The root's median ground speed (m/s, y up) over a window."""
    vel = np.gradient(np.asarray(root_pos, dtype=float), axis=0) * fps
    return float(np.median(np.hypot(vel[:, 0], vel[:, 2]))) if len(vel) else 0.0


def labelled_moves(rig, clip, fps, cfg):
    """[(start, end, label)]: the moving windows of a cut clip, labelled from the clip's name.

    cfg["min_window_mps"] maps a label to the median speed its windows need: most video shuffles
    run about 0.7 m/s, and the game's shuffles at 1 to 1.5 m/s, beyond what the play rate can
    stretch (0012); slower windows played under a real shuffle slid.
    """
    label = label_from_clip(clip)
    least = cfg.get("min_window_mps", {}).get(label, 0.0)
    return [(s, e, label) for s, e in active_windows(rig["root_pos"], fps, cfg)
            if median_speed(rig["root_pos"][s:e], fps) >= least]


def slice_rig(rig, s, e, pelvis_rest):
    """Frames s..e of a rig, with the root holding one heading (the move's mean).

    In the game the body stays square to the net through a shuffle and the search's query keeps
    one facing; a root that turned with the hips (about 70 degrees over a shuffle) made every
    footwork pose lose the trajectory cost by about ten times. The pelvis keeps its own turn.
    pelvis_rest: the pelvis joint's rest position (J_rest[0]).
    """
    from scipy.spatial.transform import Rotation as R
    out = {k: v[s:e].copy() for k, v in rig.items()}
    old = R.from_quat(out["root_q"])
    yaw = old.as_rotvec()[:, 1]
    mean = np.arctan2(np.sin(yaw).mean(), np.cos(yaw).mean())
    new = R.from_euler("y", np.full(len(yaw), mean)[:, None])
    turn = new.inv() * old
    q = out["q"].copy()
    q[:, 0] = (turn * R.from_quat(q[:, 0])).as_quat()
    out["q"] = q
    out["pelvis_pos"] = turn.apply(out["pelvis_pos"] + pelvis_rest) - pelvis_rest
    out["root_q"] = new.as_quat()
    return out
