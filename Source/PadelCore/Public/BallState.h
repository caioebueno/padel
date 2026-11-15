// The ball's full simulation state. Cheap to copy, so prediction and replays work on
// copies (GDD 8: the AI predicts with the same deterministic code).

#pragma once

#include "VecMath.h"

namespace Padel
{
	// Regulation ball (GDD 2), not tuning.
	inline constexpr double BallRadius = 0.0325;
	inline constexpr double BallMass = 0.057;

	// CLAUDE.md: the simulation steps at a fixed 120 Hz.
	inline constexpr double SimTickRate = 120.0;
	inline constexpr double SimTickSeconds = 1.0 / SimTickRate;

	struct FBallState
	{
		// Metres, on the axes in CourtSpec.h.
		FVec3 Position;
		// m/s.
		FVec3 Velocity;
		// Angular velocity, rad/s, world axes.
		FVec3 Spin;
		// On the ground and rolling: no gravity, no more bounce reports.
		bool bRolling = false;
		// Inside the walls. False once the ball has left over a wall or through an exit.
		bool bInCourt = true;
		// Within one radius of the net plane after clipping the tape, so the clip is
		// reported once.
		bool bTouchingNet = false;
	};
}
