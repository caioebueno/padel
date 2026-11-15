# CLAUDE.md

An original 2v2 padel sim in **Unreal Engine 5**, C++ first. The design lives in [`docs/GDD.md`](docs/GDD.md): read it before you build anything, and read the sections your task names in full. If the code and the GDD disagree, the GDD wins until a PR changes it.

Current milestone: **the vertical slice** (GDD section 12). Don't build anything marked post-slice.

## Hard rules

- **Original content only.** No names, players, teams, tours, brands, logos, likenesses or assets from existing games or real padel. Search any new name before using it.
- **One agent, one area.** Touch only the folders your task owns. Changes to shared contracts (event delegates, data table schemas, input actions, the GDD) go in their own PR and say so in the title.
- **Logic in C++, not Blueprints.** Blueprints and `.uasset` files are binary: agents can't review or diff them. Gameplay, rules, AI and UI logic live in C++. Blueprints are only thin subclasses that pick meshes, materials, sounds and animation assets.
- **Tuning data is text.** Shot, player, physics, AI and camera values live in JSON (or CSV) files under `Data/`, imported into DataTables. Edit the text file, never only the imported asset. No hard-coded tuning numbers.
- **The simulation is deterministic and headless.** Everything in the `PadelCore` module (ball physics, court geometry, referee, score) is plain C++ with no `UObject`, world, rendering or frame-time dependence. It steps at a fixed 120 Hz and takes a seeded RNG from the match. No `FMath::Rand()`. This is what makes tests, replays, soak runs and future online play possible.
- **Don't use Chaos physics for the ball.** The ball is simulated by our own integrator in `PadelCore`. Chaos is fine for cosmetic things (cloth, debris).

## Engine and version

**Unreal Engine 5.8**, pinned in `Padel.uproject` (`EngineAssociation`) and in CI. C++20, the engine's default compiler per platform (MSVC on Windows, Xcode/Clang on macOS).

**Platforms:** Windows is the first ship target, and **macOS on Apple Silicon is a supported dev and test platform from day one** (Caio develops and playtests on a Mac). Every change must build and pass tests on both. No Windows-only APIs, no DirectX-only features, and no plugins without Mac support. Shaders and materials must work on Metal. Before merging, run the Unreal tests on a Mac (`Tools/RunUnrealTests.sh`). Plugins in use: Enhanced Input, CommonUI, Motion Matching / Pose Search (animation), Python Editor Script Plugin (asset import).

## Folder layout

```
Padel.uproject
CLAUDE.md
docs/        GDD.md, PLAYTEST.md, decisions/ (one short .md per decision)
Source/
  PadelCore/        ball sim, court geometry, referee, score (plain C++, no UObjects); module tests in Private/Tests/
  Padel/            each area below has Public/<Area>/*.h and Private/<Area>/*.cpp; engine tests in Private/Tests/
    Court/         APadelCourt, built from PadelCore's CourtLayout (derived from CourtSpec.h)
    Gameplay/      player pawn, movement, shot system, input (PadelInputConfig.cpp creates the IA_* actions and IMC_Match in C++)
    AI/            prediction, pair tactics, shot choice, difficulty
    UI/            HUD, menus, stats screen (UMG + CommonUI, logic in C++)
    Presentation/  camera (APadelBroadcastCamera), audio, replays, the UPadelMatchEvents subsystem and its listeners
    Data/          USTRUCT row types for the data tables (FShotRow, FPlayerRow, FDifficultyRow...)
  Padel.Target.cs, PadelEditor.Target.cs
Content/Padel/     Maps/, Court/, Equipment/, Characters/, Materials/, Animations/, UI/, Audio/, Data/ (imported DataTables)
SourceArt/         FBX and PNG sources for Content/Padel, built by Tools/AssetPipeline (procedural ones are rebuilt, not committed); manifest.csv (source + licence for every asset)
Data/              Shots.json, Players.json, Teams.json, Difficulty.json, Physics.json, Camera.json (the source of truth)
Config/            DefaultEngine.ini, DefaultInput.ini, DefaultGame.ini
Tools/             build and test scripts (RunNativeTests.sh, RunUnrealTests.sh/.ps1); AssetPipeline/ (Python, Blender, Unreal Python import; see its README)
Tests/Native/      plain C++ tests for PadelCore (CMake + doctest), outside Source/ so the engine build skips them
ThirdParty/        vendored code (doctest), each with its licence
```

## Coordinates and units

Unreal's axes: Z up, X along the court's length, Y across its width, origin at the centre of the court, the net is the plane X = 0, Team A starts on +X. **`PadelCore` works in metres** (like the GDD's tables: X −10 to +10, Y −5 to +5) and radians. **The Unreal world is in centimetres** (×100). Convert in one place at the boundary between `PadelCore` and `Padel`, never inline. See GDD section 2.

## Code conventions

- Follow the Epic C++ coding standard: `U`/`A`/`F`/`E`/`I` prefixes, PascalCase for everything, booleans start with `b`, `TObjectPtr` for UObject members, `UPROPERTY` on every UObject reference.
- One class per file pair (`.h` in `Public/`, `.cpp` in `Private/`). Keep files under about 500 lines; split when they grow.
- Gameplay and UI talk to the simulation only through the multicast delegates on `UPadelMatchEvents` (a world subsystem). Names are GDD section 3.4's events as `On` + PascalCase (`OnBallBounced`, `OnPointEnded`); don't rename them without a GDD change.
- Enums for reason codes and shot types live in `PadelCore/Public/PadelTypes.h`, as `enum class` with `UENUM` mirrors in `Padel` where Blueprints need them.
- Log through `UE_LOG(LogPadel, ...)` categories, not `printf` or on-screen debug messages in shipped paths.
- Comments explain *why*, not what. Point to the GDD section for rule logic (`// GDD 3.3: wall before bounce`).

## Tests

Unreal Automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST` and Automation Spec), run headless. Every rule in GDD section 3 needs a test. Every bug fix gets a test that would have caught it. Two layers:

- **Native tests (`Tests/Native/`, CMake + doctest):** test `PadelCore` without the engine. They run on GitHub's normal Linux runners on every PR. Every rule test belongs here, because this is the layer CI always runs.
- **Unreal Automation tests:** for anything that needs the engine (actors, the court level, input, UI). Named `Padel.<Area>.<Case>`, e.g. `Padel.Court.WallCollision`. GitHub-hosted runners have no engine install, so these run with `Tools/RunUnrealTests.sh` on a Mac, or on a self-hosted runner if one is set up.

Input Actions and `IMC_Match` are created in C++ from a binding table, not stored as `.uasset` files, so bindings stay diffable text.

Commands:

```
# native PadelCore tests (any OS with CMake and a C++20 compiler; no engine needed)
Tools/RunNativeTests.sh                       # all
Tools/RunNativeTests.sh -tc="Court.Layout*"   # one group (doctest filters)

# Unreal Automation tests, headless: builds PadelEditor, runs Padel.*, reads the report
Tools/RunUnrealTests.sh                       # macOS / Linux; UE_ROOT defaults to /Users/Shared/Epic Games/UE_5.8 on a Mac
Tools/RunUnrealTests.sh Padel.Court           # one area
Tools\RunUnrealTests.ps1                      # Windows; UE_ROOT defaults to C:\Program Files\Epic Games\UE_5.8

# asset pipeline: config and provider tests, plus Blender builds when BLENDER or BLENDER_PYTHON is set
python3 -m unittest discover -s Tools/AssetPipeline/tests -t Tools/AssetPipeline

# AI soak test (100 matches, fixed seeds), once Agent 4 adds it
Tools/RunUnrealTests.sh Padel.Soak
```

CI (`.github/workflows/tests.yml`) runs the native tests on Linux and Apple Silicon and checks the project files on every PR. The Unreal Automation job runs only on a self-hosted runner labelled `unreal` when the repository variable `UNREAL_RUNNER` is `true`; until then, run `Tools/RunUnrealTests.sh` on a Mac before merging. A PR merges only with green CI.

## Git and PRs

- Branch per task: `agent-<n>/<short-topic>`, e.g. `agent-2/referee`. If your session can only push to a branch it was given (like `claude/...`), use that and name the agent in the PR title.
- Small, focused PRs. The description says which GDD sections it implements and how to see it working.
- Meshes, textures and materials in `Content/Padel/` are generated by `python3 Tools/AssetPipeline/pipeline.py all` from `Tools/AssetPipeline/config/` and `SourceArt/`. Change the config or the script and regenerate; don't edit those assets by hand.
- Don't commit generated folders (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`). `.uasset`, `.umap` and other binaries go through Git LFS.
