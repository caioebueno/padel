// Smoke checks of the match simulation inside Unreal's Automation framework, so the
// engine build is known to compile and run the same code as the native suite
// (Tests/Native), which holds the full set of rule tests.
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "MatchSimulation.h"

#include <string>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMatchSimulationSmokeTest, "Padel.Match.SimulationSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelMatchSimulationSmokeTest::RunTest(const FString& Parameters)
{
	using namespace Padel;

	FString Text;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Physics.json"));
	if (!TestTrue(TEXT("Data/Physics.json is readable"), FFileHelper::LoadFileToString(Text, *Path)))
	{
		return false;
	}
	FBallPhysicsParams Physics;
	std::string Error;
	if (!TestTrue(TEXT("Data/Physics.json parses"), ParseBallPhysicsJson(TCHAR_TO_UTF8(*Text), Physics, Error)))
	{
		AddError(UTF8_TO_TCHAR(Error.c_str()));
		return false;
	}

	FMatchSimulation Match(FMatchConfig::OneSet(), Physics, 1);
	Match.StartMatch();
	Match.BeginPoint();
	TestTrue(TEXT("Waiting for the serve"), Match.Phase() == EMatchPhase::AwaitingServe);

	// An unreturned serve into the box: it must bounce twice and give the server the point.
	const FLaunchResult Launch = Match.PlayServe(Match.ServerId(), Match.DefaultServeContact(), EShotType::ServeFlat,
		ETimingGrade::Good, Match.ServiceBoxCentre(), 13.0, FVec3{ -30.0, 0.0, 0.0 });
	TestTrue(TEXT("Serve launch solved"), Launch.bOk);
	for (int Tick = 0; Tick < 120 * 10 && Match.Phase() == EMatchPhase::Live; ++Tick)
	{
		Match.Tick();
	}
	TestTrue(TEXT("Point ended"), Match.Phase() == EMatchPhase::PointOver);
	TestTrue(TEXT("Double bounce"), Match.Referee().LastReason() == EPointReason::DoubleBounce);
	TestTrue(TEXT("Server's team won"), Match.Referee().LastWinner() == ETeam::A);
	TestEqual(TEXT("Score"), FString(UTF8_TO_TCHAR(Match.Score().Snapshot().Points[0].c_str())), FString(TEXT("15")));
	return true;
}

#endif
