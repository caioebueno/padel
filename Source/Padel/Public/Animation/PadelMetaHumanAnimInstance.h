#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "PadelMetaHumanAnimInstance.generated.h"

class UIKRetargeter;

/** Runs the anim instance's one node, the IK retarget, as its root (no AnimBP graph). */
USTRUCT()
struct FPadelMetaHumanAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FPadelMetaHumanAnimInstanceProxy() = default;
	FPadelMetaHumanAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode);

	virtual FAnimNode_Base* GetCustomRootNode() override { return RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RetargetNode); }

	FAnimNode_RetargetPoseFromMesh* RetargetNode = nullptr;
};

/**
 * The anim instance of a MetaHuman's body (docs/decisions/0006-metahuman-players.md).
 * The player's hidden placeholder skeleton still runs ABP_PadelPlayer, so the contact-frame
 * sync of 0005 is untouched; this copies that pose onto the MetaHuman every frame through
 * an IK Retargeter. It is set up in C++ so there is no binary AnimBP to review.
 */
UCLASS(Transient, NotBlueprintable)
class PADEL_API UPadelMetaHumanAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/**
	 * Source must tick before this instance's component (the caller adds the prerequisite).
	 * IkLodThreshold: the last LOD that runs the retargeter's IK pass (-1 = all).
	 */
	void Configure(USkeletalMeshComponent* Source, UIKRetargeter* Retargeter, int32 IkLodThreshold);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;
};
