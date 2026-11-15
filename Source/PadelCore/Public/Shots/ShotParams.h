// One shot's tuning (GDD 4 and 5): a row of Data/Shots.json. No values here: they
// all come from the data file, and Validate() catches a row that left one out.

#pragma once

#include "Shots/ShotMath.h"

#include "PadelTypes.h"
#include "Shots/PlayerRatings.h"

#include <string>
#include <vector>

namespace Padel::Shots
{
	inline constexpr int ShotTypeCount = static_cast<int>(EShotType::Count);

	struct PADELCORE_API FShotParams
	{
		EShotType Type = EShotType::Drive;

		// Speeds in km/h because the GDD and designers think in km/h.
		double SpeedMinKmh = 0.0;
		double SpeedMaxKmh = 0.0;
		// rad/s. Top: topspin (+) / slice (-). Side: side spin, toward the forehand (+).
		double BaseSpinTop = 0.0;
		double BaseSpinSide = 0.0;
		// > 0: the ball engine solves for an apex between these (lobs), by charge.
		double ApexMinM = 0.0;
		double ApexMaxM = 0.0;

		// Scatter radius at the target before multipliers (GDD 5.3).
		double BaseErrorM = 0.0;

		// Half-widths of the GDD 5.1 windows.
		double PerfectWindowMs = 0.0;
		double GoodWindowMs = 0.0;
		double EarlyLateWindowMs = 0.0;

		// Rating that drives the shot on each side (GDD 4 "Key attributes").
		ERating ForehandRating = ERating::Drive;
		ERating BackhandRating = ERating::Backhand;

		// Ball height band where the shot can be played, and the ideal height (GDD 5.2).
		double ContactHeightMinM = 0.0;
		double ContactHeightMaxM = 0.0;
		double PreferredContactHeightM = 0.0;

		// Aim, in metres from the net on the opponents' half (GDD 7).
		double DepthShortM = 0.0;
		double DepthDeepM = 0.0;
		// No stick input: a safe target.
		double DepthNeutralM = 0.0;
		// Furthest across from the centre line (for serves, from the box centre).
		double LateralMaxM = 0.0;

		// Swing phases after contact, for animation sync (docs/decisions/0004-shot-system.md).
		double FollowThroughS = 0.0;
		double RecoveryS = 0.0;
		// Movement speed multiplier while swinging: planted feet.
		double SwingMoveMult = 0.0;

		double SpeedMinMps() const { return SpeedMinKmh / 3.6; }
		double SpeedMaxMps() const { return SpeedMaxKmh / 3.6; }
		bool UsesApex() const { return ApexMaxM > 0.0; }

		// Problems with this row, empty when it is usable.
		std::vector<std::string> Validate() const;
	};
}
