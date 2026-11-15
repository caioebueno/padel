// Seeded random numbers for the shot system. SplitMix64 with our own unit mapping,
// so the same seed gives the same shots on every compiler and platform (the
// standard library's distributions differ between libstdc++, libc++ and MSVC).

#pragma once

#include <cstdint>

namespace Padel::Shots
{
	class FShotRng
	{
	public:
		explicit FShotRng(std::uint64_t InSeed = 0) : State(InSeed) {}

		std::uint64_t NextU64()
		{
			std::uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}

		// [0, 1)
		double NextUnit() { return static_cast<double>(NextU64() >> 11) * (1.0 / 9007199254740992.0); }

		double Range(double Lo, double Hi) { return Lo + (Hi - Lo) * NextUnit(); }

	private:
		std::uint64_t State;
	};
}
