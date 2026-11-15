# 0002: Unreal project setup

**Date:** 2026-11-15 · **Status:** accepted · Supersedes [0001](0001-engine-and-test-setup.md)

- **Unreal Engine 5.8**, the latest stable 5.x on this date (5.9 is a preview). Pinned in `Padel.uproject` (`EngineAssociation`) and in CI (`UE_VERSION`). Upgrade both together, in their own PR.
- **Two modules.** `PadelCore` is the deterministic simulation in plain C++ with no Unreal headers, apart from its module boilerplate and its automation tests. `Padel` is the game module (actors, input, presentation). `PadelCore` compiles both inside Unreal and standalone.
- **Two test layers.** Native tests (`Tests/Native/`, CMake + doctest 2.4.12) build `PadelCore` without the engine and run on GitHub-hosted Linux and Apple Silicon runners on every PR. Unreal Automation tests (`Padel.*`) need an engine install, so they run with `Tools/RunUnrealTests.sh` on a Mac, or on a self-hosted runner labelled `unreal` once `UNREAL_RUNNER` is set to `true`. Epic's engine container images need a linked Epic account and are too big for GitHub's standard runners, so they're not used.
- **No binary assets in the setup.** The court is built from engine cubes (`/Engine/BasicShapes/Cube`). The actions (`IA_*`) and `IMC_Match` are created in C++. The default map is the engine's empty `Entry` map, with `APadelGameMode` spawning the court, lights and camera. Everything in the setup is reviewable text. The first real level (`Content/Padel/Maps/`) and materials come with the art pass.
- **The court geometry lives in `PadelCore`** (`CourtLayout.h`, built from `CourtSpec.h`). `APadelCourt` makes one component per box, and the native tests ray-cast the same boxes. This way the court CI checks is the court the game builds.
- **Glass and mesh are invisible colliders with a visible frame** until real materials exist. The placeholder material is opaque, and opaque glass would hide the court from the cameras.
- **Exits** are one 2.0 m opening per side per half, at the net end of the side glass, open to full height (Caio confirmed 2 m). **Mesh above the side glass** follows FIP variant 1: up to 4 m above the first panel and up to 3 m above the step.
