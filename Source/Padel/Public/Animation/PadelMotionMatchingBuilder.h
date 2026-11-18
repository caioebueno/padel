#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PadelMotionMatchingBuilder.generated.h"

class UAnimSequence;
class USkeleton;
class UPoseSearchDatabase;
class UPoseSearchNormalizationSet;
class UPoseSearchSchema;

/**
 * M1 asset pipeline (Tools/AssetPipeline/unreal/animation_setup.py): builds the motion matching
 * schemas, databases and normalization set on the placeholder skeleton from the Game Animation
 * Sample's manifest (gasp_export.py). In C++ because a Pose Search database's animation list
 * isn't reachable from Python. Editor only; every call fails with a message in a game build.
 *
 * Each returns an empty string on success, otherwise what went wrong.
 */
UCLASS()
class PADEL_API UPadelMotionMatchingBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The asset at PackagePath (/Game/...), loaded, or created empty when it isn't there. */
	UFUNCTION(BlueprintCallable, Category = "Padel|MotionMatching")
	static UObject* FindOrCreateAsset(UClass* AssetClass, const FString& PackagePath);

	/**
	 * Replaces Schema's channels and skeleton with SchemaJson, one entry of the manifest's
	 * "schemas": its "settings" (sample_rate) and "channels" (trajectory samples; position,
	 * velocity and heading channels inside a group). No mirror table: mirroring is off.
	 */
	UFUNCTION(BlueprintCallable, Category = "Padel|MotionMatching")
	static FString BuildSchema(UPoseSearchSchema* Schema, USkeleton* Skeleton, const FString& SchemaJson);

	/** Replaces Database's clips and settings (the manifest database's "settings" as JSON). */
	UFUNCTION(BlueprintCallable, Category = "Padel|MotionMatching")
	static FString BuildDatabase(UPoseSearchDatabase* Database, UPoseSearchSchema* Schema, UPoseSearchNormalizationSet* NormalizationSet,
		const TArray<UAnimSequence*>& Clips, const FString& SettingsJson);

	/** The databases normalized together (so costs compare across idles, loops and stops). */
	UFUNCTION(BlueprintCallable, Category = "Padel|MotionMatching")
	static FString BuildNormalizationSet(UPoseSearchNormalizationSet* NormalizationSet, const TArray<UPoseSearchDatabase*>& Databases);

	/** Builds Database's search index now and waits for it. */
	UFUNCTION(BlueprintCallable, Category = "Padel|MotionMatching")
	static FString BuildIndex(UPoseSearchDatabase* Database);
};
