# 0008: The racket, and how it sits in the hand

**Status:** proposed (racket art). **GDD:** 2 (equipment sizes), 10 (realistic look, readable from the broadcast camera), 12.

## Decision

- **An invented racket in a real category.** The brief was a modern pro power racket, the kind attacking players use, and its second pass was modelled on photos of one for shape, proportion and materials only. It has:
  - a teardrop head with a broad flat top and a long tapering throat;
  - a black carbon face with visible twill and a sand-textured hitting surface;
  - a drilled hole pattern;
  - a thick rounded frame with a protector band over the tip and a weight slot in it;
  - one open V-shaped heart;
  - an overgrip with a ribbed collar, and a wrist strap.

  It copies no product's cosmetics. The brand is **OSTREL**, the model **KEYSTONE** and the tag **CARBON 18K**. All three are invented and were searched before use. The graphic and colourway are our own, black with silver and a volt yellow-green accent:
  - a silver stone arch over the sweet spot, its keystone in volt (the model's name);
  - a volt chevron over the heart and thin shoulder lines;
  - slanted lettering on the face, and OSTREL printed along both sides of the frame;
  - volt dots on the black overgrip.

  No logo, stripe mark or name from the reference racket is used.

  The real racket that inspired the brief, and the real brands, are in `config/blocklist.txt`. `tests/test_racket.py` checks the names against it.
- **Regulation size.** It is at most 455 mm long including the bumper, at most 260 mm wide and 38 mm thick. The holes are 12 mm across, inside the 9 to 13 mm allowed. The pipeline tests check every one of these on the built mesh.
- **Built by the pipeline, like the ball.** `Tools/AssetPipeline/blender/build_racket.py` builds `SM_Racket` from `config/racket.json`. The layout maths lives in `asset_lib/racket_layout.py`, so it is tested without Blender. That covers the outline, the heart, the 66 holes, mirrored and kept clear of the lettering, the graphic and the edge's arc length (which the frame lettering is printed along). Colours are in `palette.json` `racket`. `SM_Racket` replaces `SM_RacketPlaceholder`.
- **Hand-built LODs, not Nanite.** The racket is small, moves every frame and is never seen at more than a few hundred pixels. It is a static mesh with three LODs, each rebuilt for its size:
  - **LOD0** (about 22,500 triangles) has round-edged drilled holes, the weight slot's screws, the collar ribs and the strap slider, for close-ups and replays.
  - **LOD1** (about 6,200) keeps the holes and the strap with fewer segments.
  - **LOD2** (about 600) is the flat silhouette. Its holes are only the mask texture, and it has no strap.
- **The detail is in the face material.** `M_PadelRacketFace` draws the paint, the lines, the lettering and the hole rims from one uncompressed mask. Below the paint, a twill weave shows through the clear carbon. Over everything sit a sand-grit normal map and a rougher finish. The grit and weave fade out beyond a few metres, where they would only shimmer. The frame and the grip are instances of the same master with their own masks (the frame lettering and the overgrip dots). The foam core, bumper, metal, accent, cap and strap use the shared opaque master.
- **The racket sits in the palm, worked out from the bones.** The old code attached the racket to `hand_r` with no offset, so the hand bone's pivot (the wrist) went through the middle of the head. Now `UPadelPlayerAnimComponent` solves the grip once from the skeleton's reference pose (`Animation/Model/RacketGrip.h`, plain C++ with native tests):
  - **The palm.** It finds the palm from the wrist and the middle-finger knuckle. On a skeleton without fingers it uses the forearm direction instead.
  - **The handle.** It lays the handle across the palm, 30 mm in front of it.
  - **The grip.** It tilts the head towards the fingers and rolls the face about the handle. That gives a continental grip, how most padel players hold the racket.

  It uses joint positions, not bone axes, because those differ between the placeholder skeleton and the MetaHumans while the joints are where a hand is. The result rides `hand_r`, so every animation and the MetaHuman retarget carry it. The numbers are in `Data/Racket.json` `Grip`.
- **Before a skeletal body is in,** the procedural swing still moves the racket. It now places the face centre (`FaceCentreM`), not the handle, on the swing arc, and turns the face towards the net.
- **A fresh checkout still plays.** Until `SM_Racket` is imported, the engine cube stays as the placeholder racket and a line is logged.

## Tuning

- Look: `Tools/AssetPipeline/config/racket.json` (outline, heart, holes, hardware, grip, graphic, LOD detail) and `palette.json` `racket`. Re-run `pipeline.py all`.
- Grip: `Data/Racket.json` (palm share, how far out and along the handle, head tilt, face roll). Read at run time.
- `pipeline.py preview-racket out.png --shot hero|closeup|lods` renders it in Blender with the same textures.

## Not done yet

- Team or player colourways: every player has the same racket. A second palette group and a per-player choice in `Data/Players.json` would add them.
- The racket is always in the right hand (`RacketBone` in `Data/Animation.json`), as it was before. A left-handed player in the profile needs `hand_l` and a mirrored solve, which `SolveRacketGrip` already handles (its tests cover the left arm).
- Fingers don't wrap the handle. The placeholder has no finger bones, and MetaHuman finger poses would come from the clips or a grip pose.
- The racket has no collision. Shots are the shot model's (GDD 4), so it is cosmetic only.
