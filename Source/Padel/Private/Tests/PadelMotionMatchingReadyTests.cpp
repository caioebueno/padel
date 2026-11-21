// The video footwork (docs/decisions/0012-video-footwork.md) in the ready stance: the same
// scripted path as Padel.Animation.MotionMatching, run as in a rally (knees bent), where
// MotionMatchingReadyDatabases join the search. The footwork has to be picked, and the motion
// held to the same gates as the sample's clips alone. Skipped without the motion matching
// databases or without the footwork database (both git-ignored, built by animation_setup.py
// and footwork_setup.py).
#include "Misc/AutomationTest.h"

#include "PadelMotionProbe.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "UObject/SoftObjectPath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMotionMatchingReadyTest, "Padel.Animation.MotionMatchingReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMotionMatchingReadyTest::RunTest(const FString& Parameters)
{
	using namespace PadelMotionProbe;
	const FSoftObjectPath Footwork(TEXT("/Game/Padel/Animations/MotionMatching/Footwork/PSD_Padel_Footwork.PSD_Padel_Footwork"));
	if (!Cast<UPoseSearchDatabase>(Footwork.TryLoad()))
	{
		AddInfo(TEXT("Skipped: no PSD_Padel_Footwork (Tools/AssetPipeline/unreal/footwork_setup.py)"));
		return true;
	}
	const FRun Ready = PadelMotionProbe::Run(*this, EDrive::ReadyStance);
	if (!Ready.bRan)
	{
		AddInfo(TEXT("Skipped: ") + Ready.SkipReason);
		return true;
	}
	Report(*this, TEXT("ready"), Ready);

	// Where the footwork plays, by phase.
	TMap<FString, int32> FootworkFrames;
	int32 Total = 0;
	for (int32 I = 0; I < Ready.PickPerFrame.Num(); ++I)
	{
		if (Ready.PickPerFrame[I].ToString().Contains(TEXT("Footwork")))
		{
			++Total;
			FootworkFrames.FindOrAdd(Ready.Phases.IsValidIndex(I) ? Ready.Phases[I] : TEXT("?"))++;
		}
	}
	FString ByPhase;
	for (const TPair<FString, int32>& Phase : FootworkFrames)
	{
		ByPhase += FString::Printf(TEXT("%s %d; "), *Phase.Key, Phase.Value);
	}
	AddInfo(FString::Printf(TEXT("ready: footwork picked in %d of %d frames (%s)"), Total, Ready.PickPerFrame.Num(), *ByPhase));
	// The side shuffle is the footwork's job (the sample has no strafing): it has to play there.
	TestTrue(FString::Printf(TEXT("The side shuffle picks the footwork (%d frames)"), FootworkFrames.FindRef(TEXT("shuffle"))), FootworkFrames.FindRef(TEXT("shuffle")) > 0);

	// The same gates as the sample's clips alone (Padel.Animation.MotionMatching, clips run).
	const FBodyMetrics& P = Ready.Placeholder;
	TestTrue(FString::Printf(TEXT("Ready: standing and moving steadily, planted feet slide under 25 cm/s (%.1f)"), P.SteadySlideCmps), P.SteadySlideCmps < 25.0);
	TestTrue(FString::Printf(TEXT("Ready: over the whole path under 50 cm/s (%.1f)"), P.SlideCmps), P.SlideCmps < 50.0);
	TestTrue(FString::Printf(TEXT("Ready: no body bone turns over 20 deg in a frame (%.1f)"), P.WorstBoneTurnDeg), P.WorstBoneTurnDeg < 20.0);
	TestTrue(FString::Printf(TEXT("Ready: under 1.8 clip switches a second (%.2f)"), Ready.SwitchesPerS), Ready.SwitchesPerS < 1.8);
	const double PelvisJerk = P.JerkDegPerS3.FindRef(TEXT("pelvis")).Y;
	const double SpineJerk = P.JerkDegPerS3.FindRef(TEXT("spine_03")).Y;
	TestTrue(FString::Printf(TEXT("Ready: pelvis jerk p95 under 245k deg/s^3 (%.0fk)"), PelvisJerk / 1000.0), PelvisJerk > 0.0 && PelvisJerk < 245000.0);
	TestTrue(FString::Printf(TEXT("Ready: spine_03 jerk p95 under 255k deg/s^3 (%.0fk)"), SpineJerk / 1000.0), SpineJerk > 0.0 && SpineJerk < 255000.0);
	return true;
}

#endif
