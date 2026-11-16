// The straight-line acceleration curve of a player starting from standing (GDD 6.3,
// BIOMECHANICS_MOVEMENT 5.1), in closed form. FPlayerMotion steps the same curve tick by
// tick; the AI's intercept planner asks it how long a run takes. One definition, so the
// two can't drift apart.
//
// a = a0 * (1 - Falloff * v / TopSpeed), capped at TopSpeed, with a0 = TopSpeed / TimeToTop.
// Falloff 0 is constant acceleration that reaches TopSpeed after TimeToTop; Falloff 1 falls
// to nothing at TopSpeed, which is then only approached (x(t) = V (t - tau (1 - e^(-t/tau))),
// tau = TimeToTop).

#pragma once

#include "PadelCoreApi.h"

namespace Padel::Shots::MotionCurve
{
	// Initial acceleration (m/s^2).
	PADELCORE_API double InitialAccel(double TopSpeed, double TimeToTop);

	// Speed (m/s) and distance (m) T seconds after starting from standing.
	PADELCORE_API double SpeedAt(double T, double TopSpeed, double TimeToTop, double Falloff);
	PADELCORE_API double DistanceAt(double T, double TopSpeed, double TimeToTop, double Falloff);

	// Seconds to cover Distance from standing, running straight. Infinite when TopSpeed <= 0;
	// TimeToTop <= 0 is instant top speed. Deterministic (fixed Newton steps from above).
	PADELCORE_API double TimeToCover(double Distance, double TopSpeed, double TimeToTop, double Falloff);
}
