// The match's seeded random number generator (CLAUDE.md: no FMath::Rand()).
// SplitMix64: tiny, fast, and gives identical sequences on every compiler and OS,
// which std:: distributions do not guarantee. Replays and soak runs depend on that.

#pragma once

// Determinism pragmas (no FMA contraction) for the inline maths below; see PadelCoreApi.h.
#include "PadelCoreApi.h"

#include <cstdint>

namespace Padel
{
	class FPadelRng
	{
	public:
		explicit FPadelRng(uint64_t Seed = 0) : State(Seed) {}

		uint64_t NextU64()
		{
			uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}

		// Uniform in [0, 1).
		double NextDouble()
		{
			return static_cast<double>(NextU64() >> 11) * (1.0 / 9007199254740992.0);
		}

		// Uniform in [Low, High).
		double Range(double Low, double High)
		{
			return Low + (High - Low) * NextDouble();
		}

		uint64_t GetState() const { return State; }
		void SetState(uint64_t NewState) { State = NewState; }

	private:
		uint64_t State;
	};
}
