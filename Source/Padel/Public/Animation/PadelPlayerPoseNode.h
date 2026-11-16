#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/PadelContactWarp.h"
#include "PadelPlayerPoseNode.generated.h"

class UAnimSequenceBase;

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
	/** Arm reach toward the racket target while ready and no swing clip is imported. */
	double ArmReadyAlpha = 0.0;

	bool IsValid() const { return Idle != nullptr; }
};

/**
 * The player's whole pose in C++ (docs/decisions/0006-metahuman-players.md), used when
 * ABP_PadelPlayer isn't there: UPadelPlayerAnimInstance runs this as its root, so a body
 * moves with nothing but clips on disk. It does what 0005's AnimBP recipe does:
 *
 * 1. Locomotion: idle, and the walk and jog cycles blended by direction (relative to facing
 *    the net) and speed, one shared phase so the feet don't skip between clips.
 * 2. Swing: the swing clip at its explicit time (0005's contact-frame sync), over the upper
 *    body from spine_01, or the whole body by FullBodyAlpha.
 * 3. Celebration on top.
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

private:
	bool SampleLocomotion(FPoseContext& Output) const;
	void ApplyArmReach(FPoseContext& Output) const;

	// Copied in PreUpdate.
	double SpeedMps = 0.0;
	double DirectionDeg = 0.0;
	double JogStridePlayRate = 1.0;
	/** Ready stance or split-step (a rally on): the idle mixes in the crouched Ready clip. */
	bool bReadyStance = false;
	double CrouchAlpha = 1.0;
	double SwingTime = 0.0;
	double PrevSwingTime = 0.0;
	double SwingClipAlpha = 1.0;
	double SwingWeight = 0.0;
	double FullBodyAlpha = 0.0;
	double CelebrationTime = 0.0;
	double CelebrationWeight = 0.0;
	double ArmAlpha = 0.0;
	double BodyAlpha = 0.0;
	bool bWarp = true;
	FVector RacketTargetComponent = FVector::ZeroVector;
	FVector FaceInHand = FVector::ZeroVector;
	FPadelWarpTuning WarpTuning;
	bool bLeftHanded = false;

	// Advanced in Update.
	double CyclePhase = 0.0;
	double IdleTime = 0.0;

	// Bone setup, from CacheBones: per compact bone 1 above spine_01, else 0.
	TArray<float> UpperBodyMask;
	mutable FPadelWarpBones WarpBones;
};
