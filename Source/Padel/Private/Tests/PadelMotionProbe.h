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

class FAutomationTestBase;
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
	};

	/** A foot contact path's planted spells (first and last frame): within 3 cm of its 10th percentile height for at least 0.05 s. */
	TArray<FIntPoint> PlantedSpells(const TArray<FVector>& Path, double Dt);
	/** Both feet's planted slide (cm/s): how far each planted spell travels, over the time planted. */
	double PlantedSlideCmps(const TArray<FVector> (&FootPaths)[2], double Dt, double* OutPlantedS = nullptr);

	/** Runs the path in a fresh world. Adds infos to Test; bRan false (SkipReason) without the databases. */
	FRun Run(FAutomationTestBase& Test, EDrive Drive);

	/** Logs a run's numbers as test infos, prefixed with Label. */
	void Report(FAutomationTestBase& Test, const FString& Label, const FRun& Run);
}
