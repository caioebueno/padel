# CLAUDE.md

An original 2v2 padel sim in **Godot 4** (GDScript). The design lives in [`docs/GDD.md`](docs/GDD.md): read it before you build anything, and read the sections your task names in full. If the code and the GDD disagree, the GDD wins until a PR changes it.

Current milestone: **the vertical slice** (GDD section 12). Don't build anything marked post-slice.

## Hard rules

- **Original content only.** No names, players, teams, tours, brands, logos, likenesses or assets from existing games or real padel. Search any new name before using it.
- **One agent, one area.** Touch only the folders your task owns. Changes to shared contracts (signals, resources, input actions, the GDD) go in their own PR and say so in the title.
- **No hard-coded tuning numbers.** Shot, player, physics, AI and camera values live in `.tres` resources under `data/`.
- **Engine code is headless and deterministic.** Everything in `scripts/engine/` runs without a scene tree or rendering, at a fixed step, with a seeded RNG passed in. No `randf()` from the global RNG, no frame-time dependence. This is what makes tests, replays, soak runs and future online play possible.
- **Don't use Godot's RigidBody for the ball.** The ball is simulated by our own integrator in `scripts/engine/`.

## Engine and version

**Godot 4.7.2-stable**, pinned in `project.godot` and in CI. Tests use **GUT 9.7.1**. GDScript only, no C#, no GDExtension unless the GDD asks for it.

## Folder layout

```
project.godot
CLAUDE.md
docs/        GDD.md, PLAYTEST.md, decisions/ (one short .md per decision)
scripts/
  engine/    ball sim, court geometry, referee, score (pure logic, RefCounted, no Nodes)
  gameplay/  player movement, shot system, input handling
  ai/        prediction, pair tactics, shot choice, difficulty
  ui/        HUD, menus, stats screen
  presentation/  camera, audio, replays, the event bus listeners
  data/      Resource class definitions (ShotData, PlayerData, DifficultyData...)
scenes/      court/, player/, match/, ui/
data/        shots/, players/, teams/, difficulty/, physics/, camera/ (.tres files)
assets/      models/, textures/, audio/, fonts/, animations/, manifest.csv (source + licence for every asset)
tools/       run_tests.sh, check_scripts.sh; godot/ (editor scripts that generate scenes and the input map, e.g.
             build_court.gd builds the court from scripts/engine/court_spec.gd, configure_project.gd
             writes the input map); the asset pipeline (Python, Blender)
tests/       unit/, integration/, soak/
addons/gut/  test framework
```

## Coordinates and units

Metres, seconds, kilograms, radians in code (degrees only in UI). Y is up, the court is centred on the origin, length along Z (−10 to +10), width along X (−5 to +5), the net is Z = 0, Team A starts on +Z. See GDD section 2.

## Code conventions

- Static typing everywhere: typed variables, parameters and return types. Treat untyped-declaration warnings as errors.
- Files and folders `snake_case.gd`, classes `PascalCase` with `class_name`, functions and variables `snake_case`, constants `SCREAMING_SNAKE_CASE`, signals in the past tense (`ball_bounced`, `point_ended`).
- One class per file. Keep files under about 400 lines; split when they grow.
- Gameplay and UI talk to the engine through signals on the `MatchEvents` autoload. The event names are in GDD section 3.4; don't rename them without a GDD change.
- Enums for reason codes and shot types live in `scripts/engine/types.gd`.
- Comments explain *why*, not what. Point to the GDD section for rule logic (`# GDD 3.3: wall before bounce`).

## Tests

GUT, run headless. Every rule in GDD section 3 needs a test. Every bug fix gets a test that would have caught it.

```
# all tests (GODOT points at the Godot 4.7.2 binary)
GODOT=/path/to/godot tools/run_tests.sh

# parse every script, untyped declarations count as errors
GODOT=/path/to/godot tools/check_scripts.sh

# AI soak test (100 matches, fixed seeds), once Agent 4 adds it
godot --headless --script res://tests/soak/run_soak.gd -- --matches 100
```

Don't hand-edit generated scenes or the input map in `project.godot`: change the spec or the script in `tools/godot/` and regenerate.

CI (GitHub Actions) runs both scripts on every PR. A PR merges only with green CI. Update the commands here if they change.

## Git and PRs

- Branch per task: `agent-<n>/<short-topic>`, e.g. `agent-2/referee`. If your session can only push to a branch it was given (like `claude/...`), use that and name the agent in the PR title.
- Small, focused PRs. The description says which GDD sections it implements and how to see it working.
- Don't commit generated files (`.godot/`, imported caches, exports). Large binaries (`.glb`, `.wav`, textures) go through Git LFS.
