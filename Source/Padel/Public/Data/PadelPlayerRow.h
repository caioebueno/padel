#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shots/PlayerRatings.h"
#include "PadelPlayerRow.generated.h"

/**
 * One row of Data/Players.json (GDD 6.1): profile and 1-99 ratings. The same field
 * names as Padel::Shots::FPlayerRatings, which the shot system and the AI read.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelPlayerRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Profile") FString DisplayName;
	/** "Right" or "Left". */
	UPROPERTY(EditAnywhere, Category = "Profile") FName Handedness;
	/** "Right" (the drive side) or "Left" (the revés side). */
	UPROPERTY(EditAnywhere, Category = "Profile") FName PreferredSide;
	UPROPERTY(EditAnywhere, Category = "Profile") double HeightM = 1.8;

	UPROPERTY(EditAnywhere, Category = "Technical") int32 Drive = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Backhand = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Volley = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Lob = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Overhead = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Bandeja = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Vibora = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 WallPlay = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Touch = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 Serve = 1;
	UPROPERTY(EditAnywhere, Category = "Technical") int32 ReturnSkill = 1;
	UPROPERTY(EditAnywhere, Category = "Physical") int32 Speed = 1;
	UPROPERTY(EditAnywhere, Category = "Physical") int32 Acceleration = 1;
	UPROPERTY(EditAnywhere, Category = "Physical") int32 Agility = 1;
	UPROPERTY(EditAnywhere, Category = "Physical") int32 Power = 1;
	UPROPERTY(EditAnywhere, Category = "Physical") int32 Reach = 1;
	UPROPERTY(EditAnywhere, Category = "Physical") int32 Jump = 1;
	UPROPERTY(EditAnywhere, Category = "Physical") int32 Stamina = 1;
	UPROPERTY(EditAnywhere, Category = "Mental") int32 Anticipation = 1;
	UPROPERTY(EditAnywhere, Category = "Mental") int32 Positioning = 1;
	UPROPERTY(EditAnywhere, Category = "Mental") int32 Composure = 1;
	UPROPERTY(EditAnywhere, Category = "Mental") int32 Consistency = 1;
	UPROPERTY(EditAnywhere, Category = "Mental") int32 Teamwork = 1;

	Padel::Shots::FPlayerRatings ToCore() const;
};
