class_name DifficultyData
extends Resource
## One AI difficulty level (GDD 8, "Difficulty levels"). Files live in
## data/difficulty/<id>.tres. Every value here is a tuning number.

@export var id: StringName = &"medium"
@export var display_name: String = "Medium"

## Seconds between the opponent's contact and the AI starting to move for the
## ball (scaled per player by Anticipation, see AiTacticsData).
@export_range(0.0, 1.0, 0.01) var read_delay_s: float = 0.18

@export_group("Timing grade distribution")
## Relative weights of the timing grade the AI releases with (GDD 5.1).
@export_range(0.0, 1.0, 0.01) var timing_weight_perfect: float = 0.25
@export_range(0.0, 1.0, 0.01) var timing_weight_good: float = 0.6
@export_range(0.0, 1.0, 0.01) var timing_weight_early_late: float = 0.13
@export_range(0.0, 1.0, 0.01) var timing_weight_miss: float = 0.02

@export_group("Shot choice")
## 0..1. How strongly the AI prefers the safe option (deep lob, deep middle).
@export_range(0.0, 1.0, 0.01) var safe_shot_bias: float = 0.4
## 0..1. How strongly the AI picks the situational shot (GDD 8 shot choice list).
@export_range(0.0, 1.0, 0.01) var tactical_weight: float = 0.6

@export_group("Accuracy")
## Multiplies the AI's extra aim scatter (GDD 8 table: 1.4 / 1.0 / 0.8).
@export_range(0.0, 3.0, 0.01) var aim_scatter_mult: float = 1.0
## Added to every rating of the AI's players before play (-10 / 0 / +5).
@export_range(-30, 30, 1) var attribute_offset: int = 0
## Multiplies the rebound prediction noise that Wall Play sets.
@export_range(0.0, 3.0, 0.01) var prediction_noise_mult: float = 1.0


## Timing weights in grade order: perfect, good, early/late, miss.
func timing_weights() -> PackedFloat32Array:
	return PackedFloat32Array([
		timing_weight_perfect, timing_weight_good, timing_weight_early_late, timing_weight_miss,
	])
