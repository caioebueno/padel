#include "Animation/PadelSampleLocomotion.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/BlendProfile.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace PadelSampleLocomotionDetail
{
	double Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double Default)
	{
		double Value = Default;
		return Object.IsValid() && Object->TryGetNumberField(Key, Value) ? Value : Default;
	}

	TArray<FName> Names(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
	{
		TArray<FName> Out;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(Key, Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				Out.Add(FName(*Value->AsString()));
			}
		}
		return Out;
	}

	/**
	 * Sets a UPROPERTY on an engine node by name: the motion matching node keeps its settings
	 * private to its AnimBP editor node, which this C++ graph replaces.
	 */
	template <typename TValue, typename TNode>
	void SetMember(TNode& Node, const TCHAR* Name, const TValue& Value)
	{
		if (const FProperty* Property = TNode::StaticStruct()->FindPropertyByName(Name))
		{
			*Property->ContainerPtrToValuePtr<TValue>(&Node) = Value;
		}
		else
		{
			UE_LOG(LogPadel, Warning, TEXT("Sample locomotion: %s has no property %s"), *TNode::StaticStruct()->GetName(), Name);
		}
	}

	/** Yaw (degrees) of a ground vector. */
	double YawOf(const FVector& V)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
	}
}

FPadelSampleLocomotionSettings FPadelSampleLocomotionSettings::Load()
{
	using namespace PadelSampleLocomotionDetail;
	FPadelSampleLocomotionSettings S;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("SampleLocomotion.json"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		S.Error = FString::Printf(TEXT("%s didn't load"), *Path);
		return S;
	}
	S.DriverMesh = Root->GetStringField(TEXT("DriverMesh"));
	S.MetaHumanRetargeter = Root->GetStringField(TEXT("MetaHumanRetargeter"));
	const FString Folder = Root->GetStringField(TEXT("DatabaseFolder"));
	const TSharedPtr<FJsonObject>* Databases = nullptr;
	if (Root->TryGetObjectField(TEXT("Databases"), Databases))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Databases)->Values)
		{
			const FString Name = Entry.Value->AsString();
			S.Databases.Add(FName(*Entry.Key), FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name));
		}
	}
	const TSharedPtr<FJsonObject> Chooser = Root->GetObjectField(TEXT("Chooser"));
	S.IdleBelow = Number(Chooser, TEXT("IdleBelow"), S.IdleBelow);
	S.RunStopsFrom = Number(Chooser, TEXT("RunStopsFrom"), S.RunStopsFrom);
	S.SprintStopsFrom = Number(Chooser, TEXT("SprintStopsFrom"), S.SprintStopsFrom);
	S.RunGaitFrom = Number(Chooser, TEXT("RunGaitFrom"), S.RunGaitFrom);
	S.SprintGaitFrom = Number(Chooser, TEXT("SprintGaitFrom"), S.SprintGaitFrom);
	S.MovingSpeedAbove = Number(Chooser, TEXT("MovingSpeedAbove"), S.MovingSpeedAbove);
	S.MovingFutureSpeedAbove = Number(Chooser, TEXT("MovingFutureSpeedAbove"), S.MovingFutureSpeedAbove);
	S.StartingFutureGain = Number(Chooser, TEXT("StartingFutureGain"), S.StartingFutureGain);
	S.PivotAngle = Number(Chooser, TEXT("PivotAngle"), S.PivotAngle);
	S.GaitHysteresis = Number(Chooser, TEXT("GaitHysteresis"), S.GaitHysteresis);
	S.StrafePivotAngle = Number(Chooser, TEXT("StrafePivotAngle"), S.StrafePivotAngle);
	S.TurnInPlaceAngle = Number(Chooser, TEXT("TurnInPlaceAngle"), S.TurnInPlaceAngle);
	S.FutureVelocityFromS = Number(Chooser, TEXT("FutureVelocityFromS"), S.FutureVelocityFromS);
	S.FutureVelocityToS = Number(Chooser, TEXT("FutureVelocityToS"), S.FutureVelocityToS);
	const TSharedPtr<FJsonObject> Matching = Root->GetObjectField(TEXT("MotionMatching"));
	S.BlendTimeS = Number(Matching, TEXT("BlendTimeS"), S.BlendTimeS);
	S.BlendProfile = FName(*Matching->GetStringField(TEXT("BlendProfile")));
	S.PlayRateMin = Number(Matching, TEXT("PlayRateMin"), S.PlayRateMin);
	S.PlayRateMax = Number(Matching, TEXT("PlayRateMax"), S.PlayRateMax);
	S.PoseReselectHistoryS = Number(Matching, TEXT("PoseReselectHistoryS"), S.PoseReselectHistoryS);
	S.MaxBlendInTimeToOverrideAnimationS = Number(Matching, TEXT("MaxBlendInTimeToOverrideAnimationS"), S.MaxBlendInTimeToOverrideAnimationS);
	S.PlayerDepthBlendInTimeMultiplier = Number(Matching, TEXT("PlayerDepthBlendInTimeMultiplier"), S.PlayerDepthBlendInTimeMultiplier);
	const TSharedPtr<FJsonObject> History = Root->GetObjectField(TEXT("PoseHistory"));
	S.CollectedBones = Names(History, TEXT("CollectedBones"));
	S.CollectedCurves = Names(History, TEXT("CollectedCurves"));
	S.RootBoneRecoveryTimeS = Number(History, TEXT("RootBoneRecoveryTimeS"), S.RootBoneRecoveryTimeS);
	const TSharedPtr<FJsonObject> Offset = Root->GetObjectField(TEXT("OffsetRoot"));
	S.TranslationHalfLifeS = Number(Offset, TEXT("TranslationHalfLifeS"), S.TranslationHalfLifeS);
	S.RotationHalfLifeS = Number(Offset, TEXT("RotationHalfLifeS"), S.RotationHalfLifeS);
	S.MaxTranslationError = Number(Offset, TEXT("MaxTranslationError"), S.MaxTranslationError);
	const TSharedPtr<FJsonObject> Warp = Root->GetObjectField(TEXT("OrientationWarping"));
	S.bOrientationWarping = Warp.IsValid() && Warp->GetBoolField(TEXT("Enabled"));
	S.WarpLocomotionAngleDeltaThreshold = Number(Warp, TEXT("LocomotionAngleDeltaThreshold"), S.WarpLocomotionAngleDeltaThreshold);
	S.WarpRotationInterpSpeed = Number(Warp, TEXT("RotationInterpSpeed"), S.WarpRotationInterpSpeed);
	S.WarpSpineBones = Names(Warp, TEXT("SpineBones"));
	const TSharedPtr<FJsonObject> Feet = Root->GetObjectField(TEXT("FootPlacement"));
	S.bFootPlacement = Feet.IsValid() && Feet->GetBoolField(TEXT("Enabled"));
	S.ContactCurves = Names(Feet, TEXT("ContactCurves"));
	S.PelvisMaxOffset = Number(Feet, TEXT("PelvisMaxOffset"), S.PelvisMaxOffset);
	S.PelvisLinearStiffness = Number(Feet, TEXT("PelvisLinearStiffness"), S.PelvisLinearStiffness);
	S.UnplantRadius = Number(Feet, TEXT("UnplantRadius"), S.UnplantRadius);
	S.ReplantRadiusRatio = Number(Feet, TEXT("ReplantRadiusRatio"), S.ReplantRadiusRatio);
	S.UnplantAngle = Number(Feet, TEXT("UnplantAngle"), S.UnplantAngle);
	S.ReplantAngleRatio = Number(Feet, TEXT("ReplantAngleRatio"), S.ReplantAngleRatio);
	S.UnplantLinearStiffness = Number(Feet, TEXT("UnplantLinearStiffness"), S.UnplantLinearStiffness);
	S.UnplantAngularStiffness = Number(Feet, TEXT("UnplantAngularStiffness"), S.UnplantAngularStiffness);
	S.MaxGroundPenetration = Number(Feet, TEXT("MaxGroundPenetration"), S.MaxGroundPenetration);
	S.LegSoftPercentLength = Number(Feet, TEXT("LegSoftPercentLength"), S.LegSoftPercentLength);
	const TSharedPtr<FJsonObject> Steer = Root->GetObjectField(TEXT("Steering"));
	S.SteeringAnimatedTargetTimeS = Number(Steer, TEXT("AnimatedTargetTimeS"), S.SteeringAnimatedTargetTimeS);
	S.SteeringProceduralTargetTimeS = Number(Steer, TEXT("ProceduralTargetTimeS"), S.SteeringProceduralTargetTimeS);
	S.SteeringBelowSpeed = Number(Steer, TEXT("DisableBelowSpeed"), S.SteeringBelowSpeed);
	const TSharedPtr<FJsonObject> Trajectory = Root->GetObjectField(TEXT("Trajectory"));
	S.PastCount = static_cast<int32>(Number(Trajectory, TEXT("PastCount"), S.PastCount));
	S.PastIntervalS = Number(Trajectory, TEXT("PastIntervalS"), S.PastIntervalS);
	S.FutureCount = static_cast<int32>(Number(Trajectory, TEXT("FutureCount"), S.FutureCount));
	S.FutureIntervalS = Number(Trajectory, TEXT("FutureIntervalS"), S.FutureIntervalS);
	S.FacingTurnRate = Number(Trajectory, TEXT("FacingTurnRate"), S.FacingTurnRate);
	S.StrafeAbove = Number(Trajectory, TEXT("StrafeAbove"), S.StrafeAbove);
	S.bOk = !S.DriverMesh.IsEmpty() && !S.Databases.IsEmpty() && S.PastCount >= 2 && S.FutureCount >= 2;
	if (!S.bOk)
	{
		S.Error = FString::Printf(TEXT("%s is incomplete"), *Path);
	}
	return S;
}

// ---------------------------------------------------------------------------------------------

void FAnimNode_PadelSampleLocomotion::Configure(const FPadelSampleLocomotionSettings& InSettings, const USkeleton* Skeleton)
{
	Settings = InSettings;
	DatabaseByRole.Reset();
	for (const TPair<FName, FString>& Entry : Settings.Databases)
	{
		if (UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, *Entry.Value, nullptr, LOAD_Quiet | LOAD_NoWarn))
		{
			DatabaseByRole.Add(Entry.Key, Database);
		}
		else
		{
			UE_LOG(LogPadel, Warning, TEXT("Sample locomotion: %s isn't on disk (Tools/AssetPipeline/unreal/gasp_migrate.py)."), *Entry.Value);
		}
	}

	// The sample's motion matching node (SandboxCharacter_CMC_ABP): a blend stack of half-second
	// blends weighted by its FastFeet+Root profile, play rate within 0.85..1.15.
	using PadelSampleLocomotionDetail::SetMember;
	SetMember(MotionMatching, TEXT("BlendTime"), static_cast<float>(Settings.BlendTimeS));
	SetMember(MotionMatching, TEXT("BlendOption"), EAlphaBlendOption::HermiteCubic);
	SetMember(MotionMatching, TEXT("PlayRate"), FFloatInterval(static_cast<float>(Settings.PlayRateMin), static_cast<float>(Settings.PlayRateMax)));
	SetMember(MotionMatching, TEXT("PoseReselectHistory"), static_cast<float>(Settings.PoseReselectHistoryS));
	SetMember(MotionMatching, TEXT("MaxBlendInTimeToOverrideAnimation"), static_cast<float>(Settings.MaxBlendInTimeToOverrideAnimationS));
	SetMember(MotionMatching, TEXT("PlayerDepthBlendInTimeMultiplier"), static_cast<float>(Settings.PlayerDepthBlendInTimeMultiplier));
	MotionMatching.bShouldFilterNotifies = true;
	SetMember(MotionMatching, TEXT("BlendProfile"), TObjectPtr<UBlendProfile>(Skeleton ? const_cast<USkeleton*>(Skeleton)->GetBlendProfile(Settings.BlendProfile) : nullptr));
	if (const TObjectPtr<UPoseSearchDatabase>* Idles = DatabaseByRole.Find(TEXT("Idles")))
	{
		SetMember(MotionMatching, TEXT("Database"), TObjectPtr<const UPoseSearchDatabase>(Idles->Get()));
	}

	History.CollectedBones.Reset();
	for (const FName Bone : Settings.CollectedBones)
	{
		History.CollectedBones.Add(FBoneReference(Bone));
	}
	History.CollectedCurves = Settings.CollectedCurves;
	History.RootBoneRecoveryTime = static_cast<float>(Settings.RootBoneRecoveryTimeS);
	History.SamplingInterval = 0.0f;

	Steering.AnimatedTargetTime = static_cast<float>(Settings.SteeringAnimatedTargetTimeS);
	Steering.ProceduralTargetTime = static_cast<float>(Settings.SteeringProceduralTargetTimeS);
	Steering.DisableSteeringBelowSpeed = static_cast<float>(Settings.SteeringBelowSpeed);

	// Orientation warping, as the sample's locomotion states: in graph mode it reads the pose's
	// root motion and turns the legs onto the pawn's travel, the spine (to the head) back.
	OrientationWarping.Mode = EWarpingEvaluationMode::Graph;
	OrientationWarping.LocomotionAngleDeltaThreshold = static_cast<float>(Settings.WarpLocomotionAngleDeltaThreshold);
	OrientationWarping.RotationInterpSpeed = static_cast<float>(Settings.WarpRotationInterpSpeed);
	OrientationWarping.WarpingSpace = EOrientationWarpingSpace::RootBoneTransform;
	OrientationWarping.SpineBones.Reset();
	for (const FName Bone : Settings.WarpSpineBones)
	{
		OrientationWarping.SpineBones.Add(FBoneReference(Bone));
	}
	OrientationWarping.IKFootRootBone = FBoneReference(TEXT("ik_foot_root"));
	OrientationWarping.IKFootBones = { FBoneReference(TEXT("ik_foot_l")), FBoneReference(TEXT("ik_foot_r")) };
	OrientationWarping.Alpha = Settings.bOrientationWarping ? 1.0f : 0.0f;

	// Foot placement and leg IK, as the sample's.
	ContactCurves.Curves = Settings.ContactCurves;
	FootPlacement.IKFootRootBone = FBoneReference(TEXT("ik_foot_root"));
	FootPlacement.PelvisBone = FBoneReference(TEXT("pelvis"));
	FootPlacement.PelvisSettings.MaxOffset = static_cast<float>(Settings.PelvisMaxOffset);
	FootPlacement.PelvisSettings.LinearStiffness = static_cast<float>(Settings.PelvisLinearStiffness);
	FootPlacement.LegDefinitions.Reset();
	const TCHAR* const Sides[2] = { TEXT("r"), TEXT("l") };
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FFootPlacemenLegDefinition& Leg = FootPlacement.LegDefinitions.AddDefaulted_GetRef();
		Leg.FKFootBone = FBoneReference(*FString::Printf(TEXT("foot_%s"), Sides[Side]));
		Leg.IKFootBone = FBoneReference(*FString::Printf(TEXT("ik_foot_%s"), Sides[Side]));
		Leg.BallBone = FBoneReference(*FString::Printf(TEXT("ball_%s"), Sides[Side]));
		Leg.SpeedCurveName = Settings.ContactCurves.IsValidIndex(Side) ? Settings.ContactCurves[Side] : NAME_None;
	}
	FootPlacement.PlantSettings.UnplantRadius = static_cast<float>(Settings.UnplantRadius);
	FootPlacement.PlantSettings.ReplantRadiusRatio = static_cast<float>(Settings.ReplantRadiusRatio);
	FootPlacement.PlantSettings.UnplantAngle = static_cast<float>(Settings.UnplantAngle);
	FootPlacement.PlantSettings.ReplantAngleRatio = static_cast<float>(Settings.ReplantAngleRatio);
	FootPlacement.InterpolationSettings.UnplantLinearStiffness = static_cast<float>(Settings.UnplantLinearStiffness);
	FootPlacement.InterpolationSettings.UnplantAngularStiffness = static_cast<float>(Settings.UnplantAngularStiffness);
	FootPlacement.TraceSettings.MaxGroundPenetration = static_cast<float>(Settings.MaxGroundPenetration);
	FootPlacement.Alpha = Settings.bFootPlacement ? 1.0f : 0.0f;
	LegIK.SoftPercentLength = static_cast<float>(Settings.LegSoftPercentLength);
	LegIK.LegsDefinition.Reset();
	for (int32 Side = 1; Side >= 0; --Side)
	{
		FAnimLegIKDefinition& Leg = LegIK.LegsDefinition.AddDefaulted_GetRef();
		Leg.IKFootBone = FBoneReference(*FString::Printf(TEXT("ik_foot_%s"), Sides[Side]));
		Leg.FKFootBone = FBoneReference(*FString::Printf(TEXT("foot_%s"), Sides[Side]));
		Leg.HingeRotationAxis = EAxis::Z;
		Leg.bEnableRotationLimit = true;
	}
	LegIK.Alpha = Settings.bFootPlacement ? 1.0f : 0.0f;

#if WITH_EDITORONLY_DATA
	// Folded node data: read from the node itself in editor builds (see the header).
	OffsetRoot.RotationMode = EOffsetRootBoneMode::Accumulate;
	OffsetRoot.TranslationMode = EOffsetRootBoneMode::Interpolate;
	OffsetRoot.TranslationHalflife = static_cast<float>(Settings.TranslationHalfLifeS);
	OffsetRoot.RotationHalfLife = static_cast<float>(Settings.RotationHalfLifeS);
	OffsetRoot.MaxTranslationError = static_cast<float>(Settings.MaxTranslationError);
#endif
	bLinked = false;
}

void FAnimNode_PadelSampleLocomotion::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	if (!bLinked)
	{
		History.Source.SetLinkNode(&MotionMatching);
		ToComponent.LocalPose.SetLinkNode(&History);
		Steering.ComponentPose.SetLinkNode(&ToComponent);
		OrientationWarping.ComponentPose.SetLinkNode(&Steering);
		ToLocal.ComponentPose.SetLinkNode(&OrientationWarping);
		OffsetRoot.Source.SetLinkNode(&ToLocal);
		ContactCurves.Source.SetLinkNode(&OffsetRoot);
		ToComponentAfterRoot.LocalPose.SetLinkNode(&ContactCurves);
		FootPlacement.ComponentPose.SetLinkNode(&ToComponentAfterRoot);
		LegIK.ComponentPose.SetLinkNode(&FootPlacement);
		Final.ComponentPose.SetLinkNode(&LegIK);
		bLinked = true;
	}
	Final.Initialize_AnyThread(Context);
}

void FAnimNode_PadelSampleLocomotion::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Final.CacheBones_AnyThread(Context);
}

void FAnimNode_PadelSampleLocomotion::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const UPadelSampleAnimInstance* Anim = Cast<UPadelSampleAnimInstance>(InAnimInstance);
	if (!Anim)
	{
		return;
	}
	History.TransformTrajectory = Anim->GetSampleTrajectory();
	if (const USkeletalMeshComponent* Mesh = Anim->GetSkelMeshComponent())
	{
		SteerTo = Mesh->GetComponentQuat();
	}
	Travel = Anim->GetCurrentVelocity();
	Choose(*Anim);
}

void FAnimNode_PadelSampleLocomotion::Choose(const UPadelSampleAnimInstance& Anim)
{
	// The sample's rules (SandboxCharacter_CMC_ABP: IsMoving, IsStarting, IsPivoting,
	// ShouldTurnInPlace, Get_MMInterruptMode; CHT_PoseSearchDatabases_Dense), with PadelCore's
	// trajectory standing in for the movement component's.
	const FVector Velocity = Anim.GetCurrentVelocity();
	const FVector Future = Anim.GetFutureVelocity();
	LastChoice = Choice;
	Choice = FPadelSampleChoice();
	Choice.Speed = Velocity.Size2D();
	Choice.FutureSpeed = Future.Size2D();
	Choice.bStrafing = Anim.IsStrafing();
	// The sample's input acceleration is the player wanting to move: here, a future that moves.
	Choice.bMoving = Choice.Speed > Settings.MovingSpeedAbove && Choice.FutureSpeed > Settings.MovingFutureSpeedAbove;
	const FName Playing = CurrentDatabase();
	const bool bPlayingPivot = Playing.ToString().Contains(TEXT("Pivot"));
	Choice.bStarting = Choice.bMoving && Choice.FutureSpeed >= Choice.Speed + Settings.StartingFutureGain && !bPlayingPivot;
	const double TurnAngle = FMath::Abs(FMath::FindDeltaAngleDegrees(PadelSampleLocomotionDetail::YawOf(Velocity), PadelSampleLocomotionDetail::YawOf(Future)));
	Choice.bPivoting = Choice.bMoving && Choice.FutureSpeed > 1.0 && TurnAngle >= (Choice.bStrafing ? Settings.StrafePivotAngle : Settings.PivotAngle);
	const bool bJustStopped = !Choice.bMoving && LastChoice.bMoving;
	Choice.bTurnInPlace = !Choice.bMoving && FMath::Abs(Anim.GetRootYawLag()) >= Settings.TurnInPlaceAngle && (bJustStopped || LastChoice.bTurnInPlace);
	// The gait by the speed now (the trajectory's future overshoots while speeding up), with a margin
	// either side of each band's edge so it doesn't flicker there.
	const double Up = 1.0 + Settings.GaitHysteresis;
	const double Down = 1.0 - Settings.GaitHysteresis;
	const int32 Was = LastChoice.Gait;
	const double Gait = Choice.Speed;
	Choice.Gait = Gait >= Settings.SprintGaitFrom * (Was == 2 ? Down : Up) ? 2 : Gait >= Settings.RunGaitFrom * (Was >= 1 ? Down : Up) ? 1 : 0;
	if (Choice.bMoving)
	{
		// Moving: the gait's loops, starts while starting, pivots while pivoting.
		static const TCHAR* const Starts[] = { TEXT("WalkStarts"), TEXT("RunStarts"), TEXT("SprintStarts") };
		static const TCHAR* const Loops[] = { TEXT("WalkLoops"), TEXT("RunLoops"), TEXT("SprintLoops") };
		static const TCHAR* const Pivots[] = { TEXT("WalkPivots"), TEXT("RunPivots"), TEXT("SprintPivots") };
		const bool bSprint = Choice.Gait == 2;
		if (Choice.bStarting && !(bSprint && Choice.bPivoting)) { Choice.Databases.Add(Starts[Choice.Gait]); }
		if (!(bSprint && Choice.bPivoting)) { Choice.Databases.Add(Loops[Choice.Gait]); }
		if (Choice.bPivoting) { Choice.Databases.Add(Pivots[Choice.Gait]); }
	}
	else
	{
		// Idle: the idles when slow and not turning, the stops of each gait above its speed, turns in place.
		if (Choice.Speed < Settings.IdleBelow && !Choice.bTurnInPlace) { Choice.Databases.Add(TEXT("Idles")); }
		if (Choice.Speed >= Settings.IdleBelow) { Choice.Databases.Add(TEXT("WalkStops")); }
		if (Choice.Speed >= Settings.RunStopsFrom) { Choice.Databases.Add(TEXT("RunStops")); }
		if (Choice.Speed >= Settings.SprintStopsFrom) { Choice.Databases.Add(TEXT("SprintStops")); }
		if (Choice.bTurnInPlace) { Choice.Databases.Add(TEXT("TurnInPlace")); }
	}
	ToSearch.Reset();
	for (const FName Role : Choice.Databases)
	{
		if (const TObjectPtr<UPoseSearchDatabase>* Database = DatabaseByRole.Find(Role))
		{
			ToSearch.Add(*Database);
		}
	}
	// A change of movement state or gait interrupts the continuing pose (Get_MMInterruptMode).
	Interrupt = Choice.bMoving != LastChoice.bMoving || Choice.Gait != LastChoice.Gait
		? EPoseSearchInterruptMode::InterruptOnDatabaseChange : EPoseSearchInterruptMode::DoNotInterrupt;
}

void FAnimNode_PadelSampleLocomotion::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	if (!ToSearch.IsEmpty())
	{
		TArray<UPoseSearchDatabase*> Raw;
		for (const TObjectPtr<UPoseSearchDatabase>& Database : ToSearch)
		{
			Raw.Add(Database.Get());
		}
		MotionMatching.SetDatabasesToSearch(Raw, Interrupt);
	}
	// Steer the playing clip's root motion toward the pawn's facing, so the offset root's turn is used up.
	const FPoseSearchBlueprintResult& Picked = MotionMatching.GetMotionMatchingState().SearchResult;
	Steering.TargetOrientation = SteerTo;
	Steering.CurrentAnimAsset = Cast<UAnimationAsset>(const_cast<UObject*>(Picked.SelectedAnim.Get()));
	Steering.CurrentAnimAssetTime = Picked.SelectedTime;
	Steering.bMirrored = Picked.bIsMirrored;
	OrientationWarping.LocomotionDirection = Travel;
	OrientationWarping.CurrentAnimAsset = Steering.CurrentAnimAsset;
	OrientationWarping.CurrentAnimAssetTime = Picked.SelectedTime;
	Final.Update_AnyThread(Context);
}

void FAnimNode_PadelSampleLocomotion::Evaluate_AnyThread(FPoseContext& Output)
{
	Final.Evaluate_AnyThread(Output);
}

const UAnimationAsset* FAnimNode_PadelSampleLocomotion::CurrentAsset() const
{
	return Cast<UAnimationAsset>(MotionMatching.GetMotionMatchingState().SearchResult.SelectedAnim.Get());
}

FName FAnimNode_PadelSampleLocomotion::CurrentDatabase() const
{
	const UPoseSearchDatabase* Database = MotionMatching.GetMotionMatchingState().SearchResult.SelectedDatabase.Get();
	if (!Database)
	{
		return NAME_None;
	}
	for (const TPair<FName, TObjectPtr<UPoseSearchDatabase>>& Entry : DatabaseByRole)
	{
		if (Entry.Value == Database)
		{
			return Entry.Key;
		}
	}
	return Database->GetFName();
}

void FAnimNode_PadelContactCurves::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	for (const FName Curve : Curves)
	{
		bool bFound = false;
		const float Contact = Output.Curve.Get(Curve, bFound);
		if (bFound)
		{
			Output.Curve.Set(Curve, (1.0f - Contact) * 100.0f);
		}
	}
}

void FPadelSampleAnimInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	if (Root)
	{
		OutNodes.Add(Root);
		OutNodes.Add(&Root->Final);
		OutNodes.Add(&Root->LegIK);
		OutNodes.Add(&Root->FootPlacement);
		OutNodes.Add(&Root->ToComponentAfterRoot);
		OutNodes.Add(&Root->ContactCurves);
		OutNodes.Add(&Root->OffsetRoot);
		OutNodes.Add(&Root->ToLocal);
		OutNodes.Add(&Root->OrientationWarping);
		OutNodes.Add(&Root->Steering);
		OutNodes.Add(&Root->ToComponent);
		OutNodes.Add(&Root->History);
		OutNodes.Add(&Root->MotionMatching);
	}
}

// ---------------------------------------------------------------------------------------------

UPadelSampleAnimInstance::UPadelSampleAnimInstance()
{
	// The clips' root motion is taken out of the pose and left unused: the simulation moves the pawn.
	RootMotionMode = ERootMotionMode::RootMotionFromEverything;
}

void UPadelSampleAnimInstance::Configure(const FPadelSampleLocomotionSettings& InSettings)
{
	Settings = InSettings;
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const USkeleton* Skeleton = Mesh && Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	SampleNode.Configure(Settings, Skeleton);
	// The nodes read some settings when they initialize (the history's bones, the blend stack's
	// notify filter), and the instance initialized when the class was set: initialize it again.
	if (USkeletalMeshComponent* Body = GetSkelMeshComponent())
	{
		Body->InitAnim(true);
	}
}

FAnimInstanceProxy* UPadelSampleAnimInstance::CreateAnimInstanceProxy()
{
	return new FPadelSampleAnimInstanceProxy(this, &SampleNode);
}

void UPadelSampleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	BuildSampleTrajectory();
}

void UPadelSampleAnimInstance::BuildSampleTrajectory()
{
	using namespace PadelSampleLocomotionDetail;
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const UWorld* World = GetWorld();
	if (!Mesh || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVector Here = Mesh->GetComponentLocation();
	const FQuat Facing = Mesh->GetComponentQuat();
	if (!Past.IsEmpty() && Past.Last().Get<0>() > Now)
	{
		Past.Reset(); // the clock went back: a new world
	}
	if (Past.IsEmpty() || Past.Last().Get<0>() < Now)
	{
		Past.Emplace(Now, Here, Facing);
	}
	const double Keep = Settings.PastCount * Settings.PastIntervalS + 0.1;
	while (Past.Num() > 2 && Past[1].Get<0>() < Now - Keep)
	{
		Past.RemoveAt(0);
	}
	auto PastAt = [this, &Here, &Facing](double Time, FVector& OutPosition, FQuat& OutFacing)
	{
		OutPosition = Past.IsEmpty() ? Here : Past[0].Get<1>();
		OutFacing = Past.IsEmpty() ? Facing : Past[0].Get<2>();
		for (int32 I = 1; I < Past.Num(); ++I)
		{
			if (Past[I].Get<0>() >= Time)
			{
				const double Span = Past[I].Get<0>() - Past[I - 1].Get<0>();
				const double T = Span > 1e-6 ? (Time - Past[I - 1].Get<0>()) / Span : 1.0;
				OutPosition = FMath::Lerp(Past[I - 1].Get<1>(), Past[I].Get<1>(), T);
				OutFacing = FQuat::Slerp(Past[I - 1].Get<2>(), Past[I].Get<2>(), T);
				return;
			}
		}
		OutPosition = Past.Last().Get<1>();
		OutFacing = Past.Last().Get<2>();
	};

	// PadelCore's velocity (read off the model's trajectory by ApplyFrame) carried on as the
	// legacy path does: braking ends at rest, speeding up is carried for at most half a second.
	const FVector Velocity(LastVelocity.X, LastVelocity.Y, 0.0);
	const double GroundSpeed = Velocity.Size2D();
	const double Along = GroundSpeed > 1.0 ? FVector::DotProduct(Acceleration, Velocity / GroundSpeed) : 0.0;
	const double StopIn = Along < -1.0 ? GroundSpeed / -Along : TNumericLimits<double>::Max();
	auto Offset = [&](double T)
	{
		if (StopIn < TNumericLimits<double>::Max())
		{
			const double U = FMath::Min(T, StopIn);
			return Velocity * U + (Velocity / FMath::Max(GroundSpeed, 1.0)) * (0.5 * Along * U * U);
		}
		const double U = FMath::Min(T, 0.5);
		FVector Out = Velocity * T + Acceleration * (0.5 * U * U + U * (T - U));
		Out.Z = 0.0;
		return Out;
	};
	auto VelocityAt = [&](double T)
	{
		constexpr double H = 1.0 / 60.0;
		return (Offset(T + H) - Offset(FMath::Max(0.0, T - H))) / (T + H - FMath::Max(0.0, T - H));
	};
	CurrentVelocity = Velocity;
	FutureVelocity = (Offset(Settings.FutureVelocityToS) - Offset(Settings.FutureVelocityFromS)) / FMath::Max(Settings.FutureVelocityToS - Settings.FutureVelocityFromS, 1e-3);

	// Facing: the mesh faces +Y in its component, so its forward on the ground is the component's Y axis.
	const FVector Forward = Facing.RotateVector(FVector(0.0, 1.0, 0.0));
	const double FacingYaw = YawOf(Forward);
	bStrafing = GroundSpeed > 50.0 && FMath::Abs(FMath::FindDeltaAngleDegrees(FacingYaw, YawOf(Velocity))) > Settings.StrafeAbove;

	// The drawn root against the pawn's facing: how far the offset root still has to turn.
	const FQuat RootWorld = Mesh->GetBoneQuaternion(Mesh->GetBoneName(0), EBoneSpaces::WorldSpace);
	RootYawLag = FMath::FindDeltaAngleDegrees(YawOf(RootWorld.RotateVector(FVector(0.0, 1.0, 0.0))), FacingYaw);

	SampleTrajectory.Samples.Reset();
	for (int32 I = Settings.PastCount; I >= 1; --I)
	{
		const double T = -I * Settings.PastIntervalS;
		FTransformTrajectorySample& Sample = SampleTrajectory.Samples.AddDefaulted_GetRef();
		FVector Position;
		FQuat PastFacing;
		PastAt(Now + T, Position, PastFacing);
		Sample.TimeInSeconds = static_cast<float>(T);
		Sample.Position = Position;
		Sample.Facing = PastFacing;
	}
	for (int32 I = 0; I <= Settings.FutureCount; ++I)
	{
		const double T = I * Settings.FutureIntervalS;
		FTransformTrajectorySample& Sample = SampleTrajectory.Samples.AddDefaulted_GetRef();
		Sample.TimeInSeconds = static_cast<float>(T);
		Sample.Position = Here + Offset(T);
		// Running, the body will turn to its travel; strafing, it holds its facing.
		FQuat Ahead = Facing;
		const FVector Travel = VelocityAt(T);
		if (!bStrafing && Travel.Size2D() > 50.0)
		{
			const double Delta = FMath::FindDeltaAngleDegrees(FacingYaw, YawOf(Travel));
			const double Turn = FMath::Clamp(Delta, -Settings.FacingTurnRate * T, Settings.FacingTurnRate * T);
			Ahead = FQuat(FVector::UpVector, FMath::DegreesToRadians(Turn)) * Facing;
		}
		Sample.Facing = Ahead;
	}
}
