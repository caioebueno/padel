// Contact-point warp (shots doc 4.2, movement plan N3): bends the body and reaches the arm so
// the racket's face centre meets the ball. It runs on whichever skeleton is on screen: the
// placeholder's C++ pose node, or the MetaHuman after the retarget (their arms differ in
// length, so a warp on the hidden placeholder would miss on the MetaHuman).
//
// The reach is split across the body, as a player does it:
//   down     pelvis lower (knee bend; the feet stay planted) and spine flexion
//   sideways spine side-bend, then the arm
//   forward  a small pelvis shift, then the arm
// and the right (or, for a left-hander, left) arm's two-bone IK closes the rest, aiming the
// racket's face centre, not the hand, at the target.

#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "BonePose.h"

struct FBoneContainer;

/** The bones the warp moves, cached per skeleton (CacheBones). */
struct PADEL_API FPadelWarpBones
{
	FCompactPoseBoneIndex Pelvis = FCompactPoseBoneIndex(INDEX_NONE);
	/** spine_01 upwards, as many as the skeleton has (3 on the placeholder, 5 on a MetaHuman). */
	TArray<FCompactPoseBoneIndex> Spine;
	FCompactPoseBoneIndex UpperArm = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex LowerArm = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Hand = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Thigh[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Calf[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	FCompactPoseBoneIndex Foot[2] = { FCompactPoseBoneIndex(INDEX_NONE), FCompactPoseBoneIndex(INDEX_NONE) };
	bool bLeftHanded = false;

	/** Finds the UE5-mannequin-named bones (the placeholder and MetaHumans share the names). */
	void Cache(const FBoneContainer& Bones, bool bInLeftHanded);
	bool HasArm() const { return UpperArm.GetInt() != INDEX_NONE && LowerArm.GetInt() != INDEX_NONE && Hand.GetInt() != INDEX_NONE; }
};

/**
 * How the reach splits across the body: Data/Animation.json's Warp* fields, in Unreal units
 * and degrees (FPadelAnimData converts the metres once, at load). Defaults match the file.
 */
struct PADEL_API FPadelWarpTuning
{
	double PelvisDownShare = 0.8;
	double MaxPelvisDropUnits = 50.0;
	double PelvisForwardShare = 0.25;
	double MaxPelvisForwardUnits = 15.0;
	double SpineSideShare = 0.45;
	double SpineForwardShare = 0.25;
	double SpineDownShare = 0.75;
	double MaxSpineDeg = 55.0;
	double TrunkTurnShare = 0.7;
	double MaxTrunkTurnDeg = 50.0;
	double KneeBendUnits = 20.0;
	double ElbowBendUnits = 20.0;
	double MaxReachShare = 0.98;
};

/**
 * The forearm twist bones of a MetaHuman (lowerarm_twist_01/02, both arms) and how much of
 * the hand's roll each takes. The placeholder skeleton has none, so the retarget leaves them
 * in their reference pose and a rolled wrist candy-wraps; this spreads the roll up the forearm.
 */
struct PADEL_API FPadelTwistBones
{
	struct FArm
	{
		FCompactPoseBoneIndex Hand = FCompactPoseBoneIndex(INDEX_NONE);
		TArray<FCompactPoseBoneIndex> Twist;
		/** Share of the hand's roll per twist bone: how far it sits from the elbow to the wrist. */
		TArray<float> Share;
	};
	FArm Arms[2];

	void Cache(const FBoneContainer& Bones);
};

/** One frame's warp request, in the skeletal mesh component's space (Unreal units). */
struct PADEL_API FPadelWarpInput
{
	/** Where the racket's face centre should be. */
	FVector TargetCS = FVector::ZeroVector;
	/** The face centre in the racket hand's bone space (from the grip, 0008); zero = the hand itself. */
	FVector FaceInHand = FVector::ZeroVector;
	/** 0..1: how far the arm reaches for TargetCS (today's racket-target IK). */
	float ArmAlpha = 0.0f;
	/** 0..1: how much of the reach the body takes (rises over the forward swing, peaks at contact). */
	float BodyAlpha = 0.0f;
};

namespace PadelContactWarp
{
	/** Gives each forearm twist bone its share of the hand's roll about the forearm (local space). */
	PADEL_API void DistributeForearmTwist(FCompactPose& Pose, const FPadelTwistBones& Bones);

	/** The hand's roll about the forearm axis in Pose, radians (for the tests). */
	PADEL_API double HandRoll(const FCompactPose& Pose, FCompactPoseBoneIndex Hand);

	/** Warps Pose in place. Does nothing when ArmAlpha and BodyAlpha are both 0. */
	PADEL_API void Apply(FCompactPose& Pose, const FPadelWarpBones& Bones, const FPadelWarpInput& In, const FPadelWarpTuning& Tuning);

	/**
	 * Body weight over the swing (shots doc 4.2): 0 through the backswing, rising over the
	 * forward swing to 1 at contact, falling to 0 over the follow-through. Progress is
	 * FPlayerAnimFrame::SwingProgress (1 = end of windup, 2 = contact, 3 = end of follow-through).
	 */
	PADEL_API float BodyWeight(double SwingProgress);

	/** The face centre (Unreal units) in the hand bone's space for a racket attached to it at Relative. */
	PADEL_API FVector FaceInHand(const FTransform& RacketRelativeToHand, double FaceCentreUnits);
}
