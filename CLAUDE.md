# CLAUDE.md

An original 2v2 padel sim in **Unreal Engine 5**, C++ first. The design lives in [`docs/GDD.md`](docs/GDD.md): read it before you build anything, and read the sections your task names in full. If the code and the GDD disagree, the GDD wins until a PR changes it.

Current milestone: **the vertical slice** (GDD section 12). Don't build anything marked post-slice.

## Hard rules

- **Original content only.** No names, players, teams, tours, brands, logos, likenesses or assets from existing games or real padel. Search any new name before using it.
- **One agent, one area.** Touch only the folders your task owns. Changes to shared contracts (event delegates, data table schemas, input actions, the GDD) go in their own PR and say so in the title.
- **Logic in C++, not Blueprints.** Blueprints and `.uasset` files are binary: agents can't review or diff them. Gameplay, rules, AI and UI logic live in C++. Blueprints are only thin subclasses that pick meshes, materials, sounds and animation assets.
- **Tuning data is text.** Shot, player, physics, AI and camera values live in JSON (or CSV) files under `Data/`, imported into DataTables. Edit the text file, never only the imported asset. No hard-coded tuning numbers.
- **The simulation is deterministic and headless.** Everything in the `PadelSim` module (ball physics, court geometry, referee, score) is plain C++ with no `UObject`, world, rendering or frame-time dependence. It steps at a fixed 120 Hz and takes a seeded RNG from the match. No `FMath::Rand()`. This is what makes tests, replays, soak runs and future online play possible.
- **Don't use Chaos physics for the ball.** The ball is simulated by our own integrator in `PadelSim`. Chaos is fine for cosmetic things (cloth, debris).

## Engine and version

Unreal Engine 5, latest stable 5.x, pinned in `Padel.uproject` (`EngineAssociation`) and in CI. The setup agent picks the exact version and writes it here: **TBD**. C++20, the engine's default compiler per platform (MSVC on Windows). Plugins in use: Enhanced Input, CommonUI, Motion Matching / Pose Search (animation), Python Editor Script Plugin (asset import).

## Folder layout

```
Padel.uproject
CLAUDE.md
docs/        GDD.md, PLAYTEST.md, decisions/ (one short .md per decision)
Source/
  PadelSim/        ball sim, court geometry, referee, score (plain C++, no UObjects); module tests in Private/Tests/
  Padel/
    Gameplay/      player pawn, movement, shot system, input
    AI/            prediction, pair tactics, shot choice, difficulty
    UI/            HUD, menus, stats screen (UMG + CommonUI, logic in C++)
    Presentation/  camera, audio, replays, the UPadelMatchEvents subsystem and its listeners
    Data/          USTRUCT row types for the data tables (FShotRow, FPlayerRow, FDifficultyRow...)
  Padel.Target.cs, PadelEditor.Target.cs
Content/Padel/     Maps/, Court/, Characters/, Animations/, UI/, Audio/, Data/ (imported DataTables)
Data/              Shots.json, Players.json, Teams.json, Difficulty.json, Physics.json, Camera.json (the source of truth)
Config/            DefaultEngine.ini, DefaultInput.ini, DefaultGame.ini
Tools/             build and test scripts, the asset pipeline (Python, Blender, Unreal Python import)
```

## Coordinates and units

Unreal's: **centimetres**, Z up, degrees in rotators and radians inside `PadelSim` maths. The court is centred on the origin, length along X (−1000 to +1000), width along Y (−500 to +500), the net is the plane X = 0, Team A starts on +X. The GDD's tables are in metres; convert at one place (`PadelSim/Public/PadelUnits.h`), never inline. See GDD section 2.

## Code conventions

- Follow the Epic C++ coding standard: `U`/`A`/`F`/`E`/`I` prefixes, PascalCase for everything, booleans start with `b`, `TObjectPtr` for UObject members, `UPROPERTY` on every UObject reference.
- One class per file pair (`.h` in `Public/`, `.cpp` in `Private/`). Keep files under about 500 lines; split when they grow.
- Gameplay and UI talk to the simulation only through the multicast delegates on `UPadelMatchEvents` (a world subsystem). Names are GDD section 3.4's events as `On` + PascalCase (`OnBallBounced`, `OnPointEnded`); don't rename them without a GDD change.
- Enums for reason codes and shot types live in `PadelSim/Public/PadelTypes.h`, as `enum class` with `UENUM` mirrors in `Padel` where Blueprints need them.
- Log through `UE_LOG(LogPadel, ...)` categories, not `printf` or on-screen debug messages in shipped paths.
- Comments explain *why*, not what. Point to the GDD section for rule logic (`// GDD 3.3: wall before bounce`).

## Tests

Unreal Automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST` and Automation Spec), run headless. Every rule in GDD section 3 needs a test. Every bug fix gets a test that would have caught it. Tests are named `Padel.<Area>.<Case>`, e.g. `Padel.Referee.WallBeforeBounce`.

Exact commands: **TBD by the setup agent**. They will follow this shape:

```
# all tests, headless
UnrealEditor-Cmd Padel.uproject -ExecCmds="Automation RunTests Padel.; Quit" -unattended -nullrhi -nosplash -log

# AI soak test (100 matches, fixed seeds), once Agent 4 adds it
UnrealEditor-Cmd Padel.uproject -ExecCmds="Automation RunTests Padel.Soak; Quit" -unattended -nullrhi
```

CI runs the tests on every PR. A PR merges only with green CI.

## Git and PRs

- Branch per task: `agent-<n>/<short-topic>`, e.g. `agent-2/referee`. If your session can only push to a branch it was given (like `claude/...`), use that and name the agent in the PR title.
- Small, focused PRs. The description says which GDD sections it implements and how to see it working.
- Don't commit generated folders (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`). `.uasset`, `.umap` and other binaries go through Git LFS.
