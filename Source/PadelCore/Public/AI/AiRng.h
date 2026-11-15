// The AI's seeded random numbers (GDD 8, determinism requirement). The match owns one
// per AI team, seeded from the match seed, so a replay or soak run with the same seed
// makes the same decisions. PCG32: small, fast and identical on every compiler.

#pragma once

#include <cstdint>
#include <vector>

namespace Padel::AI
{
	class FAiRng
	{
	public:
		explicit FAiRng(uint64_t Seed = 0, uint64_t Stream = 0x5eed);

		uint32_t NextU32();
		// Uniform in [0, 1).
		double NextDouble();
		// Uniform in [Lo, Hi).
		double Range(double Lo, double Hi);
		bool Chance(double Probability);
		// Index picked in proportion to the weights. Negative weights count as 0;
		// all-zero weights pick index 0.
		int WeightedIndex(const std::vector<double>& Weights);

	private:
		uint64_t State = 0;
		uint64_t Increment = 0;
	};
}
