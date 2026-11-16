# 0009: Lighting, venue and the shared look-dev level

**Date:** 2026-11-16 · **Status:** proposed (court-art lighting and venue)

GDD 10 asks for a televised indoor padel match at football-sim realism, at 60 fps at 1080p on an M2 Pro. Five court-art parts are being built in parallel (surface, glass, mesh and frame, net, lighting and venue), so they also need one place to be judged together.

## Physically based, from text

- **One data file.** `Data/Lighting.json` holds every preset: floodlight rows, sun, sky, fog and the broadcast post-process. `APadelVenueLighting` applies a preset; nothing is tuned in a map or a Blueprint.
- **Real units.** Floodlights are spot lights in **candela** with the engine's own cone falloff; the sun is in **lux**; colour is in **kelvin**; exposure is a fixed **EV100** through the physical camera (ISO, shutter, f-number). Materials must use real albedos for this to work, which is why the venue palette is tested for plausible values and the court-art agents were asked to do the same.
- **A broadcast camera, not a game camera.** Exposure is locked (no eye adaptation, so the picture never pumps as the camera tracks), white balance matches the floodlights, a light grade (contrast 1.04, saturation 1.06), mild bloom and vignette, a little shutter blur, and no film grain, fringing or lens flares, because nothing may sit between the viewer and the ball (GDD 10).

## The fixture layout is tested like a rule

`Padel::Presentation::Lighting` (engine-free, in `Source/Padel/*/Presentation/Model/`) builds every fixture from the rows and measures it. The native tests run on every PR and check each preset in the shipped data:

| Check | Indoor club | Outdoor night |
|---|---|---|
| Average horizontal lux on the court (target) | 1,159 (≥ 1,000) | 732 (≥ 500) |
| Uniformity Emin / Eavg (≥ 0.7) | 0.75 | 0.74 |
| Vertical lux toward each broadcast camera at 1 m (≥ 0.3 × horizontal) | 377 | 241 |
| Closest beam edge to the broadcast camera, either end (≥ 5°) | 7.3° | 10.2° |
| Closest luminaire to the camera's view axis (≥ 15°, ARENA_SPEC 6.3) | passes | passes |
| Stands (150 to 250 lux, court 3:1 to 8:1 brighter) | 219 lux, about 5:1 | no stands lit |
| Exposure within a stop of an incident meter | EV 8.5 vs 8.9 | EV 7.7 vs 8.2 |

Also tested: both halves and both sides get the same light at mirrored points (neither team plays into a darker corner), shadow casters come in whole mirror groups, indoor court fixtures sit at or above the 8 m ceiling (GDD 2) and outside the side walls so a lob never meets a light, and every key in the file is known to the loader, so a typo fails a test instead of falling back to a default.

**Indoor club (the slice).** Ten LED floodlights, 28,500 cd each, 40°/55° cones, 5,600 K, in the long sides of a box-truss rig at Y = ±7 whose underside is exactly the 8 m ceiling plane (`docs/art/ARENA_SPEC.md` 6.1). Fixtures run from X = −11 to +11 and aim 1 m short of the centre line, pulled 30% toward the net so the ends are lit from inside the court. None on the end trusses (they face the camera) or the cross truss over the net. Ten 3,500 K house lights on the roof trusses light the stands at about a fifth of the court.

**Outdoor day** is a physical sun (100,000 lux at 55°, behind the near left of the broadcast camera) with a sky atmosphere and real-time sky light, at EV 14.8. **Outdoor night** uses the same ten masts, switched on.

**The arena spec's vertical target.** `ARENA_SPEC.md` 6.2 asks for vertical light toward each camera of at least 0.3 × the horizontal average. With fixtures only on the long sides (6.1) and the camera at the rig's height, that is what the side rows can give: 377 lux at 1,159 horizontal. The tests hold the same floor. If playtests show the players look flat, the fix is a low end-truss fill aimed at least 60° below horizontal, added to the spec first.

## Performance on the Mac

`Config/DefaultDeviceProfiles.ini` sets the Mac budget; Windows keeps Epic defaults for now.

- **Two shadow-casting lights.** Virtual shadow maps cost the most per shadowed local light, so only the central mirror pair of court floodlights casts shadows (`MaxShadowedFixtures`, ≤ 4 enforced by a test). House lights never do. Every light is movable; venue meshes are static so shadow pages cache.
- **TSR at 75%** (about 810p upscaled to 1080p), **software Lumen** (M2 has no hardware ray tracing), Lumen GI, reflections and shadows at High, half-resolution Lumen reflections, fewer shadow rays, and a coarser volumetric-fog grid.
- **Glass reflections stay on:** `r.Lumen.TranslucencyReflections.FrontLayer.Enable=1` gives the back glass real mirror reflections of the rig and players (asked for by the glass part).
- Venue meshes are Nanite and opaque; the floodlight head is a 290-triangle non-Nanite mesh instanced on each fixture.

These are starting values that haven't been measured yet. **To profile on the Mac:** `Tools/RunLookDev.sh`, then in the console `stat unit` and `stat gpu`, from the `Broadcast` camera, for each preset. Record the frame time here and tune `DefaultDeviceProfiles.ini`, not the code.

## The venue

Procedural Blender meshes in `Tools/AssetPipeline/blender/build_venue.py`, `Content/Padel/Venue/`, all original and sponsor-free (the boards carry only a club-colour stripe):

- `SM_VenueHall`: 50 × 46 m hall, 15 m eaves, roof trusses from 12 m, dark acoustic walls (after `ARENA_SPEC.md` 2; a stand-in until the arena's parametric builder lands).
- `SM_VenueLightTruss`: the rig above, built from the indoor rows in `Data/Lighting.json` so it always carries the fixtures the game lights, plus hoists and the house-light bars.
- `SM_VenueMasts` and `SM_VenueGround`: the outdoor club, masts built from the outdoor rows.
- `SM_VenueStands`: ten-row grandstands on both long sides (ARENA_SPEC 5.1). End stands are an open decision in that spec, so they're left out.
- `SM_VenueBoards`: perimeter boards round the run-off.
- `SM_FloodlightHead`: one LED floodlight with a lens slot (`MI_Venue_FixtureLens`) whose emissive strength the game sets in nits per preset (dark when the lights are off).

`pipeline.py preview out.png --preset IndoorClub --camera Broadcast` renders the procedural assets in Cycles with the same physical values (candela to watts at 683 lm/W, the same EV100), so the lighting can be reviewed without Unreal.

## The shared look-dev level is code, not a .umap

`?game=LookDev` (`APadelLookDevGameMode`, `Tools/RunLookDev.sh`) turns any map into the look-dev level. `Data/LookDev.json` lists each court part with its owner and mesh paths, and each venue piece with the venue it belongs in; `APadelLookDevStage` places what has been imported at the origin (the pipeline builds in court coordinates), logs what's missing, and falls back to the pipeline's one-piece court, then to `APadelCourt`. Camera bookmarks (`Broadcast`, `CornerHigh`, `NetSide`, `BackGlassLow`, `NetClose`, `LobView`, `TurfMacro`) and console commands (`Padel.LookDev.Preset`, `NextPreset`, `Camera`, `NextCamera`, `Report`) drive it. Being text, it can't go stale or conflict the way a shared binary map would, and the cloud agents can change it.

The match game mode spawns `APadelVenueLighting` too, replacing its old placeholder sun and sky.

## Not done yet

- Nothing here has been built or run in Unreal: it needs a Mac build, `Tools/RunUnrealTests.sh Padel.Presentation`, the asset import, and the profiling above.
- IES profiles, real asymmetric beam shapes and cut-off visors, and the arena's afternoon preset with low sun through the clerestory (post-slice).
