// F1 body dynamics on the pose (docs/animation/MOVEMENT_FLUENCY.md): what
// Padel::Animation::FPlayerAnimModel computes (StepBody) put on the bones, over whatever
// the clips play. The chest and head turn ahead of the hips, the trunk leans into the
// acceleration, the pelvis sinks on a brake, a landing or the ready bounce with the feet
// kept planted, and the split step lifts the whole body.
//
// It runs on the placeholder pose, before the contact warp (which then aims the arm from the
// leaned body). A MetaHuman copies it through the retarget like the rest of the pose.

#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "BonePose.h"

struct FBoneContainer;

/** The bones body dynamics moves, cached per skeleton (CacheBones). UE5 mannequin names. */
struct PADEL_API FPadelBodyBones
{
	FCompactPoseBoneIndex Pelvis = FCompactPoseBoneIndex(INDEX_NONE);
	/** spine_01 upwards, as many as the skeleton has. */
	TArray<FCompactPoseBoneIndex> Spine;
	/** neck_01 upwards, then head. */
	TArray<FCompactPoseBoneIndex> NeckAndHead;
	FCompactPoseBoneIndex Thigh[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Calf[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Foot[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };

	void Cache(const FBoneContainer& Bones);
};

/** One frame of FPlayerAnimFrame's body fields, in degrees and Unreal units. */
struct PADEL_API FPadelBodyDynamics
{
	float ChestTwistDeg = 0.0f;
	float HeadTwistDeg = 0.0f;
	float LeanForwardDeg = 0.0f;
	float LeanRightDeg = 0.0f;
	/** Zero or negative: the pelvis lowered, feet planted. */
	float PelvisDropUnits = 0.0f;
	/** Zero or positive: the whole body lifted, feet too (the split-step hop). */
	float HopLiftUnits = 0.0f;

	bool IsZero() const
	{
		return ChestTwistDeg == 0.0f && HeadTwistDeg == 0.0f && LeanForwardDeg == 0.0f && LeanRightDeg == 0.0f
			&& PelvisDropUnits == 0.0f && HopLiftUnits == 0.0f;
	}
};

namespace PadelBodyDynamics
{
	/** Puts Body on Pose in place (component space maths, local pose out). */
	PADEL_API void Apply(FCompactPose& Pose, const FPadelBodyBones& Bones, const FPadelBodyDynamics& Body);
}
