// M2 locomotion warping (docs/animation/MOVEMENT_FLUENCY.md): fits the motion matched legs to
// what the simulation does when it brakes, turns and changes speed harder than the clips.
//
// The simulation leads (root motion isn't used), so where the playing clip's own root motion
// and the simulation's velocity disagree, the feet skate. Three fixes on the locomotion pose,
// in this order, in the spirit of the engine's Animation Warping nodes (orientation warping,
// stride warping, foot locking) but in C++ on our own pose node:
//
// 1. Orientation warping: the hips turn toward the simulation's direction of travel from the
//    clip's (at most OrientationMaxDeg), the spine turning back so the chest stays.
// 2. Stride warping: each foot's reach along the direction of travel scales by the ratio of the
//    simulation's speed to the clip's, beyond what the play rate already matched.
// 3. Foot locking: a foot the clip plants (low, and still in the clip's own world) stays where
//    it landed in the world; the leg reaches it by IK. It lets go when the clip lifts it, the
//    leg would drift past FootLockMaxDrift, or the foot moves in the clip, blending out.
//
// Engine-side state, worker thread, owned by FAnimNode_PadelPlayerPose.

#pragma once

#include "CoreMinimal.h"
#include "Animation/PadelBodyDynamics.h"
#include "BonePose.h"

/** Animation.json OrientationWarp*, StrideWarp*, FootLock*, in Unreal units and degrees. A zero max (or StrideWarpMin = Max = 1) turns that part off. */
struct FPadelLocomotionWarpTuning
{
	double OrientationMaxDeg = 0.0;
	double OrientationHalfLifeS = 0.08;
	/** Both the simulation and the clip moving at least this fast (cm/s) for a direction to mean anything. */
	double OrientationMinSpeedUnits = 50.0;
	/** Of the hips' turn, how much the spine turns back (1: the chest stays where the clip has it). */
	double OrientationSpineCounter = 1.0;

	double StrideMin = 1.0;
	double StrideMax = 1.0;
	double StrideHalfLifeS = 0.08;
	double StrideMinSpeedUnits = 50.0;

	/** The ankle at most this far (cm) above its height standing, and slower than FootLockSpeedUnits in the clip's own world, locks. 0 turns locking off. */
	double FootLockHeightUnits = 0.0;
	double FootLockSpeedUnits = 30.0;
	/** Lets go above this height or speed. */
	double FootLockReleaseHeightUnits = 6.0;
	double FootLockReleaseSpeedUnits = 60.0;
	/** Lets go when the locked spot is this far (cm) from where the clip has the foot. */
	double FootLockMaxDriftUnits = 20.0;
	double FootLockBlendOutS = 0.12;

	bool Any() const { return OrientationMaxDeg > 0.0 || StrideMin != 1.0 || StrideMax != 1.0 || FootLockHeightUnits > 0.0; }
};

/** What the warp needs each frame. Velocities in component space, cm/s. */
struct FPadelLocomotionWarpInput
{
	FTransform ComponentToWorld = FTransform::Identity;
	FVector SimVelocity = FVector::ZeroVector;
	FVector ClipRootVelocity = FVector::ZeroVector;
	double DeltaS = 0.0;
	/** 0..1: how much the locomotion owns the legs (a full-body swing takes them). */
	double Weight = 1.0;
};

struct PADEL_API FPadelLocomotionWarp
{
	/** Drops all state (a new pick source, a teleport, the loop mix taking over). */
	void Reset();

	/** Warps Pose in place (local in, local out). Bones from FPadelBodyBones::Cache. */
	void Apply(FCompactPose& Pose, const FPadelBodyBones& Bones, const FPadelLocomotionWarpInput& In, const FPadelLocomotionWarpTuning& Tuning);

	/** For tests: whether a foot (0 left, 1 right) is locked now, and the hips' current turn. */
	bool IsLocked(int32 Foot) const { return Feet[Foot].bLocked; }
	double OrientationDeg() const { return Orientation; }
	double StrideScale() const { return Stride; }

private:
	struct FFoot
	{
		bool bLocked = false;
		double Alpha = 0.0;
		FVector LockWorld = FVector::ZeroVector;
		FVector LastClipCS = FVector::ZeroVector;
		bool bHasLast = false;
	};
	FFoot Feet[2];
	double Orientation = 0.0;
	double OrientationRate = 0.0;
	double Stride = 1.0;
	double StrideRate = 0.0;
	double RestAnkleHeight[2] = { -1.0, -1.0 };
};
