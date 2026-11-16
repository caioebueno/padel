#include "Animation/PadelContactWarp.h"

#include "BoneContainer.h"
#include "TwoBoneIK.h"

namespace PadelContactWarpDetail
{
	const FVector FacingAxis(0.0, 1.0, 0.0); // the placeholder and every MetaHuman face +Y

	void SetCS(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Bone, const FTransform& Transform)
	{
		TArray<FBoneTransform> One;
		One.Add(FBoneTransform(Bone, Transform));
		CS.LocalBlendCSBoneTransforms(One, 1.0f);
	}

	bool Valid(FCompactPoseBoneIndex Bone)
	{
		return Bone.GetInt() != INDEX_NONE;
	}

	/** Two-bone IK that keeps the end bone's rotation and bends the middle joint towards Bend. */
	void SolveLimb(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C,
		const FVector& Effector, const FVector& Bend)
	{
		FTransform TA = CS.GetComponentSpaceTransform(A);
		FTransform TB = CS.GetComponentSpaceTransform(B);
		FTransform TC = CS.GetComponentSpaceTransform(C);
		const FQuat EndRotation = TC.GetRotation();
		const FVector JointTarget = TB.GetLocation() + Bend;
		AnimationCore::SolveTwoBoneIK(TA, TB, TC, JointTarget, Effector, false, 1.0, 1.0);
		TC.SetRotation(EndRotation);
		TArray<FBoneTransform> Solved;
		Solved.Add(FBoneTransform(A, TA));
		Solved.Add(FBoneTransform(B, TB));
		Solved.Add(FBoneTransform(C, TC));
		CS.LocalBlendCSBoneTransforms(Solved, 1.0f);
	}

	FVector FaceAt(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex Hand, const FVector& FaceInHand)
	{
		return CS.GetComponentSpaceTransform(Hand).TransformPosition(FaceInHand);
	}

	float Smooth(double T)
	{
		const double X = FMath::Clamp(T, 0.0, 1.0);
		return static_cast<float>(X * X * (3.0 - 2.0 * X));
	}
}

void FPadelWarpBones::Cache(const FBoneContainer& Bones, bool bInLeftHanded)
{
	bLeftHanded = bInLeftHanded;
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	auto Find = [&Bones, &Ref](const TCHAR* Name)
	{
		const int32 Mesh = Ref.FindBoneIndex(FName(Name));
		return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
	};
	Pelvis = Find(TEXT("pelvis"));
	Spine.Reset();
	for (const TCHAR* Name : { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05") })
	{
		const FCompactPoseBoneIndex Bone = Find(Name);
		if (Bone.GetInt() != INDEX_NONE)
		{
			Spine.Add(Bone);
		}
	}
	UpperArm = Find(bLeftHanded ? TEXT("upperarm_l") : TEXT("upperarm_r"));
	LowerArm = Find(bLeftHanded ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
	Hand = Find(bLeftHanded ? TEXT("hand_l") : TEXT("hand_r"));
	Thigh[0] = Find(TEXT("thigh_l"));
	Calf[0] = Find(TEXT("calf_l"));
	Foot[0] = Find(TEXT("foot_l"));
	Thigh[1] = Find(TEXT("thigh_r"));
	Calf[1] = Find(TEXT("calf_r"));
	Foot[1] = Find(TEXT("foot_r"));
}

void FPadelTwistBones::Cache(const FBoneContainer& Bones)
{
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FArm& Arm = Arms[Side];
		Arm = FArm();
		const TCHAR* Suffix = Side == 0 ? TEXT("_l") : TEXT("_r");
		const int32 HandMesh = Ref.FindBoneIndex(FName(FString(TEXT("hand")) + Suffix));
		if (HandMesh == INDEX_NONE)
		{
			continue;
		}
		// The hand's offset from the elbow, in the forearm's space: the twist axis and length.
		const FVector Forearm = RefPose[HandMesh].GetLocation();
		const double Length = Forearm.Size();
		for (const TCHAR* Twist : { TEXT("lowerarm_twist_01"), TEXT("lowerarm_twist_02") })
		{
			const int32 Mesh = Ref.FindBoneIndex(FName(FString(Twist) + Suffix));
			if (Mesh == INDEX_NONE || Ref.GetParentIndex(Mesh) != Ref.GetParentIndex(HandMesh) || Length < KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FCompactPoseBoneIndex Compact = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
			if (Compact.GetInt() == INDEX_NONE)
			{
				continue; // not in this LOD
			}
			Arm.Twist.Add(Compact);
			Arm.Share.Add(static_cast<float>(FMath::Clamp(FVector::DotProduct(RefPose[Mesh].GetLocation(), Forearm) / (Length * Length), 0.0, 1.0)));
		}
		Arm.Hand = Arm.Twist.Num() > 0 ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandMesh)) : FCompactPoseBoneIndex(INDEX_NONE);
	}
}

namespace PadelContactWarp
{
	double HandRoll(const FCompactPose& Pose, FCompactPoseBoneIndex Hand)
	{
		if (Hand.GetInt() == INDEX_NONE)
		{
			return 0.0;
		}
		const FTransform& Local = Pose[Hand];
		const FVector Axis = Local.GetLocation().GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			return 0.0;
		}
		// Relative to the reference pose, in the forearm's space.
		FQuat Swing, Twist;
		(Local.GetRotation() * Pose.GetRefPose(Hand).GetRotation().Inverse()).ToSwingTwist(Axis, Swing, Twist);
		return Twist.GetTwistAngle(Axis);
	}

	void DistributeForearmTwist(FCompactPose& Pose, const FPadelTwistBones& Bones)
	{
		for (const FPadelTwistBones::FArm& Arm : Bones.Arms)
		{
			if (Arm.Hand.GetInt() == INDEX_NONE)
			{
				continue;
			}
			const FTransform& HandLocal = Pose[Arm.Hand];
			const FVector Axis = HandLocal.GetLocation().GetSafeNormal();
			if (Axis.IsNearlyZero())
			{
				continue;
			}
			const double Roll = HandRoll(Pose, Arm.Hand);
			for (int32 I = 0; I < Arm.Twist.Num(); ++I)
			{
				const FCompactPoseBoneIndex Bone = Arm.Twist[I];
				const FQuat Base = Pose.GetRefPose(Bone).GetRotation();
				Pose[Bone].SetRotation(FQuat(Axis, Roll * Arm.Share[I]) * Base);
			}
		}
	}

	float BodyWeight(double SwingProgress)
	{
		using namespace PadelContactWarpDetail;
		if (SwingProgress <= 1.0 || SwingProgress >= 3.0)
		{
			return 0.0f;
		}
		return SwingProgress <= 2.0 ? Smooth(SwingProgress - 1.0) : Smooth(3.0 - SwingProgress);
	}

	FVector FaceInHand(const FTransform& RacketRelativeToHand, double FaceCentreUnits)
	{
		// SM_Racket's handle runs up its +Z from the grip centre (its origin) to the head.
		return RacketRelativeToHand.TransformPosition(FVector(0.0, 0.0, FaceCentreUnits));
	}

	void Apply(FCompactPose& Pose, const FPadelWarpBones& Bones, const FPadelWarpInput& In, const FPadelWarpTuning& Tuning)
	{
		using namespace PadelContactWarpDetail;
		if ((In.ArmAlpha <= 0.0f && In.BodyAlpha <= 0.0f) || !Bones.HasArm())
		{
			return;
		}
		FCSPose<FCompactPose> CS;
		CS.InitPose(Pose);
		double SpineUsed = 0.0;

		if (In.BodyAlpha > 0.0f)
		{
			const double W = FMath::Clamp(In.BodyAlpha, 0.0f, 1.0f);
			FTransform Feet[2];
			bool bLeg[2];
			for (int32 Side = 0; Side < 2; ++Side)
			{
				bLeg[Side] = Valid(Bones.Thigh[Side]) && Valid(Bones.Calf[Side]) && Valid(Bones.Foot[Side]);
				if (bLeg[Side])
				{
					Feet[Side] = CS.GetComponentSpaceTransform(Bones.Foot[Side]);
				}
			}

			// Knees: the pelvis goes down (and a little forward) for a low ball; the feet stay put.
			if (Valid(Bones.Pelvis))
			{
				const FVector D = In.TargetCS - FaceAt(CS, Bones.Hand, In.FaceInHand);
				const double Drop = FMath::Clamp(D.Z * Tuning.PelvisDownShare, -Tuning.MaxPelvisDropUnits, 0.0) * W;
				const double Ahead = FMath::Clamp(FVector::DotProduct(D, FacingAxis) * Tuning.PelvisForwardShare, -Tuning.MaxPelvisForwardUnits, Tuning.MaxPelvisForwardUnits) * W;
				FTransform Pelvis = CS.GetComponentSpaceTransform(Bones.Pelvis);
				Pelvis.AddToTranslation(FVector(0.0, 0.0, Drop) + FacingAxis * Ahead);
				SetCS(CS, Bones.Pelvis, Pelvis);
				for (int32 Side = 0; Side < 2; ++Side)
				{
					if (bLeg[Side])
					{
						SolveLimb(CS, Bones.Thigh[Side], Bones.Calf[Side], Bones.Foot[Side], Feet[Side].GetLocation(), FacingAxis * Tuning.KneeBendUnits);
					}
				}
			}

			// Trunk: turn the racket shoulder towards the ball (a backhand turns the shoulders).
			if (Bones.Spine.Num() > 0)
			{
				const FVector Pivot = CS.GetComponentSpaceTransform(Bones.Spine[0]).GetLocation();
				const FVector Shoulder = CS.GetComponentSpaceTransform(Bones.UpperArm).GetLocation();
				const FVector From = FVector(Shoulder.X - Pivot.X, Shoulder.Y - Pivot.Y, 0.0).GetSafeNormal();
				const FVector To = FVector(In.TargetCS.X - Pivot.X, In.TargetCS.Y - Pivot.Y, 0.0).GetSafeNormal();
				if (!From.IsNearlyZero() && !To.IsNearlyZero())
				{
					const double Full = FMath::Atan2(FVector::CrossProduct(From, To).Z, FVector::DotProduct(From, To));
					const double Yaw = FMath::Clamp(Full * Tuning.TrunkTurnShare, -FMath::DegreesToRadians(Tuning.MaxTrunkTurnDeg), FMath::DegreesToRadians(Tuning.MaxTrunkTurnDeg)) * W;
					if (FMath::Abs(Yaw) > KINDA_SMALL_NUMBER)
					{
						const FQuat Step(FVector::UpVector, Yaw / Bones.Spine.Num());
						for (const FCompactPoseBoneIndex Bone : Bones.Spine)
						{
							FTransform T = CS.GetComponentSpaceTransform(Bone);
							T.SetRotation(Step * T.GetRotation());
							SetCS(CS, Bone, T);
						}
					}
				}
			}

			// Spine: bend the shoulders towards the ball (side-bend and flexion), shared up the spine.
			if (Bones.Spine.Num() > 0)
			{
				const FVector D = In.TargetCS - FaceAt(CS, Bones.Hand, In.FaceInHand);
				const FVector Up(0.0, 0.0, 1.0);
				const FVector Along = FVector::DotProduct(D, FacingAxis) * FacingAxis;
				const FVector Side = D - Along - FVector::DotProduct(D, Up) * Up;
				const double Below = FMath::Min(FVector::DotProduct(D, Up), 0.0);
				const FVector Pivot = CS.GetComponentSpaceTransform(Bones.Spine[0]).GetLocation();
				const FVector Shoulder = CS.GetComponentSpaceTransform(Bones.UpperArm).GetLocation();
				const FVector Want = Shoulder + Side * Tuning.SpineSideShare + Along * Tuning.SpineForwardShare + Up * (Below * Tuning.SpineDownShare);
				FQuat Bend = FQuat::FindBetweenVectors(Shoulder - Pivot, Want - Pivot);
				FVector Axis;
				double Angle;
				Bend.ToAxisAndAngle(Axis, Angle);
				Angle = FMath::Min(Angle, FMath::DegreesToRadians(Tuning.MaxSpineDeg)) * W;
				SpineUsed = Angle;
				if (Angle > KINDA_SMALL_NUMBER && Axis.IsNormalized())
				{
					const FQuat Step(Axis, Angle / Bones.Spine.Num());
					for (const FCompactPoseBoneIndex Bone : Bones.Spine)
					{
						FTransform T = CS.GetComponentSpaceTransform(Bone);
						T.SetRotation(Step * T.GetRotation());
						SetCS(CS, Bone, T);
					}
				}
			}
		}

		// Arm: put the face centre (not the hand) on the target. Keep the hand's angle when the
		// arm can; past that, straighten the racket towards the ball along the arm's line, as a
		// stretched reach does, so the racket's length counts.
		auto ReachArm = [&]()
		{
			if (In.ArmAlpha <= 0.0f)
			{
				return;
			}
			const FTransform Upper = CS.GetComponentSpaceTransform(Bones.UpperArm);
			const FTransform Lower = CS.GetComponentSpaceTransform(Bones.LowerArm);
			const FTransform HandCS = CS.GetComponentSpaceTransform(Bones.Hand);
			const double Reach = (FVector::Dist(Upper.GetLocation(), Lower.GetLocation()) + FVector::Dist(Lower.GetLocation(), HandCS.GetLocation())) * Tuning.MaxReachShare;
			const FVector FaceOffset = HandCS.GetRotation().RotateVector(In.FaceInHand);
			FVector HandGoal = In.TargetCS - FaceOffset;
			FQuat Turn = FQuat::Identity;
			if (FVector::Dist(Upper.GetLocation(), HandGoal) > Reach && !FaceOffset.IsNearlyZero())
			{
				const FVector Line = (In.TargetCS - Upper.GetLocation()).GetSafeNormal();
				HandGoal = In.TargetCS - Line * FaceOffset.Size();
				Turn = FQuat::FindBetweenVectors(FaceOffset, Line * FaceOffset.Size());
			}
			const float A = FMath::Clamp(In.ArmAlpha, 0.0f, 1.0f);
			const FVector Effector = FMath::Lerp(HandCS.GetLocation(), HandGoal, A);
			// Keep the elbow bending the way the current pose bends it.
			const FVector Mid = (Upper.GetLocation() + HandCS.GetLocation()) * 0.5;
			const FVector Bend = (Lower.GetLocation() - Mid).GetSafeNormal() * Tuning.ElbowBendUnits;
			SolveLimb(CS, Bones.UpperArm, Bones.LowerArm, Bones.Hand, Effector, Bend);
			if (!Turn.Equals(FQuat::Identity))
			{
				FTransform Hand = CS.GetComponentSpaceTransform(Bones.Hand);
				Hand.SetRotation(FQuat::Slerp(FQuat::Identity, Turn, A) * Hand.GetRotation());
				SetCS(CS, Bones.Hand, Hand);
			}
		};
		ReachArm();

		// Past the arm's reach (a low backhand): bend the spine on towards what's still missing,
		// up to its limit, and reach again.
		if (In.BodyAlpha > 0.0f && In.ArmAlpha > 0.0f && Bones.Spine.Num() > 0)
		{
			const FVector Miss = In.TargetCS - FaceAt(CS, Bones.Hand, In.FaceInHand);
			const double Left = FMath::DegreesToRadians(Tuning.MaxSpineDeg) * In.BodyAlpha - SpineUsed;
			if (Miss.Size() > 1.0 && Left > KINDA_SMALL_NUMBER)
			{
				const FVector Pivot = CS.GetComponentSpaceTransform(Bones.Spine[0]).GetLocation();
				const FVector Shoulder = CS.GetComponentSpaceTransform(Bones.UpperArm).GetLocation();
				FVector Axis;
				double Angle;
				FQuat::FindBetweenVectors(Shoulder - Pivot, Shoulder + Miss - Pivot).ToAxisAndAngle(Axis, Angle);
				Angle = FMath::Min(Angle, Left);
				if (Angle > KINDA_SMALL_NUMBER && Axis.IsNormalized())
				{
					const FQuat Step(Axis, Angle / Bones.Spine.Num());
					for (const FCompactPoseBoneIndex Bone : Bones.Spine)
					{
						FTransform T = CS.GetComponentSpaceTransform(Bone);
						T.SetRotation(Step * T.GetRotation());
						SetCS(CS, Bone, T);
					}
					ReachArm();
				}
			}
		}

		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Pose);
	}
}
