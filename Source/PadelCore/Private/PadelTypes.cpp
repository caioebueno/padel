#include "PadelTypes.h"

namespace Padel
{
	const char* PointReasonCode(EPointReason Reason)
	{
		switch (Reason)
		{
		case EPointReason::None: return "NONE";
		case EPointReason::DoubleBounce: return "DOUBLE_BOUNCE";
		case EPointReason::IntoNet: return "INTO_NET";
		case EPointReason::WallBeforeBounce: return "WALL_BEFORE_BOUNCE";
		case EPointReason::OutDirect: return "OUT_DIRECT";
		case EPointReason::OwnSide: return "OWN_SIDE";
		case EPointReason::BodyHit: return "BODY_HIT";
		case EPointReason::DoubleHit: return "DOUBLE_HIT";
		case EPointReason::NetTouch: return "NET_TOUCH";
		case EPointReason::Crossed: return "CROSSED";
		case EPointReason::RacketThrown: return "RACKET_THROWN";
		case EPointReason::DoubleFault: return "DOUBLE_FAULT";
		case EPointReason::VolleyedServe: return "VOLLEYED_SERVE";
		case EPointReason::OutOfCourtWinner: return "OUT_OF_COURT_WINNER";
		case EPointReason::CameBackOver: return "CAME_BACK_OVER";
		case EPointReason::StuckInFence: return "STUCK_IN_FENCE";
		}
		return "UNKNOWN";
	}

	const char* ServeFaultCode(EServeFault Fault)
	{
		switch (Fault)
		{
		case EServeFault::None: return "NONE";
		case EServeFault::NoBounce: return "NO_BOUNCE";
		case EServeFault::AboveWaist: return "ABOVE_WAIST";
		case EServeFault::FootFault: return "FOOT_FAULT";
		case EServeFault::MissedBall: return "MISSED_BALL";
		case EServeFault::IntoNet: return "INTO_NET";
		case EServeFault::WrongBox: return "WRONG_BOX";
		case EServeFault::WallBeforeBounce: return "WALL_BEFORE_BOUNCE";
		case EServeFault::MeshAfterBounce: return "MESH_AFTER_BOUNCE";
		case EServeFault::Out: return "OUT";
		case EServeFault::HitPartner: return "HIT_PARTNER";
		}
		return "UNKNOWN";
	}

	const char* ShotTypeName(EShotType Shot)
	{
		switch (Shot)
		{
		case EShotType::Drive: return "drive";
		case EShotType::Volley: return "volley";
		case EShotType::Lob: return "lob";
		case EShotType::Bandeja: return "bandeja";
		case EShotType::Vibora: return "vibora";
		case EShotType::Smash: return "smash";
		case EShotType::Chiquita: return "chiquita";
		case EShotType::ServeFlat: return "serve_flat";
		case EShotType::ServeKick: return "serve_kick";
		case EShotType::Mishit: return "mishit";
		case EShotType::Count: break;
		}
		return "unknown";
	}

	const char* TimingGradeName(ETimingGrade Grade)
	{
		switch (Grade)
		{
		case ETimingGrade::Perfect: return "PERFECT";
		case ETimingGrade::Good: return "GOOD";
		case ETimingGrade::Early: return "EARLY";
		case ETimingGrade::Late: return "LATE";
		case ETimingGrade::Miss: return "MISS";
		}
		return "UNKNOWN";
	}
}
