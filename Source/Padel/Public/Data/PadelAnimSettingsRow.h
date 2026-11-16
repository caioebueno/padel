#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/Model/PlayerAnimTuning.h"
#include "PadelAnimSettingsRow.generated.h"

/**
 * The single row ("default") of Data/Animation.json (docs/decisions/0005-player-animation.md).
 * Field meanings are documented on Padel::Animation::FAnimSettings.
 */
USTRUCT(BlueprintType)
struct PADEL_API FPadelAnimSettingsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Locomotion") double WalkMinMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double SprintMinMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double RunClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double SprintClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double StrideRateMin = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double StrideRateMax = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double ReadyBallWindowS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double PlantedMaxSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double FullBodyBlendPerS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double SwingBlendInS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Swing") double ClipCrossfadeS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Celebration") double CelebrationS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Celebration") double CelebrationBlendS = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ReadyRacketRightM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ReadyRacketForwardM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ReadyRacketUpM = 0.0;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<double> TrajectoryTimesS;
	UPROPERTY(EditAnywhere, Category = "Celebration") TArray<FString> CelebrateAssets;
	UPROPERTY(EditAnywhere, Category = "Celebration") TArray<FString> DejectedAssets;

	/** Skeletal mesh per team (index 0 Team A, 1 Team B) and the thin AnimBP class (ABP_PadelPlayer). */
	UPROPERTY(EditAnywhere, Category = "Assets") TArray<FString> TeamMeshes;
	UPROPERTY(EditAnywhere, Category = "Assets") FString AnimClass;
	/** Bone the racket attaches to once a skeletal body is shown. */
	UPROPERTY(EditAnywhere, Category = "Assets") FString RacketBone;

	/**
	 * MetaHuman players (docs/decisions/0006-metahuman-players.md): the IK Retargeter from the
	 * placeholder skeleton to the MetaHuman body, the first LOD the MetaHumans may draw (their
	 * LOD 0 face is far more than a broadcast-distance player needs at 60 fps) and the last LOD
	 * that runs the retargeter's IK pass (-1 = every LOD).
	 */
	UPROPERTY(EditAnywhere, Category = "MetaHuman") FString MetaHumanRetargeter;
	UPROPERTY(EditAnywhere, Category = "MetaHuman") int32 MetaHumanMinLod = 0;
	UPROPERTY(EditAnywhere, Category = "MetaHuman") int32 MetaHumanIkLodThreshold = -1;

	/**
	 * The C++ pose used when ABP_PadelPlayer isn't there (0006): idle, walk and jog cycles
	 * (forward, back, left, right) on the placeholder skeleton, the walk cycle's ground speed
	 * (the jog's is RunClipSpeedMps), and how far the arm reaches for the racket target in the
	 * ready stance while no swing clip is imported.
	 */
	UPROPERTY(EditAnywhere, Category = "Locomotion") FString IdleAsset;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> WalkAssets;
	UPROPERTY(EditAnywhere, Category = "Locomotion") TArray<FString> JogAssets;
	UPROPERTY(EditAnywhere, Category = "Locomotion") double WalkClipSpeedMps = 0.0;
	UPROPERTY(EditAnywhere, Category = "Placeholder") double ArmReadyAlpha = 0.0;

	Padel::Animation::FAnimSettings ToCore() const;
};
