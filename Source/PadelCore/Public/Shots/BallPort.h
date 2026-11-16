// How the shot system reads and hits the ball without knowing which ball engine is
// behind it. The match engine (Agent 2) gets an adapter; until then the practice
// mode uses FPracticeBall.

#pragma once

#include "Shots/ShotIntent.h"

#include <vector>

namespace Padel::Shots
{
	// Mirrors the engine's predicted sample, so the adapter is a field copy.
	struct FBallView
	{
		// Seconds from now.
		double T = 0.0;
		FVec3 Position;
		FVec3 Velocity;
		// Angular velocity, rad/s, world axes (FBallState::Spin). Zero for a ball that
		// models no spin (the practice ball). Presentation uses it to turn the ball's mesh.
		FVec3 Spin;
		// Ground bounces on the half the ball is on, since the last hit.
		int GroundBounces = 0;
		// Touched a wall after bouncing on this half (GDD 4: off the glass).
		bool bOffGlass = false;
		bool bLive = true;
	};

	class IBallPort
	{
	public:
		virtual ~IBallPort() = default;

		virtual FBallView Current() const = 0;
		// Team sign (+1 A, -1 B) of the last hitter; 0 for a serve toss, a ball
		// machine feed counts as the other team.
		virtual int LastHitterTeam() const = 0;
		// Deterministic forward prediction with the same physics as the flight.
		virtual std::vector<FBallView> Predict(double HorizonS, double StepS) const = 0;
		virtual void ApplyShot(const FShotIntent& Intent) = 0;
		// Serve: drop the ball here so it bounces once (GDD 3.2).
		virtual void ServeToss(const FVec3& At, int TeamSign) = 0;
	};
}
