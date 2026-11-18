// Shared by the contact warp tests (PadelContactWarpTests.cpp) and the MetaHuman arm
// continuity test: find a pawn's racket, pose its arm at a target, evaluate both bodies.
#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/PadelPlayerPawn.h"

namespace PadelContactWarpTests
{
	constexpr double ToleranceUnits = 5.0; // done-when: within 5 cm of the ball
	constexpr double FaceCentreUnits = 27.0; // Data/Racket.json FaceCentreM

	struct FTarget
	{
		double RightM;
		double ForwardM;
		double UpM;
	};

	inline UStaticMeshComponent* FindRacket(APadelPlayerPawn* Pawn)
	{
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Pawn);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() == FName(TEXT("Racket")))
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	inline void Evaluate(USkeletalMeshComponent* Driver, USkeletalMeshComponent* Shown, UStaticMeshComponent* Racket)
	{
		// Twice: the first pass settles the IK rigs and the retarget processor.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Driver->TickAnimation(0.0f, false);
			Driver->RefreshBoneTransforms();
			if (Shown != Driver)
			{
				Shown->TickAnimation(0.0f, false);
				Shown->RefreshBoneTransforms();
			}
		}
		Racket->UpdateComponentToWorld();
	}


	/** Sets the driver's racket target directly (arm only, like a clip with zero contact fields) and evaluates. */
	inline void PoseAt(APadelPlayerPawn* Pawn, const FVector& Target)
	{
		UStaticMeshComponent* Racket = FindRacket(Pawn);
		USkeletalMeshComponent* Driver = Pawn->GetAnimation()->GetDriverBody();
		USkeletalMeshComponent* Shown = Cast<USkeletalMeshComponent>(Racket->GetAttachParent());
		UPadelPlayerAnimInstance* Anim = Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance());
		Anim->RacketTargetWorld = Target;
		Anim->RawSwingWeight = 1.0f;
		Anim->ContactWarpWeight = 0.0f;
		Anim->SwingSequence = nullptr;
		Evaluate(Driver, Shown, Racket);
	}

	/** Target at (right, forward, up) metres in front of Pawn, sides on the racket hand's side. */
	inline FVector Around(APadelPlayerPawn* Pawn, double RightM, double ForwardM, double UpM)
	{
		const FTransform Frame = Pawn->GetAnimation()->GetDriverBody()->GetComponentTransform();
		const FVector Forward = Frame.TransformVectorNoScale(FVector(0.0, 1.0, 0.0)).GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward) * (Pawn->GetAnimation()->IsLeftHanded() ? -1.0 : 1.0);
		return Frame.GetLocation() + Right * (RightM * 100.0) + Forward * (ForwardM * 100.0) + FVector(0.0, 0.0, UpM * 100.0);
	}

	inline FName Side(APadelPlayerPawn* Pawn, const TCHAR* Bone)
	{
		return FName(FString(Bone) + (Pawn->GetAnimation()->IsLeftHanded() ? TEXT("_l") : TEXT("_r")));
	}

	/** The topmost spine bone the shown body has (spine_03 on the placeholder, spine_05 on a MetaHuman). */
	inline FName TopSpine(const USkeletalMeshComponent* Body)
	{
		for (const TCHAR* Name : { TEXT("spine_05"), TEXT("spine_04"), TEXT("spine_03") })
		{
			if (Body->GetBoneIndex(FName(Name)) != INDEX_NONE)
			{
				return FName(Name);
			}
		}
		return FName(TEXT("spine_02"));
	}

}

#endif
