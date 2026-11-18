// Motion matching (docs/animation/MOVEMENT_FLUENCY.md, M1 and M2) on a real skeleton, through
// PadelMotionProbe's scripted path (stand, accelerate to a run, run 3 m, stop, a 180° reversal,
// a sideways shuffle, stop) at 60 Hz, twice: the clips alone, and the full stack (the model
// turning, leaning and looking at a ball, as in a match; a shown MetaHuman measured too).
// Skipped on a machine without the motion matching databases (git-ignored, built by
// Tools/AssetPipeline/unreal/animation_setup.py).
#include "Misc/AutomationTest.h"

#include "PadelMotionProbe.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMotionMatchingTest, "Padel.Animation.MotionMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMotionMatchingTest::RunTest(const FString& Parameters)
{
	using namespace PadelMotionProbe;
	const FRun Clips = PadelMotionProbe::Run(*this, EDrive::LocomotionOnly);
	if (!Clips.bRan)
	{
		AddInfo(TEXT("Skipped: ") + Clips.SkipReason);
		return true;
	}
	Report(*this, TEXT("clips"), Clips);
	const FRun Full = PadelMotionProbe::Run(*this, EDrive::FullStack);
	Report(*this, TEXT("full"), Full);

	if (!TestTrue(TEXT("Motion matching drove the pose"), Clips.bMotionMatched))
	{
		return false;
	}
	const FBodyMetrics& P = Clips.Placeholder;
	TestTrue(TEXT("Feet are planted some of the time"), P.PlantedS > 1.0);
	TestTrue(TEXT("The body bones are found"), P.CoreBones >= 10);
	// M1 18.6 cm/s steady, 41.4 over the path; M2 (throttled search, foot locking, warping) 21.2 and 44.8.
	TestTrue(FString::Printf(TEXT("Standing and moving steadily, planted feet slide under 25 cm/s (%.1f)"), P.SteadySlideCmps), P.SteadySlideCmps < 25.0);
	TestTrue(FString::Printf(TEXT("Over the whole path under 50 cm/s (%.1f)"), P.SlideCmps), P.SlideCmps < 50.0);
	TestTrue(FString::Printf(TEXT("No body bone turns over 20 deg in a frame (%.1f)"), P.WorstBoneTurnDeg), P.WorstBoneTurnDeg < 20.0);

	// M2: fewer, cleaner switches (M1: 1.96 a second, up to 4 clips crossfading; M2: 1.52, one).
	TestTrue(FString::Printf(TEXT("Under 1.8 clip switches a second (%.2f)"), Clips.SwitchesPerS), Clips.SwitchesPerS < 1.8);
	TestTrue(FString::Printf(TEXT("At most 2 poses play at once (%d)"), Clips.MaxPlaying), Clips.MaxPlaying <= 2);
	// Angular jerk, 95th percentile (M1: pelvis 219k, spine_03 260k deg/s^3; M2: 226k, 240k).
	const double PelvisJerk = P.JerkDegPerS3.FindRef(TEXT("pelvis")).Y;
	const double SpineJerk = P.JerkDegPerS3.FindRef(TEXT("spine_03")).Y;
	TestTrue(FString::Printf(TEXT("Pelvis jerk p95 under 245k deg/s^3 (%.0fk)"), PelvisJerk / 1000.0), PelvisJerk > 0.0 && PelvisJerk < 245000.0);
	TestTrue(FString::Printf(TEXT("spine_03 jerk p95 under 255k deg/s^3 (%.0fk)"), SpineJerk / 1000.0), SpineJerk > 0.0 && SpineJerk < 255000.0);
	if (Full.bRan)
	{
		// The full stack: the warps and foot locking keep steady running planted (M1: 33.6 cm/s; M2: 30.1).
		TestTrue(FString::Printf(TEXT("Full stack: under 1.6 switches a second (%.2f)"), Full.SwitchesPerS), Full.SwitchesPerS < 1.6);
		TestTrue(FString::Printf(TEXT("Full stack: steady planted feet slide under 33 cm/s (%.1f)"), Full.Placeholder.SteadySlideCmps), Full.Placeholder.SteadySlideCmps < 33.0);
		TestTrue(FString::Printf(TEXT("Full stack: no body bone turns over 20 deg in a frame (%.1f)"), Full.Placeholder.WorstBoneTurnDeg), Full.Placeholder.WorstBoneTurnDeg < 20.0);
		for (const TPair<FString, FVector>& Bone : Full.FollowRmsMaxLag)
		{
			// A shown MetaHuman follows the placeholder with no frame lag (M2's retarget check).
			TestTrue(FString::Printf(TEXT("MetaHuman %s follows with no lag (%.0f frames)"), *Bone.Key, Bone.Value.Z), Bone.Value.Z == 0.0);
		}
	}

	// The search follows the path: an idle standing, a start setting off, then loops, then a stop.
	auto First = [&](const TCHAR* Kind, int32 From) -> int32
	{
		for (int32 I = FMath::Max(From, 0); I < Clips.PickPerFrame.Num(); ++I)
		{
			if (Clips.PickPerFrame[I].ToString().Contains(Kind)) { return I; }
		}
		return INDEX_NONE;
	};
	const int32 StartAt = Clips.Phases.IndexOfByKey(TEXT("start"));
	const int32 StopAt = Clips.Phases.IndexOfByKey(TEXT("stop"));
	const int32 Starts = First(TEXT("Starts"), StartAt);
	// Running on: a loop, or a pivot's straight run-in (the search picks either; both run).
	const int32 RunFrom = Starts == INDEX_NONE ? StartAt : Starts;
	const int32 Loop = First(TEXT("Loops"), RunFrom);
	const int32 Pivot = First(TEXT("Pivots"), RunFrom);
	const int32 Loops = Loop == INDEX_NONE ? Pivot : Pivot == INDEX_NONE ? Loop : FMath::Min(Loop, Pivot);
	const int32 Stops = First(TEXT("Stops"), StopAt);
	TestTrue(TEXT("Standing still picks an idle"), First(TEXT("Idles"), 0) != INDEX_NONE && First(TEXT("Idles"), 0) < StartAt);
	TestTrue(TEXT("Setting off picks a start"), Starts != INDEX_NONE && Starts < StopAt);
	TestTrue(TEXT("Running on picks a running clip after the start"), Loops != INDEX_NONE && Loops < StopAt);
	TestTrue(TEXT("Stopping picks a stop"), Stops != INDEX_NONE && Stops < Clips.Phases.IndexOfByKey(TEXT("start2")));
	return true;
}

#endif
