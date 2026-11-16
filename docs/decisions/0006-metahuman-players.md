# 0006: MetaHuman players

**Date:** 2026-11-16 · **Status:** proposed (Agent 7) · Builds on [0005](0005-player-animation.md) (player animation), GDD 6.4 (the slice roster) and 10 (art)

## Decision

The four slice players (Selva Arunde, Mikko Ventara, Ilan Corvisa, Ruben Halvaro) are MetaHumans made in the MetaHuman Creator that ships with UE 5.8 (`MetaHumanCharacter` plugin). They are driven by the animation from 0005 **without changing it**: each pawn keeps its placeholder skeleton running `ABP_PadelPlayer`, hidden, and the MetaHuman's body copies that pose every frame through an IK Retargeter.

## How it works

- `Data/PlayerBodies.json`, keyed by the player's row in `Players.json`, names each player's assembled MetaHuman Blueprint and its body component (`Body`). Asset paths only, no tuning.
- `Data/Animation.json` adds `MetaHumanRetargeter` (the IK Retargeter asset), `MetaHumanMinLod` and `MetaHumanIkLodThreshold`.
- `UPadelPlayerAnimComponent::Init` takes the player's row name. When the placeholder body and `ABP_PadelPlayer` load (as in 0005) **and** the player's MetaHuman and the retargeter are on disk, it spawns the MetaHuman, attaches it to the placeholder skeleton (same origin, same facing), hides the placeholder (which keeps ticking its pose: `AlwaysTickPoseAndRefreshBones`), and gives the MetaHuman body `UPadelMetaHumanAnimInstance`. The racket moves to the MetaHuman's `hand_r`.
- `UPadelMetaHumanAnimInstance` is a C++ anim instance whose only node is the engine's `FAnimNode_RetargetPoseFromMesh` (the "Retarget Pose From Mesh" node) as a custom root, set to read the placeholder component. No AnimBP, so nothing binary to review; the MetaHuman's own face, clothing and post-process AnimBPs still run as assembled.
- Anything missing (a fresh clone has no MetaHumans) logs one line and keeps the placeholder, so CI and other machines are unaffected.

**Why a runtime retarget and not retargeted copies of the clips:** the swing clips are played at explicit times from `AnimClips.json` (0005's contact-frame sync). Retargeting at runtime keeps one set of clips, one AnimBP and one set of contact timings for every body. Its cost is one extra pose evaluation and one retarget per player per frame (four players), which the LOD settings below keep small. If profiling ever says otherwise, the fallback is to batch-retarget the clips onto the MetaHuman skeleton offline and add MetaHuman clip paths; nothing else would change.

**Why one retargeter for four bodies:** all MetaHuman bodies share one skeleton, and the retarget processor initialises against each target mesh's own reference pose, so one asset serves every height and build.

## The retargeter

`Tools/AssetPipeline/unreal/metahuman_setup.py` (run in the editor) builds `IK_PadelPlaceholder` from `SK_PlayerPlaceholderA` with the auto-generated retarget chains, takes the MetaHuman plugin's `IK_MH_IKRig` as the target, and builds `RTG_PadelPlaceholder_To_MetaHuman` with the default op stack, fuzzy chain mapping and the source pose auto-aligned to the MetaHuman's (the placeholder is in a T-pose, MetaHumans in an A-pose). It also reports which players' MetaHumans are on disk, and suggests the right `MetaHumanClass` when the assembled Blueprint has a different name. Like the other pipeline assets, the IK Rig and Retargeter are regenerated from the script, not edited by hand.

## 60 fps on an Apple M2

The broadcast camera (GDD 9) sees players from 8 to 25 m, where a MetaHuman's LOD 0 (the full face rig and the densest meshes) is never visible:

1. **Assemble with the optimised pipeline at Medium quality** (hair cards, not strands; baked, smaller textures; fewer LODs). This is the biggest saving and it's chosen in the Creator (`docs/animation/METAHUMAN_PLAYERS.md`).
2. **Rig with joints only**, no blend shapes: faces don't need expressions at broadcast distance in the slice.
3. **`MetaHumanMinLod`: 1.** The pawn sets this as the minimum LOD on every MetaHuman mesh and on its LOD Sync component.
4. **`MetaHumanIkLodThreshold`: 1.** The retargeter's IK pass (feet and hands) runs only at LOD 0–1; further away only the FK chains are copied.
5. 2K textures when downloading the texture source, not 4K or 8K.

Check with `stat unit` and `stat fps` in a match. If it's under 60, in this order: `MetaHumanMinLod` 2, the Low quality assembly, `MetaHumanIkLodThreshold` 0.

## Git

MetaHuman assets stay out of git: `.gitignore` excludes `Content/MetaHumans/` and `Content/Padel/Characters/MetaHumans/` (where the Creator is told to put the source characters and the assembled output). Each MetaHuman is hundreds of MB to a few GB of binary content, more than the repository's Git LFS can carry comfortably (and `git-lfs` isn't installed on the dev Mac yet, so `.uasset` files committed there would go in as plain blobs). The MetaHuman licence also limits sharing the assets outside Unreal projects, which matters if this repository goes public. Back the `Content/Padel/Characters/MetaHumans/` folder up on its own (it holds the editable source characters too, so the players can be rebuilt). Only code, `Data/` and config are committed.

## Originality

The four are invented: built from the Creator's presets and blends, never from a photo, scan or likeness of a real person (CLAUDE.md hard rules), and never with the "from footage / mesh" identity tools. Their looks are in `docs/animation/METAHUMAN_PLAYERS.md`.

## Moving without ABP_PadelPlayer

0005's AnimBP is still not built, so `UPadelPlayerAnimInstance` can run on its own: used without an AnimBP subclass, its proxy runs `FAnimNode_PadelPlayerPose` as the root, which does the same recipe in C++:

1. **Locomotion:** idle, and walk and jog cycles in four directions (forward, back, left, right, relative to facing the net), blended by direction and speed on one shared phase. Walk rate is `speed / WalkClipSpeedMps`, jog rate is 0005's `StridePlayRate`.
2. **Swing:** the swing clip at its explicit time (0005's contact-frame sync), over the upper body from `spine_01`, or the whole body by `FullBodyAlpha`.
3. **Celebration** on top.
4. **Until swing clips exist,** the right arm reaches for the placeholder racket target with two-bone IK (full during a swing, `ArmReadyAlpha` in the ready stance). That target meets the ball on the contact tick, so the swing is in time with the ball, without any swing clip.

The component picks this class when `ABP_PadelPlayer` is missing and the idle clip is on the mesh's skeleton. A MetaHuman player's hidden driver is always `SK_PlayerPlaceholderA`, whose skeleton the clips and the MetaHuman retargeter are made for. If an `ABP_PadelPlayer` is made later, it wins: an AnimBP subclass brings its own graph and the C++ root is not used.

**The clips** are Epic's UE5 mannequin idle, walk and jog from the engine's Third Person template, retargeted onto the placeholder skeleton by `Tools/AssetPipeline/unreal/animation_setup.py` (it copies the mannequin into `/Game/Characters/Mannequins`, git-ignored, and writes `/Game/Padel/Animations/Locomotion/A_*`). They're Epic content for use in Unreal projects; no padel swing clips exist yet. `Padel.Animation.NativePose` checks that the four pawns get skeletal bodies, the arms come down from the T-pose and the feet stay on the ground (it skips on a clone without the generated clips).

## Contact warp (movement plan N3)

At contact the racket's face centre, not the hand, meets the ball (shots doc 4.2), on the body that's on screen. `PadelContactWarp` runs in the placeholder's pose node, or on a MetaHuman after the retarget (`FAnimNode_PadelMetaHumanWarp` wraps the retarget node), because the bodies' arms differ in length. The hidden placeholder skips its own warp while a MetaHuman shows (`bWarpThisBody`). The face centre in the hand's bone space comes from each body's racket grip (0008, `PadelContactWarp::FaceInHand`).

- The arm's two-bone IK aims the face centre at the racket target and keeps the hand's angle. Past the arm's reach it turns the racket along the arm's line, so the racket's length counts. This is all a clip with zero contact fields gets (K1's `ContactRightM/ForwardM/UpM` all zero), which is today's arm reach, now aimed with the face.
- A clip with contact fields also brings in the body. Its weight rises over the forward swing, peaks at `ContactS` and falls over the follow-through (`PadelContactWarp::BodyWeight`). Below the face, the pelvis drops and the feet stay planted (leg IK). The trunk turns the racket shoulder towards the ball, and the spine bends sideways and forward. If the ball is still out of reach, the spine bends on, up to its limit, and the arm reaches again. The shares and limits are `Data/Animation.json`'s `Warp*` fields (metres and degrees, converted to Unreal units once, in `FPadelAnimData`).
- `Padel.Animation.ContactWarp` checks that the face centre is within 5 cm of the target across heights and both sides: the arm alone from 0.9 to 1.8 m, and with the body down to 0.4 m forehand and 0.5 m backhand. It drives the clip path by giving `ApplyFrame` a clip row with contact fields, because no swing clip has them yet.

## The MetaHuman's arms (movement plan N4)

- **Left-handers** (Players.json `Handedness: Left`, Ruben): on the first step, the anim component moves the racket to `hand_l` and mirrors the grip bones (`_r` to `_l`). The contact warp (`FPadelWarpBones`, `bLeftHanded`) then uses the left arm on the placeholder and on the MetaHuman. The racket target was already mirrored by `ForehandSign`.
- **Forearm twist:** the placeholder skeleton has no twist bones, so the retarget can't drive the MetaHuman's `lowerarm_twist_01/02`. A MetaHuman assembled with its post-process AnimBP spreads the hand's roll up the forearm itself: measured on Ilan, the twist bones carry 80% and 20% of the roll. Without that AnimBP, `PadelContactWarp::DistributeForearmTwist` does it after the warp, each bone in proportion to how far it sits towards the wrist. `Padel.Animation.MetaHumanArms` checks on an overhead that both twist bones turn with the hand (no candy-wrap).
- **Hand IK at the broadcast distance:** the contact warp runs on the MetaHuman whatever its LOD, separately from the retargeter's own IK pass (`MetaHumanIkLodThreshold`). The test forces LOD 1 and 2 and checks that the racket face still meets the ball within 5 cm.
- **Overheads (N4b):** for a target above the shoulder the arm reaches long. The hand goes where the full reach meets the racket's length from the ball, the elbow points up and out to the racket side, and past the arm's reach the player rises onto the toes and leans the trunk in. All of this blends in over `WarpOverheadBlendM` above the shoulder, so forehands and backhands are unchanged. `Padel.Animation.OverheadArm` checks targets from 2.1 to 2.4 m: the elbow is above the shoulder and at least 150° (157° when it isn't fully straight), and the face is within 5 cm.
- **Torso clearance (N4b):** through any arc, the arm IK keeps the forearm `WarpTorsoClearanceM` from the spine (a capsule from the pelvis to the top spine bone) and pushes the hand out when an arc would cross the body. `Padel.Animation.SwingClearance` runs every phase of the game's own forehand, backhand, both chiquitas, smash and bandeja arcs (AnimClips.json keys) and checks that the forearm's midpoint stays outside a 12 cm capsule.
- **Not yet checked:** the 60 fps budget with four MetaHumans and a left-handed MetaHuman. Only Ilan is assembled on the Mac, so these are re-run once all four exist.

## Not covered yet

- Swing clips (0005's list); until then the swing is the arm reaching for the racket target.
- Left-handed players (Ruben) use the same right-hand racket bone as 0005, and the arm reach uses the right arm.
- The racket mesh is still the placeholder box.
