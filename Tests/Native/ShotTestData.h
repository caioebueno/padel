// Loads the real Data/*.json files into the PadelCore shot structs for the native
// tests. Field names are the same as the Unreal row structs (Padel/Public/Data).
#pragma once

#include "JsonLite.h"
#include "Shots/BallMachineFeed.h"
#include "Shots/MovementTuning.h"
#include "Shots/PlayerRatings.h"
#include "Shots/PracticeBallParams.h"
#include "Shots/ShotLibrary.h"

#ifndef PADEL_REPO_ROOT
#error "PADEL_REPO_ROOT must point at the repository root"
#endif

namespace ShotTestData
{
	using namespace Padel;
	using namespace Padel::Shots;
	using JsonLite::FValue;

	inline std::string DataPath(const char* File) { return std::string(PADEL_REPO_ROOT) + "/Data/" + File; }

	inline double Num(const FValue& Row, const char* Key) { return Row[Key].Number; }

	inline const FValue& Row(const FValue& Table, const std::string& Name)
	{
		for (const FValue& R : Table.Array)
		{
			if (R["Name"].String == Name)
			{
				return R;
			}
		}
		throw std::runtime_error("no row " + Name);
	}

	inline FShotLibrary LoadLibrary()
	{
		FShotLibrary Lib;
		const FValue Shots = JsonLite::ParseFile(DataPath("Shots.json"));
		for (int Index = 0; Index < ShotTypeCount; ++Index)
		{
			const EShotType Type = static_cast<EShotType>(Index);
			if (Type == EShotType::Mishit)
			{
				continue;
			}
			const FValue R = Row(Shots, ShotTypeName(Type));
			FShotParams P;
			P.Type = Type;
			P.SpeedMinKmh = Num(R, "SpeedMinKmh");
			P.SpeedMaxKmh = Num(R, "SpeedMaxKmh");
			P.BaseSpinTop = Num(R, "BaseSpinTop");
			P.BaseSpinSide = Num(R, "BaseSpinSide");
			P.ApexMinM = Num(R, "ApexMinM");
			P.ApexMaxM = Num(R, "ApexMaxM");
			P.BaseErrorM = Num(R, "BaseErrorM");
			P.PerfectWindowMs = Num(R, "PerfectWindowMs");
			P.GoodWindowMs = Num(R, "GoodWindowMs");
			P.EarlyLateWindowMs = Num(R, "EarlyLateWindowMs");
			P.ForehandRating = RatingFromName(R["ForehandRating"].String);
			P.BackhandRating = RatingFromName(R["BackhandRating"].String);
			P.ContactHeightMinM = Num(R, "ContactHeightMinM");
			P.ContactHeightMaxM = Num(R, "ContactHeightMaxM");
			P.PreferredContactHeightM = Num(R, "PreferredContactHeightM");
			P.ContactHeightPerBodyHeight = Num(R, "ContactHeightPerBodyHeight");
			P.LowContactHeightM = Num(R, "LowContactHeightM");
			P.LowContactNetClearanceM = Num(R, "LowContactNetClearanceM");
			P.IdealContactSideM = Num(R, "IdealContactSideM");
			P.IdealContactForwardM = Num(R, "IdealContactForwardM");
			P.IdealContactSideBackM = Num(R, "IdealContactSideBackM");
			P.IdealContactForwardBackM = Num(R, "IdealContactForwardBackM");
			P.DepthShortM = Num(R, "DepthShortM");
			P.DepthDeepM = Num(R, "DepthDeepM");
			P.DepthNeutralM = Num(R, "DepthNeutralM");
			P.LateralMaxM = Num(R, "LateralMaxM");
			P.ForwardS = Num(R, "ForwardS");
			P.FollowThroughS = Num(R, "FollowThroughS");
			P.RecoveryS = Num(R, "RecoveryS");
			P.SwingMoveMult = Num(R, "SwingMoveMult");
			Lib.SetShot(P);
		}

		const FValue Table = JsonLite::ParseFile(DataPath("ShotModel.json"));
		const FValue M = Table.Array.at(0);
		FShotModel& O = Lib.Model;
#define PADEL_READ(Field) O.Field = Num(M, #Field)
		PADEL_READ(ChargeTimeS); PADEL_READ(PerfectErrorMult); PADEL_READ(PerfectSpeedBonus); PADEL_READ(GoodErrorMult);
		PADEL_READ(EarlyLateErrorMult); PADEL_READ(EarlyLatePullM); PADEL_READ(WindowWiden); PADEL_READ(WindowNarrow);
		PADEL_READ(ShadowSwingWindows); PADEL_READ(IdealContactSideM); PADEL_READ(IdealContactForwardM);
		PADEL_READ(PositionErrorPerM); PADEL_READ(PositionPowerLossPerM); PADEL_READ(PositionPowerMin);
		PADEL_READ(RunningErrorMult); PADEL_READ(RunningSpeedFraction); PADEL_READ(ReachMinM); PADEL_READ(ReachMaxM);
		PADEL_READ(ReachSlackM); PADEL_READ(OverheadReachAboveHeightM); PADEL_READ(JumpExtraHeightM);
		PADEL_READ(SpeedPowerMin); PADEL_READ(SpeedPowerMax); PADEL_READ(SpinRatingMin); PADEL_READ(SpinRatingMax);
		PADEL_READ(ScatterRatingReduction); PADEL_READ(PowerMultStartCharge); PADEL_READ(PowerMultMax);
		PADEL_READ(PressureScale); PADEL_READ(ConsistencyScatterAt1); PADEL_READ(ConsistencyScatterAt99);
		PADEL_READ(WallScatterAt1); PADEL_READ(WallScatterAt99); PADEL_READ(StaminaFullAbove); PADEL_READ(StaminaFactorAtZero);
		PADEL_READ(SmashMinCharge); PADEL_READ(AiChargeLeadS); PADEL_READ(SetMaxSpeedMps); PADEL_READ(SetMinAheadM);
		PADEL_READ(UnsetFullChargeErrorMult); PADEL_READ(BackhandOverheadErrorMult); PADEL_READ(MishitSpeedMinKmh);
		PADEL_READ(MishitSpeedMaxKmh); PADEL_READ(MishitDepthMaxM); PADEL_READ(MishitLateralMaxM);
		PADEL_READ(MishitPopupChance); PADEL_READ(MishitPopupApexMinM); PADEL_READ(MishitPopupApexMaxM);
		PADEL_READ(ServeTossHeightM); PADEL_READ(ReturnWeight); PADEL_READ(WrongSideMult);
		PADEL_READ(PredictHorizonS); PADEL_READ(PredictStepS); PADEL_READ(HeightCostWeight);
		PADEL_READ(OutOfBandCostWeight); PADEL_READ(TimeCostPerS); PADEL_READ(GravityMps2);
#undef PADEL_READ
		return Lib;
	}

	inline FMovementTuning LoadMovement()
	{
		const FValue Table = JsonLite::ParseFile(DataPath("Movement.json"));
		const FValue M = Table.Array.at(0);
		FMovementTuning O;
#define PADEL_READ(Field) O.Field = Num(M, #Field)
		PADEL_READ(TopSpeedAt1Mps); PADEL_READ(TopSpeedAt99Mps); PADEL_READ(TimeToTopAt1S); PADEL_READ(TimeToTopAt99S);
		PADEL_READ(BrakeMult); PADEL_READ(SprintSpeedBonus); PADEL_READ(SprintDrainAt1); PADEL_READ(SprintDrainAt99);
		PADEL_READ(RegenAt1); PADEL_READ(RegenAt99); PADEL_READ(SprintMinStamina); PADEL_READ(SplitMaxSpeedMps);
		PADEL_READ(SplitWindowS); PADEL_READ(SplitHopS); PADEL_READ(SplitFirstStepAt1); PADEL_READ(SplitFirstStepAt99);
		PADEL_READ(SplitBoostS); PADEL_READ(HomeDriftAt1); PADEL_READ(HomeDriftAt99); PADEL_READ(HomeArriveM);
		PADEL_READ(WallMarginM); PADEL_READ(NetMarginM);
		PADEL_READ(AccelFalloff); PADEL_READ(ShuffleSpeedFrac); PADEL_READ(BackpedalSpeedFrac); PADEL_READ(TurnToRunS);
		PADEL_READ(CrossoverMinDistM); PADEL_READ(TurnBackMinDistM); PADEL_READ(FatigueHardAccelMps2);
		PADEL_READ(FatigueDrainPerHardAccel); PADEL_READ(FatigueRallyStartS); PADEL_READ(FatigueDrainPerRallyS);
		PADEL_READ(FatigueTopSpeedLossAtZero);
#undef PADEL_READ
		return O;
	}

	inline FPracticeBallParams LoadPracticeBall()
	{
		const FValue Table = JsonLite::ParseFile(DataPath("PracticeBall.json"));
		const FValue M = Table.Array.at(0);
		FPracticeBallParams O;
#define PADEL_READ(Field) O.Field = Num(M, #Field)
		PADEL_READ(GravityMps2); PADEL_READ(RadiusM); PADEL_READ(GroundRestitution); PADEL_READ(GroundTangentKeep);
		PADEL_READ(GlassRestitution); PADEL_READ(GlassTangentKeep); PADEL_READ(MeshRestitution); PADEL_READ(MeshTangentKeep);
		PADEL_READ(NetRestitution); PADEL_READ(DeadLingerS); PADEL_READ(StepS);
#undef PADEL_READ
		return O;
	}

	inline FPlayerRatings LoadPlayer(const std::string& Name = "practice_default")
	{
		const FValue Table = JsonLite::ParseFile(DataPath("Players.json"));
		const FValue R = Row(Table, Name);
		FPlayerRatings P;
		P.Handedness = R["Handedness"].String == "Left" ? EHandedness::Left : EHandedness::Right;
		P.PreferredSide = R["PreferredSide"].String == "Left" ? ECourtSide::Left : ECourtSide::Right;
		P.HeightM = Num(R, "HeightM");
		for (int Index = 0; Index < static_cast<int>(ERating::Count); ++Index)
		{
			const ERating Rating = static_cast<ERating>(Index);
			P.Set(Rating, static_cast<int>(Num(R, RatingName(Rating))));
		}
		return P;
	}

	inline std::vector<FBallMachineFeed> LoadFeeds()
	{
		std::vector<FBallMachineFeed> Out;
		const FValue Table = JsonLite::ParseFile(DataPath("BallMachine.json"));
		for (const FValue& R : Table.Array)
		{
			FBallMachineFeed F;
			F.Name = R["Name"].String;
			F.Drill = R["Drill"].String;
			F.OriginX = Num(R, "OriginX"); F.OriginY = Num(R, "OriginY"); F.OriginZ = Num(R, "OriginZ");
			F.TargetX = Num(R, "TargetX"); F.TargetY = Num(R, "TargetY"); F.TargetJitterM = Num(R, "TargetJitterM");
			F.SpeedKmh = Num(R, "SpeedKmh"); F.ApexM = Num(R, "ApexM"); F.IntervalS = Num(R, "IntervalS");
			Out.push_back(F);
		}
		return Out;
	}
}
