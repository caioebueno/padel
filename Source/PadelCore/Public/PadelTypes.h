// Shared enums for the simulation, gameplay, AI and UI (CLAUDE.md: reason codes and
// shot types live here). Adding a value is a contract change: append, never reorder,
// so replays and data files keep their meaning. Padel mirrors these as UENUMs where
// Blueprints need them.
//
// Plain C++ (no Unreal headers).

#pragma once

#include <cstdint>

namespace Padel
{
	// Team A starts on the +X half (GDD 2). Player ids 0, 1 are Team A; 2, 3 are Team B.
	enum class ETeam : int
	{
		A = 0,
		B = 1,
	};

	inline constexpr int NumPlayers = 4;

	// GDD 4. Shared with the shot system (Agent 3).
	enum class EShotType : std::uint8_t
	{
		Drive,
		Volley,
		Lob,
		Bandeja,
		Vibora,
		Smash,
		Chiquita,
		ServeFlat,
		ServeKick,
		Mishit,
		Count,
	};

	// GDD 5.1.
	enum class ETimingGrade : std::uint8_t
	{
		Perfect,
		Good,
		Early,
		Late,
		Miss,
	};

	// Why a point ended (GDD 3.3). Each value names what the LOSING team did, except
	// the last three, which describe what happened to the winning team's shot.
	enum class EPointReason : int
	{
		None,
		DoubleBounce,
		IntoNet,
		WallBeforeBounce,
		OutDirect,
		OwnSide,
		BodyHit,
		DoubleHit,
		NetTouch,
		Crossed,
		RacketThrown,
		DoubleFault,
		VolleyedServe,
		// GDD 3.3 special case: bounced in, then left the court or hit the ceiling.
		OutOfCourtWinner,
		// GDD 3.3 special case: bounced in, then spin or the glass carried it back over.
		CameBackOver,
		// GDD 3.3 special case: stuck in the mesh or a gap, or stopped dead.
		StuckInFence,
	};

	// Why a single serve was a fault (GDD 3.2). Two faults make a DoubleFault point.
	enum class EServeFault : int
	{
		None,
		NoBounce,
		AboveWaist,
		FootFault,
		MissedBall,
		IntoNet,
		WrongBox,
		WallBeforeBounce,
		MeshAfterBounce,
		Out,
		HitPartner,
	};

	// Which half a serve is played from, as the player faces the net (GDD 3.2).
	enum class EServeSide : int
	{
		Right,
		Left,
	};

	constexpr int TeamIndex(ETeam Team)
	{
		return static_cast<int>(Team);
	}

	constexpr ETeam OtherTeam(ETeam Team)
	{
		return Team == ETeam::A ? ETeam::B : ETeam::A;
	}

	constexpr ETeam TeamOfPlayer(int PlayerId)
	{
		return PlayerId < 2 ? ETeam::A : ETeam::B;
	}

	constexpr int PartnerOf(int PlayerId)
	{
		return PlayerId ^ 1;
	}

	constexpr bool IsValidPlayer(int PlayerId)
	{
		return PlayerId >= 0 && PlayerId < NumPlayers;
	}

	constexpr bool IsServeShot(EShotType Shot)
	{
		return Shot == EShotType::ServeFlat || Shot == EShotType::ServeKick;
	}

	// The GDD 3.3 code, e.g. "DOUBLE_BOUNCE". The UI and stats read reasons by these names.
	const char* PointReasonCode(EPointReason Reason);
	const char* ServeFaultCode(EServeFault Fault);
	// Row id in Data/Shots.json, e.g. "bandeja".
	const char* ShotTypeName(EShotType Shot);
	const char* TimingGradeName(ETimingGrade Grade);
}
