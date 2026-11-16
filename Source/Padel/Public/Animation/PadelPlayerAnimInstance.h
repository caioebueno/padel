#pragma once

#include "CoreMinimal.h"
#include "Animation/PadelContactWarp.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/PadelPlayerPoseNode.h"
#include "PadelPlayerAnimInstance.generated.h"

class UAnimSequenceBase;

/** Mirrors Padel::Animation::ELocomotionState for the Animation Blueprint. */
UENUM(BlueprintType)
enum class EPadelLocomotionState : uint8
{
	Idle,
	Ready,
	Run,
	Sprint,
	SplitStep,
};

/** Runs FAnimNode_PadelPlayerPose as the root when the class is used without an AnimBP. */
USTRUCT()
struct FPadelPlayerAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FPadelPlayerAnimInstanceProxy() = default;
	FPadelPlayerAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_PadelPlayerPose* InPoseNode)
		: FAnimInstanceProxy(InAnimInstance), PoseNode(InPoseNode) {}

	// Used only when there's no AnimBP graph (an ABP_PadelPlayer subclass brings its own root).
	virtual FAnimNode_Base* GetCustomRootNode() override { return PoseNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(PoseNode); }

	FAnimNode_PadelPlayerPose* PoseNode = nullptr;
};

/**
 * Parent class of ABP_PadelPlayer (docs/decisions/0005-player-animation.md). Every value
 * the graph reads is computed in C++ by Padel::Animation::FPlayerAnimModel and pushed here
 * by UPadelPlayerAnimComponent on the game thread; the AnimBP only wires the nodes
 * (Motion Matching for locomotion, two Sequence Evaluators for the swing, a layered blend
 * from spine_01 by FullBodyAlpha, and the celebration on top). No logic in the graph.
 *
 * Swing playheads are explicit times, not montages, so the contact frame is on screen on
 * the simulation tick that hits the ball (FPlayerAnimFrame::bContactFrame).
 *
 * Used directly (no AnimBP), it runs FAnimNode_PadelPlayerPose, the same recipe in C++
 * (docs/decisions/0006-metahuman-players.md), on the clips in LocomotionSet.
 */
UCLASS(Transient, Blueprintable)
class PADEL_API UPadelPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Sequences are resolved by the component (the frame names them by asset path). */
	void ApplyFrame(const Padel::Animation::FPlayerAnimFrame& Frame, UAnimSequenceBase* InSwing, UAnimSequenceBase* InPrevSwing,
		UAnimSequenceBase* InCelebration, const TArray<FVector>& InTrajectoryWorld);

	/** Clips for the C++ pose; set once by the component. */
	UPROPERTY(Transient)
	FPadelLocomotionSet LocomotionSet;

	/** The model's swing layer weight even with no swing clip imported (SwingWeight is 0 then). */
	float RawSwingWeight = 0.0f;

	/** The placeholder racket target, world cm: the C++ pose reaches for it until swing clips exist. */
	FVector RacketTargetWorld = FVector::ZeroVector;

	/**
	 * Contact warp (PadelContactWarp.h): the body's share of the reach for the ball, 0..1,
	 * rising over the forward swing to contact. 0 when the clip's contact fields are all zero
	 * (the arm alone reaches, as before).
	 */
	float ContactWarpWeight = 0.0f;

	/** The racket face centre in the racket hand's bone space, cm (set by the component; zero = the hand). */
	FVector RacketFaceInHand = FVector::ZeroVector;

	/** The racket arm is the left one (a left-hander); the pose's warp mirrors to it. */
	bool bLeftHanded = false;

	/** Contact warp tuning from Animation.json (set once by the component). */
	FPadelWarpTuning WarpTuning;

	/** False while a MetaHuman shows this pose: the warp then runs on the MetaHuman, after the retarget. */
	bool bWarpThisBody = true;

	/**
	 * How far the arm reaches for RacketTargetWorld, 0..1: through a swing with no swing clip
	 * (the racket target meets the ball on the contact tick), and a little in the ready stance.
	 */
	float WarpArmAlpha() const;

	/** The body's share, 0..1: ContactWarpWeight, but not over an imported swing clip. */
	float WarpBodyAlpha() const { return SwingSequence ? 0.0f : ContactWarpWeight; }

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	EPadelLocomotionState Locomotion = EPadelLocomotionState::Idle;

	/** Planar ground speed, cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.0f;

	/** Movement direction relative to facing the net, degrees (+90 = to the right). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.0f;

	/** Play rate for run/sprint cycles so the stride matches the ground speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float StridePlayRate = 1.0f;

	/** Motion Matching trajectory, world positions (cm), at Data/Animation.json TrajectoryTimesS. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	TArray<FVector> Trajectory;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	TArray<float> TrajectoryTimes;

	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	TObjectPtr<UAnimSequenceBase> SwingSequence;

	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	float SwingTime = 0.0f;

	/** The clip fading out under SwingSequence (side switch, or family clip at contact). */
	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	TObjectPtr<UAnimSequenceBase> PrevSwingSequence;

	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	float PrevSwingTime = 0.0f;

	/** Weight of SwingSequence against PrevSwingSequence. */
	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	float SwingClipAlpha = 1.0f;

	/** Weight of the swing layer over locomotion. */
	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	float SwingWeight = 0.0f;

	/** 1: the swing drives the legs too (planted). 0: upper body only. */
	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	float FullBodyAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Swing")
	bool bContactFrame = false;

	UPROPERTY(BlueprintReadOnly, Category = "Celebration")
	TObjectPtr<UAnimSequenceBase> CelebrationSequence;

	UPROPERTY(BlueprintReadOnly, Category = "Celebration")
	float CelebrationTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Celebration")
	float CelebrationWeight = 0.0f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	UPROPERTY(Transient)
	FAnimNode_PadelPlayerPose PoseNode;
};
