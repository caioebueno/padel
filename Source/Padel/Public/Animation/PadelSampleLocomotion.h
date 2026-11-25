// The movement restart (docs/decisions/0014-sample-locomotion.md): Epic's Game Animation Sample
// locomotion as Epic ships it, assembled in C++ from the engine's own animation nodes instead of
// an AnimBP, on the sample's own UEFN mannequin (no retarget) with Epic's retarget to the
// MetaHuman.
//
//   motion matching (the sample's Dense databases, chosen by the sample's rules in C++)
//     -> pose history collector (the trajectory comes from PadelCore, not a movement component)
//     -> steering (the clip's root motion turned toward the pawn's facing)
//     -> offset root bone (the drawn body may lag the simulation a little, within limits)
//
// PadelCore stays the authority: the pawn is where the simulation puts it. Only the pose and
// how far its root may trail the pawn come from here. Settings: Data/SampleLocomotion.json.
// On while Padel.Animation.SampleLocomotion is 1; the legacy path (FAnimNode_PadelPlayerPose)
// is untouched otherwise.
//
// Editor builds only for now: the motion matching and offset root nodes keep some settings
// as folded anim node data, which a cooked build only has for nodes compiled into an AnimBP
// (FAnimNode_Base::GetData). The decision record lists the follow-up.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "BoneControllers/AnimNode_Steering.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PadelSampleLocomotion.generated.h"

class UPadelSampleAnimInstance;
class UPoseSearchDatabase;
class USkeletalMesh;
class UIKRetargeter;

/** Data/SampleLocomotion.json. */
struct PADEL_API FPadelSampleLocomotionSettings
{
	bool bOk = false;
	FString Error;
	FString DriverMesh;
	FString MetaHumanRetargeter;
	/** By role name (Idles, WalkLoops...): the asset path. */
	TMap<FName, FString> Databases;

	// Chooser.
	double IdleBelow = 20.0;
	double RunStopsFrom = 100.0;
	double SprintStopsFrom = 550.0;
	double RunGaitFrom = 300.0;
	double SprintGaitFrom = 600.0;
	double MovingSpeedAbove = 0.1;
	double MovingFutureSpeedAbove = 10.0;
	double StartingFutureGain = 100.0;
	double PivotAngle = 45.0;
	/** The gait changes only once the speed is this share past a band's edge (the sample's gait is the player's input; here it is the speed). */
	double GaitHysteresis = 0.1;
	double StrafePivotAngle = 30.0;
	double TurnInPlaceAngle = 50.0;
	double FutureVelocityFromS = 0.4;
	double FutureVelocityToS = 0.5;

	// Motion matching.
	double BlendTimeS = 0.5;
	FName BlendProfile;
	double PlayRateMin = 0.85;
	double PlayRateMax = 1.15;
	double PoseReselectHistoryS = 0.3;
	double MaxBlendInTimeToOverrideAnimationS = 0.03;
	double PlayerDepthBlendInTimeMultiplier = 1.1;

	// Pose history.
	TArray<FName> CollectedBones;
	TArray<FName> CollectedCurves;
	double RootBoneRecoveryTimeS = 0.3;

	// Offset root.
	double TranslationHalfLifeS = 0.2;
	double RotationHalfLifeS = 0.1;
	double MaxTranslationError = 30.0;

	// Trajectory.
	int32 PastCount = 30;
	double PastIntervalS = 0.04;
	int32 FutureCount = 15;
	double FutureIntervalS = 0.1;
	double FacingTurnRate = 540.0;
	// Steering (the sample's locomotion steering node).
	double SteeringAnimatedTargetTimeS = 3.0;
	double SteeringProceduralTargetTimeS = 0.2;
	double SteeringBelowSpeed = 10.0;
	double StrafeAbove = 60.0;

	static FPadelSampleLocomotionSettings Load();
};

/** What the chooser decided this update (for tests and logs). */
struct FPadelSampleChoice
{
	bool bMoving = false;
	bool bStarting = false;
	bool bPivoting = false;
	bool bTurnInPlace = false;
	bool bStrafing = false;
	/** 0 walk, 1 run, 2 sprint. */
	int32 Gait = 0;
	double Speed = 0.0;
	double FutureSpeed = 0.0;
	TArray<FName> Databases;
};

/**
 * The C++ graph: motion matching -> pose history -> offset root bone, the engine's own nodes
 * wired by pose links. Inputs are copied from UPadelSampleAnimInstance on the game thread.
 */
USTRUCT()
struct PADEL_API FAnimNode_PadelSampleLocomotion : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY() FAnimNode_MotionMatching MotionMatching;
	UPROPERTY() FAnimNode_PoseSearchHistoryCollector History;
	UPROPERTY() FAnimNode_ConvertLocalToComponentSpace ToComponent;
	UPROPERTY() FAnimNode_Steering Steering;
	UPROPERTY() FAnimNode_ConvertComponentToLocalSpace ToLocal;
	UPROPERTY() FAnimNode_OffsetRootBone OffsetRoot;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	/** Applies the settings to the engine nodes (once, from the anim instance). */
	void Configure(const FPadelSampleLocomotionSettings& Settings, const USkeleton* Skeleton);

	/** The latest chooser decision and the clip playing, for tests. */
	const FPadelSampleChoice& GetChoice() const { return Choice; }
	const UAnimationAsset* CurrentAsset() const;
	FName CurrentDatabase() const;

private:
	void Choose(const UPadelSampleAnimInstance& Anim);

	bool bLinked = false;
	FPadelSampleLocomotionSettings Settings;
	/** The databases by role, loaded once. */
	TMap<FName, TObjectPtr<UPoseSearchDatabase>> DatabaseByRole;
	TArray<TObjectPtr<UPoseSearchDatabase>> ToSearch;
	EPoseSearchInterruptMode Interrupt = EPoseSearchInterruptMode::DoNotInterrupt;
	/** Steering's target: the pawn's facing (mesh component rotation, world). */
	FQuat SteerTo = FQuat::Identity;
	FPadelSampleChoice Choice;
	FPadelSampleChoice LastChoice;
};

/** Runs FAnimNode_PadelSampleLocomotion as the anim graph's root. */
USTRUCT()
struct FPadelSampleAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FPadelSampleAnimInstanceProxy() = default;
	FPadelSampleAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_PadelSampleLocomotion* InRoot)
		: FAnimInstanceProxy(InAnimInstance), Root(InRoot) {}

	virtual FAnimNode_Base* GetCustomRootNode() override { return Root; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;

	FAnimNode_PadelSampleLocomotion* Root = nullptr;
};

/**
 * The driver body's anim instance on the sample path. A UPadelPlayerAnimInstance, so the
 * component, the MetaHuman retarget and the probes feed and read it as before (ApplyFrame);
 * only its graph differs. It keeps its own trajectory in the sample's shape (past where the
 * pawn was, future from PadelCore's velocity carried on) and extracts root motion so the pose
 * stays in place: the simulation, not the clip, moves the pawn.
 */
UCLASS(Transient, NotBlueprintable)
class PADEL_API UPadelSampleAnimInstance : public UPadelPlayerAnimInstance
{
	GENERATED_BODY()

public:
	UPadelSampleAnimInstance();

	/** Set once by the component, before the first update. */
	void Configure(const FPadelSampleLocomotionSettings& InSettings);

	/** The sample-shaped trajectory (mesh component world space) and its velocities (cm/s), rebuilt each update. */
	const FTransformTrajectory& GetSampleTrajectory() const { return SampleTrajectory; }
	FVector GetCurrentVelocity() const { return CurrentVelocity; }
	FVector GetFutureVelocity() const { return FutureVelocity; }
	/** Yaw (degrees) from the drawn root to the pawn's facing: how far the offset root lags the turn. */
	double GetRootYawLag() const { return RootYawLag; }
	bool IsStrafing() const { return bStrafing; }
	const FPadelSampleLocomotionSettings& GetSettings() const { return Settings; }

	const FAnimNode_PadelSampleLocomotion& GetSampleNode() const { return SampleNode; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void BuildSampleTrajectory();

	FPadelSampleLocomotionSettings Settings;
	FTransformTrajectory SampleTrajectory;
	/** Where the mesh was (world seconds, position, facing), newest last. */
	TArray<TTuple<double, FVector, FQuat>> Past;
	FVector CurrentVelocity = FVector::ZeroVector;
	FVector FutureVelocity = FVector::ZeroVector;
	double RootYawLag = 0.0;
	bool bStrafing = false;

	UPROPERTY(Transient)
	FAnimNode_PadelSampleLocomotion SampleNode;
};
