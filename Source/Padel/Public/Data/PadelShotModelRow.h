#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shots/ShotModel.h"
#include "PadelShotModelRow.generated.h"

/**
 * The single row ("default") of Data/ShotModel.json: the contact model shared by every shot (GDD 5, 7).
 * Field meanings are documented on Padel::Shots::FShotModel.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelShotModelRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tuning") double ChargeTimeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PerfectErrorMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PerfectSpeedBonus = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double GoodErrorMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double EarlyLateErrorMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double EarlyLatePullM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double WindowWiden = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double WindowNarrow = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ShadowSwingWindows = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double IdealContactSideM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double IdealContactForwardM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PositionErrorPerM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PositionPowerLossPerM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PositionPowerMin = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double RunningErrorMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double RunningSpeedFraction = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ReachMinM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ReachMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ReachSlackM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double OverheadReachAboveHeightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double JumpExtraHeightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SpeedPowerMin = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SpeedPowerMax = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SpinRatingMin = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SpinRatingMax = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ScatterRatingReduction = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PowerMultStartCharge = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PowerMultMax = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PressureScale = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ConsistencyScatterAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ConsistencyScatterAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double WallScatterAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double WallScatterAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double StaminaFullAbove = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double StaminaFactorAtZero = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SmashMinCharge = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double AiChargeLeadS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SetMaxSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SetMinAheadM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double UnsetFullChargeErrorMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double BackhandOverheadErrorMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MishitSpeedMinKmh = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MishitSpeedMaxKmh = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MishitDepthMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MishitLateralMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MishitPopupChance = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MishitPopupApexMinM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MishitPopupApexMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ServeTossHeightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ReturnWeight = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double WrongSideMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PredictHorizonS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double PredictStepS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double HeightCostWeight = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double OutOfBandCostWeight = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double TimeCostPerS = 0.0;

	Padel::Shots::FShotModel ToCore() const;
};
