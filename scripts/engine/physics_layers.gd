class_name PhysicsLayers
extends RefCounted
## Godot 3D physics layer numbers and bit masks. The names are registered in
## project.godot by tools/godot/configure_project.gd and must match LAYER_NAMES.
##
## The ball is simulated by our own integrator (CLAUDE.md), so these layers are
## for player movement, raycasts and debug tools, not for the ball's bounces.

const GROUND: int = 1
const GLASS: int = 2
const MESH: int = 3
const NET: int = 4
const CEILING: int = 5
## Run-off floor outside the playing area (out-of-court play).
const RUNOFF: int = 6
const PLAYERS: int = 7
const BALL: int = 8

## Layer number -> name shown in the editor.
const LAYER_NAMES: Dictionary[int, String] = {
	GROUND: "court_ground",
	GLASS: "court_glass",
	MESH: "court_mesh",
	NET: "court_net",
	CEILING: "court_ceiling",
	RUNOFF: "runoff_ground",
	PLAYERS: "players",
	BALL: "ball",
}


## Bit mask for a layer number (layer 1 is bit 0).
static func bit(layer: int) -> int:
	return 1 << (layer - 1)


## Mask with every court surface: what a player or a raycast "sees" as the court.
static func court_mask() -> int:
	return bit(GROUND) | bit(GLASS) | bit(MESH) | bit(NET) | bit(CEILING) | bit(RUNOFF)
