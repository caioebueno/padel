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

## Not covered yet

- The prerequisites of 0005 are still needed before any MetaHuman shows: the placeholder skeletal meshes (`python3 Tools/AssetPipeline/pipeline.py all`, which needs Blender), the placeholder clips and `ABP_PadelPlayer`.
- Left-handed players (Ruben) use the same right-hand racket bone as 0005; the mirroring is the animation model's.
- The racket mesh is still the placeholder box.
