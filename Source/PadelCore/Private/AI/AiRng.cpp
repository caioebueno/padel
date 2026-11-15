#include "AI/AiRng.h"

namespace Padel::AI
{
FAiRng::FAiRng(uint64_t Seed, uint64_t Stream)
	: Increment((Stream << 1u) | 1u)
{
	NextU32();
	State += Seed;
	NextU32();
}

uint32_t FAiRng::NextU32()
{
	const uint64_t Old = State;
	State = Old * 6364136223846793005ULL + Increment;
	const uint32_t XorShifted = static_cast<uint32_t>(((Old >> 18u) ^ Old) >> 27u);
	const uint32_t Rot = static_cast<uint32_t>(Old >> 59u);
	return (XorShifted >> Rot) | (XorShifted << ((32u - Rot) & 31u));
}

double FAiRng::NextDouble()
{
	return NextU32() * (1.0 / 4294967296.0);
}

double FAiRng::Range(double Lo, double Hi)
{
	return Lo + (Hi - Lo) * NextDouble();
}

bool FAiRng::Chance(double Probability)
{
	return NextDouble() < Probability;
}

int FAiRng::WeightedIndex(const std::vector<double>& Weights)
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
	double Roll = NextDouble() * Total;
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
