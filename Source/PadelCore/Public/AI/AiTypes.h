// AI-only enums and the values the AI hands to the shot system and movement.
// Referee reason codes and EShotType live in PadelTypes.h (CLAUDE.md); EAiShot maps
// one-to-one onto EShotType in the adapter, and matches the rows in Data/Shots.json.

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiMath.h"

#include <optional>

namespace Padel::AI
{
	// GDD 8: split (one up, one back) is a mistake state, so it is never a target.
	enum class EFormation : int
	{
		BothBack,
		BothNet,
	};

	// GDD 7 and 8: the four partner commands. They last until the end of the point
	// or the next command.
	enum class EPartnerCommand : int
	{
		None,
		GoNet,
		StayBack,
		TakeMiddle,
		Yours,
	};

	// GDD 6.1 profile. Same order as Padel::Shots::ECourtSide / EHandedness.
	enum class ESide : int
	{
		Right,
		Left,
	};

	enum class EHand : int
	{
		Right,
		Left,
	};

	enum class EContactKind : int
	{
		Groundstroke,
		Volley,
		Overhead,
	};

	enum class EAiShot : int
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
	};

	// GDD 5.1 grades. Early and late share a window.
	enum class EAiTimingGrade : int
	{
		Perfect,
		Good,
		Early,
		Late,
		Miss,
	};

	PADELCORE_API const char* AiShotName(EAiShot Shot);
	PADELCORE_API bool IsOverhead(EAiShot Shot);
	PADELCORE_API bool IsServe(EAiShot Shot);

	// What the AI asks the shot system to play (Agent 3's RequestAiShot).
	struct FAiShotDecision
	{
		EAiShot Shot = EAiShot::Drive;
		// Landing target in metres (Z = 0).
		FAiVec Target;
		double Charge = 0.0;
		// Release time relative to the ideal contact moment; negative is early.
		double TimingOffsetS = 0.0;
		EAiTimingGrade Grade = EAiTimingGrade::Good;
	};

	// One tick of decisions for one AI player.
	struct FAiIntent
	{
		// Team-relative, like the stick: X = the player's right, Y = toward the net.
		// Length 0..1.
		FAiVec2 Move;
		bool bSprint = false;
		// How far this move is going (A3: FPlayerMotion picks the gait the planner timed).
		double PlannedDistanceM = -1.0;
		std::optional<FAiShotDecision> Shot;
		// Absolute match time of the ideal contact for Shot.
		double ShotContactTime = 0.0;
	};
}
