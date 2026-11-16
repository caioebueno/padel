#include "Animation/PadelMetaHumanAnimInstance.h"

#include "Animation/PadelPlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Retargeter/IKRetargeter.h"

FPadelMetaHumanAnimInstanceProxy::FPadelMetaHumanAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_PadelMetaHumanWarp* InRootNode)
	: FAnimInstanceProxy(InAnimInstance)
	, RootNode(InRootNode)
{
}

void FAnimNode_PadelMetaHumanWarp::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	if (Retarget)
	{
		Retarget->Initialize_AnyThread(Context);
	}
}

void FAnimNode_PadelMetaHumanWarp::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	if (Retarget)
	{
		Retarget->CacheBones_AnyThread(Context);
	}
	Bones.Cache(Context.AnimInstanceProxy->GetRequiredBones(), bLeftHanded);
}

void FAnimNode_PadelMetaHumanWarp::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	if (Retarget)
	{
		Retarget->Update_AnyThread(Context);
	}
}

void FAnimNode_PadelMetaHumanWarp::PreUpdate(const UAnimInstance* InAnimInstance)
{
	if (Retarget)
	{
		Retarget->PreUpdate(InAnimInstance);
	}
	Request = FPadelWarpInput();
	const USkeletalMeshComponent* Driver = Source.Get();
	const UPadelPlayerAnimInstance* Anim = Driver ? Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance()) : nullptr;
	const USkeletalMeshComponent* Mesh = InAnimInstance ? InAnimInstance->GetSkelMeshComponent() : nullptr;
	if (!Anim || !Mesh)
	{
		return;
	}
	Request.TargetCS = Mesh->GetComponentTransform().InverseTransformPosition(Anim->RacketTargetWorld);
	Request.FaceInHand = FaceInHand;
	Request.ArmAlpha = Anim->WarpArmAlpha();
	Request.BodyAlpha = Anim->WarpBodyAlpha();
}

void FAnimNode_PadelMetaHumanWarp::Evaluate_AnyThread(FPoseContext& Output)
{
	if (Retarget)
	{
		Retarget->Evaluate_AnyThread(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}
	PadelContactWarp::Apply(Output.Pose, Bones, Request);
}

void UPadelMetaHumanAnimInstance::Configure(USkeletalMeshComponent* Source, UIKRetargeter* Retargeter, int32 IkLodThreshold)
{
	RetargetNode.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
	RetargetNode.SourceMeshComponent = Source;
	RetargetNode.IKRetargeterAsset = Retargeter;
	RetargetNode.LODThresholdForIK = IkLodThreshold;
	WarpNode.Retarget = &RetargetNode;
	WarpNode.Source = Source;
	if (FIKRetargetProcessor* Processor = RetargetNode.GetRetargetProcessor())
	{
		Processor->SetNeedsInitialized();
	}
}

void UPadelMetaHumanAnimInstance::SetRacketFace(const FVector& InFaceInHand, bool bInLeftHanded)
{
	WarpNode.FaceInHand = InFaceInHand;
	if (WarpNode.bLeftHanded != bInLeftHanded)
	{
		WarpNode.bLeftHanded = bInLeftHanded;
		RecalcRequiredBones();
	}
}

FAnimInstanceProxy* UPadelMetaHumanAnimInstance::CreateAnimInstanceProxy()
{
	WarpNode.Retarget = &RetargetNode;
	return new FPadelMetaHumanAnimInstanceProxy(this, &WarpNode);
}
