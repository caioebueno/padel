// The animation tuning loads through Unreal DataTables from the same Data/*.json files
// the native tests read, and a practice drive shows its contact frame on the hit tick
// (docs/decisions/0005-player-animation.md).
#include "Misc/AutomationTest.h"

#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/PadelAnimData.h"
#include "Data/PadelShotData.h"
#include "Shots/BallMachine.h"
#include "Shots/PlayerShotController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelAnimDataLoadTest, "Padel.Animation.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelAnimDataLoadTest::RunTest(const FString& Parameters)
{
	const FPadelAnimData Data = FPadelAnimData::Load();
	for (const FString& Error : Data.Errors)
	{
		AddError(Error);
	}
	TestTrue(TEXT("Animation data loads"), Data.bOk);
	TestTrue(TEXT("Clips for every slice shot"), Data.Clips.Validate().empty());
	TestEqual(TEXT("One mesh per team"), Data.TeamMeshes.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelAnimContactSyncTest, "Padel.Animation.ContactSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelAnimContactSyncTest::RunTest(const FString& Parameters)
{
	using namespace Padel::Shots;
	using namespace Padel::Animation;
	const FPadelShotData Shots = FPadelShotData::Load();
	const FPadelAnimData Anim = FPadelAnimData::Load();
	FPlayerRatings Player;
	if (!TestTrue(TEXT("Data"), Shots.bOk && Anim.bOk && FPadelShotData::LoadPlayer(FName(TEXT("practice_default")), Player)))
	{
		return false;
	}
	FPracticeBall Ball(&Shots.PracticeBall);
	FBallMachine Machine(Shots.Feeds, 1.0, 3);
	FPlayerShotController Controller(0, 1.0, &Player, &Shots.Library, &Shots.Movement, FVec3{ 8.5, 0.0, 0.0 }, 3);
	FPlayerAnimModel Model(&Anim.Settings, &Anim.Clips, &Shots.Library);

	// Hold the drive from the feed until the controller fires: the hit tick must show
	// the clip's contact frame, with the placeholder racket on the ball.
	const double Dt = Shots.PracticeBall.StepS;
	bool bHit = false;
	for (int32 Step = 0; Step < 120 * 6 && !bHit; ++Step)
	{
		Machine.Tick(Dt, Ball);
		FPlayerInputFrame Input;
		Input.bDrive = Controller.HasIdealMoment() && Controller.TimeToIdealS() < 0.3;
		const FSwingEvents Events = Controller.Tick(Input, Dt, Ball);
		const FPlayerAnimFrame& Frame = Model.Step(FPlayerAnimInput::FromController(Controller, Events, true), Dt);
		Ball.Advance(Dt);
		if (Events.Outcome == ESwingOutcome::Hit)
		{
			bHit = true;
			TestTrue(TEXT("Contact frame on the hit tick"), Frame.bContactFrame);
			if (TestNotNull(TEXT("Swing clip"), Frame.Clip))
			{
				TestEqual(TEXT("Playhead at the clip's contact time"), Frame.ClipTimeS, Frame.Clip->ContactS, 1e-6);
			}
			TestTrue(TEXT("Placeholder racket on the ball"), Length(Sub(Frame.RacketPosition, Events.Intent.Origin)) < 1e-6);
		}
		else
		{
			TestFalse(TEXT("No contact frame before the hit"), Frame.bContactFrame);
		}
	}
	TestTrue(TEXT("The practice player hits a fed ball"), bHit);
	return true;
}

#endif
