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
	CrouchAlpha = static_cast<float>(Frame.StancePose.CrouchAlpha);
	ArmCarryAlpha = static_cast<float>(Frame.StancePose.ArmAlpha);
	StridePlayRate = static_cast<float>(Frame.StridePlayRate);
	Body.ChestTwistDeg = static_cast<float>(Frame.ChestTwistDeg);
	Body.HeadTwistDeg = static_cast<float>(Frame.HeadTwistDeg);
	Body.HeadPitchDeg = static_cast<float>(Frame.HeadPitchDeg);
	Body.LeanForwardDeg = static_cast<float>(Frame.LeanForwardDeg);
	Body.LeanRightDeg = static_cast<float>(Frame.LeanRightDeg);
	Body.PelvisDropUnits = static_cast<float>(Padel::MetresToWorld(FVector(0.0, 0.0, Frame.PelvisDropM)).Z);
	// P1: the stance's trunk tip, knee bend and feet width, kept apart so a swing can rise out of them.
	Stance = FPadelBodyDynamics();
	Stance.LeanForwardDeg = static_cast<float>(Frame.StancePose.TrunkDeg);
	Stance.PelvisDropUnits = static_cast<float>(Padel::MetresToWorld(FVector(0.0, 0.0, -Frame.StancePose.DropM)).Z);
	Stance.FeetWidenUnits = static_cast<float>(Padel::MetresToWorld(FVector(0.0, 0.0, Frame.StancePose.WidenM)).Z);
	Body.HopLiftUnits = static_cast<float>(Padel::MetresToWorld(FVector(0.0, 0.0, Frame.HopLiftM)).Z);
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

float UPadelPlayerAnimInstance::StanceKeep() const
{
	return 1.0f - FMath::Clamp(RawSwingWeight, 0.0f, 1.0f);
}

FPadelBodyDynamics UPadelPlayerAnimInstance::PosedBody() const
{
	FPadelBodyDynamics Out = Body;
	const float Keep = StanceKeep();
	Out.LeanForwardDeg += Stance.LeanForwardDeg * Keep;
	Out.PelvisDropUnits += Stance.PelvisDropUnits * Keep;
	Out.FeetWidenUnits += Stance.FeetWidenUnits * Keep;
	return Out;
}

float UPadelPlayerAnimInstance::WarpArmAlpha() const
{
	if (SwingSequence)
	{
		return 0.0f;
	}
	return FMath::Clamp(FMath::Max(RawSwingWeight, ArmCarryAlpha), 0.0f, 1.0f);
}

FAnimInstanceProxy* UPadelPlayerAnimInstance::CreateAnimInstanceProxy()
{
	return new FPadelPlayerAnimInstanceProxy(this, &PoseNode);
}
