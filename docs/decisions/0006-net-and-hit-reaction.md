# 0006: The net, and how it reacts to the ball

**Status:** accepted (court art, net). **GDD:** 2 (net 0.88 m centre, 0.92 m posts), 3.4 (`ball_bounced` on `net`), 10.

## Decision

- The net is its own asset, `SM_CourtNet`, built by `Tools/AssetPipeline/blender/build_net.py` from `CourtSpec.h`: knotted cord cloth, headband tape folded over the cable, border and side bands, lacing, the exposed cable, two posts on bolted plates, a ratchet winch on one post and an eye bolt with a turnbuckle on the other. Posts sit where `CourtLayout`'s `NetPost` colliders are, so the art and the collision agree. It is placed at the world origin in court space, like the other court parts.
- The cord is a two-sided masked card (`T_NetGrid_Mask` plus `T_NetGrid_Normal`, one UV tile per 4.5 cm cell), not modelled strands. From the broadcast camera a cell is a few pixels, so strands would cost tens of thousands of triangles for nothing visible. Cell rows follow the sag at the top and the flat bottom cord, the way a real net hangs.
- **Hit reaction on the GPU.** The cloth is a dense grid (about 11k triangles). `M_PadelNet` and `M_PadelNetSolid` move its vertices along X with World Position Offset, reading six vectors from `MPC_NetReaction`. `UPadelNetReactionSubsystem` listens to `UPadelMatchEvents`, runs the engine-free model `FNetReaction` (`Presentation/Model/`) and writes the collection only while the net moves. So any `SM_CourtNet` in any level reacts, there is no per-vertex CPU work, and the model is checked by the native tests on every PR.
- The shape is analytic, not a cloth solver: a pocket where the ball went in, plus a shiver running along the cable, damped over about two seconds, zero at the posts and the ground, and smaller at the tape (the cable is tight). The same formula is in `PadelNetReaction.cpp` (tested) and `Tools/AssetPipeline/unreal/NetReaction.hlsl` (the material). Change both together.
- The direction comes from the simulation: a ball that goes into the net is reported a ball radius in front of it, on the side it came from. `ball_bounced` carries no speed, so the last `ball_hit` speed sizes the pocket. Tuning is in `Data/Net.json`.

## Why not Chaos cloth

Chaos cloth needs a skeletal mesh and a cloth asset made in the editor (binary, not diffable), costs CPU every frame, and isn't deterministic across platforms. The net's motion is cosmetic and short, so an analytic shape driven by the match's own events looks right for a fraction of the cost.

## Not done yet

- A player touching the net (`NET_TOUCH`) doesn't move it: the event has no position.
- Wind or idle sway: indoor slice court, so none.
