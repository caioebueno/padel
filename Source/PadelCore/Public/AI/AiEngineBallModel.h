// The AI's ball model on the engine's own integrator (GDD 8: the AI predicts with the
// same deterministic code as the match). Steps with no RNG, so prediction never
// consumes the match's randomness; the mesh's scatter is what the AI can't foresee.
#pragma once

#include "PadelCoreApi.h"
#include "AI/AiBallModel.h"
#include "BallSim.h"

namespace Padel::AI
{
	class PADELCORE_API FAiEngineBallModel : public IAiBallModel
	{
	public:
		explicit FAiEngineBallModel(const FBallSim& InSim) : Sim(InSim) {}

		std::vector<FAiContact> Step(FAiBall& State, double Dt) const override;
		bool IsDead(const FAiBall& State) const override;

	private:
		const FBallSim& Sim;
	};
}
