#include "Animation/PadelPlayerAnimInstance.h"

#include "Animation/PadelContactWarp.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
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
	SimVelocity = Padel::ToWorld(Frame.VelocityMps) * FVector(1.0, 1.0, 0.0);
	DesiredVelocity = Padel::ToWorld(Frame.DesiredVelocityMps) * FVector(1.0, 1.0, 0.0);
	AccelLimit = Padel::MetresToWorld(FVector(Frame.AccelLimitMps2, 0.0, 0.0)).X;
	bHasDesired = Frame.bHasDesired;
	SimMode = Frame.Mode;
	Trajectory = InTrajectoryWorld;
	BuildMotionTrajectory();

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

FPadelBodyDynamics UPadelPlayerAnimInstance::PosedBody(float LegKeep, float LeanKeep) const
{
	FPadelBodyDynamics Out = Body;
	const float Keep = StanceKeep();
	Out.LeanForwardDeg = (Out.LeanForwardDeg + Stance.LeanForwardDeg * Keep) * LeanKeep;
	Out.LeanRightDeg *= LeanKeep;
	Out.PelvisDropUnits = (Out.PelvisDropUnits + Stance.PelvisDropUnits * Keep) * LegKeep;
	Out.FeetWidenUnits = (Out.FeetWidenUnits + Stance.FeetWidenUnits * Keep) * LegKeep;
	return Out;
}

void UPadelPlayerAnimInstance::BuildMotionTrajectory()
{
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const UWorld* World = GetWorld();
	if (!Mesh || !World || Trajectory.Num() != TrajectoryTimes.Num())
	{
		MotionTrajectory.Samples.Reset();
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FQuat Facing = Mesh->GetComponentQuat();
	const double Height = Mesh->GetComponentLocation().Z;
	const double Oldest = TrajectoryTimes.IsEmpty() ? 0.0 : FMath::Min(0.0, static_cast<double>(TrajectoryTimes[0]));
	if (!FacingHistory.IsEmpty() && FacingHistory.Last().Key > Now)
	{
		FacingHistory.Reset(); // the clock went back: a new world
	}
	FacingHistory.Emplace(Now, Facing);
	while (FacingHistory.Num() > 1 && FacingHistory[1].Key <= Now + Oldest)
	{
		FacingHistory.RemoveAt(0);
	}
	auto FacingAt = [this, &Facing](double Time)
	{
		// The latest recorded facing at or before Time (the oldest kept before the history fills).
		FQuat Out = FacingHistory.IsEmpty() ? Facing : FacingHistory[0].Value;
		for (const TPair<double, FQuat>& Entry : FacingHistory)
		{
			if (Entry.Key > Time)
			{
				break;
			}
			Out = Entry.Value;
		}
		return Out;
	};
	// The model's future is its velocity carried on: read the velocity from it, and the
	// acceleration from how that velocity changed since last frame (smoothed over ~0.1 s).
	int32 NowIndex = INDEX_NONE;
	int32 NextIndex = INDEX_NONE;
	for (int32 Index = 0; Index < TrajectoryTimes.Num(); ++Index)
	{
		if (NowIndex == INDEX_NONE && TrajectoryTimes[Index] >= 0.0f) { NowIndex = Index; }
		else if (NowIndex != INDEX_NONE && NextIndex == INDEX_NONE && TrajectoryTimes[Index] > TrajectoryTimes[NowIndex]) { NextIndex = Index; }
	}
	FVector Velocity = FVector::ZeroVector;
	if (NowIndex != INDEX_NONE && NextIndex != INDEX_NONE)
	{
		Velocity = (Trajectory[NextIndex] - Trajectory[NowIndex]) / (TrajectoryTimes[NextIndex] - TrajectoryTimes[NowIndex]);
		Velocity.Z = 0.0;
	}
	const double SinceLast = Now - LastVelocityTime;
	if (LastVelocityTime >= 0.0 && SinceLast > 1e-4 && SinceLast < 0.5)
	{
		const FVector Measured = (Velocity - LastVelocity) / SinceLast;
		Acceleration = FMath::Lerp(Acceleration, Measured, static_cast<float>(FMath::Clamp(SinceLast / 0.1, 0.0, 1.0)));
	}
	else
	{
		Acceleration = FVector::ZeroVector;
	}
	LastVelocity = Velocity;
	LastVelocityTime = Now;
	// Braking (acceleration against the velocity) ends at rest; speeding up is carried on for at most half a second.
	const double GroundSpeed = Velocity.Size2D();
	const double Along = GroundSpeed > 1.0 ? FVector::DotProduct(Acceleration, Velocity / GroundSpeed) : 0.0;
	const double StopIn = Along < -1.0 ? GroundSpeed / -Along : TNumericLimits<double>::Max();
	auto Future = [&](double T)
	{
		if (StopIn < TNumericLimits<double>::Max())
		{
			const double U = FMath::Min(T, StopIn);
			return Velocity * U + (Velocity / FMath::Max(GroundSpeed, 1.0)) * (0.5 * Along * U * U);
		}
		const double U = FMath::Min(T, 0.5);
		return Velocity * T + Acceleration * (0.5 * U * U + U * (T - U));
	};
	const FVector NowPosition = NowIndex != INDEX_NONE ? Trajectory[NowIndex] : FVector::ZeroVector;
	MotionTrajectory.Samples.SetNum(Trajectory.Num());
	for (int32 Index = 0; Index < Trajectory.Num(); ++Index)
	{
		const double T = TrajectoryTimes[Index];
		FTransformTrajectorySample& Sample = MotionTrajectory.Samples[Index];
		Sample.TimeInSeconds = static_cast<float>(T);
		const FVector Ground = T > 0.0 && NowIndex != INDEX_NONE ? NowPosition + Future(T) : Trajectory[Index];
		Sample.Position = FVector(Ground.X, Ground.Y, Height);
		Sample.Facing = T < 0.0 ? FacingAt(Now + T) : Facing;
	}
}

float UPadelPlayerAnimInstance::WarpArmAlpha() const
{
	if (SwingSequence)
	{
		return 0.0f;
	}
	return FMath::Clamp(FMath::Max(RawSwingWeight, ArmCarryAlpha), 0.0f, 1.0f);
}

float UPadelPlayerAnimInstance::WarpCarryFollow() const
{
	const double From = LocomotionSet.MocapLegFadeFromMps;
	const double Linear = FMath::Clamp((Speed / 100.0 - From) / FMath::Max(LocomotionSet.MocapLegFadeToMps - From, 0.01), 0.0, 1.0);
	const double Moving = Linear * Linear * (3.0 - 2.0 * Linear);
	const double Carry = 1.0 - FMath::Clamp(RawSwingWeight, 0.0f, 1.0f);
	return static_cast<float>(WarpTuning.CarryFollowShare * Carry * Moving);
}

FAnimInstanceProxy* UPadelPlayerAnimInstance::CreateAnimInstanceProxy()
{
	return new FPadelPlayerAnimInstanceProxy(this, &PoseNode);
}
