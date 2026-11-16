// The HUD's binder to UPadelMatchEvents, inside the engine: a real simulation's events
// reach an IUiEventSink as GDD codes, with positions back in metres, and stop on Unbind.
// The code and score conversions themselves are tested in the native suite.
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

#include "MatchSimulation.h"
#include "Presentation/PadelMatchEvents.h"
#include "UI/PadelMatchEventBinder.h"

#include <string>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FBinderTestSink final : Padel::Ui::IUiEventSink
	{
		int32 Serves = 0;
		int32 Points = 0;
		std::string LastReason;
		std::string LastShot;
		double MaxBounceX = 0.0;
		std::string PointsA;

		virtual void OnServeStarted(int) override { ++Serves; }
		virtual void OnServeFault(std::string_view) override {}
		virtual void OnServeLet() override {}
		virtual void OnBallHit(int, std::string_view Shot, std::string_view, double) override { LastShot = Shot; }
		virtual void OnBallBounced(std::string_view, int, const Padel::Ui::FUiVec3& PositionM) override
		{
			MaxBounceX = FMath::Max(MaxBounceX, FMath::Abs(PositionM.X));
		}
		virtual void OnBallLeftCourt(const Padel::Ui::FUiVec3&) override {}
		virtual void OnPointEnded(int, std::string_view Reason, int) override { ++Points; LastReason = Reason; }
		virtual void OnScoreChanged(const Padel::Ui::FScoreView& Score) override { PointsA = Score.Points[0]; }
		virtual void OnGameWon(int) override {}
		virtual void OnSetWon(int) override {}
		virtual void OnMatchWon(int) override {}
		virtual void OnChangeOfEnds() override {}
		virtual void OnGoldenPointStarted(int) override {}
	};

	bool LoadPhysicsForBinder(FAutomationTestBase& Test, Padel::FBallPhysicsParams& OutPhysics)
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
	void PlayUnreturnedServeForBinder(Padel::FMatchSimulation& Match)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelUiEventBinderTest, "Padel.UI.MatchEventBinder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelUiEventBinderTest::RunTest(const FString& Parameters)
{
	using namespace Padel;

	FBallPhysicsParams Physics;
	if (!LoadPhysicsForBinder(*this, Physics))
	{
		return false;
	}

	UPadelMatchEvents* Events = NewObject<UPadelMatchEvents>(GetTransientPackage());
	FBinderTestSink Sink;
	FPadelMatchEventBinder Binder;
	Binder.Bind(*Events, Sink, true);
	TestTrue(TEXT("Bound"), Binder.IsBound());
	TestFalse(TEXT("No live match before Attach"), Binder.HasLiveMatch());

	FMatchSimulation Match(FMatchConfig::OneSet(), Physics, 1);
	Events->Attach(Match);
	TestTrue(TEXT("Live match after Attach"), Binder.HasLiveMatch());

	Match.StartMatch();
	PlayUnreturnedServeForBinder(Match);
	TestEqual(TEXT("serve_started reached the sink"), Sink.Serves, 1);
	TestEqual(TEXT("point_ended reached the sink"), Sink.Points, 1);
	TestEqual(TEXT("Reason as its GDD code"), FString(UTF8_TO_TCHAR(Sink.LastReason.c_str())), FString(TEXT("DOUBLE_BOUNCE")));
	TestFalse(TEXT("Shot type named"), Sink.LastShot.empty());
	// The service box is 0 to 6.95 m from the net: metres, not the delegate's centimetres.
	TestTrue(TEXT("Bounce positions are back in metres"), Sink.MaxBounceX > 0.5 && Sink.MaxBounceX < 10.5);
	TestEqual(TEXT("Score view built from the snapshot"), FString(UTF8_TO_TCHAR(Sink.PointsA.c_str())), FString(TEXT("15")));

	Binder.Unbind();
	TestFalse(TEXT("Unbound"), Binder.IsBound());
	PlayUnreturnedServeForBinder(Match);
	TestEqual(TEXT("Nothing after Unbind"), Sink.Points, 1);
	Events->Detach();
	return true;
}

#endif
