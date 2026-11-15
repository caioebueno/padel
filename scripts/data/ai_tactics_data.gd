class_name AiTacticsData
extends Resource
## Tuning for the AI's positioning, prediction and shot choice (GDD 8).
## The file is data/ai/tactics.tres. Distances are metres, times seconds.
## "Depth" is metres from the net toward a team's own back glass (0..10).

@export_group("Formation (GDD 8 pair tactics)")
## Depth of both players in the "both at the net" formation (GDD: 2-3 m).
@export var net_depth_m: float = 2.5
## Depth in the "both back" formation (GDD: 1-2 m in front of the back glass).
@export var back_depth_m: float = 8.5
## Side-to-side distance between partners (GDD: 4-5 m).
@export var partner_spacing_m: float = 4.5
## How far the pair's centre follows the ball across the court (0..1).
@export var ball_shift_factor: float = 0.35
@export var max_lateral_shift_m: float = 1.5
## Extra shift toward the centre for a player told to take the middle.
@export var take_middle_shift_m: float = 0.8
## A human partner closer to the net than this pulls the AI partner up.
@export var human_net_depth_m: float = 4.5
## Hysteresis on the human's depth so the AI partner doesn't flicker.
@export var human_depth_hysteresis_m: float = 1.0
## An own drive played closer than this to the net lets the pair advance.
@export var drive_to_net_max_depth_m: float = 6.0
## A lob landing deeper than this gets the pair time to advance.
@export var lob_advance_min_depth_m: float = 7.0
## An opponent closer to the net than this counts as "at the net".
@export var at_net_depth_m: float = 4.5
## Home position error for Positioning 1 (none at 99).
@export var home_error_max_m: float = 0.8
## Players keep this far from the walls.
@export var court_margin_m: float = 0.3
## Closest a player goes to the net.
@export var min_player_depth_m: float = 0.8

@export_group("Movement")
@export var arrive_radius_m: float = 0.15
@export var slow_radius_m: float = 1.0
## Sprint when the time to spare on an interception is below this.
@export var sprint_slack_s: float = 0.35
@export var sprint_distance_m: float = 3.0
## GDD 6.1 Speed: 4.5 m/s at 1, 6.5 m/s at 99 (used to plan, not to move).
@export var speed_at_1_mps: float = 4.5
@export var speed_at_99_mps: float = 6.5
## GDD 6.1 Acceleration: time to top speed.
@export var accel_time_at_1_s: float = 0.9
@export var accel_time_at_99_s: float = 0.45
## Plan with slightly less than top speed, so the AI isn't always just late.
@export var planning_speed_factor: float = 0.9
## GDD 6.1 Reach: sideways reach.
@export var reach_side_at_1_m: float = 0.9
@export var reach_side_at_99_m: float = 1.2
## Highest standing overhead contact.
@export var overhead_reach_at_1_m: float = 2.35
@export var overhead_reach_at_99_m: float = 2.6
## GDD 6.1 Jump: extra overhead height.
@export var jump_extra_at_1_m: float = 0.05
@export var jump_extra_at_99_m: float = 0.45
## GDD 6.1 Anticipation scales the difficulty's read delay.
@export var anticipation_delay_scale_at_1: float = 1.25
@export var anticipation_delay_scale_at_99: float = 0.8

@export_group("Prediction (GDD 8)")
@export var predict_dt_s: float = 1.0 / 120.0
@export var predict_horizon_s: float = 4.0
## Keep every Nth simulated step as a candidate interception sample.
@export var sample_every_steps: int = 2
## Rebound prediction noise for Wall Play 1 and 99.
@export var wall_noise_at_1_m: float = 0.6
@export var wall_noise_at_99_m: float = 0.05
## After the ball hits the glass the AI reads it again, this fraction of its read delay later.
@export var glass_reread_delay_scale: float = 0.5

@export_group("Interception")
@export var min_contact_height_m: float = 0.2
@export var ideal_contact_height_m: float = 1.0
## Above this the ball is played as an overhead.
@export var overhead_min_height_m: float = 1.7
## Distance between the player and the ball at contact (GDD 5.2: about 0.8 m).
@export var contact_offset_m: float = 0.6
## Balls closer to a wall than this aren't interception candidates (the player can't stand there).
@export var intercept_wall_margin_m: float = 0.25
@export var score_height_weight: float = 1.0
@export var score_slack_weight: float = 1.0
@export var slack_cap_s: float = 0.5
@export var score_time_weight: float = 0.3
@export var volley_at_net_bonus: float = 0.6
@export var overhead_bonus: float = 0.4
## The shot request goes to the controller this long before contact.
@export var swing_lead_s: float = 0.35

@export_group("Who takes the ball")
## Width either side of the pair's centre that counts as "down the middle".
@export var middle_band_m: float = 0.8
## The non-default partner takes a middle ball when better placed by this much.
@export var better_placed_margin_s: float = 0.25
## Chance (Teamwork 1) that a middle ball goes to the wrong partner.
@export var confusion_max: float = 0.15

@export_group("Shot choice (GDD 8)")
@export var smash_max_depth_m: float = 5.0
@export var smash_min_height_m: float = 2.3
## Time to spare that counts as "set under the ball" (GDD 4).
@export var set_slack_s: float = 0.25
## Deeper than this, or off the glass, the player is pinned back.
@export var pinned_depth_m: float = 8.0
@export var chiquita_max_depth_m: float = 7.0
@export var low_ball_height_m: float = 1.0
@export var weight_bandeja: float = 1.0
@export var weight_vibora_left: float = 0.8
@export var weight_vibora_right: float = 0.15
@export var weight_smash: float = 1.2
@export var weight_overhead_lob_from_back: float = 0.5
@export var weight_lob_vs_net: float = 1.2
@export var weight_chiquita_vs_net: float = 0.7
@export var weight_drive_vs_net: float = 0.4
@export var weight_drive_base: float = 1.0
@export var weight_lob_base: float = 0.3
@export var weight_chiquita_base: float = 0.15
@export var weight_lob_pinned: float = 0.8
@export var pinned_drive_scale: float = 0.7
@export var weight_split_drive: float = 1.0
@export var weight_volley: float = 1.0
@export var weight_lob_volley: float = 0.15
@export var safe_lob_bonus: float = 0.6
@export var kick_serve_chance: float = 0.3

@export_group("Targets (opponents' depth)")
@export var lob_depth_m: float = 8.6
@export var lob_side_m: float = 2.0
@export var chiquita_depth_m: float = 2.5
@export var drive_deep_depth_m: float = 7.5
@export var drive_side_m: float = 3.0
@export var volley_deep_depth_m: float = 7.5
@export var feet_depth_offset_m: float = 0.5
@export var bandeja_depth_m: float = 8.0
@export var bandeja_side_m: float = 1.5
@export var vibora_depth_m: float = 7.5
@export var vibora_side_m: float = 4.0
@export var smash_depth_m: float = 8.5
@export var serve_depth_m: float = 5.5
@export var serve_side_m: float = 2.5
@export var target_side_limit_m: float = 4.5
@export var target_depth_min_m: float = 1.0
@export var target_depth_max_m: float = 9.3
## Extra aim scatter radius before the difficulty multiplier (GDD 8 table).
@export var aim_scatter_base_m: float = 0.35

@export_group("Charge per shot (0..1)")
@export var charge_drive: float = 0.75
@export var charge_volley: float = 0.6
@export var charge_lob: float = 0.6
@export var charge_bandeja: float = 0.55
@export var charge_vibora: float = 0.8
@export var charge_smash: float = 1.0
@export var charge_chiquita: float = 0.35
@export var charge_serve: float = 0.7

@export_group("Timing windows (GDD 5.1 base; shot data wins once it lands)")
@export var window_perfect_s: float = 0.04
@export var window_good_s: float = 0.09
@export var window_early_late_s: float = 0.16
## A miss is released this much further outside the early/late window, at most.
@export var window_miss_extra_s: float = 0.08


## Linear map of a 1..99 rating onto [at_1, at_99].
static func by_rating(rating: int, at_1: float, at_99: float) -> float:
	return lerpf(at_1, at_99, clampf((rating - 1) / 98.0, 0.0, 1.0))


func charge_for(shot_id: StringName) -> float:
	match shot_id:
		AiEnums.SHOT_DRIVE: return charge_drive
		AiEnums.SHOT_VOLLEY: return charge_volley
		AiEnums.SHOT_LOB: return charge_lob
		AiEnums.SHOT_BANDEJA: return charge_bandeja
		AiEnums.SHOT_VIBORA: return charge_vibora
		AiEnums.SHOT_SMASH: return charge_smash
		AiEnums.SHOT_CHIQUITA: return charge_chiquita
		AiEnums.SHOT_SERVE_FLAT, AiEnums.SHOT_SERVE_KICK: return charge_serve
	return charge_drive
