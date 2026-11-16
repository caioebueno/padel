#include "PadelMath.h"

#include <cmath>

// Only +, -, *, /, sqrt, floor and comparisons below: the operations IEEE 754 requires to
// be correctly rounded or exact, so every compiler gets the same bits. Polynomials are
// Taylor series evaluated with Horner's rule in a fixed order, with enough terms that the
// truncation error is far below one ulp on the reduced range.

namespace Padel::Math
{
	namespace
	{
		// pi/2 split in three (Cody-Waite), so k * pi/2 is subtracted almost exactly. The
		// first two parts have 33 trailing zero bits, so k * part is exact for |k| < 2^20.
		constexpr double PiOver2Hi = 1.57079632673412561417e+00;
		constexpr double PiOver2Mid = 6.07710050630396597660e-11;
		constexpr double PiOver2Lo = 2.02226624879595063154e-21;
		constexpr double TwoOverPi = 6.36619772367581382433e-01;

		constexpr double PiOver6 = 5.23598775598298873077e-01;
		constexpr double Sqrt3 = 1.73205080756887729353e+00;
		// tan(pi / 12): above this, atan's argument is shifted down by pi / 6.
		constexpr double TanPiOver12 = 2.67949192431122706473e-01;

		// sin(r) for |r| <= pi/4: r - r^3/3! + ... + r^19/19! (next term < 1e-22).
		double SinKernel(double R)
		{
			const double R2 = R * R;
			double P = -1.0 / 121645100408832000.0; // -1/19!
			P = P * R2 + 1.0 / 355687428096000.0;   // 1/17!
			P = P * R2 - 1.0 / 1307674368000.0;     // 1/15!
			P = P * R2 + 1.0 / 6227020800.0;        // 1/13!
			P = P * R2 - 1.0 / 39916800.0;          // 1/11!
			P = P * R2 + 1.0 / 362880.0;            // 1/9!
			P = P * R2 - 1.0 / 5040.0;              // 1/7!
			P = P * R2 + 1.0 / 120.0;               // 1/5!
			P = P * R2 - 1.0 / 6.0;                 // 1/3!
			return R + R * (R2 * P);
		}

		// cos(r) for |r| <= pi/4: 1 - r^2/2! + ... + r^20/20!.
		double CosKernel(double R)
		{
			const double R2 = R * R;
			double P = 1.0 / 2432902008176640000.0; // 1/20!
			P = P * R2 - 1.0 / 6402373705728000.0;  // 1/18!
			P = P * R2 + 1.0 / 20922789888000.0;    // 1/16!
			P = P * R2 - 1.0 / 87178291200.0;       // 1/14!
			P = P * R2 + 1.0 / 479001600.0;         // 1/12!
			P = P * R2 - 1.0 / 3628800.0;           // 1/10!
			P = P * R2 + 1.0 / 40320.0;             // 1/8!
			P = P * R2 - 1.0 / 720.0;               // 1/6!
			P = P * R2 + 1.0 / 24.0;                // 1/4!
			return (1.0 - 0.5 * R2) + R2 * (R2 * P);
		}

		// Reduces X to R in [-pi/4, pi/4] and the quadrant (X = R + Quadrant * pi/2).
		double Reduce(double X, int& Quadrant)
		{
			const double K = std::floor(X * TwoOverPi + 0.5);
			Quadrant = static_cast<int>(static_cast<long long>(K) & 3);
			return ((X - K * PiOver2Hi) - K * PiOver2Mid) - K * PiOver2Lo;
		}

		// atan(z) for |z| <= tan(pi/12) ~ 0.268: z - z^3/3 + ... - z^31/31 (next term < 4e-20).
		double AtanKernel(double Z)
		{
			const double Z2 = Z * Z;
			double P = -1.0 / 31.0;
			for (int N = 29; N >= 3; N -= 2)
			{
				// Alternating signs: +1/29, -1/27, ..., -1/3.
				const double Coefficient = ((N - 1) / 2) % 2 == 0 ? 1.0 / N : -1.0 / N;
				P = P * Z2 + Coefficient;
			}
			return Z + Z * (Z2 * P);
		}
	}

	double Sin(double X)
	{
		if (!std::isfinite(X))
		{
			return X - X; // NaN
		}
		int Quadrant = 0;
		const double R = Reduce(X, Quadrant);
		switch (Quadrant)
		{
		case 0: return SinKernel(R);
		case 1: return CosKernel(R);
		case 2: return -SinKernel(R);
		default: return -CosKernel(R);
		}
	}

	double Cos(double X)
	{
		if (!std::isfinite(X))
		{
			return X - X;
		}
		int Quadrant = 0;
		const double R = Reduce(X, Quadrant);
		switch (Quadrant)
		{
		case 0: return CosKernel(R);
		case 1: return -SinKernel(R);
		case 2: return -CosKernel(R);
		default: return SinKernel(R);
		}
	}

	double Atan(double X)
	{
		if (X != X)
		{
			return X;
		}
		const bool bNegative = X < 0.0;
		double A = bNegative ? -X : X;
		double Result = 0.0;
		bool bInverted = false;
		if (A > 1.0)
		{
			// atan(a) = pi/2 - atan(1/a)
			A = 1.0 / A;
			bInverted = true;
		}
		if (A > TanPiOver12)
		{
			// atan(a) = pi/6 + atan((a*sqrt3 - 1) / (a + sqrt3))
			Result = PiOver6 + AtanKernel((A * Sqrt3 - 1.0) / (A + Sqrt3));
		}
		else
		{
			Result = AtanKernel(A);
		}
		if (bInverted)
		{
			Result = HalfPi - Result;
		}
		return bNegative ? -Result : Result;
	}

	double Atan2(double Y, double X)
	{
		if (X != X || Y != Y)
		{
			return X + Y;
		}
		if (X == 0.0)
		{
			if (Y == 0.0)
			{
				return std::signbit(X) ? (std::signbit(Y) ? -Pi : Pi) : Y;
			}
			return Y > 0.0 ? HalfPi : -HalfPi;
		}
		const double Base = Atan(Y / X);
		if (X > 0.0)
		{
			return Base;
		}
		// X < 0: shift by pi toward Y's side.
		return std::signbit(Y) ? Base - Pi : Base + Pi;
	}
}
