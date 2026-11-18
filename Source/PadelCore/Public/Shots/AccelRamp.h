// The human limit on how fast a push or a brake builds (BIOMECHANICS_MOVEMENT 2.6, 5.7).
// FPlayerMotion steps the player with it and the AI's motion model steps its plans with
// it, so the run the AI times is the run the player makes.
//
// The applied acceleration A follows a target T: T points at the wanted velocity, at most
// MaxAccel, and no more than sqrt(2 JerkMax e) for a change e still to make, so the last
// of a stop or a start eases off instead of ending in one tick. A itself moves toward T by
// at most JerkMax * Dt a tick, so a push or a brake takes RiseS to build. RiseS 0 is the
// old instant change.

#pragma once

#include "PadelCoreApi.h"

namespace Padel::Shots::AccelRamp
{
	// Below this the wanted velocity counts as reached (m/s).
	inline constexpr double SettledMps = 0.01;

	struct FState
	{
		double AX = 0.0;
		double AY = 0.0;
	};

	// One tick of the ground velocity (VX, VY) toward (DX, DY).
	PADELCORE_API void Step(double& VX, double& VY, FState& A, double DX, double DY, double MaxAccel, double RiseS, double JerkMax, double Dt);

	// How far a player moving at V (>= 0) along one axis, applying A along it (+ speeding
	// up), travels before stopping when braking flat out now: Step itself, stepped.
	PADELCORE_API double StopDistance(double V, double A, double MaxAccel, double RiseS, double JerkMax, double Dt);

	// The fastest speed that still stops inside DistanceM at Decel with the ramp, when
	// following a braking plan: the velocity trails the plan by about RiseS (Speed * RiseS
	// further than v^2 / 2a; a straight stop loses only half that, so this is the safe
	// side), and easing the brake off at the end covers Decel * RiseS^2 / 24 more.
	PADELCORE_API double StopSpeed(double DistanceM, double Decel, double RiseS);

	// A reversal (BIOMECHANICS_MOVEMENT 2.6, 5.7): a runner can't go back through zero in
	// one stride. Asked to move at least the pivot angle away from the way they are running,
	// faster than StartMps, they brake to PlantMps, stay planted PlantS, then push off.
	struct FPivotParams
	{
		// Cosine of Movement.json PivotMinDeg; above 1 turns pivots off.
		double CosMin = 2.0;
		double StartMps = 0.0;
		double PlantMps = 0.0;
		double PlantS = 0.0;
	};

	struct FPivot
	{
		enum class EStage : unsigned char { None, Braking, Planted };
		EStage Stage = EStage::None;
		double HX = 0.0;
		double HY = 0.0;
		double LeftS = 0.0;

		// One tick, before the velocity step. True while the player should brake to a stop
		// (wanted velocity zero) instead of pushing toward (DX, DY).
		PADELCORE_API bool Update(double VX, double VY, double DX, double DY, const FPivotParams& P, double Dt);
	};
}
