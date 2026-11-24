#include "Animation/PadelLocomotionWarp.h"

#include "BoneContainer.h"

#include <cmath>

namespace PadelLocomotionWarpDetail
{
	bool Valid(FCompactPoseBoneIndex Bone)
	{
		return Bone.GetInt() != INDEX_NONE;
	}

	/** A critically damped spring of HalfLifeS carrying X (with velocity V) toward Goal over Dt, exactly. */
	void SpringTo(double& X, double& V, double Goal, double HalfLifeS, double Dt)
	{
		const double Y = 0.5 * (4.0 * 0.69314718056) / (HalfLifeS + 1e-5);
		const double J0 = X - Goal;
		const double J1 = V + J0 * Y;
		const double EyDt = std::exp(-Y * Dt);
		X = EyDt * (J0 + J1 * Dt) + Goal;
		V = EyDt * (V - J1 * Y * Dt);
	}

	/** A bone's height in component space in the reference pose. */
	double RefHeight(const FCompactPose& Pose, FCompactPoseBoneIndex Bone)
	{
		FTransform CS = FTransform::Identity;
		for (FCompactPoseBoneIndex At = Bone; At.GetInt() != INDEX_NONE; At = Pose.GetParentBoneIndex(At))
		{
			CS = CS * Pose.GetRefPose(At);
		}
		return CS.GetLocation().Z;
	}

	void SetCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Bone, const FTransform& Transform)
	{
		TArray<FBoneTransform> One;
		One.Add(FBoneTransform(Bone, Transform));
		CS.LocalBlendCSBoneTransforms(One, 1.0f);
	}
}

void FPadelLocomotionWarp::Reset()
{
	for (FFoot& Foot : Feet)
	{
		Foot = FFoot();
	}
	Orientation = OrientationRate = 0.0;
	Stride = 1.0;
	StrideRate = 0.0;
}

void FPadelLocomotionWarp::Apply(FCompactPose& Pose, const FPadelBodyBones& Bones, const FPadelLocomotionWarpInput& In, const FPadelLocomotionWarpTuning& Tuning)
{
	using namespace PadelLocomotionWarpDetail;
	if (!Tuning.Any() || !Valid(Bones.Pelvis) || In.DeltaS <= 0.0)
	{
		return;
	}
	bool bLeg[2];
	for (int32 Side = 0; Side < 2; ++Side)
	{
		bLeg[Side] = Valid(Bones.Thigh[Side]) && Valid(Bones.Calf[Side]) && Valid(Bones.Foot[Side]);
		if (bLeg[Side] && RestAnkleHeight[Side] < 0.0)
		{
			RestAnkleHeight[Side] = RefHeight(Pose, Bones.Foot[Side]);
		}
	}
	const double Dt = In.DeltaS;
	const double Weight = FMath::Clamp(In.Weight, 0.0, 1.0);
	const FVector Sim(In.SimVelocity.X, In.SimVelocity.Y, 0.0);
	const FVector Clip(In.ClipRootVelocity.X, In.ClipRootVelocity.Y, 0.0);
	const double SimSpeed = Sim.Size();
	const double ClipSpeed = Clip.Size();

	FCSPose<FCompactPose> CS;
	CS.InitPose(Pose);

	// The clip's own foot positions, before any warp: whether the clip plants them.
	FVector ClipFoot[2];
	double ClipFootSpeed[2] = { 0.0, 0.0 };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (!bLeg[Side])
		{
			continue;
		}
		ClipFoot[Side] = CS.GetComponentSpaceTransform(Bones.Foot[Side]).GetLocation();
		FFoot& Foot = Feet[Side];
		// A planted foot in a root-locked clip moves back at the clip's root speed: add it back.
		ClipFootSpeed[Side] = Foot.bHasLast ? ((ClipFoot[Side] - Foot.LastClipCS) / Dt + Clip).Size2D() : 0.0;
		Foot.LastClipCS = ClipFoot[Side];
		Foot.bHasLast = true;
	}

	// 1. Orientation: the hips toward the simulation's travel, the spine back.
	double OrientationGoal = 0.0;
	if (Tuning.OrientationMaxDeg > 0.0 && SimSpeed >= Tuning.OrientationMinSpeedUnits && ClipSpeed >= Tuning.OrientationMinSpeedUnits)
	{
		const double Delta = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Clip, Sim).Z, FVector::DotProduct(Clip, Sim)));
		OrientationGoal = FMath::Clamp(Delta, -Tuning.OrientationMaxDeg, Tuning.OrientationMaxDeg);
	}
	SpringTo(Orientation, OrientationRate, OrientationGoal * Weight, Tuning.OrientationHalfLifeS, Dt);
	if (FMath::Abs(Orientation) > 0.01)
	{
		const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(Orientation));
		FTransform Pelvis = CS.GetComponentSpaceTransform(Bones.Pelvis);
		Pelvis.SetRotation(Turn * Pelvis.GetRotation());
		SetCS(CS, Bones.Pelvis, Pelvis);
		if (!Bones.Spine.IsEmpty() && Tuning.OrientationSpineCounter > 0.0)
		{
			// Back along the spine, shared out, so the chest comes back to the clip's facing.
			const FQuat Back(FVector::UpVector, -FMath::DegreesToRadians(Orientation) * Tuning.OrientationSpineCounter / Bones.Spine.Num());
			for (const FCompactPoseBoneIndex Bone : Bones.Spine)
			{
				FTransform T = CS.GetComponentSpaceTransform(Bone);
				T.SetRotation(Back * T.GetRotation());
				SetCS(CS, Bone, T);
			}
		}
	}

	// 2. Stride: each foot's reach along the travel scales with the speed the clip falls short by.
	double StrideGoal = 1.0;
	if (SimSpeed >= Tuning.StrideMinSpeedUnits && ClipSpeed >= Tuning.StrideMinSpeedUnits)
	{
		StrideGoal = FMath::Clamp(SimSpeed / ClipSpeed, Tuning.StrideMin, Tuning.StrideMax);
	}
	SpringTo(Stride, StrideRate, FMath::Lerp(1.0, StrideGoal, Weight), Tuning.StrideHalfLifeS, Dt);
	FVector Target[2];
	const FVector Along = Sim.GetSafeNormal();
	const FVector Hips = CS.GetComponentSpaceTransform(Bones.Pelvis).GetLocation();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (!bLeg[Side])
		{
			continue;
		}
		Target[Side] = CS.GetComponentSpaceTransform(Bones.Foot[Side]).GetLocation();
		if (FMath::Abs(Stride - 1.0) > 1e-3 && !Along.IsNearlyZero())
		{
			// Not on a nearly straight leg: there the knee's bend has no steady way, and shortening a
			// sprinting leg at push-off (where the clip's knee passes through straight) swung the knee
			// 27° in a frame (Padel.Animation.MotionMatchingReady, the run back).
			const FVector ThighAt = CS.GetComponentSpaceTransform(Bones.Thigh[Side]).GetLocation();
			const FVector KneeAt = CS.GetComponentSpaceTransform(Bones.Calf[Side]).GetLocation();
			const double Full = FVector::Dist(ThighAt, KneeAt) + FVector::Dist(KneeAt, Target[Side]);
			const double Straight = Full > 1.0 ? FVector::Dist(ThighAt, Target[Side]) / Full : 0.0;
			const double Share = FMath::Min(Tuning.StrideStraightLegShare, 1.0 - 1e-3);
			const double Keep = 1.0 - FMath::SmoothStep(Share, 1.0, Straight);
			const double Reach = FVector::DotProduct(Target[Side] - Hips, Along);
			Target[Side] += Along * (Reach * (Stride - 1.0) * Keep);
		}
	}

	// 3. Foot locking.
	const FTransform WorldToComponent = In.ComponentToWorld.Inverse();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FFoot& Foot = Feet[Side];
		if (!bLeg[Side] || Tuning.FootLockHeightUnits <= 0.0)
		{
			Foot.bLocked = false;
			Foot.Alpha = 0.0;
			continue;
		}
		const double Height = ClipFoot[Side].Z - RestAnkleHeight[Side];
		const FVector LockedCS = WorldToComponent.TransformPosition(Foot.LockWorld);
		if (Foot.bLocked)
		{
			const bool bLift = Height > Tuning.FootLockReleaseHeightUnits || ClipFootSpeed[Side] > Tuning.FootLockReleaseSpeedUnits;
			const bool bDrift = FVector::Dist(LockedCS, Target[Side]) > Tuning.FootLockMaxDriftUnits;
			if (bLift || bDrift || Weight <= 0.0)
			{
				Foot.bLocked = false;
			}
		}
		else if (Foot.Alpha <= 0.0 && Weight > 0.0 && Height < Tuning.FootLockHeightUnits && ClipFootSpeed[Side] < Tuning.FootLockSpeedUnits)
		{
			// Lands: lock where it is now (warped), so nothing jumps.
			Foot.bLocked = true;
			Foot.LockWorld = In.ComponentToWorld.TransformPosition(Target[Side]);
		}
		if (Foot.bLocked)
		{
			Foot.Alpha = 1.0;
		}
		else
		{
			Foot.Alpha = FMath::Max(0.0, Foot.Alpha - (Tuning.FootLockBlendOutS > 0.0 ? Dt / Tuning.FootLockBlendOutS : 1.0));
		}
		if (Foot.Alpha > 0.0)
		{
			const double Ease = Foot.Alpha * Foot.Alpha * (3.0 - 2.0 * Foot.Alpha);
			Target[Side] = FMath::Lerp(Target[Side], WorldToComponent.TransformPosition(Foot.LockWorld), Ease * Weight);
		}
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (bLeg[Side] && !Target[Side].Equals(CS.GetComponentSpaceTransform(Bones.Foot[Side]).GetLocation(), 0.01))
		{
			PadelBodyDynamics::SolveLeg(CS, Bones.Thigh[Side], Bones.Calf[Side], Bones.Foot[Side], Target[Side]);
		}
	}
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Pose);
}
