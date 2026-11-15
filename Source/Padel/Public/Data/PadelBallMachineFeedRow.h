#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shots/BallMachineFeed.h"
#include "PadelBallMachineFeedRow.generated.h"

/** One row of Data/BallMachine.json. Field meanings are on Padel::Shots::FBallMachineFeed. */
USTRUCT(BlueprintType)
struct PADEL_API FPadelBallMachineFeedRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Feed") FString Drill;
	UPROPERTY(EditAnywhere, Category = "Feed") double OriginX = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double OriginY = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double OriginZ = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double TargetX = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double TargetY = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double TargetJitterM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double SpeedKmh = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double ApexM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Feed") double IntervalS = 0.0;

	Padel::Shots::FBallMachineFeed ToCore(const FName& RowName) const;
};
