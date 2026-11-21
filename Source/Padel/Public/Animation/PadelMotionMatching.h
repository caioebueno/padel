#pragma once

#include "CoreMinimal.h"
#include "Animation/TrajectoryTypes.h"
#include "PadelMotionMatching.generated.h"

class UAnimSequenceBase;
class UPoseSearchDatabase;
struct FAnimationUpdateContext;
struct FPoseContext;

namespace UE::PoseSearch
{
	struct FPoseHistory;
}

/** M1 motion matching tuning (Data/Animation.json MotionMatching*), in seconds and Unreal units. */
struct FPadelMotionMatchingTuning
{
	double SearchThrottleS = 0.0;
	double BlendTimeS = 0.2;
	double PoseJumpThresholdS = 0.25;
	double PlayRateMin = 0.8;
	double PlayRateMax = 1.3;
	int32 MaxBlends = 4;
	/** A new pick waits until the latest is this far (0..1) through its crossfade, so quick re-picks don't stack up. */
	double RepickAfterBlend = 0.5;
	int32 HistoryPoses = 10;
	double HistorySamplingS = 0.04;
	/** The chooser (Animation.json MotionMatchingStill*, *TurnInPlace*): below this speed (cm/s) and not speeding up, the still databases. */
	double StillBelowUnits = 100.0;
	/** Still and turned at least this far (degrees) over the trajectory's past: the turn-in-place databases too. */
	double TurnInPlaceDeg = 30.0;
	/**
	 * M2: switches by inertialization (Animation.json MotionMatchingInertialHalfLifeS): the new
	 * clip plays alone at once and the difference from the pose that was showing, and its
	 * velocity, decay on a critically damped spring of this half-life. 0 keeps M1's crossfade.
	 */
	double InertialHalfLifeS = 0.0;
	/** M2: a new candidate must beat the continuing pose's cost by this much to switch (MotionMatchingSwitchCostMargin). */
	double SwitchCostMargin = 0.0;
	/** Still, and the trajectory's past travelled at least this far (cm): the stop databases too. */
	double StopAfterUnits = 20.0;
	/** Gaits: the walk databases up to RunFromUnits, the run ones from there, the sprint ones from SprintFromUnits (cm/s, the current speed: the prediction overshoots while speeding up), each reaching GaitOverlap (a share) past its edges so a gait change can blend. */
	double RunFromUnits = 250.0;
	double SprintFromUnits = 500.0;
	double GaitOverlap = 0.2;
	/** M3: the playing clip's root speed this far (cm/s) from the simulation's: search soon and switch without SwitchCostMargin (MotionMatchingSpeedMismatchMps; 0 off). */
	double SpeedMismatchUnits = 0.0;
};

/** Which databases a search may use (the Game Animation Sample picks them with a chooser table; this is it in C++). */
enum class EPadelMotionMatchingRole : uint8
{
	/** Starts, loops and pivots: searched while moving or setting off, each gait in its speed band. */
	Moving,
	MovingWalk,
	MovingSprint,
	/** Idles: searched while still. */
	Still,
	/** Stops: searched while slow after moving (the trajectory's past travelled), so a stop runs into an idle. */
	Stop,
	/** Turns in place: searched while still and turning. */
	TurnInPlace,
};

/**
 * M1 locomotion by motion matching (docs/animation/MOVEMENT_FLUENCY.md): the simulation leads
 * and this follows it. Each update it searches the Pose Search databases with the player's
 * trajectory (past and predicted, from the animation model) and its own pose history, and
 * plays what it finds in place (the clips are root locked; the game moves the pawn).
 *
 * The node's own pose history replaces a history collector node, and this replaces
 * FAnimNode_MotionMatching (whose settings are private). M2: it searches every
 * SearchThrottleS, switches only when a candidate beats the continuing pose by
 * SwitchCostMargin, and switches by inertialization (InertialHalfLifeS): one clip plays, the
 * difference from the pose shown decaying on a spring. With InertialHalfLifeS 0, M1's
 * crossfade stack (BlendTimeS, MaxBlends). A pick's play rate matches the clip's root speed
 * to the simulation's (within PlayRateMin..Max), so the feet don't slide where the speeds differ.
 *
 * Owned by FAnimNode_PadelPlayerPose; Update and Evaluate run on the anim worker thread.
 */
USTRUCT()
struct PADEL_API FPadelMotionMatcher
{
	GENERATED_BODY()

	FPadelMotionMatcher();
	~FPadelMotionMatcher();
	FPadelMotionMatcher(const FPadelMotionMatcher& Other);
	FPadelMotionMatcher& operator=(const FPadelMotionMatcher& Other);

	/** The databases to search (Animation.json MotionMatchingDatabases), and the tuning. */
	void Configure(const TArray<TObjectPtr<const UPoseSearchDatabase>>& InDatabases, const TArray<EPadelMotionMatchingRole>& InRoles, const FPadelMotionMatchingTuning& InTuning);

	/** True once a search has found a pose: Evaluate then gives the locomotion pose. */
	bool HasPose() const { return !Stack.IsEmpty(); }

	/** Drops everything (a body that was hidden, or a new skeleton): the next search starts afresh. */
	void Reset();

	/**
	 * Advances the clips playing and searches. Trajectory is in the skeletal mesh component's
	 * world space (cm), the sample at time 0 the current one. SpeedUnits is the simulation's
	 * ground speed (cm/s), for the play rate.
	 */
	void Update(const FAnimationUpdateContext& Context, const FTransformTrajectory& Trajectory, double SpeedUnits);

	/** Writes the locomotion pose into Output (the pick, inertialized or crossfaded); false (Output untouched) with nothing picked yet. */
	bool Evaluate(FPoseContext& Output);

	/**
	 * Adds this frame's locomotion pose (the picks, or the loop mix before the first pick) to
	 * the pose history the next search reads: before swings and the body layers, so the
	 * search compares clip poses with clip poses.
	 */
	void RecordHistory(const FPoseContext& Locomotion);

	/** The database and clip of the latest pick (for tests and debugging); none before the first. */
	FName CurrentDatabase() const;
	const UAnimSequenceBase* CurrentClip() const;
	double CurrentPlayRate() const { return Stack.IsEmpty() ? 1.0 : Stack.Last().PlayRate; }
	/** M2 probes: how many picks since the last reset (each one a switch), and how many poses play at once now. */
	int32 NumPicks() const { return Picks; }
	int32 NumPlaying() const { return Stack.Num(); }
	/** M2: the playing clip's own root velocity (cm/s, root space = component space, at its play rate), for the warps. */
	FVector ClipRootVelocity() const { return RootVelocity; }

private:
	struct FPlaying
	{
		const UAnimSequenceBase* Clip = nullptr;
		const UPoseSearchDatabase* Database = nullptr;
		double Time = 0.0;
		double PlayRate = 1.0;
		/** 0..1 through its crossfade in. */
		double BlendIn = 1.0;
		bool bLoop = false;
	};

	void Search(const FAnimationUpdateContext& Context, const FTransformTrajectory& Trajectory, double SpeedUnits, bool bRunningOut, bool bForceSwitch);
	double MatchedPlayRate(const FPlaying& Playing, double SpeedUnits) const;
	void CacheHistoryBones();
	FVector MeasureRootVelocity(const FPlaying& Playing) const;
	void Inertialize(FPoseContext& Output);

	UPROPERTY() TArray<TObjectPtr<const UPoseSearchDatabase>> Databases;
	TArray<EPadelMotionMatchingRole> Roles;
	FPadelMotionMatchingTuning Tuning;
	TSharedPtr<UE::PoseSearch::FPoseHistory, ESPMode::ThreadSafe> History;
	TArray<FBoneIndexType> HistoryBones;
	TArray<FPlaying> Stack;
	double SinceSearchS = 0.0;
	int32 Picks = 0;
	FVector RootVelocity = FVector::ZeroVector;
	/** M2 inertialization: the pose shown last frame and the one before, and the decaying offset (rotation vectors and translations, per compact bone). */
	TArray<FTransform> LastOut;
	TArray<FTransform> PrevOut;
	TArray<FVector> OffRot, OffRotVel, OffPos, OffPosVel;
	bool bTransitionPending = false;
	float LastDeltaS = 0.0f;
	bool bHistoryReset = true;
	bool bHistoryBonesCached = false;
};
