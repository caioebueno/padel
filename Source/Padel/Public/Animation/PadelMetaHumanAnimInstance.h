#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "Animation/PadelContactWarp.h"
#include "PadelMetaHumanAnimInstance.generated.h"

class UIKRetargeter;

/**
 * The MetaHuman's root: the IK retarget, then the contact warp on the MetaHuman's own arm
 * (PadelContactWarp.h), using this body's racket grip. The warp reads its request from the
 * placeholder's UPadelPlayerAnimInstance, which skips its own warp while a MetaHuman shows.
 */
USTRUCT()
struct FAnimNode_PadelMetaHumanWarp : public FAnimNode_Base
{
	GENERATED_BODY()

	FAnimNode_RetargetPoseFromMesh* Retarget = nullptr;
	TWeakObjectPtr<USkeletalMeshComponent> Source;
	FVector FaceInHand = FVector::ZeroVector;
	bool bLeftHanded = false;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

private:
	FPadelWarpBones Bones;
	FPadelWarpInput Request;
};

/** Runs the anim instance's root node (retarget, then contact warp) with no AnimBP graph. */
USTRUCT()
struct FPadelMetaHumanAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FPadelMetaHumanAnimInstanceProxy() = default;
	FPadelMetaHumanAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_PadelMetaHumanWarp* InRootNode);

	virtual FAnimNode_Base* GetCustomRootNode() override { return RootNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override
	{
		OutNodes.Add(RootNode);
		OutNodes.Add(RootNode->Retarget);
	}

	FAnimNode_PadelMetaHumanWarp* RootNode = nullptr;
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

	/** This body's racket grip: the face centre in its racket hand's bone space, cm (PadelContactWarp::FaceInHand). */
	void SetRacketFace(const FVector& InFaceInHand, bool bInLeftHanded);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;

	UPROPERTY(Transient)
	FAnimNode_PadelMetaHumanWarp WarpNode;
};
