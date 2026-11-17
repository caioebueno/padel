// GDD 8 prediction: simulate a copy of the ball forward with the match's own ball
// code and keep the positions where it could be played, glass rebounds included.

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiBallModel.h"
#include "AI/AiRandom.h"
#include "AI/AiTuning.h"
#include "PadelTypes.h"

#include <vector>

namespace Padel::AI
{
	struct FTrajectorySample
	{
		// Seconds after the prediction started.
		double T = 0.0;
		FAiVec Position;
		FAiVec Velocity;
		// Sign of the half the ball is over (+1 or -1).
		int Half = 1;
		// Ground bounces on this half since the ball last crossed the net.
		int GroundBounces = 0;
		// The ball hit a wall on this half after bouncing there (GDD 4: off the glass).
		bool bOffGlass = false;
		// The last wall it hit on this half after bouncing there; None unless bOffGlass.
		EGlassWall GlassWall = EGlassWall::None;
		// A contact happened in the step that produced this sample.
		bool bContact = false;
	};

	class PADELCORE_API FAiBallPredictor
	{
	public:
		FAiBallPredictor(const IAiBallModel& InModel, const FAiTactics& InTactics);

		// Stops at the ball's second bounce on a half, when the model says it is dead,
		// or at the horizon. NoiseM: after the first wall contact every later position
		// is offset by one random ground-plane vector of up to this length, drawn from
		// Rng (GDD 8: Wall Play adds noise to the predicted rebound).
		std::vector<FTrajectorySample> Predict(const FAiBall& Start, double NoiseM, FPadelRng& Rng) const;

	private:
		const IAiBallModel& Model;
		const FAiTactics& Tactics;
	};
}
