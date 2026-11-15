// Contact-model constants shared by every shot (GDD 5 and 7): the single row of
// Data/ShotModel.json. Per-shot values are in FShotParams.

#pragma once

#include "Shots/ShotMath.h"

#include <string>
#include <vector>

namespace Padel::Shots
{
	struct PADELCORE_API FShotModel
	{
		// GDD 7: hold time from 0% to 100% charge.
		double ChargeTimeS = 0.0;

		// GDD 5.1
		double PerfectErrorMult = 0.0;
		double PerfectSpeedBonus = 0.0;
		double GoodErrorMult = 0.0;
		double EarlyLateErrorMult = 0.0;
		// How far an early or late hit is pulled across at the target.
		double EarlyLatePullM = 0.0;
		// Rating 99 widens the windows by WindowWiden, rating 1 narrows by WindowNarrow.
		double WindowWiden = 0.0;
		double WindowNarrow = 0.0;
		// A release this many early/late windows before the ideal moment is a shadow
		// swing (no contact) rather than a mishit.
		double ShadowSwingWindows = 0.0;

		// GDD 5.2
		double IdealContactSideM = 0.0;
		double IdealContactForwardM = 0.0;
		double PositionErrorPerM = 0.0;
		double PositionPowerLossPerM = 0.0;
		double PositionPowerMin = 0.0;
		double RunningErrorMult = 0.0;
		// Fraction of top speed above which the player is "running through the shot".
		double RunningSpeedFraction = 0.0;

		// GDD 6.1 Reach and Jump
		double ReachMinM = 0.0;
		double ReachMaxM = 0.0;
		// Stretch beyond the reach before the swing whiffs.
		double ReachSlackM = 0.0;
		double OverheadReachAboveHeightM = 0.0;
		double JumpExtraHeightM = 0.0;

		// GDD 5.3
		double SpeedPowerMin = 0.0;
		double SpeedPowerMax = 0.0;
		double SpinRatingMin = 0.0;
		double SpinRatingMax = 0.0;
		double ScatterRatingReduction = 0.0;
		double PowerMultStartCharge = 0.0;
		double PowerMultMax = 0.0;
		double PressureScale = 0.0;
		// GDD 6.1 Consistency: scales the random part of scatter.
		double ConsistencyScatterAt1 = 0.0;
		double ConsistencyScatterAt99 = 0.0;
		double WallScatterAt1 = 0.0;
		double WallScatterAt99 = 0.0;
		double StaminaFullAbove = 0.0;
		double StaminaFactorAtZero = 0.0;

		// GDD 4 overhead selection: below this charge (or not set) a Y press is a bandeja.
		double SmashMinCharge = 0.0;
		// "Set under the ball": slower than this...
		double SetMaxSpeedMps = 0.0;
		// ...and the ball no further behind the player than this (m, negative = behind).
		double SetMinAheadM = 0.0;
		double UnsetFullChargeErrorMult = 0.0;
		double BackhandOverheadErrorMult = 0.0;

		// GDD 5.1 Miss
		double MishitSpeedMinKmh = 0.0;
		double MishitSpeedMaxKmh = 0.0;
		double MishitDepthMaxM = 0.0;
		double MishitLateralMaxM = 0.0;
		double MishitPopupChance = 0.0;
		double MishitPopupApexMinM = 0.0;
		double MishitPopupApexMaxM = 0.0;

		// GDD 3.2: the serve toss drops the ball from this height beside the server, so it
		// bounces once and rises to the serve's contact band.
		double ServeTossHeightM = 0.0;

		// GDD 6.1 Return: weight of Return against the shot's own rating.
		double ReturnWeight = 0.0;
		// GDD 6.2
		double WrongSideMult = 0.0;

		// Looking for the ideal contact moment in the predicted flight.
		double PredictHorizonS = 0.0;
		double PredictStepS = 0.0;
		// Cost of a metre of height mismatch against a metre of distance.
		double HeightCostWeight = 0.0;
		// Extra cost per metre outside the shot's contact band.
		double OutOfBandCostWeight = 0.0;
		// Cost per second of waiting, so the earlier of two equal chances wins.
		double TimeCostPerS = 0.0;

		std::vector<std::string> Validate() const;
	};
}
