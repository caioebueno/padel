class_name TeamFrame
extends RefCounted
## Converts between court space (GDD 2) and a team's own frame.
##
## A team is named by the sign of the half it stands on: +1 for the +Z half,
## -1 for the -Z half. In the team frame, x is metres to the player's right as
## they face the net, and y is depth: metres from the net toward their own back
## glass. Team A on +Z faces -Z, so its right is -X.


static func to_local(half_sign: int, court: Vector3) -> Vector2:
	return Vector2(-court.x * half_sign, court.z * half_sign)


static func to_court(half_sign: int, local: Vector2, height: float = 0.0) -> Vector3:
	return Vector3(-local.x * half_sign, height, local.y * half_sign)


## A court-space direction on the ground as Agent 3's move vector: x to the
## player's right, y toward the net.
static func move_vector(half_sign: int, court_dir: Vector3) -> Vector2:
	var local: Vector2 = to_local(half_sign, court_dir)
	return Vector2(local.x, -local.y)


## Inverse of move_vector.
static func court_direction(half_sign: int, move: Vector2) -> Vector3:
	return to_court(half_sign, Vector2(move.x, -move.y))
