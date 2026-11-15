// The ball physics the AI predicts with. GDD 8: the AI simulates the ball forward
// with the same deterministic code as the match. In the game this wraps the engine's
// ball sim (Agent 2); the native soak test uses a stand-in until that lands.

#pragma once

#include "AI/AiMath.h"
#include "CourtSpec.h"

#include <vector>

namespace Padel::AI
{
	struct FAiBall
	{
		FAiVec Position;
		FAiVec Velocity;
		FAiVec Spin;
	};

	struct FAiContact
	{
		Court::ESurface Surface = Court::ESurface::None;
		FAiVec Position;
	};

	class IAiBallModel
	{
	public:
		virtual ~IAiBallModel() = default;

		// Advances State by Dt with no randomness (prediction never consumes the
		// match's RNG) and returns the contacts in that step.
		virtual std::vector<FAiContact> Step(FAiBall& State, double Dt) const = 0;

		// True when the ball can no longer be played: it left the court or stopped.
		virtual bool IsDead(const FAiBall& State) const = 0;
	};
}
