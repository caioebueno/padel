class_name AiBallModel
extends RefCounted
## The ball physics the AI predicts with (GDD 8: the AI simulates the ball
## forward with the same deterministic code as the match). In the game this
## wraps the engine's BallSim; the soak stand-in lives in tests/soak/support/.


## Advances `state` in place by `dt` and returns the contacts in that step.
## `rng` drives mesh randomness; prediction passes null for none.
func step(_state: AiBallState, _dt: float, _rng: RandomNumberGenerator) -> Array[AiContact]:
	push_error("AiBallModel.step() must be overridden")
	return []


## True when the ball can no longer be played: it left the court or stopped.
func is_dead(_state: AiBallState) -> bool:
	return false
