#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shots/PracticeBallParams.h"
#include "PadelPracticeBallRow.generated.h"

/**
 * The single row ("default") of Data/PracticeBall.json: the stand-in practice ball.
 * Field meanings are documented on Padel::Shots::FPracticeBallParams.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelPracticeBallRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tuning") double GravityMps2 = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double RadiusM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double GroundRestitution = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double GroundTangentKeep = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double GlassRestitution = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double GlassTangentKeep = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MeshRestitution = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double MeshTangentKeep = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double NetRestitution = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double DeadLingerS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Tuning") double StepS = 0.0;

	Padel::Shots::FPracticeBallParams ToCore() const;
};
