// APadelMatchDirector in a real world: it loads the data, builds a match from the setup
// screen's choices, attaches UPadelMatchEvents, spawns the four pawns, plays points at
// the fixed tick, and lets go of everything on close. AI vs AI, so no input is needed.
#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Misc/ScopeExit.h"
#include "Presentation/PadelMatchEvents.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMatchDirectorTest, "Padel.Gameplay.MatchDirector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMatchDirectorTest::RunTest(const FString& Parameters)
{
	using namespace Padel;

	// Declared before the world so the delegates' lambdas never outlive them.
	int32 Serves = 0;
	int32 Points = 0;
	int32 Scores = 0;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	UPadelMatchEvents* Events = World->GetSubsystem<UPadelMatchEvents>();
	APadelMatchDirector* Director = World->SpawnActor<APadelMatchDirector>();
	if (!TestNotNull(TEXT("Events subsystem"), Events) || !TestNotNull(TEXT("Director spawned"), Director))
	{
		return false;
	}

	Events->OnServeStarted.AddLambda([&](int32) { ++Serves; });
	Events->OnPointEnded.AddLambda([&](ETeam, EPointReason, int32) { ++Points; });
	Events->OnScoreChanged.AddLambda([&](const FScoreSnapshot&) { ++Scores; });

	Ui::FMatchSetup Setup;
	Setup.SetsToWin = 1;
	Setup.HumanPlayerId = -1;
	if (!TestTrue(TEXT("Match started"), Director->StartMatch(Setup)))
	{
		return false;
	}
	TestTrue(TEXT("Events attached"), Events->IsAttached());
	TestTrue(TEXT("Match running"), Director->IsMatchRunning());
	for (int32 Id = 0; Id < NumPlayers; ++Id)
	{
		TestNotNull(FString::Printf(TEXT("Pawn %d"), Id), Director->GetPawn(Id));
	}
	TestTrue(TEXT("First score broadcast"), Scores >= 1);

	// Two simulated minutes: several points at any difficulty.
	Director->StepTicks(static_cast<int32>(SimTickRate) * 120);
	TestTrue(TEXT("Serves reached the delegates"), Serves >= 1);
	TestTrue(TEXT("Points were played"), Points >= 1);
	TestTrue(TEXT("Each point changed the score"), Scores >= Points);

	const APadelPlayerPawn* PawnA = Director->GetPawn(0);
	TestTrue(TEXT("Pawns are on the court, in centimetres"), PawnA && FMath::Abs(PawnA->GetActorLocation().X) < 1200.0
		&& FMath::Abs(PawnA->GetActorLocation().Y) < 600.0);

	Director->CloseMatch();
	TestFalse(TEXT("Events detached"), Events->IsAttached());
	TestFalse(TEXT("Match closed"), Director->IsMatchRunning());
	TestNull(TEXT("Pawns gone"), Director->GetPawn(0));
	return true;
}

#endif
