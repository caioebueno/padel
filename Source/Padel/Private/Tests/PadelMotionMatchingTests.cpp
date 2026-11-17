// M1 motion matching (docs/animation/MOVEMENT_FLUENCY.md) on a real skeleton: one placeholder
// pawn's pose driven along a scripted path (stand, accelerate to a run, run 3 m, stop, a 180°
// reversal, a sideways shuffle, stop) with its anim ticked at 60 Hz. The planted feet barely
// slide standing and moving steadily (the transitions still skate some, until M2's foot locking
// and warping), no body bone jumps between frames, and the search moves from idle to a start to
// loops to a stop.
// Skipped on a machine without the motion matching databases (git-ignored, built by
// Tools/AssetPipeline/unreal/animation_setup.py).
#include "Misc/AutomationTest.h"

#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Animation/PadelPlayerPoseNode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Misc/ScopeExit.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelMotionMatchingTests
{
	constexpr double Dt = 1.0 / 60.0;

	/** The scripted player: ground position (cm), velocity (cm/s) and the yaw the body faces. */
	struct FScript
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		double FacingYawDeg = 0.0;
		FString Phase;
	};

	/** Moves Velocity toward Want at Accel (cm/s²) for one step. */
	void Accelerate(FScript& S, const FVector& Want, double Accel)
	{
		const FVector Delta = Want - S.Velocity;
		const double Step = Accel * Dt;
		S.Velocity = Delta.Size() <= Step ? Want : S.Velocity + Delta.GetSafeNormal() * Step;
	}

	/** Turns the facing toward Yaw at the model's old constant rate. */
	void Face(FScript& S, double YawDeg)
	{
		const double Delta = FMath::FindDeltaAngleDegrees(S.FacingYawDeg, YawDeg);
		S.FacingYawDeg += FMath::Clamp(Delta, -540.0 * Dt, 540.0 * Dt);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelMotionMatchingTest, "Padel.Animation.MotionMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelMotionMatchingTest::RunTest(const FString& Parameters)
{
	using namespace PadelMotionMatchingTests;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	APadelMatchDirector* Director = World->SpawnActor<APadelMatchDirector>();
	Padel::Ui::FMatchSetup Setup;
	Setup.SetsToWin = 1;
	Setup.HumanPlayerId = -1;
	if (!TestNotNull(TEXT("Director"), Director) || !TestTrue(TEXT("Match started"), Director->StartMatch(Setup)))
	{
		return false;
	}
	ON_SCOPE_EXIT { Director->CloseMatch(); };
	Director->Tick(1.0f / 60.0f);

	APadelPlayerPawn* Pawn = nullptr;
	USkeletalMeshComponent* Driver = nullptr;
	UPadelPlayerAnimInstance* Anim = nullptr;
	for (int32 Id = 0; Id < Padel::NumPlayers && !Anim; ++Id)
	{
		Pawn = Director->GetPawn(Id);
		Driver = Pawn && Pawn->GetAnimation()->HasSkeletalBody() ? Pawn->GetAnimation()->GetDriverBody() : nullptr;
		Anim = Driver ? Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance()) : nullptr;
	}
	if (!Anim || Anim->LocomotionSet.MotionMatchingDatabases.IsEmpty())
	{
		AddInfo(TEXT("No motion matching databases on this machine (run animation_setup.py); skipped."));
		return true;
	}
#if WITH_EDITOR
	for (const UPoseSearchDatabase* Database : Anim->LocomotionSet.MotionMatchingDatabases)
	{
		using namespace UE::PoseSearch;
		const EAsyncBuildIndexResult Result = FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database,
			ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
		if (!TestTrue(FString::Printf(TEXT("%s has a search index"), *Database->GetName()), Result == EAsyncBuildIndexResult::Success))
		{
			return false;
		}
	}
#endif
	// The pose alone, nothing else on top.
	Anim->RawSwingWeight = 0.0f;
	Anim->ContactWarpWeight = 0.0f;
	Anim->ArmCarryAlpha = 0.0f;
	Anim->Body = FPadelBodyDynamics();
	Anim->Stance = FPadelBodyDynamics();

	// The contact point of each foot: the lower of the ankle and the ball (a foot rolls from heel
	// to toe, so the ankle alone travels over a planted foot).
	const FName Feet[2] = { FName(TEXT("foot_l")), FName(TEXT("foot_r")) };
	const FName Balls[2] = { FName(TEXT("ball_l")), FName(TEXT("ball_r")) };
	const double Ground = Pawn->GetActorLocation().Z;
	FScript S;
	S.Position = FVector(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Ground);
	S.FacingYawDeg = Pawn->GetActorRotation().Yaw;
	const FVector Start = S.Position;
	const FVector Ahead = FRotator(0.0, S.FacingYawDeg, 0.0).Vector();
	const FVector Side = FRotator(0.0, S.FacingYawDeg + 90.0, 0.0).Vector();
	const double RunSpeed = 400.0;
	const double StartYaw = S.FacingYawDeg;

	TArray<FVector> Past;
	TArray<FString> Phases;
	TArray<FName> Picks;
	TArray<FVector> FootPath[2];
	TArray<FVector> FootLocal[2];
	TArray<double> Rates;
	TArray<TArray<FQuat>> Rotations;
	int32 Step = 0;
	auto Tick = [&](const FString& Phase)
	{
		S.Phase = Phase;
		S.Position += S.Velocity * Dt;
		World->TimeSeconds += Dt;
		Pawn->SetActorLocationAndRotation(S.Position, FRotator(0.0, S.FacingYawDeg, 0.0));
		// The model's trajectory: the past as it happened, the future carried on at this velocity.
		Past.Add(S.Position);
		TArray<FVector> Trajectory;
		for (const float T : Anim->TrajectoryTimes)
		{
			const int32 Back = FMath::Min(Past.Num() - 1, FMath::RoundToInt(-T / Dt));
			Trajectory.Add(T < 0.0f ? Past[Past.Num() - 1 - Back] : S.Position + S.Velocity * T);
		}
		Padel::Animation::FPlayerAnimFrame Frame;
		Frame.SpeedMps = S.Velocity.Size2D() / 100.0;
		Frame.Locomotion = Frame.SpeedMps < 0.3 ? Padel::Animation::ELocomotionState::Idle : Padel::Animation::ELocomotionState::Run;
		const double TravelYaw = FMath::RadiansToDegrees(FMath::Atan2(S.Velocity.Y, S.Velocity.X));
		Frame.DirectionDeg = FMath::FindDeltaAngleDegrees(S.FacingYawDeg, TravelYaw);
		Anim->ApplyFrame(Frame, nullptr, nullptr, nullptr, Trajectory);
		Anim->Body = FPadelBodyDynamics();
		Anim->Stance = FPadelBodyDynamics();
		Driver->TickAnimation(static_cast<float>(Dt), false);
		Driver->RefreshBoneTransforms();
		Phases.Add(Phase);
		Picks.Add(Anim->GetPoseNode().GetMotionMatcher().CurrentDatabase());
		Rates.Add(Anim->GetPoseNode().GetMotionMatcher().CurrentPlayRate());
		for (int32 F = 0; F < 2; ++F)
		{
			const FVector Ankle = Driver->GetSocketLocation(Feet[F]);
			const FVector Ball = Driver->GetBoneIndex(Balls[F]) != INDEX_NONE ? Driver->GetSocketLocation(Balls[F]) : Ankle;
			FootPath[F].Add(Ball.Z < Ankle.Z ? Ball : Ankle);
			FootLocal[F].Add(Driver->GetComponentTransform().InverseTransformPosition(Ball.Z < Ankle.Z ? Ball : Ankle));
		}
		TArray<FQuat>& Frame0 = Rotations.AddDefaulted_GetRef();
		for (const FTransform& Bone : Driver->GetComponentSpaceTransforms())
		{
			Frame0.Add(Bone.GetRotation());
		}
		++Step;
	};

	for (int32 I = 0; I < 60; ++I) { Tick(TEXT("stand")); }
	while (S.Velocity.Size2D() < RunSpeed - 1.0) { Accelerate(S, Ahead * RunSpeed, 800.0); Tick(TEXT("start")); }
	const FVector RunFrom = S.Position;
	while (FVector::Dist2D(S.Position, RunFrom) < 300.0) { Tick(TEXT("run")); }
	while (S.Velocity.Size2D() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Tick(TEXT("stop")); }
	S.Velocity = FVector::ZeroVector;
	for (int32 I = 0; I < 60; ++I) { Tick(TEXT("settle")); }
	// A 180° reversal out of a run: accelerate ahead, then turn back through zero.
	while (S.Velocity.Size2D() < RunSpeed - 1.0) { Accelerate(S, Ahead * RunSpeed, 800.0); Tick(TEXT("start2")); }
	for (int32 I = 0; I < 30; ++I) { Tick(TEXT("run2")); }
	while (FVector::DotProduct(S.Velocity, -Ahead) < RunSpeed - 1.0)
	{
		Accelerate(S, -Ahead * RunSpeed, 1200.0);
		if (FVector::DotProduct(S.Velocity, Ahead) < 0.0)
		{
			Face(S, StartYaw + 180.0);
		}
		Tick(TEXT("reverse"));
	}
	for (int32 I = 0; I < 45; ++I) { Face(S, StartYaw + 180.0); Tick(TEXT("run back")); }
	while (S.Velocity.Size2D() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Tick(TEXT("stop2")); }
	S.Velocity = FVector::ZeroVector;
	for (int32 I = 0; I < 30; ++I) { Tick(TEXT("settle2")); }
	// A side shuffle, square to where the body faces.
	for (int32 I = 0; I < 75; ++I) { Accelerate(S, Side * 200.0, 800.0); Tick(TEXT("shuffle")); }
	while (S.Velocity.Size2D() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Tick(TEXT("stop3")); }
	for (int32 I = 0; I < 45; ++I) { Tick(TEXT("end")); }

	if (!TestTrue(TEXT("Motion matching drove the pose"), Anim->GetPoseNode().IsMotionMatching()))
	{
		return false;
	}

	// 1. Planted feet: a foot within 3 cm of the ground (its height at the 10th percentile: a
	// clip's odd low frame doesn't set it) for at least 0.05 s is planted; how far it travels over
	// that spell, for its length, is its slide (the simulation moves the pawn, the clip the
	// limbs, so a stride that doesn't match the speed skates). Frame-to-frame jitter around touch
	// down and lift off cancels out, as it does on screen.
	double SlideCm = 0.0;
	double PlantedS = 0.0;
	double WorstSlideCmps = 0.0;
	TMap<FString, FVector2D> SlideByPhase; // cm slid, seconds planted
	for (int32 F = 0; F < 2; ++F)
	{
		TArray<double> Heights;
		for (const FVector& P : FootPath[F]) { Heights.Add(P.Z); }
		Heights.Sort();
		const double Floor = Heights.IsEmpty() ? 0.0 : Heights[Heights.Num() / 10] + 3.0;
		int32 From = INDEX_NONE;
		for (int32 I = 0; I <= FootPath[F].Num(); ++I)
		{
			const bool bDown = I < FootPath[F].Num() && FootPath[F][I].Z < Floor;
			if (bDown && From == INDEX_NONE) { From = I; }
			if (!bDown && From != INDEX_NONE)
			{
				const int32 To = I - 1;
				const double Spell = (To - From) * Dt;
				if (Spell >= 0.05)
				{
					const double Moved = FVector::Dist2D(FootPath[F][To], FootPath[F][From]);
					SlideCm += Moved;
					PlantedS += Spell;
					WorstSlideCmps = FMath::Max(WorstSlideCmps, Moved / Spell);
					SlideByPhase.FindOrAdd(Phases[From]) += FVector2D(Moved, Spell);
				}
				From = INDEX_NONE;
			}
		}
	}
	const double SlideCmps = PlantedS > 0.0 ? SlideCm / PlantedS : 0.0;
	TArray<FString> ByPhase;
	for (const TPair<FString, FVector2D>& Entry : SlideByPhase)
	{
		ByPhase.Add(FString::Printf(TEXT("%s %.0f cm/s over %.2f s"), *Entry.Key, Entry.Value.X / FMath::Max(Entry.Value.Y, 1e-3), Entry.Value.Y));
	}
	AddInfo(TEXT("Slide by phase: ") + FString::Join(ByPhase, TEXT("; ")));
	AddInfo(FString::Printf(TEXT("Planted foot slide: %.1f cm/s on average over %.1f s planted, worst spell %.0f cm/s"), SlideCmps, PlantedS, WorstSlideCmps));
	TestTrue(TEXT("Feet are planted some of the time"), PlantedS > 1.0);
	// Standing and moving steadily the stride matches the speed; the transitions (starts, stops,
	// the 180° reversal) still skate where the simulation brakes and turns harder than the clips,
	// which foot locking and stride and orientation warping take out (M2). Both are kept down here.
	FVector2D Steady = FVector2D::ZeroVector;
	for (const TCHAR* Phase : { TEXT("stand"), TEXT("run"), TEXT("run2"), TEXT("run back"), TEXT("settle"), TEXT("settle2"), TEXT("shuffle"), TEXT("end") })
	{
		if (const FVector2D* Found = SlideByPhase.Find(Phase)) { Steady += *Found; }
	}
	const double SteadyCmps = Steady.Y > 0.0 ? Steady.X / Steady.Y : 0.0;
	AddInfo(FString::Printf(TEXT("Steady phases: %.1f cm/s over %.1f s"), SteadyCmps, Steady.Y));
	TestTrue(FString::Printf(TEXT("Standing and moving steadily, planted feet slide under 25 cm/s (%.1f)"), SteadyCmps), SteadyCmps < 25.0);
	TestTrue(FString::Printf(TEXT("Over the whole path, starts, stops and the reversal too, under 50 cm/s (%.1f)"), SlideCmps), SlideCmps < 50.0);

	// 2. No pops: no body bone (pelvis, spine, neck, head, legs, upper arms) turns more than 20°
	// between two 60 Hz frames in component space. Hands, fingers and toes swing faster than that
	// in a real sprint.
	TArray<int32> Core;
	for (const TCHAR* Name : { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"),
		TEXT("neck_01"), TEXT("head"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"),
		TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("upperarm_l"), TEXT("upperarm_r") })
	{
		const int32 Index = Driver->GetBoneIndex(FName(Name));
		if (Index != INDEX_NONE) { Core.Add(Index); }
	}
	double WorstDeg = 0.0;
	int32 WorstFrame = 0;
	int32 WorstBone = 0;
	for (int32 I = 1; I < Rotations.Num(); ++I)
	{
		for (const int32 B : Core)
		{
			if (!Rotations[I].IsValidIndex(B) || !Rotations[I - 1].IsValidIndex(B)) { continue; }
			const double Deg = FMath::RadiansToDegrees(Rotations[I][B].AngularDistance(Rotations[I - 1][B]));
			if (Deg > WorstDeg) { WorstDeg = Deg; WorstFrame = I; WorstBone = B; }
		}
	}
	AddInfo(FString::Printf(TEXT("Largest bone turn in one frame: %.1f deg (%s, %s)"), WorstDeg,
		*Driver->GetBoneName(WorstBone).ToString(), *Phases[WorstFrame]));
	TestTrue(TEXT("The body bones are found"), Core.Num() >= 10);
	TestTrue(FString::Printf(TEXT("No body bone turns over 20 deg in a frame (%.1f)"), WorstDeg), WorstDeg < 20.0);

	// 3. The search follows the path: a start out of standing, then loops, then a stop.
	auto First = [&](const TCHAR* Kind, int32 From) -> int32
	{
		for (int32 I = From; I < Picks.Num(); ++I)
		{
			if (Picks[I].ToString().Contains(Kind)) { return I; }
		}
		return INDEX_NONE;
	};
	TArray<FString> Seen;
	for (int32 I = 0; I < Picks.Num(); ++I)
	{
		const FString Pick = Picks[I].ToString() + TEXT(" (") + Phases[I] + TEXT(")");
		if (Seen.IsEmpty() || Seen.Last() != Pick) { Seen.Add(Pick); }
	}
	AddInfo(TEXT("Picks: ") + FString::Join(Seen, TEXT(", ")));
	const int32 StartAt = Phases.IndexOfByKey(TEXT("start"));
	const int32 StopAt = Phases.IndexOfByKey(TEXT("stop"));
	const int32 Starts = First(TEXT("Starts"), StartAt);
	const int32 Loops = First(TEXT("Loops"), Starts == INDEX_NONE ? StartAt : Starts);
	const int32 Stops = First(TEXT("Stops"), StopAt);
	TestTrue(TEXT("Standing still picks an idle"), First(TEXT("Idles"), 0) != INDEX_NONE && First(TEXT("Idles"), 0) < StartAt);
	TestTrue(TEXT("Setting off picks a start"), Starts != INDEX_NONE && Starts < StopAt);
	TestTrue(TEXT("Running on picks a loop after the start"), Loops != INDEX_NONE && Loops < StopAt);
	TestTrue(TEXT("Stopping picks a stop"), Stops != INDEX_NONE && Stops < Phases.IndexOfByKey(TEXT("start2")));
	return true;
}

#endif
