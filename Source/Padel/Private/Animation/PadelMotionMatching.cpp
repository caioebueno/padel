#include "Animation/PadelMotionMatching.h"

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "BonePose.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchEvent.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchRole.h"
#include "PoseSearch/PoseSearchSchema.h"

#include <cmath>

namespace PadelMotionMatchingDetail
{
	/** Smooth ease for a crossfade, so a new clip comes in without a velocity step. */
	float Ease(double T)
	{
		const double C = FMath::Clamp(T, 0.0, 1.0);
		return static_cast<float>(C * C * (3.0 - 2.0 * C));
	}

	/** A critically damped spring's offset decaying toward 0 over Dt, exactly (Holden's decay_spring_damper_exact). */
	void Decay(FVector& X, FVector& V, double HalfLifeS, double Dt)
	{
		const double Y = 0.5 * (4.0 * 0.69314718056) / (HalfLifeS + 1e-5);
		const FVector J1 = V + X * Y;
		const double EyDt = std::exp(-Y * Dt);
		X = EyDt * (X + J1 * Dt);
		V = EyDt * (V - J1 * (Y * Dt));
	}

	/** The rotation from A to B (B = Delta * A) as a rotation vector, shortest way round. */
	FVector RotationBetween(const FQuat& A, const FQuat& B)
	{
		FQuat Delta = B * A.Inverse();
		Delta.EnforceShortestArcWith(FQuat::Identity);
		return Delta.ToRotationVector();
	}

	double Wrap(double Time, double Length, bool bLoop)
	{
		return bLoop ? FMath::Fmod(Time + Length * 4.0, FMath::Max(Length, 1e-3)) : FMath::Clamp(Time, 0.0, Length);
	}

	bool Sample(const UAnimSequenceBase* Seq, double Time, bool bLoop, FPoseContext& Out)
	{
		if (!Seq || Seq->GetSkeleton() != Out.AnimInstanceProxy->GetRequiredBones().GetSkeletonAsset())
		{
			return false;
		}
		FAnimationPoseData Data(Out);
		Seq->GetAnimationPose(Data, FAnimExtractContext(Time, false, {}, bLoop));
		return true;
	}
}

FPadelMotionMatcher::FPadelMotionMatcher() = default;
FPadelMotionMatcher::~FPadelMotionMatcher() = default;

// The history and the clips playing belong to one node: a copy (the anim instance copies its
// node struct) starts empty and builds its own.
FPadelMotionMatcher::FPadelMotionMatcher(const FPadelMotionMatcher& Other)
	: Databases(Other.Databases)
	, Roles(Other.Roles)
	, Tuning(Other.Tuning)
{
}

FPadelMotionMatcher& FPadelMotionMatcher::operator=(const FPadelMotionMatcher& Other)
{
	if (this != &Other)
	{
		Databases = Other.Databases;
		Roles = Other.Roles;
		Tuning = Other.Tuning;
		Reset();
	}
	return *this;
}

void FPadelMotionMatcher::Configure(const TArray<TObjectPtr<const UPoseSearchDatabase>>& InDatabases, const TArray<EPadelMotionMatchingRole>& InRoles, const FPadelMotionMatchingTuning& InTuning)
{
	Roles = InRoles;
	if (InDatabases != Databases)
	{
		Databases = InDatabases;
		Reset();
		bHistoryBonesCached = false;
	}
	Tuning = InTuning;
}

void FPadelMotionMatcher::Reset()
{
	Stack.Reset();
	History.Reset();
	SinceSearchS = 0.0;
	Picks = 0;
	RootVelocity = FVector::ZeroVector;
	LastOut.Reset();
	PrevOut.Reset();
	OffRot.Reset();
	OffRotVel.Reset();
	OffPos.Reset();
	OffPosVel.Reset();
	bTransitionPending = false;
	bHistoryReset = true;
}

void FPadelMotionMatcher::CacheHistoryBones()
{
	// The bones the schemas read from the history (feet and pelvis), as skeleton indices.
	HistoryBones.Reset();
	for (const UPoseSearchDatabase* Database : Databases)
	{
		const UPoseSearchSchema* Schema = Database ? Database->Schema.Get() : nullptr;
		if (!Schema)
		{
			continue;
		}
		for (const FBoneReference& Bone : Schema->GetBoneReferences(Schema->GetDefaultRole()))
		{
			if (Bone.BoneIndex != INDEX_NONE)
			{
				HistoryBones.AddUnique(static_cast<FBoneIndexType>(Bone.BoneIndex));
			}
		}
	}
	bHistoryBonesCached = true;
}

void FPadelMotionMatcher::Update(const FAnimationUpdateContext& Context, const FTransformTrajectory& Trajectory, double SpeedUnits)
{
	const double Dt = Context.GetDeltaTime();
	LastDeltaS = static_cast<float>(Dt);
	if (Databases.IsEmpty())
	{
		return;
	}
	for (FPlaying& Playing : Stack)
	{
		const double Length = Playing.Clip->GetPlayLength();
		Playing.Time += Dt * Playing.PlayRate;
		Playing.Time = Playing.bLoop ? FMath::Fmod(Playing.Time, FMath::Max(Length, 1e-3)) : FMath::Min(Playing.Time, Length);
	}
	if (!Stack.IsEmpty())
	{
		FPlaying& Newest = Stack.Last();
		Newest.BlendIn = FMath::Min(1.0, Newest.BlendIn + (Tuning.BlendTimeS > 0.0 ? Dt / Tuning.BlendTimeS : 1.0));
		if (Newest.BlendIn >= 1.0 && Stack.Num() > 1)
		{
			// Fully in: what's under it no longer shows.
			Stack.RemoveAt(0, Stack.Num() - 1);
		}
		// Keep the newest pick matched to the simulation's speed as it changes.
		Newest.PlayRate = MatchedPlayRate(Newest, SpeedUnits);
		RootVelocity = MeasureRootVelocity(Newest);
	}

	SinceSearchS += Dt;
	// The search reads the pose history, which RecordHistory fills after each evaluate.
	// M2: a clip that doesn't loop, about to run out, searches now and may switch without beating
	// the continuing pose by the margin: held on its last frame it would freeze and skate.
	const bool bRunningOut = !Stack.IsEmpty() && !Stack.Last().bLoop
		&& Stack.Last().Time >= Stack.Last().Clip->GetPlayLength() - FMath::Max(Tuning.SearchThrottleS, Dt) * Stack.Last().PlayRate;
	if (History.IsValid() && !History->IsEmpty() && (Stack.IsEmpty() || bRunningOut || SinceSearchS >= Tuning.SearchThrottleS))
	{
		SinceSearchS = 0.0;
		Search(Context, Trajectory, SpeedUnits, bRunningOut);
	}
}

void FPadelMotionMatcher::Search(const FAnimationUpdateContext& Context, const FTransformTrajectory& Trajectory, double SpeedUnits, bool bRunningOut)
{
	using namespace UE::PoseSearch;
	History->SetTrajectory(Trajectory);

	FPoseSearchContinuingProperties Continuing;
	if (!Stack.IsEmpty())
	{
		const FPlaying& Newest = Stack.Last();
		Continuing.PlayingAsset = Newest.Clip;
		Continuing.PlayingAssetAccumulatedTime = static_cast<float>(Newest.Time);
		Continuing.PlayingAssetDatabase = Newest.Database;
	}
	// The chooser: still (slow and not speeding up) searches the idles, the stops after moving and
	// the turns in place when the body has turned; otherwise the starts, loops and pivots. All at once, the
	// stops' and turns' cost biases (they favour carrying on) win where they don't belong.
	const FTransformTrajectorySample Now = Trajectory.GetSampleAtTime(0.0f);
	const FTransformTrajectorySample Before = Trajectory.GetSampleAtTime(-0.1f);
	const FTransformTrajectorySample Oldest = Trajectory.Samples.IsEmpty() ? Now : Trajectory.Samples[0];
	const FTransformTrajectorySample Soon = Trajectory.GetSampleAtTime(0.35f);
	// Still: coming to rest (the predicted speed a moment ahead slow, and not speeding up).
	const double SpeedSoon = FVector::Dist2D(Trajectory.GetSampleAtTime(0.45f).Position, Soon.Position) / 0.1;
	const double SpeedBefore = FVector::Dist2D(Now.Position, Before.Position) / 0.1;
	const bool bStill = SpeedSoon < Tuning.StillBelowUnits && SpeedSoon <= FMath::Max(SpeedUnits, SpeedBefore) + 1.0;
	const bool bTurning = FMath::RadiansToDegrees(Now.Facing.AngularDistance(Oldest.Facing)) >= Tuning.TurnInPlaceDeg;
	// Stopping: still coming, the player still moving (once at rest, an idle takes over).
	const bool bStopping = FVector::Dist2D(Now.Position, Oldest.Position) >= Tuning.StopAfterUnits && SpeedUnits >= 10.0;
	TArray<const UObject*, TInlineAllocator<16>> Assets;
	for (int32 Index = 0; Index < Databases.Num(); ++Index)
	{
		const EPadelMotionMatchingRole Role = Roles.IsValidIndex(Index) ? Roles[Index] : EPadelMotionMatchingRole::Moving;
		const double Gait = SpeedUnits; // the prediction overshoots while speeding up
		const double Over = 1.0 + Tuning.GaitOverlap;
		const double Under = 1.0 - Tuning.GaitOverlap;
		const bool bUse = Role == EPadelMotionMatchingRole::MovingWalk ? !bStill && Gait < Tuning.RunFromUnits * Over
			: Role == EPadelMotionMatchingRole::Moving ? !bStill && Gait >= Tuning.RunFromUnits * Under && Gait < Tuning.SprintFromUnits * Over
			: Role == EPadelMotionMatchingRole::MovingSprint ? !bStill && Gait >= Tuning.SprintFromUnits * Under
			: Role == EPadelMotionMatchingRole::Still ? bStill
			// Footwork: most of it is under 1 m/s (adjustments, split steps), where the chooser
			// counts the body as still, and the rest is walk-band shuffling.
			: Role == EPadelMotionMatchingRole::StillOrWalk ? bStill || Gait < Tuning.RunFromUnits * Over
			: Role == EPadelMotionMatchingRole::Stop ? bStill && bStopping
			: bStill && bTurning;
		if (Databases[Index] && bUse)
		{
			Assets.Add(Databases[Index]);
		}
	}
	const UObject* AnimContext = Context.AnimInstanceProxy->GetAnimInstanceObject();
	const FRole SearchRole = DefaultRole;
	const IPoseHistory* PoseHistory = History.Get();
	// M2: keep the best result and the continuing pose's own, so a switch has to beat carrying on
	// by SwitchCostMargin (the search alone picks a neighbour for any tiny gain, which reads as jitter).
	FSearchResult Best;
	FSearchResult ContinuingResult;
	FSearchResults Results;
	Results.UpdateWith = [&Best, &ContinuingResult](const FSearchResult& Candidate)
	{
		if (Candidate.bIsContinuingPoseSearch && Candidate.PoseCost < ContinuingResult.PoseCost)
		{
			ContinuingResult = Candidate;
		}
		if (Candidate.PoseCost < Best.PoseCost)
		{
			Best = Candidate;
		}
	};
	Results.FinalizeResults = [&Best]()
	{
		if (Best.IsValid() && !Best.IsAssetTimeValid())
		{
			Best.SetAssetTime(Best.Database->GetNormalizedAssetTime(Best.PoseIdx));
		}
	};
	Results.GetBestResult = [&Best]() { return Best; };
	Results.IterateOverSearchResults = [&Best](const TFunctionRef<bool(FSearchResult&)> Function) { return Best.IsValid() && Function(Best); };
PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
	UPoseSearchLibrary::MotionMatch(
		MakeArrayView(&AnimContext, 1), MakeArrayView(&SearchRole, 1), MakeArrayView(&PoseHistory, 1),
		MakeArrayView(Assets.GetData(), Assets.Num()), Continuing, FPoseSearchFutureProperties(), FPoseSearchEvent(), Results);
PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
	Results.FinalizeResults();
	if (!Best.IsValid() || Best.bIsContinuingPoseSearch)
	{
		return;
	}
	if (!bRunningOut && ContinuingResult.IsValid() && static_cast<float>(Best.PoseCost) + Tuning.SwitchCostMargin >= static_cast<float>(ContinuingResult.PoseCost))
	{
		return;
	}
	FPoseSearchBlueprintResult Picked;
	if (!Picked.InitFrom(Best, 1.0f) || Picked.bIsContinuingPoseSearch)
	{
		return;
	}
	const UAnimSequenceBase* Clip = Cast<UAnimSequenceBase>(Picked.SelectedAnim.Get());
	if (!Clip)
	{
		return;
	}
	const bool bInertial = Tuning.InertialHalfLifeS > 0.0;
	if (!Stack.IsEmpty())
	{
		const FPlaying& Newest = Stack.Last();
		if (!bInertial && Newest.BlendIn < Tuning.RepickAfterBlend)
		{
			return;
		}
		// The same clip close to where it's playing: carry on rather than blend to itself.
		if (Newest.Clip == Clip && FMath::Abs(Newest.Time - Picked.SelectedTime) < Tuning.PoseJumpThresholdS)
		{
			return;
		}
	}
	FPlaying Next;
	Next.Clip = Clip;
	Next.Database = Picked.SelectedDatabase.Get();
	Next.Time = Picked.SelectedTime;
	Next.bLoop = Picked.bLoop;
	Next.BlendIn = Stack.IsEmpty() || bInertial ? 1.0 : 0.0;
	Next.PlayRate = MatchedPlayRate(Next, SpeedUnits);
	if (bInertial)
	{
		// M2: one clip plays; the switch's difference is carried as a decaying offset (Inertialize).
		bTransitionPending = !Stack.IsEmpty();
		Stack.Reset();
	}
	Stack.Add(Next);
	RootVelocity = MeasureRootVelocity(Next);
	++Picks;
	while (Stack.Num() > FMath::Max(Tuning.MaxBlends, 1))
	{
		// Drop the clip that shows least (its weight is its own fade in times what every newer one
		// leaves): dropping the base while it still showed would pop.
		int32 Least = 0;
		double LeastWeight = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Stack.Num() - 1; ++Index)
		{
			double Weight = Index == 0 ? 1.0 : PadelMotionMatchingDetail::Ease(Stack[Index].BlendIn);
			for (int32 Newer = Index + 1; Newer < Stack.Num(); ++Newer)
			{
				Weight *= 1.0 - PadelMotionMatchingDetail::Ease(Stack[Newer].BlendIn);
			}
			if (Weight < LeastWeight)
			{
				LeastWeight = Weight;
				Least = Index;
			}
		}
		Stack.RemoveAt(Least);
		if (Least == 0 && !Stack.IsEmpty())
		{
			Stack[0].BlendIn = 1.0; // the next one up is the base now
		}
	}
}

FVector FPadelMotionMatcher::MeasureRootVelocity(const FPlaying& Playing) const
{
	constexpr double Window = 0.1;
	const double Length = Playing.Clip ? Playing.Clip->GetPlayLength() : 0.0;
	const double Start = FMath::Clamp(Playing.Time, 0.0, FMath::Max(Length - Window, 0.0));
	const double End = FMath::Min(Start + Window, Length);
	if (End - Start < 1e-3)
	{
		return FVector::ZeroVector;
	}
	const FTransform Motion = Playing.Clip->ExtractRootMotionFromRange(Start, End, FAnimExtractContext());
	FVector Velocity = Motion.GetTranslation() / (End - Start) * Playing.PlayRate;
	Velocity.Z = 0.0;
	return Velocity;
}

double FPadelMotionMatcher::MatchedPlayRate(const FPlaying& Playing, double SpeedUnits) const
{
	// The clip's own root speed around its playhead (root motion is read even though the
	// pose plays it in place), against the simulation's.
	constexpr double Window = 0.1;
	const double Length = Playing.Clip->GetPlayLength();
	const double Start = FMath::Clamp(Playing.Time, 0.0, FMath::Max(Length - Window, 0.0));
	const double End = FMath::Min(Start + Window, Length);
	if (End - Start < 1e-3)
	{
		return 1.0;
	}
	const FTransform Motion = Playing.Clip->ExtractRootMotionFromRange(Start, End, FAnimExtractContext());
	const double ClipSpeed = Motion.GetTranslation().Size2D() / (End - Start);
	// Below a slow walk the ratio is noise (idles, plants, turns in place): play as authored.
	constexpr double MinSpeed = 30.0;
	if (ClipSpeed < MinSpeed || SpeedUnits < MinSpeed)
	{
		return 1.0;
	}
	return FMath::Clamp(SpeedUnits / ClipSpeed, Tuning.PlayRateMin, Tuning.PlayRateMax);
}

bool FPadelMotionMatcher::Evaluate(FPoseContext& Output)
{
	using namespace PadelMotionMatchingDetail;
	bool bPosed = false;
	for (const FPlaying& Playing : Stack)
	{
		if (!bPosed)
		{
			bPosed = Sample(Playing.Clip, Playing.Time, Playing.bLoop, Output);
			continue;
		}
		FPoseContext Next(Output);
		if (Sample(Playing.Clip, Playing.Time, Playing.bLoop, Next))
		{
			const float Alpha = Ease(Playing.BlendIn);
			for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				Output.Pose[Bone].BlendWith(Next.Pose[Bone], Alpha);
			}
			Output.Pose.NormalizeRotations();
		}
	}
	if (bPosed && Tuning.InertialHalfLifeS > 0.0)
	{
		Inertialize(Output);
	}
	return bPosed;
}

void FPadelMotionMatcher::Inertialize(FPoseContext& Output)
{
	// M2 (docs/animation/MOVEMENT_FLUENCY.md): Holden's offset inertialization. At a switch the
	// difference between the pose shown last frame and the new clip at that same moment, and the
	// difference of their velocities, become an offset that decays on a critically damped spring;
	// the new clip plays under it at once. Unlike a crossfade, nothing of two clips ever averages
	// (no floaty in-between pose) and the motion carries its velocity through the switch.
	using namespace PadelMotionMatchingDetail;
	const int32 NumBones = Output.Pose.GetNumBones();
	const double Dt = FMath::Max(static_cast<double>(LastDeltaS), 1e-4);
	if (OffRot.Num() != NumBones)
	{
		OffRot.Init(FVector::ZeroVector, NumBones);
		OffRotVel.Init(FVector::ZeroVector, NumBones);
		OffPos.Init(FVector::ZeroVector, NumBones);
		OffPosVel.Init(FVector::ZeroVector, NumBones);
		LastOut.Reset();
		PrevOut.Reset();
		bTransitionPending = false;
	}
	if (bTransitionPending && LastOut.Num() == NumBones && !Stack.IsEmpty())
	{
		// The new clip one frame back, at the instant LastOut showed, and now (for its velocity).
		const FPlaying& Playing = Stack.Last();
		FPoseContext Before(Output);
		const double Length = Playing.Clip->GetPlayLength();
		if (Sample(Playing.Clip, Wrap(Playing.Time - Dt * Playing.PlayRate, Length, Playing.bLoop), Playing.bLoop, Before))
		{
			const bool bPrev = PrevOut.Num() == NumBones;
			for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
			{
				const int32 I = Bone.GetInt();
				const FTransform& Src = Before.Pose[Bone];
				const FTransform& Now = Output.Pose[Bone];
				const FVector SrcRotVel = RotationBetween(Src.GetRotation(), Now.GetRotation()) / Dt;
				const FVector SrcPosVel = (Now.GetTranslation() - Src.GetTranslation()) / Dt;
				const FVector OutRotVel = bPrev ? RotationBetween(PrevOut[I].GetRotation(), LastOut[I].GetRotation()) / Dt : SrcRotVel;
				const FVector OutPosVel = bPrev ? (LastOut[I].GetTranslation() - PrevOut[I].GetTranslation()) / Dt : SrcPosVel;
				OffRot[I] = RotationBetween(Src.GetRotation(), LastOut[I].GetRotation());
				OffRotVel[I] = OutRotVel - SrcRotVel;
				OffPos[I] = LastOut[I].GetTranslation() - Src.GetTranslation();
				OffPosVel[I] = OutPosVel - SrcPosVel;
			}
		}
	}
	bTransitionPending = false;
	for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		const int32 I = Bone.GetInt();
		Decay(OffRot[I], OffRotVel[I], Tuning.InertialHalfLifeS, Dt);
		Decay(OffPos[I], OffPosVel[I], Tuning.InertialHalfLifeS, Dt);
		FTransform& T = Output.Pose[Bone];
		T.SetRotation((FQuat::MakeFromRotationVector(OffRot[I]) * T.GetRotation()).GetNormalized());
		T.AddToTranslation(OffPos[I]);
	}
	PrevOut = MoveTemp(LastOut);
	LastOut.SetNum(NumBones);
	for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
	{
		LastOut[Bone.GetInt()] = Output.Pose[Bone];
	}
}

void FPadelMotionMatcher::RecordHistory(const FPoseContext& Locomotion)
{
	if (Databases.IsEmpty())
	{
		return;
	}
	if (!History.IsValid())
	{
		History = MakeShared<UE::PoseSearch::FPoseHistory, ESPMode::ThreadSafe>();
		History->Initialize_AnyThread(FMath::Max(Tuning.HistoryPoses, 2), static_cast<float>(Tuning.HistorySamplingS));
		bHistoryReset = true;
	}
	if (!bHistoryBonesCached)
	{
		CacheHistoryBones();
		bHistoryReset = true;
	}
	FCSPose<FCompactPose> ComponentSpace;
	ComponentSpace.InitPose(Locomotion.Pose);
	UE::PoseSearch::FComponentSpacePoseProvider Provider(ComponentSpace);
	History->EvaluateComponentSpace_AnyThread(LastDeltaS, Provider, false, 0.0f, 1.0f, 1.0f, bHistoryReset, bHistoryReset, HistoryBones, Locomotion.Curve);
	bHistoryReset = false;
}

FName FPadelMotionMatcher::CurrentDatabase() const
{
	return Stack.IsEmpty() || !Stack.Last().Database ? NAME_None : Stack.Last().Database->GetFName();
}

const UAnimSequenceBase* FPadelMotionMatcher::CurrentClip() const
{
	return Stack.IsEmpty() ? nullptr : Stack.Last().Clip;
}
