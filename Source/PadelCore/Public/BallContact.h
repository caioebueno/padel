// What happened to the ball during a step. The referee is driven only by these, so
// the rules can be tested with hand-written contacts as well as with real flights.

#pragma once

#include "VecMath.h"

namespace Padel
{
	enum class EContactKind : int
	{
		// Touched a surface: ground, glass, mesh, net or ceiling.
		Surface,
		// The ball's centre crossed the net plane inside the court. Half is the new half.
		CrossedNet,
		// Left the court: over a wall, through an exit, or wide.
		LeftCourt,
	};

	struct FBallContact
	{
		EContactKind Kind = EContactKind::Surface;
		Court::ESurface Surface = Court::ESurface::None;
		FVec3 Position;
		// Surface normal, pointing back toward the ball.
		FVec3 Normal;
		Court::EHalf Half = Court::EHalf::A;
		// False for the run-off floor and anything outside the walls.
		bool bInCourt = true;
		// The extra ground report sent when a bounce dies into a roll, so the referee
		// always sees a second bounce (a rolling ball has bounced "many" times).
		bool bRolling = false;
		// Seconds from the start of the Step or Predict call.
		double Time = 0.0;
	};

	// A point on a predicted path (GDD 8, AI prediction).
	struct FBallSample
	{
		double T = 0.0;
		FVec3 Position;
		FVec3 Velocity;
		// Ground bounces since the prediction started.
		int GroundBounces = 0;
		// True once the ball has touched any glass since the prediction started.
		bool bOffGlass = false;
	};
}
