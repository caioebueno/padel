// Validate() for the data structs: a missing JSON field reads as 0, so catch the
// values that must be positive or ordered.
#include "Shots/MovementTuning.h"
#include "Shots/ShotModel.h"
#include "Shots/ShotParams.h"

namespace Padel::Shots
{
	// Named, not anonymous: Unreal unity builds merge .cpp files.
	namespace ShotDataValidationDetail
	{
		void Positive(std::vector<std::string>& Errors, const char* Name, double Value)
		{
			if (!(Value > 0.0))
			{
				Errors.push_back(std::string(Name) + " must be > 0");
			}
		}
		void NonNegative(std::vector<std::string>& Errors, const char* Name, double Value)
		{
			if (!(Value >= 0.0))
			{
				Errors.push_back(std::string(Name) + " must be >= 0");
			}
		}
		void UnitRange(std::vector<std::string>& Errors, const char* Name, double Value)
		{
			if (!(Value >= 0.0 && Value <= 1.0))
			{
				Errors.push_back(std::string(Name) + " must be between 0 and 1");
			}
		}
		// A speed fraction: 0 would freeze the player, so a missing field shows up.
		void Fraction(std::vector<std::string>& Errors, const char* Name, double Value)
		{
			if (!(Value > 0.0 && Value <= 1.0))
			{
				Errors.push_back(std::string(Name) + " must be above 0 and at most 1");
			}
		}
		void Ordered(std::vector<std::string>& Errors, const char* Name, double Lo, double Hi)
		{
			if (Lo > Hi)
			{
				Errors.push_back(std::string(Name) + " min is above max");
			}
		}
	}
	using namespace ShotDataValidationDetail;

	std::vector<std::string> FShotParams::Validate() const
	{
		std::vector<std::string> E;
		Positive(E, "SpeedMaxKmh", SpeedMaxKmh);
		Ordered(E, "Speed", SpeedMinKmh, SpeedMaxKmh);
		Ordered(E, "Apex", ApexMinM, ApexMaxM);
		Positive(E, "BaseErrorM", BaseErrorM);
		Positive(E, "PerfectWindowMs", PerfectWindowMs);
		Ordered(E, "Perfect/Good window", PerfectWindowMs, GoodWindowMs);
		Ordered(E, "Good/EarlyLate window", GoodWindowMs, EarlyLateWindowMs);
		Positive(E, "ContactHeightMaxM", ContactHeightMaxM);
		Ordered(E, "ContactHeight", ContactHeightMinM, ContactHeightMaxM);
		NonNegative(E, "ContactHeightPerBodyHeight", ContactHeightPerBodyHeight);
		Positive(E, "IdealContactSideM", IdealContactSideM);
		Positive(E, "IdealContactSideBackM", IdealContactSideBackM);
		Ordered(E, "Depth", DepthShortM, DepthDeepM);
		if (DepthNeutralM < DepthShortM || DepthNeutralM > DepthDeepM)
		{
			E.push_back("DepthNeutralM must be between short and deep");
		}
		NonNegative(E, "ForwardS", ForwardS);
		Positive(E, "FollowThroughS", FollowThroughS);
		Positive(E, "SwingMoveMult", SwingMoveMult);
		if (ForehandRating == ERating::Count || BackhandRating == ERating::Count)
		{
			E.push_back("unknown rating name");
		}
		return E;
	}

	std::vector<std::string> FShotModel::Validate() const
	{
		std::vector<std::string> E;
		Positive(E, "ChargeTimeS", ChargeTimeS);
		Positive(E, "PerfectErrorMult", PerfectErrorMult);
		Positive(E, "GoodErrorMult", GoodErrorMult);
		Positive(E, "EarlyLateErrorMult", EarlyLateErrorMult);
		Positive(E, "ShadowSwingWindows", ShadowSwingWindows);
		Positive(E, "IdealContactSideM", IdealContactSideM);
		Positive(E, "PositionPowerMin", PositionPowerMin);
		Positive(E, "RunningErrorMult", RunningErrorMult);
		Ordered(E, "Reach", ReachMinM, ReachMaxM);
		Positive(E, "ReachMinM", ReachMinM);
		Positive(E, "SpeedPowerMax", SpeedPowerMax);
		Positive(E, "SpinRatingMax", SpinRatingMax);
		Positive(E, "PowerMultMax", PowerMultMax);
		Positive(E, "ConsistencyScatterAt99", ConsistencyScatterAt99);
		Positive(E, "WallScatterAt99", WallScatterAt99);
		Positive(E, "StaminaFullAbove", StaminaFullAbove);
		Positive(E, "SmashMinCharge", SmashMinCharge);
		if (AiChargeLeadS < 0.0)
		{
			E.push_back("AiChargeLeadS must be >= 0");
		}
		Positive(E, "SetMaxSpeedMps", SetMaxSpeedMps);
		Ordered(E, "MishitSpeed", MishitSpeedMinKmh, MishitSpeedMaxKmh);
		Positive(E, "WrongSideMult", WrongSideMult);
		Positive(E, "ServeTossHeightM", ServeTossHeightM);
		Positive(E, "PredictHorizonS", PredictHorizonS);
		Positive(E, "PredictStepS", PredictStepS);
		return E;
	}

	std::vector<std::string> FMovementTuning::Validate() const
	{
		std::vector<std::string> E;
		Positive(E, "TopSpeedAt1Mps", TopSpeedAt1Mps);
		Positive(E, "TopSpeedAt99Mps", TopSpeedAt99Mps);
		Positive(E, "TimeToTopAt1S", TimeToTopAt1S);
		Positive(E, "TimeToTopAt99S", TimeToTopAt99S);
		Positive(E, "BrakeMult", BrakeMult);
		Positive(E, "SplitMaxSpeedMps", SplitMaxSpeedMps);
		Positive(E, "SplitWindowS", SplitWindowS);
		Positive(E, "SplitFirstStepAt99", SplitFirstStepAt99);
		Positive(E, "HomeArriveM", HomeArriveM);
		UnitRange(E, "AccelFalloff", AccelFalloff);
		Fraction(E, "ShuffleSpeedFrac", ShuffleSpeedFrac);
		Fraction(E, "BackpedalSpeedFrac", BackpedalSpeedFrac);
		NonNegative(E, "TurnToRunAt1S", TurnToRunAt1S);
		NonNegative(E, "TurnToRunAt99S", TurnToRunAt99S);
		Positive(E, "CrossoverMinDistM", CrossoverMinDistM);
		Positive(E, "TurnBackMinDistM", TurnBackMinDistM);
		Positive(E, "FatigueHardAccelMps2", FatigueHardAccelMps2);
		NonNegative(E, "FatigueDrainPerHardAccelAt1", FatigueDrainPerHardAccelAt1);
		NonNegative(E, "FatigueDrainPerHardAccelAt99", FatigueDrainPerHardAccelAt99);
		NonNegative(E, "FatigueRallyStartS", FatigueRallyStartS);
		NonNegative(E, "FatigueDrainPerRallySAt1", FatigueDrainPerRallySAt1);
		NonNegative(E, "FatigueDrainPerRallySAt99", FatigueDrainPerRallySAt99);
		NonNegative(E, "FatigueBetweenPointRestS", FatigueBetweenPointRestS);
		UnitRange(E, "FatigueTopSpeedLossAtZero", FatigueTopSpeedLossAtZero);
		return E;
	}
}
