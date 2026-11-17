// How long a player takes to run to a spot, stepped with the rules FPlayerMotion runs
// (GDD 6.3; movement doc 3.2, 5.2; realism plan A3): the acceleration curve that falls as
// speed rises, the square-gait direction caps (shuffle, backpedal), the hip turn into a
// crossover or turn-and-run on a long sideways or backward move, and the arrival the AI's
// MoveTo brakes into. The intercept planner asks it, so a plan is the run the player makes.

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiMath.h"

namespace Padel::Shots
{
	class FPlayerMotion;
	struct FMovementTuning;
}

namespace Padel::AI
{
	// One player's real movement numbers, copied from their FPlayerMotion each tick.
	struct FAiMotionParams
	{
		double TopSpeedMps = 0.0;
		double SprintSpeedBonus = 0.0;
		double TimeToTopS = 0.0;
		double AccelFalloff = 0.0;
		double BrakeMult = 0.0;
		double ShuffleSpeedFrac = 1.0;
		double BackpedalSpeedFrac = 1.0;
		double TurnTimeS = 0.0;
		double CrossoverMinDistM = 0.0;
		double TurnBackMinDistM = 0.0;

		// Tests and old callers that leave it empty get the profile-based estimate.
		bool IsValid() const { return TopSpeedMps > 0.0 && TimeToTopS > 0.0; }
	};

	struct FAiRunQuery
	{
		FAiVec From;
		FAiVec Velocity;
		FAiVec To;
		int HalfSign = 1;
		double ArriveRadiusM = 0.0;
		bool bSprint = false;
		// Seconds the player stands first (the read, the split-step landing).
		double StartDelayS = 0.0;
		// Stop stepping after this long and estimate the rest at top speed.
		double MaxTimeS = 4.0;
	};

	namespace AiMotionModel
	{
		PADELCORE_API FAiMotionParams ParamsOf(const Shots::FPlayerMotion& Motion, const Shots::FMovementTuning& Tuning);

		// The AI's arrival rule (MoveTo): no faster than the player can still stop in.
		PADELCORE_API double StopSpeedFrac(const FAiMotionParams& P, double DistanceM);

		// Seconds from now until the player is within ArriveRadiusM of To, chasing flat out.
		PADELCORE_API double ArrivalTime(const FAiMotionParams& P, const FAiRunQuery& Query);
	}
}
