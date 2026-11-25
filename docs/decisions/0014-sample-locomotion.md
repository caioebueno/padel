# 0014: Player locomotion from Epic's Game Animation Sample

**Date:** 2026-11-25 · **Status:** draft, for Caio's approval · Replaces [0005](0005-player-animation.md)'s locomotion and the placeholder-skeleton part of [0006](0006-metahuman-players.md). Swings, shot timing and the racket arm (0005, 0008) are unchanged for now.

## Why

Players looked unhuman, and a chain of probe-driven fixes (docs/animation/MOVEMENT_FLUENCY.md, M1 and M2) didn't change that. Phase 0 and phase 1 (videos in `~/Documents/PadelData/`) showed why:

- The Game Animation Sample's own character, run as Epic ships it with the same clips, looks human.
- Ours didn't. The gap is in what we built around the clips:
  - A home-made matcher and warps.
  - The pose pinned to the simulation every frame.
  - Two retargets through a 23-bone placeholder in a T pose. Epic's single retarget keeps the feet as the source has them; ours tripled the slide.

Caio chose to restart player animation from the sample (2026-11-24).

## Decision

1. **The sample's locomotion set, as shipped.**
   - **What:** the UEFN mannequin skeleton and clips (no retarget), the Dense Stand pose search databases with their schemas and normalization set, and Epic's mannequin → MetaHuman retargeter (`RTG_UEFN_to_Metahuman_nrw`).
   - **How it gets here:** `Tools/AssetPipeline/unreal/gasp_migrate.py` copies them from the sample project. They are Epic content for Unreal projects and stay git-ignored, like the clips today.
2. **The sample's graph, assembled in C++ from the engine's own nodes:** `FAnimNode_PadelSampleLocomotion` (`Source/Padel/Public/Animation/PadelSampleLocomotion.h`) wires:
   - `FAnimNode_MotionMatching` (a blend stack: 0.5 s blends, the sample's FastFeet+Root profile, play rate 0.85–1.15);
   - `FAnimNode_PoseSearchHistoryCollector`;
   - `FAnimNode_Steering` (the clip's root motion turned toward the pawn's facing);
   - `FAnimNode_OffsetRootBone` (accumulated rotation, 0.2 s translation half-life, 30 cm max lag).

   No AnimBP. The settings are the sample's, in `Data/SampleLocomotion.json`.
3. **The sample's chooser rules in C++.**
   - **Source:** `CHT_PoseSearchDatabases_Dense` and the AnimBP's IsMoving, IsStarting, IsPivoting, ShouldTurnInPlace and interrupt-mode functions, read from the sample's text export.
   - **Rules:** idles below 20 cm/s; the stops of each gait above its speed; turns in place when the drawn root lags the facing by 50° after stopping; while moving, the gait's loops, plus its starts while starting and its pivots while pivoting; a change of movement state or gait interrupts the continuing pose.
   - **Difference from the sample:** the sample takes the gait from the player's input. Here it comes from the speed, with a 10% band either side of each gait's edge.
4. **PadelCore stays the authority.**
   - The pawn is where the deterministic simulation puts it; no CharacterMovementComponent, no gameplay root motion.
   - **Trajectory:** the motion matching trajectory is built from PadelCore. The past is where the pawn was (30 samples, 0.04 s apart). The future is the model's velocity and acceleration carried on, 15 samples 0.1 s apart. The future facing turns toward the travel when running, and holds when strafing.
   - **Root motion:** the clips' root motion is extracted and only used by steering and the offset root.
5. **The drawn body may lag the simulation, within limits** (offset root bone: up to 30 cm, and the rotation it accumulates until steering or a turn in place uses it up). Presentation only: the simulation, the ball, the referee and the AI never read it. A shot's contact is still where the simulation says, and the racket-arm IK reaches it from wherever the body is drawn.
6. **One retarget.**
   - The hidden driver is the sample's mannequin. The MetaHuman copies it through Epic's retargeter (`UPadelMetaHumanAnimInstance`, unchanged).
   - The placeholder skeleton, `RTG_Mannequin_To_Placeholder` and `RTG_PadelPlaceholder_To_MetaHuman` are not used on this path.
7. **Behind a switch until accepted.** The console variable `Padel.Animation.SampleLocomotion` (default 0) picks the path when a player's body is set up. At 0 everything is exactly as before.

## What goes when the switch is removed

- The placeholder meshes and their two retargeters.
- `FPadelMotionMatcher` and `UPadelMotionMatchingBuilder`'s database building.
- `FPadelLocomotionWarp`.
- The F1/P1 body layers in `PadelBodyDynamics`: facing spring, lean, pelvis sink and stance poses. They come back only as additives where a video shows something missing.
- The loop-mix fallback, and the M1/M2 keys in `Animation.json` (a schema change, in its own PR).

## What stays

- PadelCore movement and the whole simulation.
- `FPlayerAnimModel`'s decisions as inputs: facing intent, stance choice, swing selection and timing, look-at target, split-step timing.
- `PadelContactWarp` (racket arm), re-cached on the new bones.
- `UPadelMatchEvents`.
- The probes and video recorders.

## Order of work, each step checked on video against the sample reference

1. **Locomotion on one pawn on our court.** This decision's first pass. Video: `~/Documents/PadelData/restart-step2/`.
2. **Facing the net and strafing:** orientation warping inside the blend stack, as the sample does; the shuffle databases.
3. **Ready stance:** the sample has none, so it needs clips.
4. **Split step.**
5. **Swings:** an upper-body slot over the locomotion.
6. **Movement tuning:** PadelCore speeds, acceleration and turn rates within what the clips cover. GDD movement section, with an AI soak run and a playtest.

## Open: cooked builds

The engine's motion matching, blend stack and offset root bone nodes keep some settings as folded anim node data (`FAnimNode_Base::GetData`). In the editor a node built in C++ reads them from itself. In a cooked build only a node compiled into an AnimBP has that data, so this C++ graph runs in editor builds only (tests, videos, playing in the editor) until one of these lands:
- **(a)** A node-wiring-only AnimBP generated from C++ by the asset pipeline (the 0005 recipe: no logic in the graph, every input bound to the C++ anim instance).
- **(b)** Our own thin wrappers over the engine's search and offset-root code.

This must be settled before the Windows build ships.

## Needs changing elsewhere

- **docs/animation/MOVEMENT_FLUENCY.md:** "root motion is not used; the simulation leads" becomes "the simulation leads; the drawn body may lag it within the offset root's limits".
- **CLAUDE.md:**
  - Plugins in use: add BlendStack and the AnimationWarping runtime, which Pose Search already pulls in.
  - Folder layout: the mannequin and the sample content under the git-ignored `Content/Characters/`.
- **GDD movement section and `Data/Movement.json`:** only when step 6 retunes speeds or turn rates.
- **Licences:** the sample's content is Epic content for Unreal projects (as 0006 notes). The sample's MetaHuman (Kellan) is used only in the reference video, never in the game.
