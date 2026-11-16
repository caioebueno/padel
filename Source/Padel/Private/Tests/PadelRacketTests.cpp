// docs/decisions/0008-racket.md: in a match, every player with a skeletal body holds the
// racket in the hand. Caught on the Mac: ShowController moved the racket 55 cm off the
// hand bone on the first frame, so no player showed a racket.
#include "Misc/AutomationTest.h"

#include "Animation/PadelPlayerAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Misc/ScopeExit.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelRacketInHandTest, "Padel.Animation.RacketInHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelRacketInHandTest::RunTest(const FString& Parameters)
{
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
	// A few frames, so ShowController and Present have run (the first one turns the pawns).
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		Director->Tick(1.0f / 60.0f);
	}

	int32 Checked = 0;
	for (int32 Id = 0; Id < Padel::NumPlayers; ++Id)
	{
		APadelPlayerPawn* Pawn = Director->GetPawn(Id);
		if (!TestNotNull(TEXT("Pawn"), Pawn) || !Pawn->GetAnimation()->HasSkeletalBody())
		{
			continue;
		}
		UStaticMeshComponent* Racket = nullptr;
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Pawn);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() == FName(TEXT("Racket")))
			{
				Racket = Mesh;
			}
		}
		if (!TestNotNull(TEXT("Racket component"), Racket))
		{
			continue;
		}
		USkeletalMeshComponent* Hand = Cast<USkeletalMeshComponent>(Racket->GetAttachParent());
		if (!TestNotNull(FString::Printf(TEXT("Pawn %d: racket attached to a skeletal body"), Id), Hand))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("Pawn %d: racket visible"), Id), Racket->IsVisible());
		Hand->RefreshBoneTransforms();
		const FVector HandAt = Hand->GetSocketLocation(Racket->GetAttachSocketName());
		const double Distance = FVector::Dist(HandAt, Racket->GetComponentLocation());
		// The grip centre sits a few centimetres from the wrist (Data/Racket.json); 55 cm was the bug.
		TestTrue(FString::Printf(TEXT("Pawn %d: grip %.1f cm from the hand bone"), Id, Distance), Distance < 20.0);
		if (const UStaticMesh* Mesh = Racket->GetStaticMesh(); Mesh && Mesh->GetName() == TEXT("SM_Racket"))
		{
			TestTrue(FString::Printf(TEXT("Pawn %d: SM_Racket at its built size"), Id), Racket->GetComponentScale().Equals(FVector::OneVector, 0.05));
		}
		++Checked;
	}
	if (Checked == 0)
	{
		AddInfo(TEXT("No skeletal bodies on this machine (placeholder meshes or clips not imported); skipped."));
	}
	Director->CloseMatch();
	return true;
}

#endif
