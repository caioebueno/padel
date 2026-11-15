// The ball integrator (CLAUDE.md: our own, not Chaos). Gravity, quadratic drag, the
// Magnus effect and spin-aware bounces off the ground, glass, mesh, net and ceiling
// (GDD 2). Deterministic: no globals, a fixed step, randomness only from the RNG passed in.

#pragma once

#include "PadelCoreApi.h"

#include "BallContact.h"
#include "BallPhysicsParams.h"
#include "BallState.h"
#include "PadelRng.h"

#include <vector>

namespace Padel
{
	class PADELCORE_API FBallSim
	{
	public:
		explicit FBallSim(const FBallPhysicsParams& InParams) : Params(InParams) {}

		const FBallPhysicsParams& GetParams() const { return Params; }

		// Advances State by Dt seconds (split into sub-steps) and appends what happened to
		// Out. Only State and Rng change. A null Rng means no mesh randomness, which is
		// what prediction wants.
		void Step(FBallState& State, double Dt, FPadelRng* Rng, std::vector<FBallContact>& Out) const;

		// One 120 Hz tick.
		void Tick(FBallState& State, FPadelRng* Rng, std::vector<FBallContact>& Out) const
		{
			Step(State, SimTickSeconds, Rng, Out);
		}

		// Simulates a copy of State forward for Horizon seconds and samples it every
		// SampleDt (rounded to whole ticks). Contacts, with times, go to OutContacts if given.
		std::vector<FBallSample> Predict(const FBallState& State, double Horizon, double SampleDt,
			std::vector<FBallContact>* OutContacts = nullptr) const;

		// Air forces only (gravity, drag, Magnus): the acceleration of a ball in flight.
		FVec3 Acceleration(const FVec3& Velocity, const FVec3& Spin) const;

		// Integrates flight only, without any collision. Used by the launch solver.
		void FlightSubStep(FBallState& State, double H) const;

		double SubStepSeconds() const { return SimTickSeconds / Params.SubStepsPerTick; }

	private:
		void SubStep(FBallState& State, double H, double TimeOffset, FPadelRng* Rng, std::vector<FBallContact>& Out) const;
		void Bounce(FBallState& State, const FVec3& Normal, const FSurfaceParams& Surface, FPadelRng* Rng) const;
		void CollideNet(FBallState& State, const FVec3& Previous, double TimeOffset, std::vector<FBallContact>& Out) const;
		void CollideWalls(FBallState& State, double TimeOffset, FPadelRng* Rng, std::vector<FBallContact>& Out) const;

		FBallPhysicsParams Params;
	};
}
