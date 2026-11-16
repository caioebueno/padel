// GDD 8: pick the reachable interception point from a predicted trajectory.

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiBallPredictor.h"
#include "AI/AiPlayerProfile.h"

#include <optional>
#include <vector>

namespace Padel::AI
{
	struct FInterceptPlan
	{
		FTrajectorySample Sample;
		// Where the player should stand at contact, on the ground.
		FAiVec StandPoint;
		EContactKind Kind = EContactKind::Groundstroke;
		// Read delay plus running time.
		double TimeNeeded = 0.0;
		// Sample.T - TimeNeeded. Negative means the player can't get there in time.
		double Slack = 0.0;
		bool bReachable = false;
	};

	struct FInterceptQuery
	{
		FAiVec PlayerPosition;
		// Running away from the ball costs the time and distance to stop first.
		FAiVec PlayerVelocity;
		int HalfSign = 1;
		// GDD 3.2: the serve must bounce before the receiver plays it.
		bool bMustBounce = false;
		// Seconds before the player starts moving (what is left of the read delay).
		double StartDelayS = 0.0;
		// Movement.json AccelFalloff: the player's push weakens as speed rises (GDD 6.3).
		double AccelFalloff = 0.0;
	};

	namespace InterceptPlanner
	{
		// The players' straight-line curve from standing (Shots::MotionCurve): constant
		// acceleration up to top speed with no falloff, softer as speed rises with one.
		PADELCORE_API double TimeToCover(double Distance, double TopSpeed, double AccelTime, double AccelFalloff = 0.0);

		// The best reachable sample, or the one the player misses by the least when
		// none is reachable (they still chase it). Empty when no sample is playable.
		PADELCORE_API std::optional<FInterceptPlan> Plan(const std::vector<FTrajectorySample>& Samples, const FInterceptQuery& Query,
			const FAiPlayerProfile& Profile, const FAiTactics& Tactics);
	}
}
