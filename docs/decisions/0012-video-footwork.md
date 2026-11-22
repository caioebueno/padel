# 0012: Padel footwork from match video

**Date:** 2026-11-21 · **Status:** proposed · Builds on [0005](0005-player-animation.md), [0006](0006-metahuman-players.md) and `docs/animation/MOVEMENT_FLUENCY.md` (M1/M2 motion matching)

## Decision

The players' footwork gets a motion matching database of real padel movement: split steps, side shuffles, recovery steps, lunges and the pair moving together. It's taken from video of a professional match, recorded by the project owner with the players' authorization. That gives about 13 minutes of footwork, enough for a first database, without a capture session.

The Game Animation Sample databases stay. The footwork database is searched next to them, in the walk band (under `MotionMatchingRunFromMps`), where nearly all padel footwork happens.

## Pipeline

Three steps, all scripts in the repo:

1. **Video to 3D motion:** `Tools/AssetPipeline/mocap/run_wham.sh` runs [WHAM](https://github.com/yohanshin/WHAM) (MIT licence) on a rented GPU (vast.ai, RTX 3090, about 2 hours for 32 rallies).
   - The input is one clip per rally from the main broadcast camera at 1080p. That camera doesn't move or cut, so WHAM's camera tracking is skipped (`--estimate_local_only`).
   - WHAM tracks everyone in the frame. For each person it writes SMPL body poses and a world path, using foot contacts to keep the path steady.
2. **Clean up and export to FBX:** `Tools/AssetPipeline/mocap/wham_to_fbx.py`, in Python with Blender's `bpy`, writes one FBX per near player per rally into `SourceArt/_incoming/Footwork/`. For each clip it:
   - keeps the longest on-court track in each half of the near court; the far pair is too small and too broken up to use
   - trims the track's ends and resamples it to 30 fps (UE 5.8's FBX import misreads 29.97 fps as 29 and drops the clip)
   - low-pass filters it, the torso hardest (3 Hz) and the legs least (6 Hz)
   - pins the lowest foot to the floor (WHAM's height drifts by up to 0.7 m over a rally)
   - plants the feet: during each foot contact the ankle holds one position and heading, reached with two-bone leg IK and blended in and out over 0.13 s; the heel and toe still roll, and the IK eases off before the leg straightens
   - adds a `root` bone under the pelvis, on the floor and facing the pelvis's heading smoothed to 0.5 Hz (the game's body stays square to the net, so the search's query keeps one facing)
   - names the bones like the Unreal mannequin
   - mirrors every track, then cuts out only the side shuffles (`mocap/footwork_moves.py`: sideways over 0.7 m/s and faster than forwards for at least 0.4 s, with 0.4 s either side, at most 3 s), one FBX per move, named `<rally>_near<L|R>[_mirror]_shuffle_<left|right>_<n>`. Whole-rally clips lost in the search, and their standing stretches slid; the first cut gives 96 shuffles, 142 s, half of them mirrored
   - or, with `--moves labelled`, keeps each clip whole as one move labelled from its name (`<label>-<n>_<time>_<camera>`, as `cut_clips.py` names clips cut from a reference table): for clips already cut around one lunge, split step, crossover and so on, which the shuffle finder would drop. `--net-y` and `--side-x` move the near court's box for cameras other than the broadcast one (a training match's end and side cameras want 0.42 and 0.0: their far pair sits at 0.25-0.39 H). The near pair is the two longest on-court tracks on screen together, not one per half, because a side camera often has both near players right of the middle
   - writes a JSON next to the FBX with the foot contacts and quality numbers
3. **Build the database in Unreal:** `Tools/AssetPipeline/unreal/footwork_setup.py`, run after `animation_setup.py`:
   - imports the FBXs
   - builds `IK_PadelFootwork` (auto chains, which match `IK_PadelPlaceholder`'s one to one because the bone names match, and both are T-poses) and `RTG_Footwork_To_Placeholder`
   - retargets the clips onto the placeholder skeleton (root motion on, root locked, like the sample's clips)
   - builds `PSD_Padel_Footwork` on the walk loops' schema and settings, with the cost overrides in `Tools/AssetPipeline/config/footwork.json` (a little dearer to jump into, cheaper to stay in, clip ends never picked)
   - scores it with the sample's normalization set, which stays over the sample's databases only (folding the footwork into it shifted the sample's own picks), and rebuilds every index

## The data contract

`Data/Animation.json` gains `MotionMatchingExtraDatabases`: databases searched with `MotionMatchingDatabases` when they're built, and skipped with one log line when they aren't. The main list stays all-or-none, because the sample set needs every database. The footwork database is listed there, and in `MotionMatchingReadyDatabases` (added after the first merge): it is searched only while shuffling: in a point (any locomotion state but the between-points Idle: a shuffle in a point is Run, not Ready), in the walk band, with the sideways share of the coming travel at least `MotionMatchingShuffleSideShare` (0.6). Then it is the only database searched. Searched alongside the sample's clips, even as short single-heading shuffles, it lost every shuffle frame (best cost about 0.15 against 0.015, likely its slower speeds and crouched pose), and searched while standing it won as an idle and slid. Everywhere else the sample clips play alone. `Padel.Animation.MotionMatchingReady` runs the probe's path in the ready stance, checks the footwork plays in the side shuffle, and holds it to the sample's gates. Each shuffle clip's root holds the move's mean heading, like the game's body square to the net (a root turning with the hips, about 70 degrees a shuffle, lost the trajectory cost by about ten times).

A clone without the footwork keeps working exactly as before. `FPadelAnimData` checks that the chooser lists name only main or extra databases, and that no database is in both lists.

## Quality, measured on the first full run

- 63 clips, 13.0 minutes.
- Bodies about 1.70 m tall. Each player covers 2 to 19 m per rally, at up to about 2.3 m/s.
- Straight from WHAM, a planted foot slides about 15 cm/s. With the first clips in the search, `Padel.Animation.MotionMatching` failed: 72 to 75 cm/s of planted-foot slide, 2.07 clip switches a second and about 30% more pelvis and spine jerk than its limits. The leg IK plants bring the clips' own slide to about 2 cm/s, and the torso filter cuts pelvis jerk about three times; the cost overrides take on the switching.
- On a skeleton drawn every 40 frames, stances, shuffles, lunges and reaches read as human. Moments where two players overlap, or where a track starts or ends, can show an odd pose; the 0.3 s trim takes most of them.
- **First merge: in the search, the footwork didn't win.** Searched in the walk band (its role here), it loses to the sample's clips everywhere, including the test's 2 m/s side shuffle (by 0.2-0.5 in cost): only 3% of the footwork is over 2 m/s, and about three quarters is under 1 m/s, where the chooser counts the body as still. Searching it while still as well was tried: it then replaced the idle when standing, and planted-foot slide rose past `Padel.Animation.MotionMatching`'s limits (37 against 25 cm/s), so that was reverted. Until a follow-up gives the footwork its own moments (for example a ready-position or split-step state from gameplay, cut and labelled clips, mirroring), the database is built and searched but changes nothing on screen.
- What's still missing: sampling exclusions for the bad stretches, mirroring, and the far pair.

## Rights and licences

- **The footage:** recorded by the project owner, with the players' authorization, which is kept with the project records. Nothing from the video is committed.
- **Likeness** (CLAUDE.md hard rules): the motion drives the game's own invented MetaHumans (0006). Nothing identifies the players: no names, faces, bodies, kit, or signature celebrations or moves. The footwork is generic padel movement.
- **SMPL:** WHAM's output is SMPL body parameters, and the SMPL model's licence is non-commercial. So:
  - the FBXs (which carry the SMPL body), the WHAM results and every Unreal asset made from them stay git-ignored, on the machine that made them, like the MetaHumans
  - **before anything made from this ships in a commercial build, the project needs a commercial SMPL licence from Meshcapade**, or the motion must be re-captured another way (for example, a capture session with our own performers, `docs/animation/MOCAP_SHOT_LIST.md`)
- **GVHMR** was considered and rejected: its code licence bans commercial use.

## Re-running

1. Cut the main-camera rallies (one still shot each, no replays).
2. Run `run_wham.sh` on a GPU and download `wham_results.tar.gz`. It's about 50 MB with the vertices dropped.
3. Unpack it and run `python3.11 Tools/AssetPipeline/mocap/wham_to_fbx.py <results> SourceArt/_incoming/Footwork`.
4. In the editor, run `animation_setup.py`, then `footwork_setup.py`.
