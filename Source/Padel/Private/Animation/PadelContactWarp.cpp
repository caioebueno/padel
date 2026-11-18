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

	/**
	 * Two-bone IK that keeps the end bone's rotation and bends the middle joint towards Bend.
	 * Hinge (an arm's, from the reference pose): both bones then take their roll from the solved
	 * elbow plane, not from the pose.
	 */
	void SolveLimb(FCSPose<FCompactPose>& CS, FCompactPoseBoneIndex A, FCompactPoseBoneIndex B, FCompactPoseBoneIndex C,
		const FVector& Effector, const FVector& Bend, const FPadelWarpBones* Hinge = nullptr)
	{
		FTransform TA = CS.GetComponentSpaceTransform(A);
		FTransform TB = CS.GetComponentSpaceTransform(B);
		FTransform TC = CS.GetComponentSpaceTransform(C);
		const FQuat EndRotation = TC.GetRotation();
		// The middle bone as the pose holds it relative to the first, and its direction in its own space.
		const FQuat MiddleInFirst = TA.GetRotation().Inverse() * TB.GetRotation();
		const FVector MiddleAlong = TB.GetRotation().UnrotateVector(TC.GetLocation() - TB.GetLocation()).GetSafeNormal();
		const FVector JointTarget = TB.GetLocation() + Bend;
		AnimationCore::SolveTwoBoneIK(TA, TB, TC, JointTarget, Effector, false, 1.0, 1.0);
		// The solver turns each bone the shortest way from its posed direction. Posed opposite to
		// where the IK puts it (an arm swung back in a run, hanging before an overhead) there is
		// no clear shortest way, and the upper arm and forearm rolled up to 160 degrees in a frame.
		const FVector SolvedUpper = (TB.GetLocation() - TA.GetLocation()).GetSafeNormal();
		const FVector SolvedLower = (TC.GetLocation() - TB.GetLocation()).GetSafeNormal();
		if (Hinge && Hinge->HasHinge() && !SolvedUpper.IsNearlyZero() && !SolvedLower.IsNearlyZero())
		{
			// An arm: the elbow is a hinge fixed in both bones, so their roll is the solved elbow
			// plane's. A little (a few degrees' worth) of the bend target's plane is mixed in so a
			// straight arm keeps one; the forearm turns back from the elbow's side, hence the minus.
			const FVector Pole = (Bend - FVector::DotProduct(Bend, SolvedUpper) * SolvedUpper).GetSafeNormal();
			const FVector Axis = (FVector::CrossProduct(SolvedUpper, SolvedLower) - FVector::CrossProduct(SolvedUpper, Pole) * 0.05).GetSafeNormal();
			if (!Axis.IsNearlyZero())
			{
				TA.SetRotation((FRotationMatrix::MakeFromXZ(SolvedUpper, Axis).ToQuat() * FRotationMatrix::MakeFromXZ(Hinge->UpperArmAlong, Hinge->UpperArmHinge).ToQuat().Inverse()).GetNormalized());
				TB.SetRotation((FRotationMatrix::MakeFromXZ(SolvedLower, Axis).ToQuat() * FRotationMatrix::MakeFromXZ(Hinge->LowerArmAlong, Hinge->LowerArmHinge).ToQuat().Inverse()).GetNormalized());
			}
		}
		else if (!MiddleAlong.IsNearlyZero() && !SolvedLower.IsNearlyZero())
		{
			// A leg: carry the middle bone on the solved first one, then bend it at the joint.
			const FQuat Carried = TA.GetRotation() * MiddleInFirst;
			TB.SetRotation((FQuat::FindBetweenNormals(Carried.RotateVector(MiddleAlong), SolvedLower) * Carried).GetNormalized());
		}
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

	/**
	 * How much of an overhead a target is: blended in over OverheadBlendUnits above the standing
	 * shoulder (or the live one, when the body has risen above it). Keyed to the live shoulder
	 * alone, a stop's crouch (the shoulder dropping 6 cm a frame) took a racket carried at the
	 * waist into the overhead arm shape and swung the elbow up in a frame.
	 */
	double OverheadWeight(const FVector& TargetCS, const FVector& Shoulder, const FPadelWarpBones& Bones, const FPadelWarpTuning& Tuning)
	{
		const double From = FMath::Max(Shoulder.Z, Bones.StandingShoulderZ);
		return Smooth((TargetCS.Z - From) / FMath::Max(Tuning.OverheadBlendUnits, 1.0));
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
	auto StandingZ = [&Ref](const TCHAR* Name)
	{
		const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
		FTransform Standing = FTransform::Identity;
		for (int32 Mesh = Ref.FindBoneIndex(FName(Name)); Mesh != INDEX_NONE; Mesh = Ref.GetParentIndex(Mesh))
		{
			Standing = Standing * RefPose[Mesh];
		}
		return Standing.GetLocation().Z;
	};
	StandingShoulderZ = StandingZ(bLeftHanded ? TEXT("upperarm_l") : TEXT("upperarm_r"));
	StandingPelvisZ = StandingZ(TEXT("pelvis"));
	UpperArmAlong = UpperArmHinge = LowerArmAlong = LowerArmHinge = FVector::ZeroVector;
	{
		const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
		const int32 UpperMesh = Ref.FindBoneIndex(FName(bLeftHanded ? TEXT("upperarm_l") : TEXT("upperarm_r")));
		const int32 LowerMesh = Ref.FindBoneIndex(FName(bLeftHanded ? TEXT("lowerarm_l") : TEXT("lowerarm_r")));
		const int32 HandMesh = Ref.FindBoneIndex(FName(bLeftHanded ? TEXT("hand_l") : TEXT("hand_r")));
		if (UpperMesh != INDEX_NONE && LowerMesh != INDEX_NONE && HandMesh != INDEX_NONE
			&& Ref.GetParentIndex(LowerMesh) == UpperMesh && Ref.GetParentIndex(HandMesh) == LowerMesh)
		{
			// In the upper arm's space: the bone, the forearm, and the hinge across the two.
			const FQuat LowerInUpper = RefPose[LowerMesh].GetRotation();
			const FVector Along = RefPose[LowerMesh].GetLocation().GetSafeNormal();
			const FVector Forearm = LowerInUpper.RotateVector(RefPose[HandMesh].GetLocation()).GetSafeNormal();
			const FVector Hinge = FVector::CrossProduct(Along, Forearm);
			// A degree of bend is enough to tell the plane; a straight reference arm says nothing.
			if (!Along.IsNearlyZero() && !Forearm.IsNearlyZero() && Hinge.Size() > 0.02)
			{
				UpperArmAlong = Along;
				UpperArmHinge = Hinge.GetSafeNormal();
				LowerArmAlong = RefPose[HandMesh].GetLocation().GetSafeNormal();
				LowerArmHinge = LowerInUpper.UnrotateVector(UpperArmHinge);
			}
		}
	}
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

	void Apply(FCompactPose& Pose, const FPadelWarpBones& Bones, const FPadelWarpInput& Request, const FPadelWarpTuning& Tuning)
	{
		using namespace PadelContactWarpDetail;
		if ((Request.ArmAlpha <= 0.0f && Request.BodyAlpha <= 0.0f) || !Bones.HasArm())
		{
			return;
		}
		FCSPose<FCompactPose> CS;
		CS.InitPose(Pose);
		FPadelWarpInput In = Request;
		if (In.CarryFollow > 0.0f && Valid(Bones.Pelvis))
		{
			// The racket is carried at a height set for the standing body. When the clip dips the
			// hips (a stop sinks them 25 cm in a few frames), the hands go down with them; held
			// at the old height, the arm had to swing up through 70 degrees as the shoulder fell.
			const double Dip = FMath::Max(Bones.StandingPelvisZ - CS.GetComponentSpaceTransform(Bones.Pelvis).GetLocation().Z, 0.0);
			In.TargetCS.Z -= Dip * FMath::Clamp(In.CarryFollow, 0.0f, 1.0f);
		}
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
			const FVector Shoulder = Upper.GetLocation();
			const double Reach = (FVector::Dist(Shoulder, Lower.GetLocation()) + FVector::Dist(Lower.GetLocation(), HandCS.GetLocation())) * Tuning.MaxReachShare;
			const FVector FaceOffset = HandCS.GetRotation().RotateVector(In.FaceInHand);
			const double Face = FaceOffset.Size();
			const FVector ToTarget = In.TargetCS - Shoulder;
			const double Distance = ToTarget.Size();
			const FVector Line = ToTarget.GetSafeNormal();
			// Overheads (target above the shoulder): a long arm, the elbow up and out to the racket
			// side. Blended in by height so forehands and backhands are as before.
			const double Overhead = OverheadWeight(In.TargetCS, Shoulder, Bones, Tuning);
			FVector Out = Shoulder - (Bones.Spine.Num() > 0 ? CS.GetComponentSpaceTransform(Bones.Spine.Last()).GetLocation() : Shoulder);
			Out.Z = 0.0;
			Out = Out.GetSafeNormal();
			FVector UpOut = (FVector::UpVector + Out).GetSafeNormal();
			UpOut = (UpOut - FVector::DotProduct(UpOut, Line) * Line).GetSafeNormal();

			// Down and out from the shoulder, square to the reach: the arm's steady fallback when
			// the pose gives no clear direction (a straight elbow, a hand at the shoulder).
			FVector DownOut = (Out - FVector::UpVector).GetSafeNormal();
			DownOut = (DownOut - FVector::DotProduct(DownOut, Line) * Line).GetSafeNormal();

			FVector HandGoal = In.TargetCS - FaceOffset;
			// Where the hand can be at full reach with the racket's face still on the target: the
			// circle where the reach sphere (shoulder) meets the racket sphere (target). Out of
			// reach altogether it closes to one point, the racket along the arm's line.
			const bool bCircle = Face > KINDA_SMALL_NUMBER && Distance > KINDA_SMALL_NUMBER;
			const double Along = bCircle ? (Reach * Reach - Face * Face + Distance * Distance) / (2.0 * Distance) : 0.0;
			const double Across = bCircle && Distance < Reach + Face ? FMath::Sqrt(FMath::Max(Reach * Reach - Along * Along, 0.0)) : 0.0;
			const FVector CircleCentre = bCircle && Distance < Reach + Face ? Shoulder + Line * Along : In.TargetCS - Line * Face;
			bool bAim = false;
			// How much the hand's angle comes from the forearm rather than the pose (see the aim below).
			double AimBlend = 0.0;
			const double Over = FVector::Dist(Shoulder, HandGoal) - Reach;
			if (bCircle && Over > 0.0)
			{
				// Past the arm's reach: the hand goes to the point of that circle nearest where it
				// was heading, and the racket turns onto the target. Where the hand just reaches,
				// that point is the hand goal itself, so the arm doesn't jump as the reach runs out
				// (it used to snap to the arm's line, folding the arm in one frame).
				FVector Radial = HandGoal - CircleCentre;
				Radial -= FVector::DotProduct(Radial, Line) * Line;
				HandGoal = CircleCentre + (Radial.IsNearlyZero() ? FVector::ZeroVector : Radial.GetSafeNormal() * Across);
				bAim = true;
				AimBlend = FMath::Max(AimBlend, static_cast<double>(Smooth(Over / Face)));
			}
			// Too close: a target near the shoulder (a pivot lunges the body at the carried racket,
			// a backhand's windup sweeps the face across the chest) would put the hand on the
			// shoulder, where a few centimetres turn the whole arm. Keep the hand WarpMinReachShare
			// of the arm out, straight out from the shoulder, so it moves no faster than its goal;
			// where that direction fades (the goal at the shoulder) it leans to down, the hand low
			// in front, and the racket turns up at the target. (Placing it on the circle where the
			// racket still reaches the target swung the hand round the shoulder in a few frames.)
			const double MinReach = Reach * Tuning.MinReachShare;
			const double Under = MinReach - FVector::Dist(Shoulder, HandGoal);
			if (Under > 0.0 && MinReach > KINDA_SMALL_NUMBER)
			{
				const FVector Way = (HandGoal - Shoulder + FVector(0.0, 0.0, -1.0) * Under).GetSafeNormal();
				if (!Way.IsNearlyZero())
				{
					HandGoal = Shoulder + Way * MinReach;
					bAim = true;
					AimBlend = FMath::Max(AimBlend, static_cast<double>(Smooth(Under / MinReach)));
				}
			}
			if (Overhead > 0.0 && bCircle)
			{
				// Overheads: a long arm, the hand on the circle's up-and-out side.
				const FVector Stretched = CircleCentre + UpOut * Across;
				HandGoal = FMath::Lerp(HandGoal, Stretched, Overhead);
				bAim = true;
				AimBlend = FMath::Max(AimBlend, Overhead);
			}
			const float A = FMath::Clamp(In.ArmAlpha, 0.0f, 1.0f);
			const FVector Effector = FMath::Lerp(HandCS.GetLocation(), HandGoal, A);
			// Keep the elbow bending the way the current pose bends it; up and out for an overhead.
			// A nearly straight arm (a running swing) has no clear bend: the offset is a few
			// centimetres and its direction swings, so a steady down-and-out bend is added in,
			// weighed in centimetres against the pose's own, and takes over as the arm straightens.
			const FVector Mid = (Shoulder + HandCS.GetLocation()) * 0.5;
			const FVector Current = (Lower.GetLocation() - Mid + DownOut * Tuning.ElbowPoleSteadyUnits).GetSafeNormal();
			// Turned towards up-and-out by angle, not blended straight across: the two point
			// nearly opposite ways, and a straight blend swings through the middle in a frame.
			const FVector Pole = Current.IsNearlyZero() ? UpOut : FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Current, UpOut), Overhead).RotateVector(Current);
			SolveLimb(CS, Bones.UpperArm, Bones.LowerArm, Bones.Hand, Effector, Pole * Tuning.ElbowBendUnits, &Bones);
			// Keep the forearm out of the torso (a capsule from the pelvis up the spine): when the arc
			// takes it across the body, push the hand out from the spine and reach again.
			if (Valid(Bones.Pelvis) && Bones.Spine.Num() > 0 && Tuning.TorsoClearanceUnits > 0.0)
			{
				const FVector Bottom = CS.GetComponentSpaceTransform(Bones.Pelvis).GetLocation();
				const FVector Top = CS.GetComponentSpaceTransform(Bones.Spine.Last()).GetLocation();
				FVector Goal = Effector;
				for (int32 Pass = 0; Pass < 5; ++Pass)
				{
					const FVector Elbow = CS.GetComponentSpaceTransform(Bones.LowerArm).GetLocation();
					const FVector Wrist = CS.GetComponentSpaceTransform(Bones.Hand).GetLocation();
					const FVector ForearmMid = (Elbow + Wrist) * 0.5;
					const FVector OnSpine = FMath::ClosestPointOnSegment(ForearmMid, Bottom, Top);
					const double Gap = FVector::Dist(ForearmMid, OnSpine);
					if (Gap >= Tuning.TorsoClearanceUnits)
					{
						break;
					}
					// Out from the spine, never behind the back, and leaning to the front of the body
					// as the forearm nears the spine's line (where "out" has no clear direction).
					// Pushed whichever way it already was, a forearm sweeping across went behind
					// the back one frame and was thrown in front the next.
					FVector Away = ForearmMid - OnSpine;
					Away.Z = 0.0;
					Away -= FMath::Min(FVector::DotProduct(Away, FacingAxis), 0.0) * FacingAxis;
					Away = (Away + FacingAxis * (Tuning.TorsoClearanceUnits - Gap)).GetSafeNormal();
					// The shortfall, once per pass: pushing further overshoots, and the overshoot
					// turned a few centimetres of target motion into a large arm turn.
					Goal += Away * (Tuning.TorsoClearanceUnits - Gap);
					SolveLimb(CS, Bones.UpperArm, Bones.LowerArm, Bones.Hand, Goal, Pole * Tuning.ElbowBendUnits, &Bones);
				}
			}
			if (bAim)
			{
				// Turn the racket so the face, not the hand's old angle, points at the target.
				// Aimed from the hand goal (moved by any torso push), not from where a part reach
				// (ArmAlpha < 1) left the hand: where the goal is the old one (the reach just
				// running out, an overhead just starting) that is no turn at all, so the turn
				// grows from nothing instead of jumping in.
				// The turn starts from the hand carried on the solved forearm (a set wrist) as the aim
				// takes over, not from its posed angle: posed hanging (a bandeja's windup), the
				// racket is nearly half a turn from a target behind the head, which has no steady
				// shortest way and spun the hand 80 degrees in a frame.
				FTransform Hand = CS.GetComponentSpaceTransform(Bones.Hand);
				const FQuat Carried = CS.GetComponentSpaceTransform(Bones.LowerArm).GetRotation() * (Lower.GetRotation().Inverse() * HandCS.GetRotation());
				const FQuat Base = FQuat::Slerp(Hand.GetRotation(), Carried.GetNormalized(), static_cast<float>(AimBlend)).GetNormalized();
				const FVector Now = Base.RotateVector(In.FaceInHand);
				const FVector AimFrom = HandGoal + (Hand.GetLocation() - Effector);
				const FQuat Turn = FQuat::FindBetweenVectors(Now, In.TargetCS - AimFrom);
				Hand.SetRotation(FQuat::Slerp(FQuat::Identity, Turn, A) * Base);
				SetCS(CS, Bones.Hand, Hand);
			}
		};
		// Overheads past the arm's reach: lean the trunk towards the ball and rise onto the toes
		// (the pelvis up, the legs straightening under it) before the arm reaches.
		if (In.ArmAlpha > 0.0f && Bones.Spine.Num() > 0 && Valid(Bones.Pelvis))
		{
			const FVector Shoulder = CS.GetComponentSpaceTransform(Bones.UpperArm).GetLocation();
			const double ArmLength = FVector::Dist(Shoulder, CS.GetComponentSpaceTransform(Bones.LowerArm).GetLocation())
				+ FVector::Dist(CS.GetComponentSpaceTransform(Bones.LowerArm).GetLocation(), CS.GetComponentSpaceTransform(Bones.Hand).GetLocation());
			const double Short = FVector::Dist(Shoulder, In.TargetCS) - (ArmLength * Tuning.MaxReachShare + In.FaceInHand.Size());
			const double Overhead = OverheadWeight(In.TargetCS, Shoulder, Bones, Tuning);
			const double A = FMath::Clamp(In.ArmAlpha, 0.0f, 1.0f) * Overhead;
			if (Short > 0.0 && A > 0.0)
			{
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
				const double Rise = FMath::Min(Short, Tuning.MaxToeRiseUnits) * A;
				FTransform Pelvis = CS.GetComponentSpaceTransform(Bones.Pelvis);
				Pelvis.AddToTranslation(FVector(0.0, 0.0, Rise));
				SetCS(CS, Bones.Pelvis, Pelvis);
				for (int32 Side = 0; Side < 2; ++Side)
				{
					if (bLeg[Side])
					{
						// As close to the planted foot as the straight leg allows: the heel comes up.
						SolveLimb(CS, Bones.Thigh[Side], Bones.Calf[Side], Bones.Foot[Side], Feet[Side].GetLocation(), FacingAxis * Tuning.KneeBendUnits);
					}
				}
				// Lean: turn the spine so the racket shoulder heads for the ball.
				const FVector Pivot = CS.GetComponentSpaceTransform(Bones.Spine[0]).GetLocation();
				const FVector NewShoulder = CS.GetComponentSpaceTransform(Bones.UpperArm).GetLocation();
				FVector Axis;
				double Angle;
				FQuat::FindBetweenVectors(NewShoulder - Pivot, In.TargetCS - Pivot).ToAxisAndAngle(Axis, Angle);
				Angle = FMath::Min(Angle, FMath::DegreesToRadians(Tuning.MaxOverheadLeanDeg)) * A;
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
