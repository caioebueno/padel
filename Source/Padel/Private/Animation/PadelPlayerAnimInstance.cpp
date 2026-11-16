#include "Animation/PadelPlayerAnimInstance.h"

#include "Animation/PadelContactWarp.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "Court/PadelCourtUnits.h"
#include "Gameplay/PadelShotUnits.h"

void UPadelPlayerAnimInstance::ApplyFrame(const Padel::Animation::FPlayerAnimFrame& Frame, UAnimSequenceBase* InSwing,
	UAnimSequenceBase* InPrevSwing, UAnimSequenceBase* InCelebration, const TArray<FVector>& InTrajectoryWorld)
{
	Locomotion = static_cast<EPadelLocomotionState>(Frame.Locomotion);
	// Speeds leave the simulation in m/s; the graph works in Unreal units.
	Speed = static_cast<float>(Padel::MetresToWorld(FVector(Frame.SpeedMps, 0.0, 0.0)).X);
	Direction = static_cast<float>(Frame.DirectionDeg);
	StridePlayRate = static_cast<float>(Frame.StridePlayRate);
	Trajectory = InTrajectoryWorld;

	SwingSequence = InSwing;
	SwingTime = static_cast<float>(Frame.ClipTimeS);
	PrevSwingSequence = InPrevSwing;
	PrevSwingTime = static_cast<float>(Frame.PrevClipTimeS);
	SwingClipAlpha = InPrevSwing ? static_cast<float>(Frame.ClipAlpha) : 1.0f;
	// No imported clip yet: keep the layer off so the body plays locomotion only.
	SwingWeight = InSwing ? static_cast<float>(Frame.SwingWeight) : 0.0f;
	FullBodyAlpha = static_cast<float>(Frame.FullBodyAlpha);
	bContactFrame = Frame.bContactFrame;

	CelebrationSequence = InCelebration;
	CelebrationTime = static_cast<float>(Frame.CelebrationTimeS);
	CelebrationWeight = InCelebration ? static_cast<float>(Frame.CelebrationWeight) : 0.0f;

	RawSwingWeight = static_cast<float>(Frame.SwingWeight);
	RacketTargetWorld = Padel::ToWorld(Frame.RacketPosition);
	// The body joins the reach only for clips that say where their own contact is (K1's
	// ContactRightM/ForwardM/UpM); all zero keeps today's arm-only reach.
	const bool bClipContact = Frame.Clip && Frame.Clip->HasContactPoint();
	ContactWarpWeight = bClipContact ? static_cast<float>(Frame.SwingWeight) * PadelContactWarp::BodyWeight(Frame.SwingProgress) : 0.0f;
}

float UPadelPlayerAnimInstance::WarpArmAlpha() const
{
	if (SwingSequence)
	{
		return 0.0f;
	}
	const bool bReady = Locomotion == EPadelLocomotionState::Ready || Locomotion == EPadelLocomotionState::SplitStep;
	return FMath::Clamp(FMath::Max(RawSwingWeight, bReady ? static_cast<float>(LocomotionSet.ArmReadyAlpha) : 0.0f), 0.0f, 1.0f);
}

FAnimInstanceProxy* UPadelPlayerAnimInstance::CreateAnimInstanceProxy()
{
	return new FPadelPlayerAnimInstanceProxy(this, &PoseNode);
}
