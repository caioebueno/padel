class_name InputActions
extends RefCounted
## Input action names (GDD section 7). Use these constants instead of string
## literals. The bindings themselves are written into project.godot by
## tools/godot/configure_project.gd; change them there and re-run it.

const MOVE_LEFT: StringName = &"move_left"
const MOVE_RIGHT: StringName = &"move_right"
## Toward the opponents ("up" on the stick, GDD 9).
const MOVE_FORWARD: StringName = &"move_forward"
const MOVE_BACK: StringName = &"move_back"
const SPRINT: StringName = &"sprint"

const SHOT_DRIVE: StringName = &"shot_drive"
const SHOT_LOB: StringName = &"shot_lob"
const SHOT_OVERHEAD: StringName = &"shot_overhead"
## Post-slice, bound now so the layout is stable.
const SHOT_DROP: StringName = &"shot_drop"
const MOD_ATTACK: StringName = &"mod_attack"
const MOD_TOUCH: StringName = &"mod_touch"

const CMD_NET: StringName = &"cmd_net"
const CMD_BACK: StringName = &"cmd_back"
const CMD_MIDDLE: StringName = &"cmd_middle"
const CMD_YOURS: StringName = &"cmd_yours"

const PAUSE: StringName = &"pause"

const ALL: Array[StringName] = [
	MOVE_LEFT, MOVE_RIGHT, MOVE_FORWARD, MOVE_BACK, SPRINT,
	SHOT_DRIVE, SHOT_LOB, SHOT_OVERHEAD, SHOT_DROP, MOD_ATTACK, MOD_TOUCH,
	CMD_NET, CMD_BACK, CMD_MIDDLE, CMD_YOURS,
	PAUSE,
]


## Move/aim vector from the left stick or WASD. x = right, y = forward
## (toward the opponents). Callers rotate it into court space for their team.
static func get_move_vector() -> Vector2:
	return Input.get_vector(MOVE_LEFT, MOVE_RIGHT, MOVE_BACK, MOVE_FORWARD)
