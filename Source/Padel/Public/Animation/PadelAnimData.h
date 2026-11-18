#pragma once

#include "CoreMinimal.h"
#include "Animation/PadelContactWarp.h"
#include "Animation/PadelLocomotionWarp.h"
#include "Animation/PadelMotionMatching.h"
#include "Animation/Model/PlayerAnimTuning.h"
#include "Data/PadelPlayerBodyRow.h"

/**
 * The animation tuning from Data/Animation.json and Data/AnimClips.json, read through
 * transient DataTables like the shot data (FPadelShotData) and converted to the
 * engine-free structs the animation model uses. Data/PlayerBodies.json (asset paths
 * only) loads the same way. A broken file is logged and leaves bOk false.
 */
struct PADEL_API FPadelAnimData
{
	Padel::Animation::FAnimSettings Settings;
	Padel::Animation::FAnimClipLibrary Clips;
	/** Asset paths (not tuning): skeletal mesh per team, the AnimBP class, the racket bone. */
	TArray<FString> TeamMeshes;
	FString AnimClass;
	FName RacketBone;
	/** MetaHuman players: Data/PlayerBodies.json by player row name, and the shared settings from Animation.json. */
	TMap<FName, FPadelPlayerBodyRow> PlayerBodies;
	FString MetaHumanRetargeter;
	int32 MetaHumanMinLod = 0;
	int32 MetaHumanIkLodThreshold = -1;
	FString MetaHumanFallbackClass;
	FString MetaHumanShoeMesh;
	double ShoeAnkleToBallM = 0.0;
	/** The C++ pose's clips (Animation.json IdleAsset, WalkAssets, JogAssets) and the walk's speed. */
	FString IdleAsset;
	TArray<FString> WalkAssets;
	TArray<FString> JogAssets;
	FString ReadyAsset;
	TArray<FString> WalkUprightAssets;
	double WalkClipSpeedMps = 0.0;
	/** M1 motion matching: the database paths and the search tuning, and the leg layers' fade over mocap legs. */
	TArray<FString> MotionMatchingDatabases;
	/** The chooser: which of MotionMatchingDatabases are searched still, and turning in place (the rest moving). */
	TArray<FString> MotionMatchingStillDatabases;
	TArray<FString> MotionMatchingStopDatabases;
	TArray<FString> MotionMatchingWalkDatabases;
	TArray<FString> MotionMatchingSprintDatabases;
	TArray<FString> MotionMatchingTurnInPlaceDatabases;
	FPadelMotionMatchingTuning MotionMatching;
	double MocapLegFadeFromMps = 0.3;
	double MocapLegFadeToMps = 1.5;
	double MocapLegKeep = 0.0;
	double MocapLeanKeep = 0.5;
	/** M2: the trunk at speed, and the locomotion warps (already in Unreal units). */
	double MocapChestTwistKeep = 1.0;
	double MocapHeadMaxTwistDeg = 180.0;
	FPadelLocomotionWarpTuning LocomotionWarp;
	/** Contact warp tuning (Animation.json Warp*), already in Unreal units. */
	FPadelWarpTuning Warp;
	bool bOk = false;
	TArray<FString> Errors;

	static FPadelAnimData Load();
};
