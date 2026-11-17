#pragma once

#include "CoreMinimal.h"
#include "Animation/PadelContactWarp.h"
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
	/** The C++ pose's clips (Animation.json IdleAsset, WalkAssets, JogAssets) and the walk's speed. */
	FString IdleAsset;
	TArray<FString> WalkAssets;
	TArray<FString> JogAssets;
	FString ReadyAsset;
	TArray<FString> WalkUprightAssets;
	double WalkClipSpeedMps = 0.0;
	/** Contact warp tuning (Animation.json Warp*), already in Unreal units. */
	FPadelWarpTuning Warp;
	bool bOk = false;
	TArray<FString> Errors;

	static FPadelAnimData Load();
};
