// Shared enums for the simulation, gameplay, AI and UI (CLAUDE.md: reason codes and
// shot types live here). Adding a value is a contract change: append, never reorder,
// so replays and data files keep their meaning. Padel mirrors these as UENUMs where
// Blueprints need them.
//
// Plain C++ (no Unreal headers).

#pragma once

#include "PadelCoreApi.h"

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

	// How a player is moving (GDD 6.3, BIOMECHANICS_MOVEMENT 2.3, 2.4, 5.2), read by
	// animation to pick the gait. Set by FPlayerMotion every step.
	enum class EMovementMode : std::uint8_t
	{
		// Slower than the split-step limit: small steps on the spot.
		Adjust,
		// Sideways, square to the net.
		Shuffle,
		// The hip turn into a sideways run.
		Crossover,
		// Backwards, still facing the net.
		Backpedal,
		// The turn toward the back glass before running back.
		TurnAndRun,
		// Running, turned toward the direction of travel (or forward to the net).
		Run,
		// Run with sprint held.
		Sprint,
		// Braking.
		Decel,
	};

	// The GDD 3.3 code, e.g. "DOUBLE_BOUNCE". The UI and stats read reasons by these names.
	PADELCORE_API const char* PointReasonCode(EPointReason Reason);
	PADELCORE_API const char* ServeFaultCode(EServeFault Fault);
	// Row id in Data/Shots.json, e.g. "bandeja".
	PADELCORE_API const char* ShotTypeName(EShotType Shot);
	PADELCORE_API const char* TimingGradeName(ETimingGrade Grade);
}
