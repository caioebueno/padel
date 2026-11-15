class_name CourtSpec
extends RefCounted
## Real padel court measurements and the shared coordinate system (GDD section 2).
##
## Pure data and maths: no Nodes, safe to use from headless engine code.
## The court scene in scenes/court/ is generated from these values by
## tools/godot/build_court.gd, so this file is the single source of truth.
##
## Coordinates: metres, Y up, court centred on the origin, length along Z
## (-10 to +10), width along X (-5 to +5), net on the plane Z = 0,
## ground at Y = 0. Team A starts on the +Z half.
##
## These are regulation measurements, not tuning numbers, which is why they
## live in code rather than in data/.

# --- Playing area (inside measurements) ---
const LENGTH: float = 20.0
const WIDTH: float = 10.0
const HALF_LENGTH: float = LENGTH / 2.0
const HALF_WIDTH: float = WIDTH / 2.0

# --- Net ---
const NET_HEIGHT_CENTRE: float = 0.88
const NET_HEIGHT_POSTS: float = 0.92
## Regulation maximum post height.
const NET_POST_HEIGHT: float = 1.05

# --- Lines (lines count as in) ---
const LINE_WIDTH: float = 0.05
## Service line distance from the net.
const SERVICE_LINE_FROM_NET: float = 6.95
## How far the centre service line runs past the service line toward the back wall.
const CENTRE_LINE_EXTENSION: float = 0.20

# --- Walls ---
## Back wall: glass, full width.
const BACK_GLASS_HEIGHT: float = 3.0
## Metal mesh on top of the back glass and the first side panel, up to this total height.
const FENCE_TOTAL_HEIGHT: float = 4.0
## Side glass: first panel from each back wall.
const SIDE_GLASS_FULL_LENGTH: float = 2.0
const SIDE_GLASS_FULL_HEIGHT: float = 3.0
## Side glass: the "step" panel after it.
const SIDE_GLASS_STEP_LENGTH: float = 2.0
const SIDE_GLASS_STEP_HEIGHT: float = 2.0
## Mesh on the rest of the side walls, and above the step panel.
const SIDE_MESH_HEIGHT: float = 3.0
## Exit opening: one per side per half, at the net end of the side glass.
const EXIT_WIDTH: float = 2.0
## Indoor slice court: hitting the ceiling is a fault.
const CEILING_HEIGHT: float = 8.0

# --- Placeholder build sizes (not regulation, only for collision volumes) ---
const WALL_THICKNESS: float = 0.12
const GROUND_THICKNESS: float = 0.2
## Run-off floor outside the side walls and behind the back walls, for out-of-court play.
const RUNOFF_SIDE: float = 4.0
const RUNOFF_BACK: float = 2.0

# --- Derived Z positions (measured from the origin, positive half) ---
const SERVICE_LINE_Z: float = SERVICE_LINE_FROM_NET
## Z where the full-height side glass ends and the step begins.
const SIDE_GLASS_STEP_START_Z: float = HALF_LENGTH - SIDE_GLASS_FULL_LENGTH
## Z where the step glass ends (net end of the side glass).
const SIDE_GLASS_END_Z: float = SIDE_GLASS_STEP_START_Z - SIDE_GLASS_STEP_LENGTH
## Exit opening spans Z from EXIT_NEAR_Z to EXIT_FAR_Z on each half.
const EXIT_FAR_Z: float = SIDE_GLASS_END_Z
const EXIT_NEAR_Z: float = SIDE_GLASS_END_Z - EXIT_WIDTH

enum Half { A = 1, B = -1 }

# --- Surface names (GDD 3.4, ball_bounced surface values) ---
# Every collider in the court scene carries one of these as metadata "surface".
const SURFACE_GROUND: StringName = &"ground"
const SURFACE_GLASS: StringName = &"glass"
const SURFACE_MESH: StringName = &"mesh"
const SURFACE_NET: StringName = &"net"
const SURFACE_CEILING: StringName = &"ceiling"
const SURFACES: Array[StringName] = [
	SURFACE_GROUND, SURFACE_GLASS, SURFACE_MESH, SURFACE_NET, SURFACE_CEILING,
]


## Net top height at a given X across the court. The tape sags in a parabola
## from 0.92 m at the posts to 0.88 m in the centre.
static func net_height_at(x: float) -> float:
	var t: float = clampf(absf(x) / HALF_WIDTH, 0.0, 1.0)
	return NET_HEIGHT_CENTRE + (NET_HEIGHT_POSTS - NET_HEIGHT_CENTRE) * t * t


## True when a ground point (X, Z) is inside the playing area. Lines count as in,
## and the outer edge of the boundary is the inside face of the walls.
static func is_inside_court(x: float, z: float) -> bool:
	return absf(x) <= HALF_WIDTH and absf(z) <= HALF_LENGTH


## Which half a Z position is on. The net plane itself counts as half A.
static func half_of(z: float) -> Half:
	return Half.A if z >= 0.0 else Half.B


## Height of the solid (glass) part of the side wall at a given Z.
## Returns 0.0 where the side wall is mesh or an opening.
static func side_glass_height_at(z: float) -> float:
	var az: float = absf(z)
	if az > HALF_LENGTH:
		return 0.0
	if az >= SIDE_GLASS_STEP_START_Z:
		return SIDE_GLASS_FULL_HEIGHT
	if az >= SIDE_GLASS_END_Z:
		return SIDE_GLASS_STEP_HEIGHT
	return 0.0


## True when Z falls inside an exit opening in the side walls.
static func is_exit_at(z: float) -> bool:
	var az: float = absf(z)
	return az > EXIT_NEAR_Z and az < EXIT_FAR_Z

