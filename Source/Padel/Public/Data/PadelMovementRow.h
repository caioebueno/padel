#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shots/MovementTuning.h"
#include "PadelMovementRow.generated.h"

/**
 * The single row ("default") of Data/Movement.json: player movement (GDD 6.1, 6.3).
 * Field meanings are documented on Padel::Shots::FMovementTuning.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelMovementRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tuning") double TopSpeedAt1Mps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double TopSpeedAt99Mps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double TimeToTopAt1S = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double TimeToTopAt99S = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double BrakeMult = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SprintSpeedBonus = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SprintDrainAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SprintDrainAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double RegenAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double RegenAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SprintMinStamina = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SplitMaxSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SplitWindowS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SplitHopS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SplitFirstStepAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SplitFirstStepAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double SplitBoostS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double HomeDriftAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double HomeDriftAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double HomeArriveM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double WallMarginM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double NetMarginM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double AccelFalloff = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double ShuffleSpeedFrac = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double BackpedalSpeedFrac = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double TurnToRunAt1S = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double TurnToRunAt99S = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double CrossoverMinDistM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double TurnBackMinDistM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueHardAccelMps2 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueDrainPerHardAccelAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueDrainPerHardAccelAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueRallyStartS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueDrainPerRallySAt1 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueDrainPerRallySAt99 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueBetweenPointRestS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double FatigueTopSpeedLossAtZero = 0.0;

	Padel::Shots::FMovementTuning ToCore() const;
};
