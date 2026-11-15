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
5. Press **Play**. The editor opens on an empty map, and `APadelGameMode` spawns the placeholder court, the lights and the broadcast camera. The main menu comes up over it (see [Menus and HUD](#menus-and-hud)).

On Windows, follow the same steps with Visual Studio 2022 and Unreal's Windows requirements.

## Tests

```
Tools/RunNativeTests.sh     # PadelCore and the UI/camera models without the engine (CMake + a C++20 compiler)
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

## Input

Enhanced Input, gamepad first (GDD section 7). `UPadelInputConfig` creates `IA_Move`, `IA_Sprint`, `IA_ShotDrive`, `IA_ShotLob`, `IA_ShotOverhead`, `IA_ShotDrop`, `IA_ModAttack`, `IA_ModTouch`, `IA_CmdNet`, `IA_CmdBack`, `IA_CmdMiddle`, `IA_CmdYours` and `IA_Pause` in `IMC_Match`, from the binding table in `PadelInputConfig.cpp`. `APadelPlayerController` adds the context. Bind actions with `GetInputConfig()->GetAction(EPadelInputAction::...)`. `IA_Move` is 2D, and up on the stick, or W, is +Y ("toward the opponents").

## Menus and HUD

`APadelHUD` runs Quick Match from the main menu to the stats screen (GDD 11): **Quick Match**, then the setup screen (difficulty, 1 set or best of 3, golden point, which Team A player you control), then the match, **Pause** (Start / Esc), and the stats screen. Everything works with a gamepad: the D-pad or stick moves, A selects, left and right change a setup option, and B goes back.

During the match the HUD shows the score bug (top left, serve dot, golden point and big-point tags), callouts for faults, lets and how each point ended, the timing grade after your shots, partner commands (D-pad / 1-4) with the right button name for Xbox, PlayStation or Switch pads, the golden point receiver prompt, and a big score card between games and on change of ends. The broadcast camera tracks the ball sideways and moves to the other end on a change of ends (values in `Data/Camera.json`).

Until the match simulation is wired in, a scripted demo match (`FDemoMatch`) plays the points so the whole flow can be tried. Widgets are built in C++ (`UPadelWidgetBase::BuildTree`), not Widget Blueprints. The text, stats and camera maths are engine-free (`*/Model/`) and covered by the native tests. See `docs/decisions/0003-ui-and-presentation.md`.

