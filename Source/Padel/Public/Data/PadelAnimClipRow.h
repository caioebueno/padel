#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/Model/PlayerAnimTuning.h"
#include "PadelAnimClipRow.generated.h"

/**
 * One row of Data/AnimClips.json: a swing clip and its contact timing
 * (docs/decisions/0005-player-animation.md). Field meanings are on Padel::Animation::FAnimClip.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelAnimClipRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Shot name as in Shots.json ("drive", "serve_kick"...). */
	UPROPERTY(EditAnywhere, Category = "Clip") FString Shot;
	/** "fore" or "back". */
	UPROPERTY(EditAnywhere, Category = "Clip") FString Side;
	/** "early", "perfect" or "late". */
	UPROPERTY(EditAnywhere, Category = "Clip") FString Variant;
	/** UAnimSequence path; the clip may not be imported yet. */
	UPROPERTY(EditAnywhere, Category = "Clip") FString Asset;
	UPROPERTY(EditAnywhere, Category = "Timing") double WindupS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Timing") double ContactS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Timing") double EndS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double BackswingRightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double BackswingForwardM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double BackswingUpM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double FollowRightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double FollowForwardM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double FollowUpM = 0.0;

	/** False (with the reason) when Shot, Side or Variant is not a known name. */
	bool ToCore(FName RowName, Padel::Animation::FAnimClip& Out, FString& OutError) const;
};
