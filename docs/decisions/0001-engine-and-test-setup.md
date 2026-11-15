# 0001: Engine version, test runner and generated court

**Date:** 2026-11-15 · **Status:** accepted

- **Godot 4.7.2-stable**, the latest stable 4.x when the project was set up. Pinned in `project.godot` (`config/features`) and in `.github/workflows/tests.yml` (`GODOT_VERSION`). Upgrade both together, in their own PR.
- **GUT 9.7.1** (the GUT release for Godot 4.7), vendored in `addons/gut/`. MIT licence, see `addons/gut/LICENSE.md`.
- **Static typing is enforced**: `debug/gdscript/warnings/untyped_declaration` is set to *error*, and CI parses every script with `tools/check_scripts.sh`.
- **The court scene is generated** from `scripts/engine/court_spec.gd` by `tools/godot/build_court.gd`, so the measurements live in one place that both the engine and the scene use. Don't hand-edit `scenes/court/court.tscn`; change the spec or the builder and re-run it. Visual dressing (Agent 5) can go in a separate scene that instances the court.
- **The input map is generated** by `tools/godot/configure_project.gd` from the GDD section 7 table. Action names are constants in `scripts/gameplay/input_actions.gd`.
- **Exits** are modelled as one 2.0 m opening per side per half, at the net end of the side glass, open to full height (GDD 2). FIP allows 0.72 to 2.20 m depending on the court variant; change `CourtSpec.EXIT_WIDTH` if the GDD picks a different variant.
- **Mesh above the side glass** follows FIP "variant 1": mesh up to 4 m above the first 2 m panel, up to 3 m above the step.
