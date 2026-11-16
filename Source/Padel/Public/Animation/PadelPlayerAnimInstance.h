#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/Model/PlayerAnimModel.h"
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

/**
 * Parent class of ABP_PadelPlayer (docs/decisions/0005-player-animation.md). Every value
 * the graph reads is computed in C++ by Padel::Animation::FPlayerAnimModel and pushed here
 * by UPadelPlayerAnimComponent on the game thread; the AnimBP only wires the nodes
 * (Motion Matching for locomotion, two Sequence Evaluators for the swing, a layered blend
 * from spine_01 by FullBodyAlpha, and the celebration on top). No logic in the graph.
 *
 * Swing playheads are explicit times, not montages, so the contact frame is on screen on
 * the simulation tick that hits the ball (FPlayerAnimFrame::bContactFrame).
 */
UCLASS(Transient, Blueprintable)
class PADEL_API UPadelPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Sequences are resolved by the component (the frame names them by asset path). */
	void ApplyFrame(const Padel::Animation::FPlayerAnimFrame& Frame, UAnimSequenceBase* InSwing, UAnimSequenceBase* InPrevSwing,
		UAnimSequenceBase* InCelebration, const TArray<FVector>& InTrajectoryWorld);

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
};
