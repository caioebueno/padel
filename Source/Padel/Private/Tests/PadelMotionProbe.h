// M2 motion probe (docs/animation/MOVEMENT_FLUENCY.md): one pawn driven along a scripted path
// (stand, accelerate to a run, run 3 m, stop, a 180° reversal, a sideways shuffle, stop) with
// its anim ticked at 60 Hz, measuring what reads as floppy or robotic on screen: how often the
// locomotion switches clip and how many clips blend at once, the angular jerk of the pelvis,
// spine_03 and head, planted-foot slide, and the biggest body and arm bone turns in a frame.
//
// Two drives: locomotion only (the path's own facing and trajectory, body dynamics off: what the
// clips alone do), and the full stack (the animation model turns the body, leans it and looks at
// a ball, as in a match). The full stack also measures a MetaHuman when one is shown.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class APadelPlayerPawn;
class FAutomationTestBase;
class USkeletalMeshComponent;
class UWorld;

namespace PadelMotionProbe
{
	enum class EDrive : uint8
	{
		LocomotionOnly,
		FullStack,
		/** LocomotionOnly in the ready stance, as in a rally: the footwork (0012) joins the search. */
		ReadyStance,
	};

	enum class EPath : uint8
	{
		/** M2's path: stand, accelerate to a run, run 3 m, stop, 180° reversal, side shuffle, stop. */
		Classic,
		/**
		 * Phase 0 of the movement plan (Padel.Animation.LayerVideo), about 12 s: stand 1 s, walk
		 * 3 m, speed up to a run for 4 m, stop, a 90° turn into a 2 m walk, a 180° reversal into a
		 * jog, a side shuffle 2 m each way, stop and stand 1 s.
		 */
		LayerVideo,
	};

	/** What a recorder gets before the first frame. */
	struct FSetup
	{
		UWorld* World = nullptr;
		APadelPlayerPawn* Pawn = nullptr;
		USkeletalMeshComponent* Driver = nullptr;
		/** The shown MetaHuman's body, or null. */
		USkeletalMeshComponent* Shown = nullptr;
		/** The path's ground positions (cm, world), one per frame, worked out ahead of the run. */
		TArray<FVector> Path;
		double StartYawDeg = 0.0;
	};

	/** Probe-only switches; the defaults are the probe as M2 measured it. Nothing here changes the game. */
	struct FRunOptions
	{
		EPath Path = EPath::Classic;
		/** Orientation warp, stride warp and foot lock off (Animation.json's OrientationWarp*, StrideWarp*, FootLock* zeroed for this run). */
		bool bWarpsOff = false;
		/**
		 * LocomotionOnly: the pawn moves and turns by the playing clip's own root motion instead of
		 * following the path; the search still sees the path's trajectory, carried into the pawn's
		 * own frame. A pure clip reference: what the clips look like with no fitting to the game.
		 */
		bool bRootMotion = false;
		/** After the bodies exist, before the first frame (a recorder sets up its camera here). */
		TFunction<void(const struct FSetup&)> OnSetup;
		/** After each frame's pose is evaluated and recorded. */
		TFunction<void(int32 /*Frame*/, const FString& /*Phase*/)> OnFrame;
	};

	/** One body's numbers over the path. */
	struct FBodyMetrics
	{
		FString Name;
		/** Root mean square and 95th percentile angular jerk, degrees per second cubed, world space. */
		TMap<FString, FVector2D> JerkDegPerS3;
		/** RMS angular acceleration, degrees per second squared (a gentler read of the same wobble). */
		TMap<FString, double> AccelDegPerS2;
		double SlideCmps = 0.0;
		double SteadySlideCmps = 0.0;
		double WorstSlideSpellCmps = 0.0;
		double PlantedS = 0.0;
		FString SlideByPhase;
		double WorstBoneTurnDeg = 0.0;
		FString WorstBoneTurnAt;
		/** The same for the arms (clavicle, upper arm, forearm, hand, both sides): the racket arm's IK shows here. */
		double WorstArmTurnDeg = 0.0;
		FString WorstArmTurnAt;
		int32 CoreBones = 0;
		/** Each foot's contact point height (cm, world): 10th and 90th percentile, for the plant threshold. */
		FString FootHeights;
		bool bValid = false;
	};

	struct FRun
	{
		bool bRan = false;
		bool bMotionMatched = false;
		FString SkipReason;
		double DurationS = 0.0;
		/** Picks per second over the whole path (each a clip switch). */
		double SwitchesPerS = 0.0;
		int32 MaxPlaying = 0;
		/** Share of the path each foot (left, right) was locked by the M2 foot lock. */
		double LockedShare[2] = { 0.0, 0.0 };
		double MeanPlaying = 0.0;
		/** Seconds a clip played from databases the latest search left out (FPadelMotionMatcher::StrandedS). */
		double StrandedS = 0.0;
		/**
		 * Seconds the path was moving (over 0.5 m/s) while the playing clip barely moved: its own
		 * root under a third of the path's speed, or a clip that doesn't loop held on its last
		 * frame. The body stands still or skates while the game moves. And the longest such spell.
		 */
		double StalledMovingS = 0.0;
		double WorstStalledSpellS = 0.0;
		FString WorstStalledAt;
		/** Footwork-first searches while shuffling, how many the footwork answered, and their best costs (mean, lowest). */
		int32 ReadySearches = 0;
		int32 ReadyAccepted = 0;
		double MeanReadyCost = 0.0;
		double LowestReadyCost = 0.0;
		FBodyMetrics Placeholder;
		FBodyMetrics MetaHuman;
		/**
		 * How the MetaHuman follows the placeholder, per jerk bone: the RMS and worst wander (deg) of
		 * the turn between the two bones about its value at rest, and the frame lag (0..3) whose
		 * shift best lines up their angular velocities. Lag or overshoot in the retarget shows here.
		 */
		TMap<FString, FVector> FollowRmsMaxLag;
		/** The pick sequence with the path phase, deduplicated, for the log. */
		FString Picks;
		/** First frame of each phase, and the database picked each frame. */
		TArray<FString> Phases;
		TArray<FName> PickPerFrame;
		/** Each frame's playing clip, its playhead / length and play rate, and the path's speed (for logs). */
		TArray<FString> ClipPerFrame;
	};

	/** A foot contact path's planted spells (first and last frame): within 3 cm of its 10th percentile height for at least 0.05 s. */
	TArray<FIntPoint> PlantedSpells(const TArray<FVector>& Path, double Dt);
	/** Both feet's planted slide (cm/s): how far each planted spell travels, over the time planted. */
	double PlantedSlideCmps(const TArray<FVector> (&FootPaths)[2], double Dt, double* OutPlantedS = nullptr);

	/** Runs the path in a fresh world. Adds infos to Test; bRan false (SkipReason) without the databases. */
	FRun Run(FAutomationTestBase& Test, EDrive Drive);
	FRun Run(FAutomationTestBase& Test, EDrive Drive, const FRunOptions& Options);

	/** Logs a run's numbers as test infos, prefixed with Label. */
	void Report(FAutomationTestBase& Test, const FString& Label, const FRun& Run);
}
