// PadelMath (test plan P-12): portable Sin, Cos, Atan, Atan2. Accuracy against the
// platform's libm, and golden bit patterns, so a platform that computes a different last
// bit fails here and names the function, before QA.Determinism's match checksum does.
#include "doctest.h"
#include "PadelMath.h"

#include <cmath>
#include <cstdint>
#include <cstring>

using namespace Padel::Math;

namespace
{
	std::uint64_t Bits(double Value)
	{
		std::uint64_t Out = 0;
		std::memcpy(&Out, &Value, sizeof(Out));
		return Out;
	}

	// Relative error, with an absolute floor near zero.
	double Error(double Got, double Want)
	{
		const double Scale = std::fabs(Want) > 1.0 ? std::fabs(Want) : 1.0;
		return std::fabs(Got - Want) / Scale;
	}
}

TEST_CASE("Math.Portable: Sin and Cos match libm to about 1e-15 over the simulation's range")
{
	double Worst = 0.0;
	for (int I = -200000; I <= 200000; ++I)
	{
		const double X = I * 0.0005; // -100 .. 100 rad
		Worst = std::fmax(Worst, Error(Sin(X), std::sin(X)));
		Worst = std::fmax(Worst, Error(Cos(X), std::cos(X)));
	}
	CHECK(Worst < 2e-15);
	CHECK(Sin(0.0) == 0.0);
	CHECK(Cos(0.0) == 1.0);
	CHECK(Sin(HalfPi) == doctest::Approx(1.0).epsilon(1e-15));
}

TEST_CASE("Math.Portable: Atan and Atan2 match libm to about 1e-15, every quadrant and edge")
{
	double Worst = 0.0;
	for (int I = -100000; I <= 100000; ++I)
	{
		const double X = I * 0.001; // -100 .. 100
		Worst = std::fmax(Worst, Error(Atan(X), std::atan(X)));
	}
	for (int I = -60; I <= 60; ++I)
	{
		for (int J = -60; J <= 60; ++J)
		{
			const double Y = I * 0.37;
			const double X = J * 0.29;
			Worst = std::fmax(Worst, Error(Atan2(Y, X), std::atan2(Y, X)));
		}
	}
	CHECK(Worst < 2e-15);
	CHECK(Atan2(0.0, 1.0) == 0.0);
	CHECK(Atan2(1.0, 0.0) == HalfPi);
	CHECK(Atan2(-1.0, 0.0) == -HalfPi);
	CHECK(Atan2(0.0, -1.0) == Pi);
	CHECK(Atan2(-0.0, -1.0) == -Pi);
	CHECK(Atan(1e300) == HalfPi);
}

TEST_CASE("Math.Portable: golden bits are the same on every platform")
{
	// Recorded on Linux x86-64. Each value must be identical on macOS arm64 and Windows;
	// if one differs, that function isn't portable on that compiler.
	CHECK(Bits(Sin(0.7)) == 0x3fe49d6e694619b8ULL);
	CHECK(Bits(Sin(-2.5)) == 0xbfe326af0dcfcab0ULL);
	CHECK(Bits(Sin(31.4)) == 0xbf904ede8c1cb30cULL);
	CHECK(Bits(Cos(0.7)) == 0x3fe87996529f9d92ULL);
	CHECK(Bits(Cos(3.1)) == 0xbfeff8ea4756a624ULL);
	CHECK(Bits(Atan(0.2)) == 0x3fc94441f8f7260cULL);
	CHECK(Bits(Atan(0.9)) == 0x3fe77338a80603bfULL);
	CHECK(Bits(Atan(7.5)) == 0x3ff7030cf9403197ULL);
	CHECK(Bits(Atan2(-0.3, -4.0)) == 0xc00888ab359690ddULL);
}
