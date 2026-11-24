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

	// A footwork-only shuffle search once kept a stop clip playing under the shuffling pawn (Pose
	// Search offers the playing clip's continuing pose even from a database it wasn't asked to
	// search), sliding the feet 53 cm/s with the footwork never picked. The search falls back
	// to the sample's databases instead; with no footwork database this is 0 trivially.
	TestTrue(FString::Printf(TEXT("No clip plays on from a database the search left out (%.2f s)"), Clips.StrandedS), Clips.StrandedS == 0.0);
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
		TestTrue(FString::Printf(TEXT("Full stack: no clip plays on from a database the search left out (%.2f s)"), Full.StrandedS), Full.StrandedS == 0.0);
		TestTrue(FString::Printf(TEXT("Full stack: steady planted feet slide under 33 cm/s (%.1f)"), Full.Placeholder.SteadySlideCmps), Full.Placeholder.SteadySlideCmps < 33.0);
		TestTrue(FString::Printf(TEXT("Full stack: no body bone turns over 20 deg in a frame (%.1f)"), Full.Placeholder.WorstBoneTurnDeg), Full.Placeholder.WorstBoneTurnDeg < 20.0);
		// The racket arm's IK on whichever body shows it (the MetaHuman's own warp after the
		// retarget). Before the arm-jump fix a MetaHuman arm bone turned 165.7 deg in a frame
		// running (the upper arm 97.8); after it, 32.2 (the reverse pivot lunging the shoulder at
		// the carried racket). Gated at today's level; the goal is 20 once the wrist/racket
		// follow-up lands (docs/animation/MOVEMENT_FLUENCY.md).
		for (const FBodyMetrics* Body : { &Full.Placeholder, &Full.MetaHuman })
		{
			if (Body->bValid)
			{
				TestTrue(FString::Printf(TEXT("Full stack: no %s arm bone turns over 35 deg in a frame (%.1f, %s)"), *Body->Name, Body->WorstArmTurnDeg, *Body->WorstArmTurnAt),
					Body->WorstArmTurnDeg < 35.0);
			}
		}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMotionMatchingClipKeepsUpTest, "Padel.Animation.MotionMatching.ClipKeepsUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMotionMatchingClipKeepsUpTest::RunTest(const FString& Parameters)
{
	// The phase 0 layer videos (Padel.Animation.LayerVideo) found the body standing still through
	// a 90 deg turn and a 2 m walk after a stop: the search kept playing the stop clip's still tail
	// (Pose Search offers the playing clip's continuing pose even from a database the chooser left
	// out) while the game walked on at 1.5 m/s. On the layer video path (stop, turn, walk,
	// reversal, shuffle), whenever the game moves the playing clip must move too.
	using namespace PadelMotionProbe;
	FRunOptions Options;
	Options.Path = EPath::LayerVideo;
	for (const EDrive Drive : { EDrive::LocomotionOnly, EDrive::FullStack })
	{
		const TCHAR* Label = Drive == EDrive::FullStack ? TEXT("full") : TEXT("clips");
		const FRun Run = PadelMotionProbe::Run(*this, Drive, Options);
		if (!Run.bRan)
		{
			AddInfo(TEXT("Skipped: ") + Run.SkipReason);
			return true;
		}
		Report(*this, Label, Run);
		// Each frame's clip while stalled, for the log.
		for (int32 Frame = 1; Frame < Run.ClipPerFrame.Num(); ++Frame)
		{
			if (Run.ClipPerFrame[Frame].Contains(TEXT("clip 0.0")) && !Run.ClipPerFrame[Frame].EndsWith(TEXT("path 0.00 m/s")))
			{
				AddInfo(FString::Printf(TEXT("[%s] %d %s: %s"), Label, Frame, *Run.Phases[Frame], *Run.ClipPerFrame[Frame]));
			}
		}
		// Before the fix: 2.35 s in all, the longest 1.07 s (the walk stop's tail through the turn and walk).
		// After: about 0.55 s, each spell under 0.2 s (a search or two to notice).
		TestTrue(FString::Printf(TEXT("%s: never moving on a clip that barely moves for 0.25 s or more (longest %.2f s, %s)"), Label, Run.WorstStalledSpellS, *Run.WorstStalledAt),
			Run.WorstStalledSpellS < 0.25);
		TestTrue(FString::Printf(TEXT("%s: under 0.75 s moving on clips that barely move over the path (%.2f s)"), Label, Run.StalledMovingS), Run.StalledMovingS < 0.75);
	}
	return true;
}

#endif
