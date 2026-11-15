class_name AiContact
extends RefCounted
## One contact the ball model reports during a step.

## One of CourtSpec.SURFACE_*.
var surface: StringName
var position: Vector3


func _init(s: StringName = CourtSpec.SURFACE_GROUND, p: Vector3 = Vector3.ZERO) -> void:
	surface = s
	position = p
