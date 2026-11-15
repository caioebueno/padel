// A row of Data/Difficulty.json as a DataTable row (GDD 8 difficulty table), for menus
// and Blueprints. Field names match the JSON keys so the file imports as is. The AI
// itself reads the JSON through Padel::AIData; ToCore() converts a row if a DataTable
// is the source instead.

#pragma once

#include "CoreMinimal.h"
#include "AI/AiTuning.h"
#include "Engine/DataTable.h"
#include "PadelDifficultyRow.generated.h"

USTRUCT(BlueprintType)
struct PADEL_API FDifficultyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	double ReadDelayS = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Timing")
	double TimingWeightPerfect = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Timing")
	double TimingWeightGood = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Timing")
	double TimingWeightEarlyLate = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Timing")
	double TimingWeightMiss = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Shot choice")
	double SafeShotBias = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Shot choice")
	double TacticalWeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Accuracy")
	double AimScatterMult = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Accuracy")
	int32 AttributeOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty|Accuracy")
	double PredictionNoiseMult = 0.0;

	Padel::AI::FAiDifficulty ToCore(const FName& RowName) const
	{
		Padel::AI::FAiDifficulty Out;
		Out.Name = TCHAR_TO_UTF8(*RowName.ToString());
		Out.ReadDelayS = ReadDelayS;
		Out.TimingWeightPerfect = TimingWeightPerfect;
		Out.TimingWeightGood = TimingWeightGood;
		Out.TimingWeightEarlyLate = TimingWeightEarlyLate;
		Out.TimingWeightMiss = TimingWeightMiss;
		Out.SafeShotBias = SafeShotBias;
		Out.TacticalWeight = TacticalWeight;
		Out.AimScatterMult = AimScatterMult;
		Out.AttributeOffset = AttributeOffset;
		Out.PredictionNoiseMult = PredictionNoiseMult;
		return Out;
	}
};
