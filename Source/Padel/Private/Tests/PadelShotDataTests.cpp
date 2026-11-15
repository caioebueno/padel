// The shot system's tuning loads through Unreal DataTables from the same Data/*.json
// files the native tests read, and a ball machine drive plays through it.
#include "Misc/AutomationTest.h"

#include "Data/PadelShotData.h"
#include "Shots/BallMachine.h"
#include "Shots/PlayerShotController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelShotDataLoadTest, "Padel.Gameplay.ShotData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelShotDataLoadTest::RunTest(const FString& Parameters)
{
	const FPadelShotData Data = FPadelShotData::Load();
	for (const FString& Error : Data.Errors)
	{
		AddError(Error);
	}
	TestTrue(TEXT("Shot data loads"), Data.bOk);
	TestEqual(TEXT("Drive max speed (GDD 4)"), Data.Library.Get(Padel::EShotType::Drive).SpeedMaxKmh, 100.0);
	TestEqual(TEXT("Charge time (GDD 7)"), Data.Library.Model.ChargeTimeS, 0.6, 1e-6);
	TestTrue(TEXT("Ball machine has feeds"), !Data.Feeds.empty());

	Padel::Shots::FPlayerRatings Player;
	TestTrue(TEXT("Practice player row"), FPadelShotData::LoadPlayer(FName(TEXT("practice_default")), Player));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelShotPracticeDriveTest, "Padel.Gameplay.PracticeDrive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPadelShotPracticeDriveTest::RunTest(const FString& Parameters)
{
	using namespace Padel::Shots;
	const FPadelShotData Data = FPadelShotData::Load();
	FPlayerRatings Player;
	if (!TestTrue(TEXT("Data"), Data.bOk && FPadelShotData::LoadPlayer(FName(TEXT("practice_default")), Player)))
	{
		return false;
	}
	FPracticeBall Ball(&Data.PracticeBall);
	FBallMachine Machine(Data.Feeds, 1.0, 3);
	FPlayerShotController Controller(0, 1.0, &Player, &Data.Library, &Data.Movement, FVec3{ 8.5, 0.0, 0.0 }, 3);

	// Hold the drive button from the feed until the controller fires at the late edge:
	// whatever the timing, a hit must come out of it.
	const double Dt = Data.PracticeBall.StepS;
	bool bHit = false;
	for (int32 Step = 0; Step < 120 * 6 && !bHit; ++Step)
	{
		Machine.Tick(Dt, Ball);
		FPlayerInputFrame Input;
		Input.bDrive = Controller.HasIdealMoment() && Controller.TimeToIdealS() < 0.3;
		bHit = Controller.Tick(Input, Dt, Ball).Outcome == ESwingOutcome::Hit;
		Ball.Advance(Dt);
	}
	TestTrue(TEXT("The practice player hits a fed ball"), bHit);
	return true;
}

#endif
