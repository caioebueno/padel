#include "Animation/PadelMetaHumanAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Retargeter/IKRetargeter.h"

FPadelMetaHumanAnimInstanceProxy::FPadelMetaHumanAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode)
	: FAnimInstanceProxy(InAnimInstance)
	, RetargetNode(InRetargetNode)
{
}

void UPadelMetaHumanAnimInstance::Configure(USkeletalMeshComponent* Source, UIKRetargeter* Retargeter, int32 IkLodThreshold)
{
	RetargetNode.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
	RetargetNode.SourceMeshComponent = Source;
	RetargetNode.IKRetargeterAsset = Retargeter;
	RetargetNode.LODThresholdForIK = IkLodThreshold;
	if (FIKRetargetProcessor* Processor = RetargetNode.GetRetargetProcessor())
	{
		Processor->SetNeedsInitialized();
	}
}

FAnimInstanceProxy* UPadelMetaHumanAnimInstance::CreateAnimInstanceProxy()
{
	return new FPadelMetaHumanAnimInstanceProxy(this, &RetargetNode);
}
