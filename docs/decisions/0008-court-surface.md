# 0008: Court playing surface

**Date:** 2026-11-16 · **Status:** proposed (court art: surface) · Implements GDD 2 (line positions and widths) and GDD 10 (blue artificial grass with visible sand, white lines), on the asset pipeline of [0003](0003-asset-pipeline.md)

One of five court-art parts (surface, glass, mesh and frame, net, lighting and venue). This one is the floor: `SM_CourtSurface`.

## What it is

- **Mesh, `blender/build_court_surface.py`:** the turf over the 20 x 10 playing area as a 0.25 m grid with a seeded 1.5 mm undulation that fades to zero at the court edge; the service lines and the centre line from `CourtSpec.h` as strips 1 mm above it; the charcoal run-off out to `RunoffBack`/`RunoffSide`; and a closed 2 cm slab under it all so Lumen's distance fields see a solid floor. About 7,000 triangles, Nanite on. The ball never touches this mesh: `PadelCore` bounces it on the flat Z = 0.
- **Two UV sets:** UV0 in metres (fibre tiling at real size), UV1 (`CourtUV`) mapping the playing area to 0..1 for the whole-court masks. The import turns off lightmap UV generation for this mesh, because it would overwrite UV1 and Lumen needs no lightmaps.
- **Textures, `asset_lib/turf_textures.py`:** pure Python and seeded, like the other procedural textures.
  - `T_Turf_N` and `T_Turf_Masks`: one 23.75 cm tile of tufted monofilament (25 rows at a 9.5 mm gauge, a tuft every 11.9 mm, 10 to 14 tapered blades per tuft, half-stitch offset on alternate rows, sand at 8 mm in a 12 mm pile). The tile is a whole number of rows, so it repeats without a seam.
  - `T_CourtSurface_Masks`, over the whole playing area: wear where players actually stand (back defence, serve and return, volley, the T, the back corners), with foot scuffs mostly across the court; sand drifting against the walls, into the corners and under the net; and a faint tint per 4 m turf roll.
- **Material, `unreal/turf_material.py` (`M_PadelTurf`):** built from text like the other masters, plain expressions only (no Custom HLSL), so it compiles the same on D3D and Metal. Sand shows through the gaps between blades and more where play flattens the fibres; worn fibres go lighter and smoother; the fibre normal fades to flat between 12 and 40 m from the camera, which keeps the far court from shimmering under the broadcast camera and costs less there. `MI_Court_Turf`, `MI_Court_Line` and `MI_Court_Runoff` are all instances of it, so the lines are white fibres with sand and wear too, not paint.

## Choices

- **No fibre geometry.** About 200 m² of real fibres is millions of blades; from the broadcast camera they would alias. Detail lives in the normal and mask textures, and only the gentle undulation is geometry.
- **Wear is authored, not random.** The zones in `config/court_surface.json` follow where padel players stand, so the court reads as a played-on club court. They are mirrored to both halves.
- **Physically based values.** Albedos are in the measured range for dyed polyethylene and silica sand, for the lighting part's physical lights (fixed exposure about EV100 9). Brightness is the lights' job, not the materials'.
- **Lines stay as geometry.** Strips 1 mm above the turf keep the edges exactly where `CourtSpec.h` puts them at any distance. A mask in the material would blur them at 2 cm per texel.

## Tuning

- Colours, roughness, wear and drift strength, fade distances: `config/palette.json`, `court.Turf`, `court.Line`, `court.Runoff`. Re-run `pipeline.py unreal`.
- Fibre structure, wear zones, scuffs, drift, rolls, mesh resolution: `config/court_surface.json`. Re-run `pipeline.py all`.

## Not done here

- Real Unreal screenshots: the cloud environment has no engine. The Blender preview (`pipeline.py preview`) uses a node tree that follows the same formula. Check it in the editor on a Mac.
- Footprints and ball marks that appear during a match (runtime decals) are post-slice.
