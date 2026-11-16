#include "Animation/PadelPlayerPoseNode.h"

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

namespace PadelPlayerPoseDetail
{
	// Bone names of the UE5 mannequin layout the placeholders are rigged on (Tools/AssetPipeline/README.md).
	const FName LayerRoot(TEXT("spine_01"));
	const FName UpperArmBone(TEXT("upperarm_r"));
	const FName LowerArmBone(TEXT("lowerarm_r"));
	const FName HandBone(TEXT("hand_r"));

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

	auto Find = [&Bones, &Ref](FName Name)
	{
		const int32 Mesh = Ref.FindBoneIndex(Name);
		return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
	};
	UpperArm = Find(UpperArmBone);
	LowerArm = Find(LowerArmBone);
	Hand = Find(HandBone);
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
	const bool bReady = Anim->Locomotion == EPadelLocomotionState::Ready || Anim->Locomotion == EPadelLocomotionState::SplitStep;
	ArmAlpha = Swing ? 0.0 : FMath::Clamp(FMath::Max(Anim->RawSwingWeight, bReady ? Set.ArmReadyAlpha : 0.0), 0.0, 1.0);
	if (const USkeletalMeshComponent* Mesh = Anim->GetSkelMeshComponent())
	{
		RacketTargetComponent = Mesh->GetComponentTransform().InverseTransformPosition(Anim->RacketTargetWorld);
	}
}

void FAnimNode_PadelPlayerPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	const double Dt = Context.GetDeltaTime();
	IdleTime += Dt;
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

bool FAnimNode_PadelPlayerPose::SampleLocomotion(FPoseContext& Output) const
{
	using namespace PadelPlayerPoseDetail;
	if (!Sample(Set.Idle, FMath::Fmod(IdleTime, FMath::Max(Set.Idle ? Set.Idle->GetPlayLength() : 1.0, 0.1)), true, Output))
	{
		return false;
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
	if (!SampleLocomotion(Output))
	{
		Output.ResetToRefPose();
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

	ApplyArmReach(Output);
}

void FAnimNode_PadelPlayerPose::ApplyArmReach(FPoseContext& Output) const
{
	if (ArmAlpha <= 0.0 || UpperArm.GetInt() == INDEX_NONE || LowerArm.GetInt() == INDEX_NONE || Hand.GetInt() == INDEX_NONE)
	{
		return;
	}
	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	FTransform Upper = CS.GetComponentSpaceTransform(UpperArm);
	FTransform Lower = CS.GetComponentSpaceTransform(LowerArm);
	FTransform HandCS = CS.GetComponentSpaceTransform(Hand);

	const FVector Effector = FMath::Lerp(HandCS.GetLocation(), RacketTargetComponent, static_cast<float>(ArmAlpha));
	// Keep the elbow bending the way the current pose bends it.
	const FVector Mid = (Upper.GetLocation() + HandCS.GetLocation()) * 0.5;
	const FVector Bend = (Lower.GetLocation() - Mid).GetSafeNormal();
	const FVector JointTarget = Lower.GetLocation() + Bend * 20.0;
	AnimationCore::SolveTwoBoneIK(Upper, Lower, HandCS, JointTarget, Effector, false, 1.0, 1.0);

	TArray<FBoneTransform> Solved;
	Solved.Add(FBoneTransform(UpperArm, Upper));
	Solved.Add(FBoneTransform(LowerArm, Lower));
	Solved.Add(FBoneTransform(Hand, HandCS));
	CS.LocalBlendCSBoneTransforms(Solved, 1.0f);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Output.Pose);
}
