// GDD 6.3, BIOMECHANICS_MOVEMENT 5.1 (M1): the shared closed-form acceleration curve
// that FPlayerMotion steps and the AI's intercept planner reads.
#include "doctest.h"
#include "AI/AiInterceptPlanner.h"
#include "Shots/MotionCurve.h"

#include <cmath>

using namespace Padel;
using doctest::Approx;

TEST_CASE("Shots.MotionCurve: no falloff is constant acceleration, then cruise")
{
	using Shots::MotionCurve::TimeToCover;
	CHECK(TimeToCover(0.0, 6.0, 0.6, 0.0) == Approx(0.0));
	// 0.5 * 6 * 0.6 = 1.8 m while accelerating, then 6 m/s.
	CHECK(TimeToCover(1.8, 6.0, 0.6, 0.0) == Approx(0.6));
	CHECK(TimeToCover(7.8, 6.0, 0.6, 0.0) == Approx(1.6));
	CHECK(TimeToCover(0.45, 6.0, 0.6, 0.0) == Approx(std::sqrt(2.0 * 0.45 / 10.0)));
	CHECK(TimeToCover(3.0, 6.0, 0.0, 0.5) == Approx(0.5));
	CHECK(std::isinf(TimeToCover(3.0, 0.0, 0.6, 0.5)));
}

TEST_CASE("Shots.MotionCurve: the AI planner's time to cover is the players' curve, bit for bit")
{
	// With no falloff the AI plans exactly as before M1; with one it runs the same curve.
	CHECK(AI::InterceptPlanner::TimeToCover(5.0, 6.05, 0.76, 0.3) == Shots::MotionCurve::TimeToCover(5.0, 6.05, 0.76, 0.3));
	CHECK(AI::InterceptPlanner::TimeToCover(5.0, 6.05, 0.76, 0.3) > AI::InterceptPlanner::TimeToCover(5.0, 6.05, 0.76));
	for (double D = 0.0; D <= 20.0; D += 0.173)
	{
		for (const double Top : { 4.5, 6.05, 6.5 * 1.15 })
		{
			for (const double AccelTime : { 0.0, 0.45, 0.62, 0.9 })
			{
				const double Planner = AI::InterceptPlanner::TimeToCover(D, Top, AccelTime);
				const double Curve = Shots::MotionCurve::TimeToCover(D, Top, AccelTime, 0.0);
				CHECK(Planner == Curve);
			}
		}
	}
}

TEST_CASE("Shots.MotionCurve: full falloff follows x(t) = V (t - tau (1 - e^(-t/tau)))")
{
	const double V = 6.05;
	const double Tau = 0.75;
	for (const double T : { 0.001, 0.1, 0.5, 1.0, 3.0 })
	{
		CAPTURE(T);
		CHECK(Shots::MotionCurve::DistanceAt(T, V, Tau, 1.0) == Approx(V * (T - Tau * (1.0 - std::exp(-T / Tau)))));
		CHECK(Shots::MotionCurve::SpeedAt(T, V, Tau, 1.0) == Approx(V * (1.0 - std::exp(-T / Tau))));
	}
	// The curve's own exp (G-09: no libm in PadelCore) agrees with libm to a few ulp.
	for (double T = 0.0005; T < 12.0; T *= 1.37)
	{
		CAPTURE(T);
		CHECK(Shots::MotionCurve::SpeedAt(T, V, Tau, 1.0) == Approx(V * -std::expm1(-T / Tau)).epsilon(1e-13));
	}
	// Top speed is only approached.
	CHECK(Shots::MotionCurve::SpeedAt(10.0, V, Tau, 1.0) < V);
}

TEST_CASE("Shots.MotionCurve: partial falloff reaches top speed and then cruises")
{
	const double V = 6.05;
	const double TimeToTop = 0.76;
	const double F = 0.5;
	// Heads for V / F on the time constant TimeToTop / F, capped at V: reached at
	// -(TimeToTop / F) ln(1 - F).
	const double Cap = -(TimeToTop / F) * std::log(1.0 - F);
	// The cap time uses the curve's own log; just either side of libm's value it switches.
	CHECK(Shots::MotionCurve::SpeedAt(Cap * (1.0 - 1e-12), V, TimeToTop, F) == Approx(V).epsilon(1e-11));
	CHECK(Shots::MotionCurve::SpeedAt(Cap - 0.01, V, TimeToTop, F) < V);
	CHECK(Shots::MotionCurve::SpeedAt(Cap + 0.01, V, TimeToTop, F) == Approx(V));
	const double AtCap = Shots::MotionCurve::DistanceAt(Cap, V, TimeToTop, F);
	CHECK(Shots::MotionCurve::DistanceAt(Cap + 1.0, V, TimeToTop, F) == Approx(AtCap + V));
	// The initial push is V / TimeToTop whatever the falloff.
	const double H = 1e-4;
	CHECK(Shots::MotionCurve::SpeedAt(H, V, TimeToTop, F) / H == Approx(V / TimeToTop).epsilon(0.001));
}

TEST_CASE("Shots.MotionCurve: time to cover inverts the distance")
{
	for (const double F : { 0.0, 0.25, 0.5, 0.8, 1.0 })
	{
		CAPTURE(F);
		double Last = 0.0;
		for (const double D : { 1e-4, 0.01, 0.3, 1.0, 2.0, 4.0, 8.0, 20.0 })
		{
			CAPTURE(D);
			const double T = Shots::MotionCurve::TimeToCover(D, 6.05, 0.76, F);
			CHECK(T > Last);
			CHECK(Shots::MotionCurve::DistanceAt(T, 6.05, 0.76, F) == Approx(D).epsilon(1e-9));
			Last = T;
		}
	}
	// More falloff, slower over any distance.
	CHECK(Shots::MotionCurve::TimeToCover(5.0, 6.05, 0.76, 0.5) > Shots::MotionCurve::TimeToCover(5.0, 6.05, 0.76, 0.0));
	CHECK(Shots::MotionCurve::TimeToCover(5.0, 6.05, 0.76, 1.0) > Shots::MotionCurve::TimeToCover(5.0, 6.05, 0.76, 0.5));
}
