#include "Animation/PadelBodyDynamics.h"

#include "BoneContainer.h"
#include "TwoBoneIK.h"

namespace PadelBodyDynamicsDetail
{
	const FVector FacingAxis(0.0, 1.0, 0.0); // the placeholder and every MetaHuman face +Y

	bool Valid(FCompactPoseBoneIndex Bone)
	{
		return Bone.GetInt() != INDEX_NONE;
	}

	void SetCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Bone, const FTransform& Transform)
	{
		TArray<FBoneTransform> One;
		One.Add(FBoneTransform(Bone, Transform));
		CS.LocalBlendCSBoneTransforms(One, 1.0f);
	}

	/** Rotates each bone in Chain by an equal share of Rotation (component space), so the bend is spread along it. */
	void RotateChain(FCSPose<FCompactPose>& CS, const TArray<FCompactPoseBoneIndex>& Chain, const FVector& Axis, double AngleRad)
	{
		if (Chain.Num() == 0 || FMath::Abs(AngleRad) < KINDA_SMALL_NUMBER || !Axis.IsNormalized())
		{
			return;
		}
		const FQuat Step(Axis, AngleRad / Chain.Num());
		for (const FCompactPoseBoneIndex Bone : Chain)
		{
			FTransform T = CS.GetComponentSpaceTransform(Bone);
			T.SetRotation(Step * T.GetRotation());
			SetCS(CS, Bone, T);
		}
	}

	/** Two-bone leg IK onto Effector, keeping the foot's rotation and the knee bending the way it already does. */
	void SolveLeg(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Thigh, FCompactPoseBoneIndex Calf, FCompactPoseBoneIndex Foot, const FVector& Effector)
	{
		FTransform TA = CS.GetComponentSpaceTransform(Thigh);
		FTransform TB = CS.GetComponentSpaceTransform(Calf);
		FTransform TC = CS.GetComponentSpaceTransform(Foot);
		const FQuat FootRotation = TC.GetRotation();
		FVector Bend = TB.GetLocation() - 0.5 * (TA.GetLocation() + TC.GetLocation());
		if (Bend.SizeSquared() < 1.0)
		{
			Bend = FacingAxis * 10.0;
		}
		AnimationCore::SolveTwoBoneIK(TA, TB, TC, TB.GetLocation() + Bend, Effector, false, 1.0, 1.0);
		TC.SetRotation(FootRotation);
		TArray<FBoneTransform> Solved;
		Solved.Add(FBoneTransform(Thigh, TA));
		Solved.Add(FBoneTransform(Calf, TB));
		Solved.Add(FBoneTransform(Foot, TC));
		CS.LocalBlendCSBoneTransforms(Solved, 1.0f);
	}
}

void FPadelBodyBones::Cache(const FBoneContainer& Bones)
{
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	auto Find = [&Bones, &Ref](const TCHAR* Name)
	{
		const int32 Mesh = Ref.FindBoneIndex(FName(Name));
		return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
	};
	auto AddAll = [&Find](TArray<FCompactPoseBoneIndex>& Out, std::initializer_list<const TCHAR*> Names)
	{
		Out.Reset();
		for (const TCHAR* Name : Names)
		{
			const FCompactPoseBoneIndex Bone = Find(Name);
			if (Bone.GetInt() != INDEX_NONE)
			{
				Out.Add(Bone);
			}
		}
	};
	Pelvis = Find(TEXT("pelvis"));
	AddAll(Spine, { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") });
	AddAll(NeckAndHead, { TEXT("neck_01"), TEXT("neck_02"), TEXT("head") });
	Thigh[0] = Find(TEXT("thigh_l"));
	Calf[0] = Find(TEXT("calf_l"));
	Foot[0] = Find(TEXT("foot_l"));
	Thigh[1] = Find(TEXT("thigh_r"));
	Calf[1] = Find(TEXT("calf_r"));
	Foot[1] = Find(TEXT("foot_r"));
}

namespace PadelBodyDynamics
{
	void Apply(FCompactPose& Pose, const FPadelBodyBones& Bones, const FPadelBodyDynamics& Body)
	{
		using namespace PadelBodyDynamicsDetail;
		if (Body.IsZero() || !Valid(Bones.Pelvis))
		{
			return;
		}
		FCSPose<FCompactPose> CS;
		CS.InitPose(Pose);

		bool bLeg[2];
		FVector Feet[2];
		for (int32 Side = 0; Side < 2; ++Side)
		{
			bLeg[Side] = Valid(Bones.Thigh[Side]) && Valid(Bones.Calf[Side]) && Valid(Bones.Foot[Side]);
			Feet[Side] = bLeg[Side] ? CS.GetComponentSpaceTransform(Bones.Foot[Side]).GetLocation() : FVector::ZeroVector;
		}
		// The body's right, from the hips (it doesn't depend on which way a skeleton's X points).
		FVector Right = bLeg[0] && bLeg[1]
			? CS.GetComponentSpaceTransform(Bones.Thigh[1]).GetLocation() - CS.GetComponentSpaceTransform(Bones.Thigh[0]).GetLocation()
			: FVector::ZeroVector;
		Right.Z = 0.0;
		Right = Right.GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			Right = FVector::CrossProduct(FVector::UpVector, FacingAxis); // UE is left-handed: up x forward = right
		}

		// Pelvis: the hop lifts everything; the drop is taken by the knees, feet where they were
		// (or set wider apart by the stance, half each side: foot_l to the left, foot_r right).
		const float Drop = FMath::Min(Body.PelvisDropUnits, 0.0f);
		const float Lift = FMath::Max(Body.HopLiftUnits, 0.0f);
		const float Widen = FMath::Max(Body.FeetWidenUnits, 0.0f);
		if (Drop != 0.0f || Lift != 0.0f || Widen != 0.0f)
		{
			FTransform Pelvis = CS.GetComponentSpaceTransform(Bones.Pelvis);
			Pelvis.AddToTranslation(FVector(0.0, 0.0, Drop + Lift));
			SetCS(CS, Bones.Pelvis, Pelvis);
			if (Drop != 0.0f || Widen != 0.0f)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					if (bLeg[Side])
					{
						const FVector Out = Right * (Side == 0 ? -0.5 : 0.5) * Widen;
						SolveLeg(CS, Bones.Thigh[Side], Bones.Calf[Side], Bones.Foot[Side], Feet[Side] + Out + FVector(0.0, 0.0, Lift));
					}
				}
			}
		}

		// Trunk lean: up toward forward (a start) or back (a brake), and toward the body's right
		// or left. Rotating about up x dir carries up toward dir in Unreal's left-handed axes.
		RotateChain(CS, Bones.Spine, FVector::CrossProduct(FVector::UpVector, FacingAxis).GetSafeNormal(), FMath::DegreesToRadians(Body.LeanForwardDeg));
		RotateChain(CS, Bones.Spine, FVector::CrossProduct(FVector::UpVector, Right).GetSafeNormal(), FMath::DegreesToRadians(Body.LeanRightDeg));

		// Head pitch, before the twists so the look turns with them: up toward the face carries
		// the face (forward) toward up, the opposite way to a forward lean.
		RotateChain(CS, Bones.NeckAndHead, FVector::CrossProduct(FVector::UpVector, FacingAxis).GetSafeNormal(), -FMath::DegreesToRadians(Body.HeadPitchDeg));

		// Facing lead: positive yaw is the same way as FacingYawDeg (FRotator yaw, about +Z).
		RotateChain(CS, Bones.Spine, FVector::UpVector, FMath::DegreesToRadians(Body.ChestTwistDeg));
		RotateChain(CS, Bones.NeckAndHead, FVector::UpVector, FMath::DegreesToRadians(Body.HeadTwistDeg));

		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Pose);
	}
}
