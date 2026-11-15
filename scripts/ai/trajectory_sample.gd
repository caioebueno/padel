class_name TrajectorySample
extends RefCounted
## A predicted ball position, with what has happened on the half it is on.

## Seconds after the prediction started.
var t: float
var position: Vector3
var velocity: Vector3
## Sign of the half the ball is over (+1 or -1).
var half: int
## Ground bounces on this half since the ball last crossed the net.
var ground_bounces: int
## True once the ball hit a wall on this half after bouncing there (GDD 4,
## off-the-glass returns).
var off_glass: bool


func _init(time: float, p: Vector3, v: Vector3, h: int, bounces: int, glass: bool) -> void:
	t = time
	position = p
	velocity = v
	half = h
	ground_bounces = bounces
	off_glass = glass
