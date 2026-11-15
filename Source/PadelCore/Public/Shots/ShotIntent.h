// What a swing hands to the ball engine (GDD 5): the engine solves the launch
// velocity from Origin to Target and simulates the flight. Errors are already in
// the Target (scatter), so a target in the net or out is just an error.
// Metres, Unreal axes.

#pragma once

#include "PadelTypes.h"
#include "Shots/ShotMath.h"

namespace Padel::Shots
{
	struct FShotIntent
	{
		// Engine player id: 0 and 1 are Team A, 2 and 3 Team B.
		int PlayerId = 0;
		EShotType ShotType = EShotType::Drive;
		ETimingGrade TimingGrade = ETimingGrade::Good;
		// Release minus the ideal contact moment; negative is early.
		double TimingOffsetS = 0.0;
		// Ball position at contact.
		FVec3 Origin;
		// Ground point the ball should land on, after scatter.
		FVec3 Target;
		// Before scatter, for the debug overlay and stats.
		FVec3 AimedTarget;
		double SpeedMps = 0.0;
		// x = topspin (+) / slice (-), y = side spin, rad/s, in the shot's own frame.
		FVec3 Spin;
		// > 0: solve for this apex instead of SpeedMps (lobs, mishit pop-ups).
		double ApexHeightM = 0.0;
		double Charge = 0.0;
		double ScatterRadiusM = 0.0;
		// Played after the ball came off the hitter's own glass (GDD 4, 5.3).
		bool bWallShot = false;
		bool bVolley = false;

		double SpeedKmh() const { return SpeedMps * 3.6; }
	};
}
