// UPadelMatchEvents inside the engine build: a real match simulation's events reach the
// delegates with the GDD 3.4 parameters, positions arrive in centimetres, and nothing
// arrives after Detach. The events themselves are tested in the native suite.
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

#include "MatchSimulation.h"
#include "Presentation/PadelMatchEvents.h"

#include <string>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LoadPhysicsForMatchEvents(FAutomationTestBase& Test, Padel::FBallPhysicsParams& OutPhysics)
	{
		FString Text;
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("Physics.json"));
		if (!Test.TestTrue(TEXT("Data/Physics.json is readable"), FFileHelper::LoadFileToString(Text, *Path)))
		{
			return false;
		}
		std::string Error;
		if (!Padel::ParseBallPhysicsJson(TCHAR_TO_UTF8(*Text), OutPhysics, Error))
		{
			Test.AddError(UTF8_TO_TCHAR(Error.c_str()));
			return false;
		}
		return true;
	}

	// One serve into the box that nobody returns, played to the end of the point.
	void PlayUnreturnedServeForMatchEvents(Padel::FMatchSimulation& Match)
	{
		using namespace Padel;
		Match.BeginPoint();
		Match.PlayServe(Match.ServerId(), Match.DefaultServeContact(), EShotType::ServeFlat, ETimingGrade::Good,
			Match.ServiceBoxCentre(), 13.0, FVec3{ -30.0, 0.0, 0.0 });
		for (int Tick = 0; Tick < static_cast<int>(SimTickRate) * 10 && Match.Phase() == EMatchPhase::Live; ++Tick)
		{
			Match.Tick();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMatchEventsForwardTest, "Padel.Presentation.MatchEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelMatchEventsForwardTest::RunTest(const FString& Parameters)
{
	using namespace Padel;

	FBallPhysicsParams Physics;
	if (!LoadPhysicsForMatchEvents(*this, Physics))
	{
		return false;
	}

	UPadelMatchEvents* Events = NewObject<UPadelMatchEvents>(GetTransientPackage());
	FMatchSimulation Match(FMatchConfig::OneSet(), Physics, 1);
	Events->Attach(Match);
	TestTrue(TEXT("Attached"), Events->IsAttached());

	int32 ServesStarted = 0;
	int32 ServerSeen = -1;
	int32 Hits = 0;
	EShotType ShotSeen = EShotType::Count;
	int32 GroundBounces = 0;
	FVector FirstBounce = FVector::ZeroVector;
	int32 PointsEnded = 0;
	ETeam Winner = ETeam::B;
	EPointReason Reason = EPointReason::None;
	int32 LastHitter = -1;
	FString PointsA;

	Events->OnServeStarted.AddLambda([&](int32 ServerId) { ++ServesStarted; ServerSeen = ServerId; });
	Events->OnBallHit.AddLambda([&](int32, EShotType Shot, ETimingGrade, double, const FVector&) { ++Hits; ShotSeen = Shot; });
	Events->OnBallBounced.AddLambda([&](Court::ESurface Surface, Court::EHalf, const FVector& WorldPosition)
	{
		if (Surface == Court::ESurface::Ground && GroundBounces++ == 0)
		{
			FirstBounce = WorldPosition;
		}
	});
	Events->OnPointEnded.AddLambda([&](ETeam Team, EPointReason Why, int32 Hitter)
	{
		++PointsEnded;
		Winner = Team;
		Reason = Why;
		LastHitter = Hitter;
	});
	Events->OnScoreChanged.AddLambda([&](const FScoreSnapshot& Snapshot) { PointsA = UTF8_TO_TCHAR(Snapshot.Points[0].c_str()); });

	Match.StartMatch();
	const int32 Server = Match.ServerId();
	PlayUnreturnedServeForMatchEvents(Match);

	TestEqual(TEXT("serve_started once"), ServesStarted, 1);
	TestEqual(TEXT("serve_started names the server"), ServerSeen, Server);
	TestEqual(TEXT("ball_hit once"), Hits, 1);
	TestTrue(TEXT("ball_hit carries the shot type"), ShotSeen == EShotType::ServeFlat);
	TestTrue(TEXT("Two ground bounces"), GroundBounces >= 2);
	// The receiver's service box is 0 to 6.95 m from the net: in centimetres that's
	// well past 50, in metres it never is.
	TestTrue(TEXT("Bounce position is in centimetres"), FMath::Abs(FirstBounce.X) > 50.0 && FMath::Abs(FirstBounce.X) < 700.0);
	TestEqual(TEXT("point_ended once"), PointsEnded, 1);
	TestTrue(TEXT("Server's team won"), Winner == ETeam::A);
	TestTrue(TEXT("Double bounce"), Reason == EPointReason::DoubleBounce);
	TestEqual(TEXT("Last hitter is the server"), LastHitter, Server);
	TestEqual(TEXT("score_changed reached the delegate"), PointsA, FString(TEXT("15")));

	Events->Detach();
	TestFalse(TEXT("Detached"), Events->IsAttached());
	PlayUnreturnedServeForMatchEvents(Match);
	TestEqual(TEXT("Nothing after Detach"), PointsEnded, 1);
	TestEqual(TEXT("Second point still played"), FString(UTF8_TO_TCHAR(Match.Score().Snapshot().Points[0].c_str())), FString(TEXT("30")));
	return true;
}

#endif
