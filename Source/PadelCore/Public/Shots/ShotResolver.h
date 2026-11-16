// GDD 5: turns a contact into an FShotIntent. Quality changes the intended values and
// adds scatter; it never rolls "is this an error". Randomness only comes from the
// FPadelRng passed in, so a seeded match replays exactly (CLAUDE.md).

#pragma once

#include "Shots/ShotMath.h"

#include "Shots/ShotContext.h"
#include "Shots/ShotIntent.h"
#include "Shots/ShotLibrary.h"
#include "PadelRng.h"
#include "Shots/ShotSelector.h"

namespace Padel::Shots
{
	struct PADELCORE_API FShotResolver
	{
		static FShotIntent Resolve(const FShotContext& Context, const FShotLibrary& Library, FPadelRng& Rng);

		// The rating that drives error and spin: forehand or backhand rating, blended
		// with Return on the return of serve, with the wrong-side penalty.
		static double MainRating(const FShotContext& Context, const FShotParams& Shot, const FShotModel& Model);
		// GDD 5.3: wall shots use Wall Play for the timing windows.
		static double TimingRating(const FShotContext& Context, const FShotParams& Shot, const FShotModel& Model);

		static double ScatterRadius(const FShotContext& Context, const FShotParams& Shot, const FShotChoice& Choice,
			double MainRatingUnit, ETimingGrade Grade, const FShotModel& Model);
		static double PowerMult(double Charge, const FShotModel& Model);
		static double PressureMult(const FShotContext& Context, const FShotModel& Model);
		static double RunningMult(const FShotContext& Context, const FShotModel& Model);
		static double PositionPowerLimit(double DistanceM, const FShotModel& Model);
		static double StaminaFactor(double Stamina, const FShotModel& Model);

		// Where the flatter gravity-only arc from Origin to Target at SpeedMps crosses the
		// net plane (X = 0), metres; -1 when the speed can't reach the target.
		static double NetCrossingHeight(const FVec3& Origin, const FVec3& Target, double SpeedMps, double Gravity);
		// The fastest speed up to SpeedMps whose arc passes ClearanceM over the tape.
		static double NetClearingSpeed(const FVec3& Origin, const FVec3& Target, double SpeedMps, double ClearanceM, double Gravity);

		// GDD 5.1 Miss: a weak, random ball, often into the net or a short pop-up.
		static FShotIntent Mishit(const FShotContext& Context, const FShotModel& Model, FPadelRng& Rng);
	};
}
