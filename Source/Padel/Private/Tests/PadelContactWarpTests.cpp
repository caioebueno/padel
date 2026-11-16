// Movement plan N3 (shots doc 4.2): at contact the racket's face centre meets the ball,
// on the body actually on screen (the placeholder, or a MetaHuman after the retarget).
// The match's own AI places the targets; here they're set directly, across a band of
// heights and both sides, so the check doesn't depend on what the rally happens to do.
#include "Misc/AutomationTest.h"

#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/PadelContactWarp.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Gameplay/PadelShotUnits.h"
#include "Misc/ScopeExit.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

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

	UStaticMeshComponent* FindRacket(APadelPlayerPawn* Pawn)
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

	void Evaluate(USkeletalMeshComponent* Driver, USkeletalMeshComponent* Shown, UStaticMeshComponent* Racket)
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

	/**
	 * Puts each target in front of Pawn in turn and returns the worst miss (cm) of the racket's
	 * face centre. bClip: drive it through ApplyFrame with a clip that has contact fields (the
	 * body takes part of the reach); otherwise the arm alone, as for zero-field clips.
	 */
	double WorstMiss(FAutomationTestBase& Test, APadelPlayerPawn* Pawn, const TArray<FTarget>& Targets, bool bClip, FString& Report)
	{
		UStaticMeshComponent* Racket = FindRacket(Pawn);
		USkeletalMeshComponent* Driver = Pawn->GetAnimation()->GetDriverBody();
		USkeletalMeshComponent* Shown = Racket ? Cast<USkeletalMeshComponent>(Racket->GetAttachParent()) : nullptr;
		UPadelPlayerAnimInstance* Anim = Driver ? Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance()) : nullptr;
		if (!Racket || !Shown || !Anim || !Racket->GetStaticMesh() || Racket->GetStaticMesh()->GetName() != TEXT("SM_Racket"))
		{
			return -1.0;
		}
		const FTransform Frame = Driver->GetComponentTransform();
		const FVector Forward = Frame.TransformVectorNoScale(FVector(0.0, 1.0, 0.0)).GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		const FVector Ground = Frame.GetLocation();

		Padel::Animation::FAnimClip Clip;
		Clip.Name = "contact_warp_test";
		Clip.WindupS = 0.4;
		Clip.ContactS = 0.6;
		Clip.EndS = 1.2;
		// Non-zero contact fields: this clip's own sweet spot at contact, so the body warps.
		Clip.Contact = { 0.6, 0.4, 1.0 };

		double Worst = 0.0;
		for (const FTarget& T : Targets)
		{
			const FVector Target = Ground + Right * (T.RightM * 100.0) + Forward * (T.ForwardM * 100.0) + FVector(0.0, 0.0, T.UpM * 100.0);
			if (bClip)
			{
				Padel::Animation::FPlayerAnimFrame F;
				F.Locomotion = Padel::Animation::ELocomotionState::Idle;
				F.Clip = &Clip;
				F.SwingWeight = 1.0;
				F.SwingProgress = 2.0; // the contact frame
				F.bContactFrame = true;
				F.RacketPosition = Padel::ToMetres(Target);
				Anim->ApplyFrame(F, nullptr, nullptr, nullptr, {});
				Test.TestTrue(TEXT("Clip with contact fields gives the body a share"), Anim->ContactWarpWeight > 0.99f);
			}
			else
			{
				Anim->RacketTargetWorld = Target;
				Anim->RawSwingWeight = 1.0f;
				Anim->ContactWarpWeight = 0.0f;
				Anim->SwingSequence = nullptr;
			}
			Evaluate(Driver, Shown, Racket);
			const FVector Face = Racket->GetComponentTransform().TransformPosition(FVector(0.0, 0.0, FaceCentreUnits));
			const double Miss = FVector::Dist(Face, Target);
			Report += FString::Printf(TEXT(" [%.2f,%.2f,%.2f m: %.1f cm]"), T.RightM, T.ForwardM, T.UpM, Miss);
			Worst = FMath::Max(Worst, Miss);
		}
		return Worst;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelContactWarpTest, "Padel.Animation.ContactWarp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelContactWarpTest::RunTest(const FString& Parameters)
{
	using namespace PadelContactWarpTests;
	TestEqual(TEXT("Body weight 0 in the backswing"), PadelContactWarp::BodyWeight(0.8), 0.0f);
	TestEqual(TEXT("Body weight peaks at contact"), PadelContactWarp::BodyWeight(2.0), 1.0f);
	TestTrue(TEXT("Body weight rises over the forward swing"), PadelContactWarp::BodyWeight(1.5) > 0.4f && PadelContactWarp::BodyWeight(1.5) < 0.6f);
	TestEqual(TEXT("Body weight 0 after the follow-through"), PadelContactWarp::BodyWeight(3.0), 0.0f);

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
	for (int32 Tick = 0; Tick < 10; ++Tick)
	{
		Director->Tick(1.0f / 60.0f);
	}

	// The arm alone (clips with zero contact fields): where a standing player reaches.
	const TArray<FTarget> ArmBand = {
		{ 0.55, 0.35, 0.9 }, { 0.55, 0.35, 1.2 }, { 0.55, 0.35, 1.5 }, { 0.45, 0.30, 1.8 },
		{ -0.45, 0.35, 1.0 }, { -0.45, 0.35, 1.3 },
	};
	// With the body (clips with contact fields): low balls need the knees and the spine.
	const TArray<FTarget> BodyBand = {
		{ 0.6, 0.4, 0.4 }, { 0.6, 0.4, 0.6 }, { 0.6, 0.4, 0.9 }, { 0.6, 0.4, 1.3 }, { 0.5, 0.3, 1.8 },
		{ 0.85, 0.3, 1.0 }, { -0.5, 0.4, 0.5 }, { -0.5, 0.4, 1.0 },
	};

	int32 Checked = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		if (!Pawn || !Pawn->GetAnimation()->HasSkeletalBody())
		{
			continue;
		}
		const TCHAR* BodyName = Pawn->GetAnimation()->GetMetaHuman() ? TEXT("MetaHuman") : TEXT("placeholder");
		FString Report;
		const double ArmMiss = WorstMiss(*this, Pawn, ArmBand, false, Report);
		if (ArmMiss < 0.0)
		{
			continue;
		}
		AddInfo(FString::Printf(TEXT("Pawn %d (%s), arm only:%s"), Id, BodyName, *Report));
		TestTrue(FString::Printf(TEXT("Pawn %d (%s): arm-only face within %.0f cm (worst %.1f)"), Id, BodyName, ToleranceUnits, ArmMiss), ArmMiss <= ToleranceUnits);
		Report.Reset();
		const double BodyMiss = WorstMiss(*this, Pawn, BodyBand, true, Report);
		AddInfo(FString::Printf(TEXT("Pawn %d (%s), with the body:%s"), Id, BodyName, *Report));
		TestTrue(FString::Printf(TEXT("Pawn %d (%s): warped face within %.0f cm (worst %.1f)"), Id, BodyName, ToleranceUnits, BodyMiss), BodyMiss <= ToleranceUnits);
		++Checked;
	}
	if (Checked == 0)
	{
		AddInfo(TEXT("No skeletal bodies with SM_Racket on this machine; skipped."));
	}
	Director->CloseMatch();
	return true;
}

#endif
