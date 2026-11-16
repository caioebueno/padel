# Asset pipeline

Builds the game's 3D assets and imports them into Unreal with one command. The design notes are in [docs/decisions/0003-asset-pipeline.md](../../docs/decisions/0003-asset-pipeline.md).

```
python3 Tools/AssetPipeline/pipeline.py all
```

That command does three things:

1. It writes the procedural textures to `SourceArt/Textures/` (the turf ones from `config/court_surface.json`).
2. It builds every enabled asset in Blender and exports it to `SourceArt/<Category>/` as FBX in Unreal units, then updates `SourceArt/manifest.csv`.
3. It runs `unreal/import_assets.py` in the editor. That script builds the master materials and one material instance per palette entry, imports the meshes into `Content/Padel/`, assigns the materials, turns on Nanite and imports the LODs.

| Asset | Unreal path | Source |
|---|---|---|
| `CourtSurface` | `/Game/Padel/Court/SM_CourtSurface` | Blender script, from `CourtSpec.h` and `config/court_surface.json` (turf, lines, run-off; see [0006](../../docs/decisions/0006-court-surface.md)) |
| `CourtFrame` | `/Game/Padel/Court/SM_CourtFrame` | Blender script (`build_court_frame.py`): posts, corner L-posts, mesh panel frames, base plates, gussets, anchors and bolts. Nanite |
| `CourtMesh` | `/Game/Padel/Court/SM_CourtMesh` | Blender script (`build_court_frame.py`): one quad per mesh opening; the `WireMesh` material draws the wires. Masked, not Nanite |
| `CourtNet` | `/Game/Padel/Court/SM_CourtNet` | Blender script, from `CourtSpec.h` (net cloth, tape, bands, cable, posts, winch); its materials move it when the ball hits it ([decision 0006](../../docs/decisions/0006-net-and-hit-reaction.md)) |
| `CourtGlass` | `/Game/Padel/Court/SM_CourtGlass` | Blender script `build_court_glass.py` with `config/glass.json`: 18 tempered panels with arrised edges and their bolt fixings, plus the glass dirt textures. Not Nanite, because Nanite can't render translucent materials. See [0006](../../docs/decisions/0006-court-glass.md) |
| `Ball` | `/Game/Padel/Equipment/SM_Ball` | Blender script, 65 mm, felt texture with the seam |
| `RacketPlaceholder` | `/Game/Padel/Equipment/SM_RacketPlaceholder` | Blender script, 455 × 260 × 38 mm, perforated face, invented design |
| `PlayerPlaceholderA`, `PlayerPlaceholderB` | `/Game/Padel/Characters/SK_*` | Blender script, rigged on Unreal mannequin bones, in the team A and team B kits |
| `VenueHall`, `VenueStands`, `VenueBoards`, `VenueGround` | `/Game/Padel/Venue/SM_*` | Blender script (`build_venue.py`): the indoor hall, stands, perimeter boards and outdoor ground |
| `VenueLightTruss`, `VenueMasts` | `/Game/Padel/Venue/SM_*` | Blender script, from the fixture rows in `Data/Lighting.json` (the rig and masts that carry the floodlights) |
| `FloodlightHead` | `/Game/Padel/Venue/SM_FloodlightHead` | Blender script: one LED floodlight, lens at the origin facing +X; the game places one on every fixture |
| `PlayerGenA`, `CourtBench` | `/Game/Padel/...` | AI (Meshy, Tripo), switched off until there's an API key |

## Court steel and welded mesh

`build_court_frame.py` builds the fence structure from `asset_lib/court_frame_layout.py`, which places every piece from `CourtSpec.h` and the steel sections in `config/court_frame.json`, and is tested without Blender (`tests/test_court_frame.py`):

- **Posts** are 100 × 50 mm rectangular tube with rounded corners, centred on every glass and mesh joint, their court-side face 15 mm outside the court limit (12 mm glass plus a 3 mm gasket), so the glass bolts land on steel. Each corner is one L-shaped post that backs both the back glass and the side glass. Every post has a plastic cap, a 12 mm base plate with four galvanised anchors, and a rear gusset, all outside the court limit.
- **Mesh panels** cover exactly the mesh zones of `CourtSpec.h` (`BackWallSurfaceAt`, `SideWallSurfaceAt`), at most 2 m long, each welded into a 30 × 15 mm tube frame flush with the glass inner face and bolted to the posts. Panels taller than 2.5 m get a mid rail at 1.5 m. The exits stay open to full height.
- **Materials** (`unreal/metal_materials.py`). `M_PadelMetal` is physically based steel: powder coat is a paint (metallic 0), galvanised zinc is a metal (metallic 1), with a tiling orange-peel normal map (`T_PowderCoat_N`, written by `asset_lib/metal_textures.py`) at real scale, slight roughness variation, and dirt near the floor in world space. `M_PadelWireMesh` draws the 4 mm wires on a 50 mm grid analytically from the UVs (one UV unit is one cell) with a box filter sized by the screen-space derivatives. Up close the wires are sharp and round; once a wire is thinner than a pixel it turns into the right amount of partial coverage, which is dithered into the masked opacity and resolved by TSR. That avoids the moire and flicker that a mask texture with mipmaps gives from the broadcast camera, and there are no textures or LODs to manage.

## Setup

- **Python 3.11+** on your PATH. The pipeline itself needs only the standard library. `pip install -r Tools/AssetPipeline/requirements.txt` adds `requests`, which only `generate` needs.
- **Blender 4.2 LTS or newer.** Point `BLENDER` at the app:

  ```
  export BLENDER=/Applications/Blender.app/Contents/MacOS/Blender
  ```

  You can use the `bpy` module instead: run `pip install bpy` in a virtualenv, then `export BLENDER_PYTHON=/path/to/venv/bin/python`.
- **Unreal Engine 5.8**, the same install as the tests (`UE_ROOT`, which defaults to `/Users/Shared/Epic Games/UE_5.8` on a Mac). The Python Editor Script Plugin and Editor Scripting Utilities are turned on in `Padel.uproject`. Close the editor before running `unreal` or `all`.
- **Git LFS.** The `.uasset` files and the cleaned AI and hand-off FBX files go through LFS. Procedural FBX and PNG files aren't committed: `build` recreates them, and `SourceArt/.gitignore` lists them.

## Commands

```
pipeline.py check                    # validate the catalogue, palette, prompts and names (no Blender)
pipeline.py build [Id ...]           # Blender only: SourceArt/ and the manifest
pipeline.py unreal                   # import SourceArt/ into Content/Padel/
pipeline.py all [--generate]         # build, then import
pipeline.py generate Id [Id ...]     # call the AI service and download to SourceArt/_raw/<Id>/ (costs credits)
pipeline.py preview out.png [--preset IndoorClub] [--camera Broadcast]
                                     # Cycles render lit like the game: a Data/Lighting.json preset
                                     # (candela, lux, kelvin, EV100) from a Data/LookDev.json camera
```

`build` never calls an AI service unless you pass `--generate`. An AI asset that hasn't been generated yet is skipped with a note. Once it has been downloaded, `build` cleans it up again from the saved file at no cost.

## Adding an asset

Add an entry to `config/assets.json`:

- `id` is PascalCase. It becomes `SM_<Id>` or `SK_<Id>`.
- `kind` is `static` or `skeletal`.
- `category` is `Court`, `Equipment`, `Characters` or `Venue`.
- `source` is `procedural`, `ai` or `external`.
- `size` is in metres: `height_m` for characters, `length_m` for props (longest horizontal side) or `diameter_m`.
- `budget_tris` is the triangle budget.
- A skeletal mesh gets `lods` (`ratio` of LOD0, Unreal `screen_size`). A static mesh gets `"nanite": true` instead.

**AI assets** also need a `provider` (`meshy` or `tripo`), a `template` from `prompts/` and the template's `vars`. Characters need `"rig": {"skeleton": "ue5_mannequin"}`. Set `"enabled": true`, export the key (`MESHY_API_KEY` or `TRIPO_API_KEY`, never committed), then run `pipeline.py generate <Id>` and `pipeline.py all`. Before shipping an AI asset, check your plan's licence terms: free plans put output under CC BY 4.0.

**Hand-off files** are `source: "external"`: Mixamo, AccuRIG, a MetaHuman export, or a purchased model. Put the file in `SourceArt/_incoming/` (git ignores it), set `file`, `licence` and `credit`, and the same cleanup step sizes it, decimates it and exports it.

Material slots in procedural assets are named after the material instance they get in Unreal (`MI_Court_Turf`, `MI_TeamA_Shirt`...). To change a colour or a roughness, edit `config/palette.json` and run `pipeline.py unreal` again.

## Conventions

- **Axes.** Blender scripts use Unreal's axes in metres: X along the court (Team A on +X), Y across it, Z up. Only `bl_common.export_fbx` converts to centimetres.
- **Characters** face Blender −Y, which arrives facing +Y in Unreal like the engine mannequins. They're in a T-pose, and the armature object is named `Armature` so Unreal adds no extra root bone.
- **The racket's** grip centre is at the origin, with the head up (+Z) and the faces looking along ±Y, so a hand socket holds it without offsets.
- **Originality.** Prompts, negative prompts and IDs go through `config/blocklist.txt` before anything is sent. Add to it whenever a new real name comes up.

## Tests

```
python3 -m unittest discover -s Tools/AssetPipeline/tests -t Tools/AssetPipeline
```

The config, prompt, provider (against a fake HTTP session), manifest and texture tests always run. The Blender tests build each procedural asset, re-import the FBX and check the sizes from GDD 2, the budgets and the bone names. They run when `BLENDER` or `BLENDER_PYTHON` is set, and CI runs them with `bpy`. The Unreal import has no automated test yet, because CI has no engine. Run `pipeline.py unreal` on a Mac and check its `[padel-import]` log.
