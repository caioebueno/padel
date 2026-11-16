#include "Shots/MotionCurve.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Padel::Shots::MotionCurve
{
namespace
{
	// G-09 / test plan P-12: libm's exp and log round differently per platform, so the
	// curve uses these, built from correctly rounded operations in a fixed order (and the
	// exact frexp / ldexp). Accurate to a few ulp, the same bits on every compiler.
	constexpr double Ln2Hi = 6.93147180369123816490e-01;
	constexpr double Ln2Lo = 1.90821492927058770002e-10;

	// e^R - 1 for |R| <= about 0.35, Taylor to R^16.
	double Expm1Small(double R)
	{
		double Term = R;
		double Sum = R;
		for (int N = 2; N <= 16; ++N)
		{
			Term = Term * R / N;
			Sum += Term;
		}
		return Sum;
	}

	double PortableExpm1(double X)
	{
		if (X > -0.35 && X < 0.35)
		{
			return Expm1Small(X);
		}
		if (X < -40.0)
		{
			return -1.0;
		}
		const double K = std::floor(X / (Ln2Hi + Ln2Lo) + 0.5);
		const double R = (X - K * Ln2Hi) - K * Ln2Lo;
		return std::ldexp(1.0 + Expm1Small(R), static_cast<int>(K)) - 1.0;
	}

	// ln(Y) for Y > 0: Y = M 2^E with M in [sqrt(1/2), sqrt(2)), ln M = 2 atanh((M-1)/(M+1)).
	double PortableLog(double Y)
	{
		int E = 0;
		double M = std::frexp(Y, &E);
		if (M < 0.70710678118654752440)
		{
			M *= 2.0;
			--E;
		}
		const double S = (M - 1.0) / (M + 1.0);
		const double S2 = S * S;
		double Power = S;
		double Sum = S;
		for (int N = 3; N <= 31; N += 2)
		{
			Power *= S2;
			Sum += Power / N;
		}
		return 2.0 * Sum + (E * Ln2Lo + E * Ln2Hi);
	}

	double ClampFalloff(double Falloff)
	{
		return std::clamp(Falloff, 0.0, 1.0);
	}

	// With Falloff f > 0 the speed heads for TopSpeed / f along v = (V / f)(1 - e^(-t / Tf)),
	// Tf = TimeToTop / f, and is capped at TopSpeed, which it reaches at CapTime (never for f = 1).
	double CapTime(double TimeToTop, double F)
	{
		return F >= 1.0 ? std::numeric_limits<double>::infinity() : -(TimeToTop / F) * PortableLog(1.0 - F);
	}

	double UncappedDistance(double T, double TopSpeed, double TimeToTop, double F)
	{
		const double Tf = TimeToTop / F;
		const double U = T / Tf;
		// t - Tf (1 - e^(-u)) loses everything to cancellation for small u; use the series.
		const double Shape = U < 1e-4 ? Tf * (U * U / 2.0 - U * U * U / 6.0) : T + Tf * PortableExpm1(-U);
		return TopSpeed / F * Shape;
	}

	double UncappedSpeed(double T, double TopSpeed, double TimeToTop, double F)
	{
		return -TopSpeed / F * PortableExpm1(-T * F / TimeToTop);
	}
}

double InitialAccel(double TopSpeed, double TimeToTop)
{
	return TimeToTop > 0.0 ? TopSpeed / TimeToTop : std::numeric_limits<double>::infinity();
}

double SpeedAt(double T, double TopSpeed, double TimeToTop, double Falloff)
{
	if (T <= 0.0 || TopSpeed <= 0.0)
	{
		return 0.0;
	}
	if (TimeToTop <= 0.0)
	{
		return TopSpeed;
	}
	const double F = ClampFalloff(Falloff);
	if (F <= 0.0)
	{
		return std::min(TopSpeed, TopSpeed / TimeToTop * T);
	}
	return T >= CapTime(TimeToTop, F) ? TopSpeed : std::min(TopSpeed, UncappedSpeed(T, TopSpeed, TimeToTop, F));
}

double DistanceAt(double T, double TopSpeed, double TimeToTop, double Falloff)
{
	if (T <= 0.0 || TopSpeed <= 0.0)
	{
		return 0.0;
	}
	if (TimeToTop <= 0.0)
	{
		return TopSpeed * T;
	}
	const double F = ClampFalloff(Falloff);
	if (F <= 0.0)
	{
		return T <= TimeToTop ? 0.5 * TopSpeed / TimeToTop * T * T : 0.5 * TopSpeed * TimeToTop + TopSpeed * (T - TimeToTop);
	}
	const double Cap = CapTime(TimeToTop, F);
	if (T <= Cap)
	{
		return UncappedDistance(T, TopSpeed, TimeToTop, F);
	}
	return UncappedDistance(Cap, TopSpeed, TimeToTop, F) + TopSpeed * (T - Cap);
}

double TimeToCover(double Distance, double TopSpeed, double TimeToTop, double Falloff)
{
	if (Distance <= 0.0)
	{
		return 0.0;
	}
	if (TopSpeed <= 0.0)
	{
		return std::numeric_limits<double>::infinity();
	}
	if (TimeToTop <= 0.0)
	{
		return Distance / TopSpeed;
	}
	const double F = ClampFalloff(Falloff);
	if (F <= 0.0)
	{
		// Constant acceleration, then cruise. Kept term for term as the AI planner had it,
		// so Falloff 0 gives its numbers bit for bit.
		const double Accel = TopSpeed / TimeToTop;
		const double AccelDistance = 0.5 * TopSpeed * TimeToTop;
		if (Distance <= AccelDistance)
		{
			return std::sqrt(2.0 * Distance / Accel);
		}
		return TimeToTop + (Distance - AccelDistance) / TopSpeed;
	}

	const double Cap = CapTime(TimeToTop, F);
	if (std::isfinite(Cap))
	{
		const double CapDistance = UncappedDistance(Cap, TopSpeed, TimeToTop, F);
		if (Distance >= CapDistance)
		{
			return Cap + (Distance - CapDistance) / TopSpeed;
		}
	}
	// x(t) >= (V / f)(t - Tf), so this start is past the root; x is convex and rising, so
	// Newton from there walks down to the root without overshooting. A fixed iteration cap
	// and a fixed tolerance keep it deterministic.
	double T = F * Distance / TopSpeed + TimeToTop / F;
	for (int Iteration = 0; Iteration < 60; ++Iteration)
	{
		const double Speed = UncappedSpeed(T, TopSpeed, TimeToTop, F);
		if (Speed <= 0.0)
		{
			break;
		}
		const double StepS = (UncappedDistance(T, TopSpeed, TimeToTop, F) - Distance) / Speed;
		T -= StepS;
		if (std::abs(StepS) < 1e-12)
		{
			break;
		}
	}
	return T;
}
}
