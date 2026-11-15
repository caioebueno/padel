#include "AI/AiRandom.h"

namespace Padel::AI::Random
{
bool Chance(FPadelRng& Rng, double Probability)
{
	return Rng.NextDouble() < Probability;
}

int WeightedIndex(FPadelRng& Rng, const std::vector<double>& Weights)
{
	double Total = 0.0;
	for (const double W : Weights)
	{
		Total += W > 0.0 ? W : 0.0;
	}
	if (Total <= 0.0)
	{
		return 0;
	}
	double Roll = Rng.NextDouble() * Total;
	for (size_t Index = 0; Index < Weights.size(); ++Index)
	{
		const double W = Weights[Index] > 0.0 ? Weights[Index] : 0.0;
		if (Roll < W)
		{
			return static_cast<int>(Index);
		}
		Roll -= W;
	}
	// Rounding can leave a sliver past the end: the last positive weight wins.
	for (size_t Index = Weights.size(); Index-- > 0;)
	{
		if (Weights[Index] > 0.0)
		{
			return static_cast<int>(Index);
		}
	}
	return 0;
}
}
