// Helpers on the match's shared RNG (Padel::FPadelRng, PadelRng.h) for the AI's
// weighted choices. GDD 8: AI decisions use a seeded RNG owned by the match, so a
// replay or soak run with the same seed makes the same decisions.

#pragma once

#include "PadelCoreApi.h"
#include "PadelRng.h"

#include <vector>

namespace Padel::AI::Random
{
	PADELCORE_API bool Chance(FPadelRng& Rng, double Probability);

	// Index picked in proportion to the weights. Negative weights count as 0; all-zero
	// weights pick index 0.
	PADELCORE_API int WeightedIndex(FPadelRng& Rng, const std::vector<double>& Weights);
}
