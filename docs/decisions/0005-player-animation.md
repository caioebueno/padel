# 0005: Player animation

**Date:** 2026-11-16 · **Status:** proposed (Agent 6) · Implements GDD 6.3 and 10 (animation), on the swing phases of [0004](0004-shot-system.md)

## Where things live

- **Engine-free model in `Source/Padel/{Public,Private}/Animation/Model/`** (namespace `Padel::Animation`): `FPlayerAnimModel` turns the shot controller's phases and the player's movement into an animation frame. It's plain C++ like the other `*/Model/` folders, so the native tests run it against the real `FPlayerShotController` and ball machine on every PR.
- **Tuning in `Data/`**: `Animation.json` (one `default` row: locomotion thresholds, blend times, trajectory sample times, celebration clips, asset paths) and `AnimClips.json` (one row per swing clip: its contact timing and the placeholder racket arc). Both load through DataTables (`FPadelAnimSettingsRow`, `FPadelAnimClipRow`, `FPadelAnimData`).
- **Unreal side in `Source/Padel/*/Animation/`**: `UPadelPlayerAnimComponent` (owns the model, steps it, presents the frame) and `UPadelPlayerAnimInstance` (the C++ parent of `ABP_PadelPlayer`; the AnimBP only wires nodes to its properties).
- **Hook-up:** `APadelPlayerPawn` gets a hidden `USkeletalMeshComponent` and the anim component; `APadelPracticeDirector` calls `StepAnimation` right after `FPlayerShotController::Tick`. The match director will do the same for all four players.
- **Small additions outside this area:** read-only getters on `FPlayerShotController` (`IsServing`, `IsIdealBounced`, `IsAttackHeld`, `IsTouchHeld`, `GetRatings`, `GetLibrary`), the `PoseSearch` plugin in `Padel.uproject` (already listed in CLAUDE.md) and the `Animation/` line in CLAUDE.md's folder layout.

## Contact-frame sync

Every swing clip is driven by one **swing progress** `P`, and the AnimBP plays the clip at an explicit time (a Sequence Evaluator, not a montage), so nothing drifts between the simulation and the pose:

| `P` | Phase (0004) | Clip time |
|---|---|---|
| 0 → 1 | `Windup` | `WindupS × Charge`: the backswing is scrubbed by the charge and holds at full |
| 1 → 2 | `Forward` (early release only) | from the backswing to `ContactS`, timed to arrive on the tick the controller makes contact |
| **2** | **contact tick** (`Hit`, `Whiff` or `ShadowSwing`) | **exactly `ContactS`** |
| 2 → 3 | `FollowThrough` | `ContactS` → `EndS` over the played shot's `FollowThroughS` |
| 3 | `Recovery` | holds `EndS` while the swing layer fades out over `RecoveryS` |

- **Early release:** the forward swing runs over the time left to the ideal moment, rounded up to the tick the controller hits on, and holds just short of the contact frame until that tick.
- **Late release (and a release at the ideal moment):** 0004 makes contact on the release tick, so there is no forward phase to show. The pose goes straight from the backswing to the contact frame in one tick, which reads as a fast whip at 60 fps. This is the one place to watch in playtests (`docs/PLAYTEST.md`); the fix, if it looks wrong, is a short anticipation in the controller, not in animation.
- **Which clip:** the windup plays the clip of the shot the button most likely plays (drive or volley by the predicted bounce, lob, bandeja or víbora with RB, chiquita with LB, flat or kick serve). At contact the played shot's clip for the grade takes over with a short crossfade. Both clips are driven by the same `P`, so both show their own contact frame on the contact tick.
- **Versions:** Perfect and Good play `perfect`, Early `early`, Late `late`. A mishit plays its button's family clip in the early or late version by which way it missed. A whiff or shadow swing plays the family clip.
- **Side:** forehand or backhand from where the ball is across the player, flipped for left-handers. A missing backhand clip falls back to the forehand, a missing version to `perfect`.

The native tests (`Tests/Native/PlayerAnimTests.cpp`) check this for every slice shot in all three versions, both serves, the backhand, an AI swing and the early forward swing: the contact frame shows on the hit tick and on no other tick, and the placeholder racket is on the ball. `Padel.Animation.ContactSync` checks the same through the Unreal data loader.

## Feet

- A swing drives the legs only while the player is slower than `PlantedMaxSpeedMps` (0.8 m/s). Above that the swing plays on the upper body (layered blend from `spine_01`) over the locomotion, so a swing on the run never drags planted feet. `FootSlideMps` reports the speed the feet would slide at; the tests keep it under the planted limit through every slice shot.
- The run and sprint cycles play at `speed / authored speed` (clamped), so the stride matches the ground speed. With M1 motion matching (below) the picked clip plays at `speed / its root speed`, within `MotionMatchingPlayRateMin`..`Max`; the loop mix is the fallback.
- 0004 already slows movement to the shot's `SwingMoveMult` during a swing, which keeps the player nearly planted.

## Locomotion

`Idle` (between points), `Ready` (rally live, a ball coming within `ReadyBallWindowS`, or swinging), `Run`, `Sprint` (sprint held and fast), `SplitStep` (`FPlayerMotion::IsSplitHopping`). Direction is relative to facing the net: padel players shuffle sideways and backwards far more than they turn, so the body faces the net and the locomotion set needs strafes and backpedals.

For Motion Matching the model gives a trajectory: the real past positions and the current velocity carried forward, at `TrajectoryTimesS` (−0.4 to +1.0 s, covering the Game Animation Sample schemas' samples at −0.05, 0, 0.35, 0.7 and 1.0 s). `UPadelPlayerAnimInstance` turns it into an `FTransformTrajectory` on the mesh component (its height, and its facing at each past time and now for the future), and the C++ pose searches with it (M1, `docs/animation/MOVEMENT_FLUENCY.md`).

## Placeholder body until clips exist

Until the skeletal mesh and `ABP_PadelPlayer` are imported, the pawn keeps its cylinder and the anim component moves the placeholder racket along a procedural arc: ready → backswing (from the clip row) → **the ball** at contact → follow-through, mirrored for the backhand. That makes "players visibly swing in time with the ball" testable today in practice mode. When the skeletal body loads, the racket attaches to `hand_r` instead.

## What needs a Mac (no engine in CI)

1. Build `PadelEditor` and run `Tools/RunUnrealTests.sh Padel.Animation` (two new tests) plus the rest of `Padel.*`. The C++ here has not been compiled against UE 5.8 yet.
2. Get placeholder clips (next section) and import them under `/Game/Padel/Animations/Swings/` with the names in `AnimClips.json` (`A_Drive_Fore_Perfect`...).
3. Create `ABP_PadelPlayer` (recipe below) on the `SK_PlayerPlaceholderA` skeleton.
4. Play practice mode and check the swing against the ball.

## Placeholder clips

The placeholder characters from the asset pipeline use the UE5 mannequin bone names, so clips made for the mannequin play on them directly (or through an IK Retargeter if the proportions look off).

- **Locomotion:** idle, ready stance, run, sprint, strafes, backpedal and a small hop from Epic's free animation samples for the UE5 mannequin, or Mixamo. Retarget to the placeholder skeleton.
- **Swings:** there are no padel swings to download. Use the closest tennis-like or generic swing clips for the forehand, backhand, overhead and serve, retarget them, and set `WindupS`, `ContactS` and `EndS` in `AnimClips.json` to each clip's real frames (contact time = contact frame ÷ frame rate). The same clip can stand in for all three versions until mocap.
- **Licences:** Mixamo and Epic sample clips can be used in a game but not redistributed as raw files. Keep the downloaded files in `SourceArt/_incoming/` (git-ignored), and record each one in `SourceArt/manifest.csv` with its source and licence, as the asset pipeline does for hand-off files. Nothing that copies a real player's signature motion.

## ABP_PadelPlayer recipe

Parent class `UPadelPlayerAnimInstance`, skeleton `SK_PlayerPlaceholderA`'s. The graph reads only the C++ properties; no Event Graph logic.

1. **Locomotion:** a `Motion Matching` node on a Pose Search database of the locomotion clips (schema: trajectory at the `TrajectoryTimes`, feet and pelvis positions and velocities). Until the database exists, a 2D blend space on `Speed` and `Direction` with play rate `StridePlayRate`. A blend-poses-by-enum on `Locomotion` picks Idle, Ready and SplitStep poses over it.
2. **Swing:** two `Sequence Evaluator` nodes, Sequence pins bound to `SwingSequence` and `PrevSwingSequence`, Explicit Time to `SwingTime` and `PrevSwingTime`, "Teleport to explicit time" on. Blend them by `SwingClipAlpha`.
3. **Upper and full body:** `Layered blend per bone` from `spine_01` (locomotion base, swing on top) blended with the full-body swing by `FullBodyAlpha`; the result over locomotion by `SwingWeight`.
4. **Celebration:** a third Sequence Evaluator on `CelebrationSequence` / `CelebrationTime`, blended on top by `CelebrationWeight`.
5. **Output:** an `Inertialization` node before the output pose for state changes. It never touches the contact frame, because the swing time is set explicitly every frame.

Motion Matching needs its trajectory as the engine's trajectory struct: `MotionTrajectory` (an `FTransformTrajectory`, 5.8's type in the Engine module; PoseSearch's `FPoseSearchQueryTrajectory` is deprecated) is built from `Trajectory` and `TrajectoryTimes` in `ApplyFrame`. An AnimBP would feed it to a Pose History node's Trajectory pin; the C++ pose (0006) uses it directly.

## Motion capture

The shot list is in [`docs/animation/MOCAP_SHOT_LIST.md`](../animation/MOCAP_SHOT_LIST.md).
