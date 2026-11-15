# 0003: UI and presentation in C++

**Date:** 2026-11-15 · **Status:** proposed (Agent 7)

- **Widgets are built in C++, not Widget Blueprints.** Each `UPadelWidgetBase` subclass builds its tree in `BuildTree()`, so layout and logic are reviewable text. A thin Blueprint subclass may restyle one later.
- **Plain UMG for now, CommonUI later.** CommonUI needs input data assets (binary) and `CommonGameViewportClient` in `DefaultEngine.ini`. Plain UMG buttons already work with a gamepad through Slate navigation. Move to CommonUI with the UI art pass, when its assets can be made at the same time.
- **An engine-free UI model.** Score text, stats, the roster, pad button names, the setup options and the camera maths live in `Source/Padel/{Public,Private}/*/Model/`. They're plain C++20 with no Unreal headers, and `Tests/Native/CMakeLists.txt` builds them into the native suite, so CI tests them on every PR without an engine.
- **One adapter for match events.** `APadelHUD` implements `IUiEventSink`, which mirrors PadelCore's `IMatchEventListener` (Agent 2) with codes as GDD names. When `UPadelMatchEvents` lands, a short binder forwards its delegates into it. Until then `FDemoMatch`, a scripted stand-in with a simple score, drives the flow. It isn't the referee, and it is deleted once the real events are wired.
- **Camera values are read from `Data/Camera.json`** at `BeginPlay`, with defaults if the file is missing. A DataTable import can replace the direct read when the other `Data/` tables get theirs. For packaged builds, `Data/` must be staged (add it to "Additional Non-Asset Directories to Copy").
- **Player names** for the slice (GDD 6.4) are Selva Arunde, Mikko Ventara (Team Lagoon), Ilan Corvisa and Ruben Halvaro (Team Ember). Each surname was web-searched and matched no padel player, club, tour or racket brand. They move to `Data/Players.json` when that table has a display name.
- **Not in this change:** replays (post-slice, GDD 12), the walk-out (not in the GDD), the server close-up cut-away (needs the player pawns), and real sounds (the hooks are in `UPadelMatchAudio`).
