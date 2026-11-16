// APadelBall inside the engine: it shows a 65 mm ball (the imported SM_Ball, or the
// engine sphere until the pipeline has run), follows Present() in world units, hides,
// and Data/Ball.json loads. The spin and trail maths are tested natively
// (Tests/Native/UI/BallVisualTests.cpp).
#include "Misc/AutomationTest.h"

#include "BallState.h"
#include "Components/StaticMeshComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Presentation/PadelBall.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelBallActorTest, "Padel.Presentation.Ball",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelBallActorTest::RunTest(const FString& Parameters)
{
	const Padel::Presentation::FBallVisualSettings Loaded = APadelBall::LoadSettings();
	TestEqual(TEXT("Data/Ball.json loads"), Loaded.TrailSeconds, Padel::Presentation::FBallVisualSettings().TrailSeconds);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);

	APadelBall* Ball = World->SpawnActor<APadelBall>();
	if (TestNotNull(TEXT("Ball spawned"), Ball))
	{
		UStaticMeshComponent* Mesh = Ball->GetBallMesh();
		TestNotNull(TEXT("It has a mesh"), Mesh->GetStaticMesh().Get());
		TestFalse(TEXT("Hidden until presented"), Mesh->IsVisible());

		// GDD 2: 65 mm across, imported or not.
		const double ExpectedCm = Padel::MetresToWorld(FVector(Padel::BallRadius * 2.0)).X;
		const FBox Box = Mesh->GetStaticMesh()->GetBoundingBox();
		const FVector Size = Box.GetSize() * Mesh->GetRelativeScale3D();
		TestEqual(TEXT("65 mm across (X)"), Size.X, ExpectedCm, 0.2);
		TestEqual(TEXT("65 mm across (Z)"), Size.Z, ExpectedCm, 0.2);
		if (!Ball->IsShowingImportedBall())
		{
			AddInfo(TEXT("SM_Ball isn't imported here: checked the engine-sphere fallback."));
		}

		const Padel::FVec3 At{ 3.0, -1.5, 1.2 };
		Ball->Present(At, { -20.0, 0.0, 2.0 }, { 0.0, 40.0, 0.0 }, true);
		TestTrue(TEXT("Visible once presented"), Mesh->IsVisible());
		TestEqual(TEXT("At the simulation's position, in cm"), Ball->GetActorLocation(),
			Padel::MetresToWorld(FVector(At.X, At.Y, At.Z)), 1e-3);

		Ball->Present(At, {}, {}, false);
		TestFalse(TEXT("Hidden when the ball isn't live"), Mesh->IsVisible());
		TestFalse(TEXT("No trail while hidden"), Ball->GetTrailMesh()->IsVisible());
		Ball->Destroy();
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
