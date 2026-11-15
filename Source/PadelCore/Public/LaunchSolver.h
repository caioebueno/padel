// Turns a shot's intent (target, speed, spin) into a launch velocity (GDD 5: the shot
// system gives a target point, a speed and a spin; the ball engine solves the launch).
// Shared by the shot system (Agent 3) and the AI (Agent 4), so both hit the same way.

#pragma once

#include "PadelCoreApi.h"

#include "BallSim.h"

namespace Padel
{
	struct FLaunchRequest
	{
		// Ball position at contact, metres.
		FVec3 Origin;
		// Where the ball should first touch the ground. Z is ignored (ground level).
		FVec3 Target;
		// Launch speed, m/s. Used by the flat solve; ignored when ApexHeight > 0.
		double Speed = 0.0;
		// Spin in the shot's own frame, rad/s: X = topspin (+) / slice (-), Y = side spin
		// (+ curves to the left of the direction of travel, seen from above).
		FVec3 SpinLocal;
		// > 0: solve for a flight whose highest point is this many metres above the ground
		// (lobs, pop-ups). 0: the flatter of the two arcs at Speed.
		double ApexHeight = 0.0;
	};

	struct FLaunchResult
	{
		// The landing is within the solver tolerance of the target.
		bool bOk = false;
		FVec3 Velocity;
		// World spin to give the ball with Velocity.
		FVec3 Spin;
		// First ground contact of the solved flight (ignoring walls and the net), and when.
		FVec3 Landing;
		double FlightTime = 0.0;
		double Apex = 0.0;
		// Passed over the net tape with the ball's radius to spare (or never crossed it).
		bool bClearsNet = true;
		// Distance from Landing to the target, metres.
		double Miss = 0.0;
	};

	// The world spin for a local spin on a flight along Forward (horizontal part used).
	PADELCORE_API FVec3 SpinLocalToWorld(const FVec3& SpinLocal, const FVec3& Forward);

	PADELCORE_API FLaunchResult SolveLaunch(const FBallSim& Sim, const FLaunchRequest& Request);
}
