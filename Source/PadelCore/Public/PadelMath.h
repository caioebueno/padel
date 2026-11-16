// Portable, bit-for-bit deterministic maths for the simulation (GDD 8: the same seed must
// play the same match on every platform; test plan P-12).
//
// The C++ standard only requires +, -, *, / and sqrt to be correctly rounded. std::sin,
// std::cos, std::atan and std::atan2 come from each platform's maths library, and glibc,
// Apple's libm and MSVC's CRT round them differently in the last bit. In a chaotic system
// like a rally, one bit turns into a different bounce a few seconds later. So PadelCore
// never calls them: it calls these, which use only correctly rounded operations in a
// fixed order. Tools/QA/check_contracts.py (G-09) enforces it.
//
// Accuracy is within a few ulp of the libm results over the ranges the simulation uses.
// What matters is that every compiler produces the same bits.
//
// Including this header (directly or through PadelCoreApi.h) also turns off floating-point
// contraction for the rest of the translation unit: Clang fuses a*b+c into one FMA by
// default on Apple Silicon, which rounds differently from x86.

#pragma once

#include "PadelCoreApi.h"

namespace Padel::Math
{
	inline constexpr double Pi = 3.14159265358979323846;
	inline constexpr double HalfPi = 1.57079632679489661923;

	PADELCORE_API double Sin(double X);
	PADELCORE_API double Cos(double X);
	PADELCORE_API double Atan(double X);
	PADELCORE_API double Atan2(double Y, double X);
}
