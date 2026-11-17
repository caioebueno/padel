// The MetaHuman's outfit is skinned mostly to the pelvis and its helper joints. When the
// retargeter's Root Motion op ran, the pelvis joint landed on the root (the floor) while
// spine_01 carried the hips' height, so the body still looked right but the shirt and shorts
// were dragged down into a flap between the knees. The pelvis must stay at hip height and
// spine_01 at its own short offset above it.
#include "Misc/AutomationTest.h"

#include "Animation/PadelAnimData.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Misc/ScopeExit.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetOps.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMetaHumanPelvisTest, "Padel.Animation.MetaHumanPelvis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMetaHumanPelvisTest::RunTest(const FString& Parameters)
{
	const FPadelAnimData Data = FPadelAnimData::Load();
	UIKRetargeter* Retargeter = Data.bOk ? LoadObject<UIKRetargeter>(nullptr, *Data.MetaHumanRetargeter, nullptr, LOAD_Quiet | LOAD_NoWarn) : nullptr;
	if (Retargeter)
	{
		const FIKRetargetOpBase* RootMotion = Retargeter->GetRetargetOpByName(FName(TEXT("Root Motion")));
		TestTrue(TEXT("The MetaHuman retargeter runs no Root Motion op (it puts the pelvis on the floor)"), !RootMotion || !RootMotion->IsEnabled());
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	APadelMatchDirector* Director = World->SpawnActor<APadelMatchDirector>();
	Padel::Ui::FMatchSetup Setup;
	Setup.SetsToWin = 1;
	Setup.HumanPlayerId = -1;
	if (!TestNotNull(TEXT("Director"), Director) || !TestTrue(TEXT("Match started"), Director->StartMatch(Setup)))
	{
		return false;
	}
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		Director->Tick(1.0f / 60.0f);
	}

	int32 MetaHumans = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		AActor* MetaHuman = Pawn ? Pawn->GetAnimation()->GetMetaHuman() : nullptr;
		USkeletalMeshComponent* Driver = Pawn ? Pawn->GetAnimation()->GetDriverBody() : nullptr;
		USkeletalMeshComponent* Shown = nullptr;
		if (MetaHuman)
		{
			TInlineComponentArray<USkeletalMeshComponent*> Parts(MetaHuman);
			for (USkeletalMeshComponent* Part : Parts)
			{
				if (Part->GetAnimInstance() && Part->GetAnimInstance()->GetClass()->GetName() == TEXT("PadelMetaHumanAnimInstance"))
				{
					Shown = Part;
				}
			}
		}
		if (!Driver || !Shown)
		{
			continue;
		}
		++MetaHumans;
		// The MetaHuman ticks only when rendered: evaluate it here as the retarget would on screen.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Driver->TickAnimation(1.0f / 60.0f, false);
			Driver->RefreshBoneTransforms();
			Shown->TickAnimation(1.0f / 60.0f, false);
			Shown->RefreshBoneTransforms();
		}
		const FReferenceSkeleton& Ref = Shown->GetSkeletalMeshAsset()->GetRefSkeleton();
		const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
		const int32 Spine = Ref.FindBoneIndex(TEXT("spine_01"));
		if (!TestTrue(FString::Printf(TEXT("Pawn %d: MetaHuman has pelvis and spine_01"), Id), Pelvis != INDEX_NONE && Spine != INDEX_NONE))
		{
			continue;
		}
		const FReferenceSkeleton& DriverRef = Driver->GetSkeletalMeshAsset()->GetRefSkeleton();
		const int32 DriverPelvis = DriverRef.FindBoneIndex(TEXT("pelvis"));
		if (!TestTrue(FString::Printf(TEXT("Pawn %d: placeholder has a pelvis"), Id), DriverPelvis != INDEX_NONE))
		{
			continue;
		}
		// The pelvis follows the placeholder's, scaled by the two hip heights at rest: the bug put it at 0.
		const double RefHeight = Ref.GetRefBonePose()[Pelvis].GetLocation().Z;
		const double DriverRefHeight = DriverRef.GetRefBonePose()[DriverPelvis].GetLocation().Z;
		const double Expected = Driver->GetBoneTransform(DriverPelvis, FTransform::Identity).GetLocation().Z * RefHeight / DriverRefHeight;
		const double Height = Shown->GetBoneTransform(Pelvis, FTransform::Identity).GetLocation().Z;
		const double SpineOffset = Shown->GetBoneSpaceTransforms()[Spine].GetLocation().Size();
		const double RefSpineOffset = Ref.GetRefBonePose()[Spine].GetLocation().Size();
		AddInfo(FString::Printf(TEXT("Pawn %d: pelvis %.1f cm up (placeholder's scaled %.1f, rest %.1f), spine_01 %.1f cm from it (rest %.1f)"), Id, Height, Expected, RefHeight, SpineOffset, RefSpineOffset));
		TestTrue(FString::Printf(TEXT("Pawn %d: pelvis at the placeholder's hip height (%.1f cm, expected %.1f)"), Id, Height, Expected),
			Expected > 10.0 && FMath::Abs(Height - Expected) < Expected * 0.25);
		TestTrue(FString::Printf(TEXT("Pawn %d: spine_01 keeps its offset from the pelvis (%.1f cm, rest %.1f)"), Id, SpineOffset, RefSpineOffset),
			FMath::Abs(SpineOffset - RefSpineOffset) < 2.0);
	}
	if (MetaHumans == 0)
	{
		AddInfo(TEXT("No MetaHuman assembled on this machine: pelvis checks skipped."));
	}
	Director->CloseMatch();
	return true;
}

#endif
