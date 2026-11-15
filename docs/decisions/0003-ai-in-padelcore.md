# 0003: AI decision logic lives in PadelCore

**Date:** 2026-11-15 · **Status:** proposed (Agent 4)

- **Where:** the AI's decisions (prediction, interception, pair tactics, shot choice, difficulty, partner commands) are plain C++ in `Source/PadelCore/{Public,Private}/AI/`, namespace `Padel::AI`. `Source/Padel/AI/` only holds engine glue: loading the data files and mapping the `IA_Cmd*` actions to commands. CLAUDE.md lists AI under `Padel/AI/`; the logic moved down so it is deterministic, headless and covered by the native tests that CI runs on every PR (GDD 8 determinism, GDD 12 soak). The folder is Agent 4's; no other PadelCore file was touched.
- **Ball model:** the AI predicts through `IAiBallModel`. In the game it wraps Agent 2's `FBallSim` (prediction passes no RNG, so no mesh randomness). Until that sim merges, the native tests and the soak run use a stand-in ball and match in `Tests/Native/Support/`, marked test-only, to be deleted when the soak switches to the real engine.
- **RNG:** each AI pair owns a seeded `FAiRng` (PCG32) seeded from the match seed. It moves to Agent 2's shared `FPadelRng` once that lands.
- **Data:** `Data/Difficulty.json` (the GDD 8 table) and `Data/AiTactics.json` (every other AI number). Both use the DataTable JSON layout. PadelCore reads them with a small strict reader (`AiTuningJson`): every field must be present and unknown keys are errors, so the data and the structs can't drift. `FDifficultyRow` mirrors the difficulty file for menus.
- **Soak:** `Ai.Soak` is a native test. CI runs 10 Medium matches plus a few Easy and Hard ones; `PADEL_SOAK_MATCHES=100 Tools/RunNativeTests.sh -tc="Ai.Soak*"` runs the full 100. A `Padel.Soak` automation test can wrap the same code once the real match engine exists.
