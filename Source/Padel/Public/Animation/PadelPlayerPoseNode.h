#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/PadelBodyDynamics.h"
#include "Animation/PadelContactWarp.h"
#include "Animation/PadelLocomotionWarp.h"
#include "Animation/PadelMotionMatching.h"
#include "Animation/TrajectoryTypes.h"
#include "PadelPlayerPoseNode.generated.h"

class UAnimSequenceBase;
class UPoseSearchDatabase;

/** Locomotion clips for the C++ pose: idle and a walk and jog cycle each way (forward, back, left, right). */
USTRUCT()
struct FPadelLocomotionSet
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UAnimSequenceBase> Idle;
	/** Index 0 forward, 1 back, 2 left, 3 right; null where not imported. */
	UPROPERTY() TArray<TObjectPtr<UAnimSequenceBase>> Walk;
	UPROPERTY() TArray<TObjectPtr<UAnimSequenceBase>> Jog;
	/** N1: the crouched ready idle, and upright walk cycles mixed with the crouched Walk by CrouchAlpha. Optional. */
	UPROPERTY() TObjectPtr<UAnimSequenceBase> Ready;
	UPROPERTY() TArray<TObjectPtr<UAnimSequenceBase>> WalkUpright;
	double WalkClipSpeedMps = 1.0;
	double JogClipSpeedMps = 1.0;
	double WalkMinMps = 0.3;
	double StrideRateMin = 0.75;
	double StrideRateMax = 1.3;

	/** M1: the motion matching databases (Animation.json MotionMatchingDatabases); empty keeps the loop mix. */
	UPROPERTY() TArray<TObjectPtr<const UPoseSearchDatabase>> MotionMatchingDatabases;
	TArray<EPadelMotionMatchingRole> MotionMatchingRoles;
	FPadelMotionMatchingTuning MotionMatching;
	/** M1: how the procedural leg layers fade with speed over mocap legs (Animation.json Mocap*). */
	double MocapLegFadeFromMps = 0.3;
	double MocapLegFadeToMps = 1.5;
	double MocapLegKeep = 0.0;
	double MocapLeanKeep = 0.5;
	/** M2: at speed the chest's facing lead keeps this much of itself, and the head's turn from the chest is capped (Animation.json MocapChestTwistKeep, MocapHeadMaxTwistDeg). */
	double MocapChestTwistKeep = 1.0;
	double MocapHeadMaxTwistDeg = 180.0;
	/** M2: orientation and stride warping and foot locking on the motion matched legs (Animation.json OrientationWarp*, StrideWarp*, FootLock*). */
	FPadelLocomotionWarpTuning LocomotionWarp;

	bool IsValid() const { return Idle != nullptr; }
};

/**
 * The player's whole pose in C++ (docs/decisions/0006-metahuman-players.md), used when
 * ABP_PadelPlayer isn't there: UPadelPlayerAnimInstance runs this as its root, so a body
 * moves with nothing but clips on disk. It does what 0005's AnimBP recipe does:
 *
 * 1. Locomotion: M1 motion matching (PadelMotionMatching.h) over the Game Animation Sample's
 *    starts, stops, pivots, loops, idles and turns in place, searched with the player's
 *    trajectory. Until a search finds a pose, or without the databases (a clone without the
 *    git-ignored assets), the loop mix: idle, and the walk and jog cycles blended by direction
 *    (relative to facing the net) and speed, one shared phase so the feet don't skip.
 * 2. Swing: the swing clip at its explicit time (0005's contact-frame sync), over the upper
 *    body from spine_01, or the whole body by FullBodyAlpha.
 * 3. Celebration on top.
 * 3b. F1 body dynamics (PadelBodyDynamics.h): the chest and head lead turns, the trunk leans
 *    into the acceleration, the pelvis sinks on brakes and landings, the split step hops.
 * 4. The contact warp (PadelContactWarp.h): the arm puts the racket's face centre on the
 *    racket target, which meets the ball on the contact tick, and for clips with contact
 *    fields the knees and spine take part of the reach. Skipped while a MetaHuman shows
 *    this pose; it warps itself after the retarget.
 *
 * Inputs are copied from the anim instance on the game thread (PreUpdate).
 */
USTRUCT()
struct PADEL_API FAnimNode_PadelPlayerPose : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY() FPadelLocomotionSet Set;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> Swing;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> PrevSwing;
	UPROPERTY() TObjectPtr<UAnimSequenceBase> Celebration;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	/** M2: the locomotion warps, for tests and debugging. */
	const FPadelLocomotionWarp& GetLocomotionWarp() const { return LocomotionWarp; }
	/** M1: the motion matcher, for tests and debugging (its CurrentDatabase and CurrentClip). */
	const FPadelMotionMatcher& GetMotionMatcher() const { return Matcher; }
	/** Tests only: pin a clip (FPadelMotionMatcher::PinForTest) or change the matcher directly. */
	FPadelMotionMatcher& GetMotionMatcherForTest() { return Matcher; }
	/** True when the last evaluate played motion matching rather than the loop mix. */
	bool IsMotionMatching() const { return bMotionMatched; }

private:
	bool SampleLocomotion(FPoseContext& Output);
	bool SampleLoopMix(FPoseContext& Output) const;
	void ApplyArmReach(FPoseContext& Output) const;

	// Copied in PreUpdate.
	double SpeedMps = 0.0;
	double DirectionDeg = 0.0;
	double JogStridePlayRate = 1.0;
	/** Ready stance or split-step (a rally on): the idle mixes in the crouched Ready clip. */
	bool bReadyStance = false;
	/** In a point (not the between-points Idle): the motion matcher's Ready databases join the search. */
	bool bInRally = false;
	double CrouchAlpha = 1.0;
	double SwingTime = 0.0;
	double PrevSwingTime = 0.0;
	double SwingClipAlpha = 1.0;
	double SwingWeight = 0.0;
	double FullBodyAlpha = 0.0;
	double CelebrationTime = 0.0;
	double CelebrationWeight = 0.0;
	double ArmAlpha = 0.0;
	double CarryFollow = 0.0;
	double BodyAlpha = 0.0;
	bool bWarp = true;
	FVector RacketTargetComponent = FVector::ZeroVector;
	FVector FaceInHand = FVector::ZeroVector;
	FPadelWarpTuning WarpTuning;
	bool bLeftHanded = false;
	FPadelBodyDynamics Body;

	// M1: the trajectory (mesh component world space, cm) and the matcher that searches with it.
	FTransformTrajectory Trajectory;
	UPROPERTY() FPadelMotionMatcher Matcher;
	bool bMotionMatched = false;
	/** M2: the simulation's ground velocity in component space (cm/s), for the warps, and the warps' state. */
	FVector TravelVelocityComponent = FVector::ZeroVector;
	FPadelLocomotionWarp LocomotionWarp;
	double UpdateDeltaS = 0.0;

	// Advanced in Update.
	double CyclePhase = 0.0;
	double IdleTime = 0.0;

	// Bone setup, from CacheBones: per compact bone 1 above spine_01, else 0.
	TArray<float> UpperBodyMask;
	mutable FPadelWarpBones WarpBones;
	FPadelBodyBones BodyBones;
};
