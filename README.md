# Padel

An original 2v2 padel sim built in Unreal Engine 5. See `docs/GDD.md` and `CLAUDE.md`.

## Setup (macOS on Apple Silicon)

1. Install **Unreal Engine 5.8** from the Epic Games Launcher.
2. Install **Xcode**, using the version Epic lists for 5.8 on [macOS development requirements](https://dev.epicgames.com/documentation/unreal-engine/macos-development-requirements-for-unreal-engine). Open it once to accept the licence. A C++ project can't open without it.
3. Run `git lfs install` once. Binary assets (`.uasset`, `.umap`, textures, audio) go through Git LFS.
4. Double-click `Padel.uproject`. The first time, Unreal says the modules are missing and offers to rebuild them: choose **Yes**. Or build from a terminal:

   ```
   "/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" PadelEditor Mac Development -Project="$PWD/Padel.uproject"
   ```

   To work in Xcode, right-click `Padel.uproject` and choose **Services > Generate Xcode Project**, then open `Padel (Mac).xcworkspace`.
5. Press **Play**. The editor opens on an empty map, and `APadelGameMode` spawns the placeholder court, the lights and the broadcast camera.

On Windows, follow the same steps with Visual Studio 2022 and Unreal's Windows requirements.

## Tests

```
Tools/RunNativeTests.sh     # PadelCore without the engine (CMake + a C++20 compiler)
Tools/RunUnrealTests.sh     # Unreal Automation tests (Padel.*), headless; needs UE 5.8
```

In the editor, the same Unreal tests are under **Tools > Session Frontend > Automation**, filtered by `Padel`.

CI (`.github/workflows/tests.yml`) runs the native tests on Linux and Apple Silicon and checks the project files on every PR. GitHub's own runners have no Unreal install, so the Unreal job only runs on a self-hosted runner. To turn it on, register your Mac as a [self-hosted runner](https://docs.github.com/en/actions/hosting-your-own-runners) with the label `unreal`, then set the repository variable `UNREAL_RUNNER` to `true`.

## The court

`APadelCourt` builds a regulation court out of engine cubes (GDD section 2):

- 20 × 10 m.
- 3 m back glass, with mesh up to 4 m.
- Side glass of 3 m, then a 2 m step.
- A 2 m exit on each side of each half.
- Mesh from the exits to the net.
- A net of 0.88 m in the centre and 0.92 m at the posts.
- Run-off floor for out-of-court play, and an invisible ceiling at 8 m.

The geometry comes from `Source/PadelCore/Public/CourtLayout.h`, which is built from `CourtSpec.h`. The native tests ray-cast against the same boxes.

Every collider carries the component tags `Surface.<Ground|Glass|Mesh|Net|Ceiling>` and `Zone.<Court|Runoff|Ceiling>`. Glass and mesh are invisible colliders with a dark frame until real materials exist. The ball doesn't use these colliders: it runs on `PadelCore`'s own integrator, which reads `CourtSpec` directly.

Coordinates: X along the length (Team A on +X), Y across, Z up, net at X = 0. `PadelCore` is in metres, and the Unreal world in centimetres. The only conversion is in `Court/PadelCourtUnits.h`.

## Art assets

`python3 Tools/AssetPipeline/pipeline.py all` builds the court, glass, net, ball, racket and two rigged placeholder players in Blender, and imports them into `Content/Padel/` with their materials. You need Blender and Unreal 5.8 installed, and the editor closed. AI-generated models (Meshy or Tripo) are switched off until an API key is set. Setup and options are in [Tools/AssetPipeline/README.md](Tools/AssetPipeline/README.md).

## Input

Enhanced Input, gamepad first (GDD section 7). `UPadelInputConfig` creates `IA_Move`, `IA_Sprint`, `IA_ShotDrive`, `IA_ShotLob`, `IA_ShotOverhead`, `IA_ShotDrop`, `IA_ModAttack`, `IA_ModTouch`, `IA_CmdNet`, `IA_CmdBack`, `IA_CmdMiddle`, `IA_CmdYours` and `IA_Pause` in `IMC_Match`, from the binding table in `PadelInputConfig.cpp`. `APadelPlayerController` adds the context. Bind actions with `GetInputConfig()->GetAction(EPadelInputAction::...)`. `IA_Move` is 2D, and up on the stick, or W, is +Y ("toward the opponents").
