class_name AiBallState
extends RefCounted
## The ball as the AI sees it. Mirrors the engine's BallState so the adapter
## is a field copy.

var position: Vector3
var velocity: Vector3
var spin: Vector3


func _init(p: Vector3 = Vector3.ZERO, v: Vector3 = Vector3.ZERO, s: Vector3 = Vector3.ZERO) -> void:
	position = p
	velocity = v
	spin = s


func copy() -> AiBallState:
	return AiBallState.new(position, velocity, spin)
