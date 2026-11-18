#include "Animation/PadelPlayerPoseNode.h"

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

#include <cmath>

namespace PadelPlayerPoseDetail
{
	// Bone names of the UE5 mannequin layout the placeholders are rigged on (Tools/AssetPipeline/README.md).
	const FName LayerRoot(TEXT("spine_01"));

	enum EDir : int32 { Forward = 0, Back = 1, Left = 2, Right = 3 };

	/** Samples Seq at Time into Out; false (Out untouched) when the clip is missing or for another skeleton. */
	bool Sample(const UAnimSequenceBase* Seq, double Time, bool bLoop, FPoseContext& Out)
	{
		if (!Seq || Seq->GetSkeleton() != Out.AnimInstanceProxy->GetRequiredBones().GetSkeletonAsset())
		{
			return false;
		}
		FAnimationPoseData Data(Out);
		Seq->GetAnimationPose(Data, FAnimExtractContext(Time, false, {}, bLoop));
		return true;
	}

	/** Out = lerp(Out, Other, Alpha * Mask[bone]); a null Mask is 1 for every bone. */
	void BlendInto(FPoseContext& Out, const FPoseContext& Other, float Alpha, const TArray<float>* Mask = nullptr)
	{
		if (Alpha <= 0.0f)
		{
			return;
		}
		for (const FCompactPoseBoneIndex Bone : Out.Pose.ForEachBoneIndex())
		{
			const float W = Alpha * (Mask && Mask->IsValidIndex(Bone.GetInt()) ? (*Mask)[Bone.GetInt()] : 1.0f);
			if (W >= 1.0f)
			{
				Out.Pose[Bone] = Other.Pose[Bone];
			}
			else if (W > 0.0f)
			{
				Out.Pose[Bone].BlendWith(Other.Pose[Bone], W);
			}
		}
		Out.Pose.NormalizeRotations();
	}

	/** The two cardinal directions around DirectionDeg (0 forward, +90 right) and the weight of the second. */
	void Neighbours(double DirectionDeg, EDir& A, EDir& B, float& T)
	{
		const double D = FRotator::NormalizeAxis(DirectionDeg);
		if (D >= 0.0 && D <= 90.0) { A = Forward; B = Right; T = static_cast<float>(D / 90.0); }
		else if (D > 90.0) { A = Right; B = Back; T = static_cast<float>((D - 90.0) / 90.0); }
		else if (D >= -90.0) { A = Left; B = Forward; T = static_cast<float>((D + 90.0) / 90.0); }
		else { A = Back; B = Left; T = static_cast<float>((D + 180.0) / 90.0); }
	}

	UAnimSequenceBase* At(const TArray<TObjectPtr<UAnimSequenceBase>>& Clips, int32 Index)
	{
		return Clips.IsValidIndex(Index) ? Clips[Index].Get() : nullptr;
	}

	/** One cycle set (walk or jog) at a normalised phase, blended between two directions. */
	bool SampleCycle(const TArray<TObjectPtr<UAnimSequenceBase>>& Clips, double Phase, double DirectionDeg, FPoseContext& Out)
	{
		EDir A, B;
		float T;
		Neighbours(DirectionDeg, A, B, T);
		UAnimSequenceBase* ClipA = At(Clips, A) ? At(Clips, A) : At(Clips, Forward);
		UAnimSequenceBase* ClipB = At(Clips, B) ? At(Clips, B) : At(Clips, Forward);
		if (!ClipA || !Sample(ClipA, Phase * ClipA->GetPlayLength(), true, Out))
		{
			return false;
		}
		if (ClipB != ClipA && T > 0.0f)
		{
			FPoseContext Other(Out);
			if (Sample(ClipB, Phase * ClipB->GetPlayLength(), true, Other))
			{
				BlendInto(Out, Other, T);
			}
		}
		return true;
	}
}

void FAnimNode_PadelPlayerPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	CyclePhase = 0.0;
	IdleTime = 0.0;
	Matcher.Reset();
	LocomotionWarp.Reset();
}

void FAnimNode_PadelPlayerPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	using namespace PadelPlayerPoseDetail;
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	const int32 RootMesh = Ref.FindBoneIndex(LayerRoot);

	UpperBodyMask.SetNumZeroed(Bones.GetCompactPoseNumBones());
	for (int32 Compact = 0; Compact < UpperBodyMask.Num(); ++Compact)
	{
		int32 Mesh = Bones.MakeMeshPoseIndex(FCompactPoseBoneIndex(Compact)).GetInt();
		while (Mesh != INDEX_NONE && Mesh != RootMesh)
		{
			Mesh = Ref.GetParentIndex(Mesh);
		}
		UpperBodyMask[Compact] = (RootMesh != INDEX_NONE && Mesh == RootMesh) ? 1.0f : 0.0f;
	}

	WarpBones.Cache(Bones, bLeftHanded);
	BodyBones.Cache(Bones);
}

void FAnimNode_PadelPlayerPose::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const UPadelPlayerAnimInstance* Anim = Cast<UPadelPlayerAnimInstance>(InAnimInstance);
	if (!Anim)
	{
		return;
	}
	Set = Anim->LocomotionSet;
	SpeedMps = Anim->Speed / 100.0;
	DirectionDeg = Anim->Direction;
	bReadyStance = Anim->Locomotion == EPadelLocomotionState::Ready || Anim->Locomotion == EPadelLocomotionState::SplitStep;
	CrouchAlpha = Anim->CrouchAlpha * Anim->StanceKeep();
	JogStridePlayRate = Anim->StridePlayRate;
	Swing = Anim->SwingSequence;
	SwingTime = Anim->SwingTime;
	PrevSwing = Anim->PrevSwingSequence;
	PrevSwingTime = Anim->PrevSwingTime;
	SwingClipAlpha = Anim->SwingClipAlpha;
	SwingWeight = Anim->SwingWeight;
	FullBodyAlpha = Anim->FullBodyAlpha;
	Celebration = Anim->CelebrationSequence;
	CelebrationTime = Anim->CelebrationTime;
	CelebrationWeight = Anim->CelebrationWeight;

	// No swing clip: the arm reaches for the placeholder racket arc instead (0005's racket
	// target meets the ball on the contact tick); held a little in the ready stance.
	ArmAlpha = Anim->WarpArmAlpha();
	BodyAlpha = Anim->WarpBodyAlpha();
	bWarp = Anim->bWarpThisBody;
	FaceInHand = Anim->RacketFaceInHand;
	WarpTuning = Anim->WarpTuning;
	bLeftHanded = Anim->bLeftHanded;
	Trajectory = Anim->MotionTrajectory;
	Matcher.Configure(Set.MotionMatchingDatabases, Set.MotionMatchingRoles, Set.MotionMatching);
	if (Matcher.HasPose())
	{
		// M1: over mocap legs the procedural crouch, feet widen and pelvis drops fade with speed
		// (the clips bend their own knees and plant their own feet); the head look stays, the
		// trunk lean lightens (the clips lean too).
		const double From = Set.MocapLegFadeFromMps;
		// A smooth step, so the layers fade in and out with no kink in their speed (M2: a kink reads as a jolt).
		const double Linear = FMath::Clamp((SpeedMps - From) / FMath::Max(Set.MocapLegFadeToMps - From, 0.01), 0.0, 1.0);
		const double Moving = Linear * Linear * (3.0 - 2.0 * Linear);
		Body = Anim->PosedBody(static_cast<float>(FMath::Lerp(1.0, Set.MocapLegKeep, Moving)), static_cast<float>(FMath::Lerp(1.0, Set.MocapLeanKeep, Moving)));
		// M2: the clips own the trunk at speed (they turn and lean it themselves; a procedural twist
		// on top reads as a loose spine). The head keeps looking where it did, the chest's share
		// handed to the neck, within a cap while running.
		const double ChestKeep = FMath::Lerp(1.0, Set.MocapChestTwistKeep, Moving);
		const double Handed = Body.ChestTwistDeg * (1.0 - ChestKeep);
		Body.ChestTwistDeg = static_cast<float>(Body.ChestTwistDeg * ChestKeep);
		// A soft cap (tanh): the head slows into it rather than stopping dead.
		const double HeadCap = FMath::Max(FMath::Lerp(180.0, Set.MocapHeadMaxTwistDeg, Moving), 1.0);
		const double Head = Body.HeadTwistDeg + Handed;
		Body.HeadTwistDeg = static_cast<float>(HeadCap * std::tanh(Head / HeadCap));
	}
	else
	{
		Body = Anim->PosedBody();
	}
	if (const USkeletalMeshComponent* Mesh = Anim->GetSkelMeshComponent())
	{
		RacketTargetComponent = Mesh->GetComponentTransform().InverseTransformPosition(Anim->RacketTargetWorld);
		TravelVelocityComponent = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Anim->GetTravelVelocity());
	}
}

void FAnimNode_PadelPlayerPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	const double Dt = Context.GetDeltaTime();
	UpdateDeltaS = Dt;
	IdleTime += Dt;
	Matcher.Update(Context, Trajectory, SpeedMps * 100.0);
	if (SpeedMps <= 0.0)
	{
		return;
	}
	// One phase for every cycle: the rate of whichever cycle dominates at this speed.
	const double JogAlpha = FMath::Clamp((SpeedMps - Set.WalkClipSpeedMps) / FMath::Max(Set.JogClipSpeedMps - Set.WalkClipSpeedMps, 0.01), 0.0, 1.0);
	const double WalkRate = FMath::Clamp(SpeedMps / FMath::Max(Set.WalkClipSpeedMps, 0.01), Set.StrideRateMin, Set.StrideRateMax);
	const double Rate = FMath::Lerp(WalkRate, JogStridePlayRate, JogAlpha);
	const UAnimSequenceBase* Clip = PadelPlayerPoseDetail::At(JogAlpha > 0.5 ? Set.Jog : Set.Walk, PadelPlayerPoseDetail::Forward);
	const double Length = Clip ? FMath::Max(Clip->GetPlayLength(), 0.1) : 1.0;
	CyclePhase = FMath::Fmod(CyclePhase + Dt * Rate / Length, 1.0);
}

bool FAnimNode_PadelPlayerPose::SampleLocomotion(FPoseContext& Output)
{
	// M1: motion matching once it has a pick; the loop mix until then and without databases.
	return Matcher.Evaluate(Output) || SampleLoopMix(Output);
}

bool FAnimNode_PadelPlayerPose::SampleLoopMix(FPoseContext& Output) const
{
	using namespace PadelPlayerPoseDetail;
	if (!Sample(Set.Idle, FMath::Fmod(IdleTime, FMath::Max(Set.Idle ? Set.Idle->GetPlayLength() : 1.0, 0.1)), true, Output))
	{
		return false;
	}
	// N1 ready-stance depth: the upright idle between points; in a rally, the crouched ready
	// idle mixed in by CrouchAlpha (lower at the back glass). The C0 crouch alone was a squat.
	const float Crouch = static_cast<float>(FMath::Clamp(CrouchAlpha, 0.0, 1.0));
	if (bReadyStance && Set.Ready && Crouch > 0.0f)
	{
		FPoseContext Ready(Output);
		if (Sample(Set.Ready, FMath::Fmod(IdleTime, FMath::Max(Set.Ready->GetPlayLength(), 0.1)), true, Ready))
		{
			BlendInto(Output, Ready, Crouch);
		}
	}
	// Idle up to half the walk threshold, fully moving at the threshold.
	const float MoveAlpha = static_cast<float>(FMath::Clamp((SpeedMps - 0.5 * Set.WalkMinMps) / FMath::Max(0.5 * Set.WalkMinMps, 0.01), 0.0, 1.0));
	if (MoveAlpha <= 0.0f)
	{
		return true;
	}
	FPoseContext Moving(Output);
	if (!SampleCycle(Set.Walk, CyclePhase, DirectionDeg, Moving))
	{
		return true;
	}
	// The crouched walk (low shuffle) mixed with the upright one by the same depth; both sets
	// move at WalkClipSpeedMps, so they share the phase without the feet sliding.
	if (Set.WalkUpright.Num() == 4 && Crouch < 1.0f)
	{
		FPoseContext Upright(Output);
		if (SampleCycle(Set.WalkUpright, CyclePhase, DirectionDeg, Upright))
		{
			BlendInto(Moving, Upright, 1.0f - Crouch);
		}
	}
	const float JogAlpha = static_cast<float>(FMath::Clamp((SpeedMps - Set.WalkClipSpeedMps) / FMath::Max(Set.JogClipSpeedMps - Set.WalkClipSpeedMps, 0.01), 0.0, 1.0));
	if (JogAlpha > 0.0f)
	{
		FPoseContext Jog(Output);
		if (SampleCycle(Set.Jog, CyclePhase, DirectionDeg, Jog))
		{
			BlendInto(Moving, Jog, JogAlpha);
		}
	}
	BlendInto(Output, Moving, MoveAlpha);
	return true;
}

void FAnimNode_PadelPlayerPose::Evaluate_AnyThread(FPoseContext& Output)
{
	using namespace PadelPlayerPoseDetail;
	bMotionMatched = Matcher.HasPose();
	if (!SampleLocomotion(Output))
	{
		Output.ResetToRefPose();
	}
	Matcher.RecordHistory(Output);
	// M2: the warps fit the clip's legs to the simulation (after the history: the search compares clip poses).
	if (bMotionMatched && Set.LocomotionWarp.Any())
	{
		FPadelLocomotionWarpInput Warp;
		Warp.ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
		Warp.SimVelocity = TravelVelocityComponent;
		Warp.ClipRootVelocity = Matcher.ClipRootVelocity();
		Warp.DeltaS = UpdateDeltaS;
		Warp.Weight = (1.0 - FMath::Clamp(SwingWeight * FullBodyAlpha, 0.0, 1.0)) * (1.0 - FMath::Clamp(CelebrationWeight, 0.0, 1.0));
		LocomotionWarp.Apply(Output.Pose, BodyBones, Warp, Set.LocomotionWarp);
	}
	else
	{
		LocomotionWarp.Reset();
	}

	if (Swing && SwingWeight > 0.0)
	{
		FPoseContext SwingPose(Output);
		if (Sample(Swing, SwingTime, false, SwingPose))
		{
			if (PrevSwing && SwingClipAlpha < 1.0)
			{
				FPoseContext Prev(Output);
				if (Sample(PrevSwing, PrevSwingTime, false, Prev))
				{
					BlendInto(Prev, SwingPose, static_cast<float>(SwingClipAlpha));
					SwingPose.Pose.CopyBonesFrom(Prev.Pose);
				}
			}
			// The upper body always takes the swing; the legs only as far as FullBodyAlpha.
			TArray<float> Mask = UpperBodyMask;
			for (float& W : Mask)
			{
				W = FMath::Max(W, static_cast<float>(FullBodyAlpha));
			}
			BlendInto(Output, SwingPose, static_cast<float>(SwingWeight), &Mask);
		}
	}

	if (Celebration && CelebrationWeight > 0.0)
	{
		FPoseContext Cheer(Output);
		if (Sample(Celebration, CelebrationTime, false, Cheer))
		{
			BlendInto(Output, Cheer, static_cast<float>(CelebrationWeight));
		}
	}

	// On this pose whether or not a MetaHuman shows it: the MetaHuman copies it in the retarget.
	PadelBodyDynamics::Apply(Output.Pose, BodyBones, Body);

	ApplyArmReach(Output);
}

void FAnimNode_PadelPlayerPose::ApplyArmReach(FPoseContext& Output) const
{
	if (!bWarp)
	{
		return;
	}
	if (WarpBones.bLeftHanded != bLeftHanded)
	{
		WarpBones.Cache(Output.AnimInstanceProxy->GetRequiredBones(), bLeftHanded);
	}
	FPadelWarpInput In;
	In.TargetCS = RacketTargetComponent;
	In.FaceInHand = FaceInHand;
	In.ArmAlpha = static_cast<float>(ArmAlpha);
	In.BodyAlpha = static_cast<float>(BodyAlpha);
	PadelContactWarp::Apply(Output.Pose, WarpBones, In, WarpTuning);
}
