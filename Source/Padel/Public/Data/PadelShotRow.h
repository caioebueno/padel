#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shots/ShotParams.h"
#include "PadelShotRow.generated.h"

/**
 * One row of Data/Shots.json (GDD 4). The row name is the shot id ("drive",
 * "serve_flat"...). Field meanings are documented on Padel::Shots::FShotParams.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelShotRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Speed") double SpeedMinKmh = 0.0;
	UPROPERTY(EditAnywhere, Category = "Speed") double SpeedMaxKmh = 0.0;
	UPROPERTY(EditAnywhere, Category = "Spin") double BaseSpinTop = 0.0;
	UPROPERTY(EditAnywhere, Category = "Spin") double BaseSpinSide = 0.0;
	UPROPERTY(EditAnywhere, Category = "Speed") double ApexMinM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Speed") double ApexMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Error") double BaseErrorM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Timing") double PerfectWindowMs = 0.0;
	UPROPERTY(EditAnywhere, Category = "Timing") double GoodWindowMs = 0.0;
	UPROPERTY(EditAnywhere, Category = "Timing") double EarlyLateWindowMs = 0.0;
	UPROPERTY(EditAnywhere, Category = "Ratings") FName ForehandRating;
	UPROPERTY(EditAnywhere, Category = "Ratings") FName BackhandRating;
	UPROPERTY(EditAnywhere, Category = "Contact") double ContactHeightMinM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Contact") double ContactHeightMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Contact") double PreferredContactHeightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Contact") double ContactHeightPerBodyHeight = 0.0;
	UPROPERTY(EditAnywhere, Category = "Contact") double IdealContactSideM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Contact") double IdealContactForwardM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Contact") double IdealContactSideBackM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Contact") double IdealContactForwardBackM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Aim") double DepthShortM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Aim") double DepthDeepM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Aim") double DepthNeutralM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Aim") double LateralMaxM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Phases") double ForwardS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Phases") double FollowThroughS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Phases") double RecoveryS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Phases") double SwingMoveMult = 0.0;

	Padel::Shots::FShotParams ToCore(Padel::EShotType Type) const;
};
