# Padel

An original 2v2 padel sim built in Godot 4. See `docs/GDD.md` and `CLAUDE.md`.

## Setup

1. Install **Godot 4.7.2-stable** (standard build, not .NET). This exact version is pinned in `project.godot` and CI.
2. Open `project.godot` in Godot, or run it from the command line:

   ```
   godot --path .
   ```

   The main scene (`scenes/match/main.tscn`) shows the court from the broadcast camera.

Large binaries under `assets/` go through Git LFS, so run `git lfs install` once before you pull assets.

## Tests

[GUT](https://github.com/bitwes/Gut) 9.7.1 is vendored in `addons/gut/`. Tests live in `tests/unit/` and `tests/integration/` (files named `test_*.gd`); the soak test goes in `tests/soak/` and is run separately.

```
# all tests, headless (set GODOT if the binary is not on PATH as "godot")
GODOT=/path/to/godot tools/run_tests.sh

# one file
tools/run_tests.sh -gselect=test_court_scene

# parse every script with static typing enforced
tools/check_scripts.sh
```

In the editor, the GUT panel at the bottom runs the same suite (settings in `.gutconfig.json`).

CI (`.github/workflows/tests.yml`) runs both scripts on every pull request and on pushes to `main`.

## Generated files

Two pieces of the project are generated from code. Edit the source, re-run the tool, and commit the result:

| What | Source of truth | Regenerate with |
|---|---|---|
| Input map and physics layer names in `project.godot` | `tools/godot/configure_project.gd`, `scripts/gameplay/input_actions.gd`, `scripts/engine/physics_layers.gd` | `godot --headless --path . --script res://tools/godot/configure_project.gd` |
| `scenes/court/court.tscn`, `scenes/match/main.tscn`, `assets/materials/placeholder/` | `scripts/engine/court_spec.gd`, `tools/godot/build_court.gd` | `godot --headless --path . --script res://tools/godot/build_court.gd` |

## Court scene

`scenes/court/court.tscn` is a primitive placeholder at regulation size (GDD section 2): 20 × 10 m, 3 m back glass with mesh to 4 m, side glass of 3 m then a 2 m step, a 2 m exit on each side of each half, mesh to the net, a net of 0.88 m in the centre and 0.92 m at the posts, run-off floor outside for out-of-court play, and an invisible ceiling at 8 m.

Every collider is a `StaticBody3D` with:

- metadata `surface`: one of `ground`, `glass`, `mesh`, `net`, `ceiling` (`CourtSpec.SURFACE_*`, the same values as the `ball_bounced` event)
- metadata `zone`: `court`, `runoff` or `ceiling`
- one physics layer from `PhysicsLayers` (`court_ground`, `court_glass`, `court_mesh`, `court_net`, `court_ceiling`, `runoff_ground`; `players` and `ball` are reserved)

The ball uses its own integrator in `scripts/engine/`, not these colliders; they are for players, raycasts and debug tools. Engine code should read measurements from `CourtSpec`, never from the scene.
