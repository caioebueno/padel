class_name AiEnums
extends RefCounted
## AI-only states and the shot ids the AI asks for. Referee reason codes and
## the engine's ShotType enum live in scripts/engine/types.gd (GDD 3.4); the
## ids here match data/shots/<id>.tres and map one-to-one onto that enum.

## GDD 8: split (one up, one back) is a mistake state, so it is never a target.
enum Formation { BOTH_BACK, BOTH_NET }

## GDD 7 and 8: the four partner commands. They last until the end of the point
## or the next command.
enum PartnerCommand { NONE, GO_NET, STAY_BACK, TAKE_MIDDLE, YOURS }

## GDD 6.1 profile: preferred side and handedness. Same order as PlayerData.
enum Side { RIGHT, LEFT }
enum Hand { RIGHT, LEFT }

enum ContactKind { GROUNDSTROKE, VOLLEY, OVERHEAD }

const SHOT_DRIVE: StringName = &"drive"
const SHOT_VOLLEY: StringName = &"volley"
const SHOT_LOB: StringName = &"lob"
const SHOT_BANDEJA: StringName = &"bandeja"
const SHOT_VIBORA: StringName = &"vibora"
const SHOT_SMASH: StringName = &"smash"
const SHOT_CHIQUITA: StringName = &"chiquita"
const SHOT_SERVE_FLAT: StringName = &"serve_flat"
const SHOT_SERVE_KICK: StringName = &"serve_kick"

const OVERHEADS: Array[StringName] = [SHOT_BANDEJA, SHOT_VIBORA, SHOT_SMASH]

const GRADE_PERFECT: StringName = &"perfect"
const GRADE_GOOD: StringName = &"good"
const GRADE_EARLY: StringName = &"early"
const GRADE_LATE: StringName = &"late"
const GRADE_MISS: StringName = &"miss"
