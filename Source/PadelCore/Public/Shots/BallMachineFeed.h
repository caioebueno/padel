// One ball machine feed: a row of Data/BallMachine.json. Positions are written for a
// player on Team A's half (+X) and mirrored for Team B.

#pragma once

// Determinism pragmas (no FMA contraction) for the inline maths below; see PadelCoreApi.h.
#include "PadelCoreApi.h"

#include <string>

namespace Padel::Shots
{
	struct FBallMachineFeed
	{
		std::string Name;
		// Practice drill this feed belongs to (feeds with the same drill are mixed).
		std::string Drill;
		double OriginX = 0.0, OriginY = 0.0, OriginZ = 0.0;
		double TargetX = 0.0, TargetY = 0.0;
		// Uniform jitter of the landing point, metres.
		double TargetJitterM = 0.0;
		double SpeedKmh = 0.0;
		// > 0: a lobbed feed with this apex.
		double ApexM = 0.0;
		// Seconds between the last ball finishing and this feed.
		double IntervalS = 0.0;
	};
}
