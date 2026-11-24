#include "PadelMotionProbe.h"

#include "Animation/Model/PlayerAnimModel.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/PadelAnimData.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Animation/PadelPlayerPoseNode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "Gameplay/PadelShotUnits.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "UI/Model/PadelUiMatchSetup.h"

#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelMotionProbe
{
	namespace Detail
	{
		constexpr double Dt = 1.0 / 60.0;
		/** The first frames settle the body from its reference pose (and the MetaHuman's first retarget): not counted for pops and jerk. */
		constexpr int32 WarmUpFrames = 15;

		/** The scripted player: ground position (cm), velocity (cm/s) and the yaw the body faces. */
		struct FScript
		{
			FVector Position = FVector::ZeroVector;
			FVector Velocity = FVector::ZeroVector;
			double FacingYawDeg = 0.0;
		};

		void Accelerate(FScript& S, const FVector& Want, double Accel)
		{
			const FVector Delta = Want - S.Velocity;
			const double Step = Accel * Dt;
			S.Velocity = Delta.Size() <= Step ? Want : S.Velocity + Delta.GetSafeNormal() * Step;
		}

		/** Turns the facing toward Yaw at the model's old constant rate (the locomotion-only drive). */
		void Face(FScript& S, double YawDeg)
		{
			const double Delta = FMath::FindDeltaAngleDegrees(S.FacingYawDeg, YawDeg);
			S.FacingYawDeg += FMath::Clamp(Delta, -540.0 * Dt, 540.0 * Dt);
		}

		double YawOf(const FVector& V) { return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)); }

		/**
		 * The layer video's path (EPath::LayerVideo). Sets S's velocity and facing for each frame
		 * and calls Step(Phase, Mode), which must move S.Position on by S.Velocity * Dt.
		 */
		void LayerVideoPath(FScript& S, double StartYaw, TFunctionRef<void(const TCHAR*, Padel::EMovementMode)> Step)
		{
			using Padel::EMovementMode;
			const FVector Ahead = FRotator(0.0, StartYaw, 0.0).Vector();
			const FVector Side = FRotator(0.0, StartYaw + 90.0, 0.0).Vector();
			const double Walk = 150.0;
			const double RunSpeed = 450.0;
			const double Jog = 300.0;
			const double Shuffle = 200.0;
			auto Speed = [&S]() { return S.Velocity.Size2D(); };
			auto Along = [&S](const FVector& From, const FVector& Dir) { return FVector::DotProduct(S.Position - From, Dir); };

			for (int32 I = 0; I < 60; ++I) { Step(TEXT("stand"), EMovementMode::Adjust); }
			// Walk 3 m.
			FVector From = S.Position;
			while (Along(From, Ahead) < 300.0)
			{
				Accelerate(S, Ahead * Walk, 500.0);
				Face(S, StartYaw);
				Step(Speed() < Walk - 1.0 ? TEXT("start") : TEXT("walk"), EMovementMode::Run);
			}
			// Speed up to a run, 4 m in all.
			From = S.Position;
			while (Along(From, Ahead) < 400.0)
			{
				Accelerate(S, Ahead * RunSpeed, 800.0);
				Step(Speed() < RunSpeed - 1.0 ? TEXT("speed up") : TEXT("run"), EMovementMode::Run);
			}
			while (Speed() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Step(TEXT("stop"), EMovementMode::Run); }
			S.Velocity = FVector::ZeroVector;
			for (int32 I = 0; I < 15; ++I) { Step(TEXT("settle"), EMovementMode::Adjust); }
			// A 90° turn into a 2 m walk.
			From = S.Position;
			while (Along(From, Side) < 200.0)
			{
				Accelerate(S, Side * Walk, 500.0);
				Face(S, StartYaw + 90.0);
				Step(Speed() < Walk - 1.0 ? TEXT("turn") : TEXT("walk"), EMovementMode::Run);
			}
			// A 180° reversal into a jog, 2 m past the turn.
			const FVector TurnAt = S.Position;
			while (FVector::DotProduct(S.Velocity, -Side) < Jog - 1.0)
			{
				Accelerate(S, -Side * Jog, 1200.0);
				if (FVector::DotProduct(S.Velocity, Side) < 0.0)
				{
					Face(S, StartYaw - 90.0);
				}
				Step(TEXT("reverse"), EMovementMode::Run);
			}
			while (Along(TurnAt, -Side) < 200.0) { Face(S, StartYaw - 90.0); Step(TEXT("jog"), EMovementMode::Run); }
			while (Speed() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Face(S, StartYaw - 90.0); Step(TEXT("stop2"), EMovementMode::Run); }
			S.Velocity = FVector::ZeroVector;
			// Square up to the net, then shuffle 2 m each way.
			for (int32 I = 0; I < 20; ++I) { Face(S, StartYaw); Step(TEXT("square up"), EMovementMode::Adjust); }
			From = S.Position;
			while (Along(From, Side) < 180.0)
			{
				Accelerate(S, Side * Shuffle, 800.0);
				Face(S, StartYaw);
				Step(Speed() < Shuffle - 1.0 ? TEXT("shuffle start") : TEXT("shuffle"), EMovementMode::Shuffle);
			}
			while (FVector::DotProduct(S.Velocity, -Side) < Shuffle - 1.0)
			{
				Accelerate(S, -Side * Shuffle, 1000.0);
				Face(S, StartYaw);
				Step(TEXT("shuffle turn"), EMovementMode::Shuffle);
			}
			while (Along(From, Side) > 20.0) { Face(S, StartYaw); Step(TEXT("shuffle"), EMovementMode::Shuffle); }
			while (Speed() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Face(S, StartYaw); Step(TEXT("stop3"), EMovementMode::Shuffle); }
			S.Velocity = FVector::ZeroVector;
			for (int32 I = 0; I < 60; ++I) { Step(TEXT("end"), EMovementMode::Adjust); }
		}

		/** The base check paths (EPath::WalkSteady..WalkTurn90): one move each. Step as LayerVideoPath. */
		void BasePath(EPath Path, FScript& S, double StartYaw, TFunctionRef<void(const TCHAR*, Padel::EMovementMode)> Step)
		{
			using Padel::EMovementMode;
			const FVector Ahead = FRotator(0.0, StartYaw, 0.0).Vector();
			const FVector Right = FRotator(0.0, StartYaw + 90.0, 0.0).Vector();
			const double Walk = 150.0;
			const double RunSpeed = 450.0;
			auto Speed = [&S]() { return S.Velocity.Size2D(); };
			auto Hold = [&](const TCHAR* Phase, int32 Frames, double Want, const FVector& Dir, double Accel, EMovementMode Mode)
			{
				for (int32 I = 0; I < Frames; ++I)
				{
					Accelerate(S, Dir * Want, Accel);
					if (Want > 0.0)
					{
						Face(S, YawOf(Dir));
					}
					Step(Phase, Mode);
				}
			};
			switch (Path)
			{
			case EPath::WalkSteady:
				// Already walking (a moment to get there), then four steady seconds.
				Hold(TEXT("start"), 30, Walk, Ahead, 500.0, EMovementMode::Run);
				Hold(TEXT("walk"), 240, Walk, Ahead, 500.0, EMovementMode::Run);
				break;
			case EPath::RunSteady:
				Hold(TEXT("start"), 60, RunSpeed, Ahead, 800.0, EMovementMode::Run);
				Hold(TEXT("run"), 180, RunSpeed, Ahead, 800.0, EMovementMode::Run);
				break;
			case EPath::WalkStart:
				Hold(TEXT("stand"), 60, 0.0, Ahead, 500.0, EMovementMode::Adjust);
				Hold(TEXT("start"), 150, Walk, Ahead, 500.0, EMovementMode::Run);
				break;
			case EPath::WalkStop:
				Hold(TEXT("walk"), 120, Walk, Ahead, 500.0, EMovementMode::Run);
				while (Speed() > 1.0) { Accelerate(S, FVector::ZeroVector, 500.0); Step(TEXT("stop"), EMovementMode::Run); }
				S.Velocity = FVector::ZeroVector;
				Hold(TEXT("stand"), 90, 0.0, Ahead, 500.0, EMovementMode::Adjust);
				break;
			case EPath::WalkTurn90:
				Hold(TEXT("walk"), 90, Walk, Ahead, 500.0, EMovementMode::Run);
				Hold(TEXT("turn"), 150, Walk, Right, 500.0, EMovementMode::Run);
				break;
			default:
				break;
			}
		}

		/** What one body recorded each frame. */
		struct FTrack
		{
			USkeletalMeshComponent* Mesh = nullptr;
			TArray<FVector> FootPath[2];
			TArray<TArray<FQuat>> CoreRotations; // component space, for the per-frame turn
			TArray<TArray<FQuat>> ArmRotations;
			TMap<FString, TArray<FQuat>> World;  // world space, for jerk
			TArray<int32> Core;
			TArray<FString> CoreNames;
			TArray<int32> Arm;
			TArray<FString> ArmNames;
		};

		const TCHAR* const JerkBones[] = { TEXT("pelvis"), TEXT("spine_03"), TEXT("head") };

		void InitTrack(FTrack& Track, USkeletalMeshComponent* Mesh)
		{
			Track.Mesh = Mesh;
			for (const TCHAR* Name : { TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"),
				TEXT("neck_01"), TEXT("head"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"),
				TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("upperarm_l"), TEXT("upperarm_r") })
			{
				const int32 Index = Mesh->GetBoneIndex(FName(Name));
				if (Index != INDEX_NONE)
				{
					Track.Core.Add(Index);
					Track.CoreNames.Add(Name);
				}
			}
			for (const TCHAR* Name : { TEXT("clavicle_l"), TEXT("clavicle_r"), TEXT("upperarm_l"), TEXT("upperarm_r"),
				TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("hand_l"), TEXT("hand_r") })
			{
				const int32 Index = Mesh->GetBoneIndex(FName(Name));
				if (Index != INDEX_NONE)
				{
					Track.Arm.Add(Index);
					Track.ArmNames.Add(Name);
				}
			}
		}

		void Record(FTrack& Track)
		{
			USkeletalMeshComponent* Mesh = Track.Mesh;
			const FName Feet[2] = { FName(TEXT("foot_l")), FName(TEXT("foot_r")) };
			const FName Balls[2] = { FName(TEXT("ball_l")), FName(TEXT("ball_r")) };
			for (int32 F = 0; F < 2; ++F)
			{
				// The contact point: the lower of the ankle and the ball (a foot rolls heel to toe).
				const FVector Ankle = Mesh->GetSocketLocation(Feet[F]);
				// A MetaHuman in padel shoes hides its toes (scaled away): only the ankle counts there.
				const bool bBall = Mesh->GetBoneIndex(Balls[F]) != INDEX_NONE && !Mesh->IsBoneHiddenByName(Balls[F]);
				const FVector Ball = bBall ? Mesh->GetSocketLocation(Balls[F]) : Ankle;
				Track.FootPath[F].Add(Ball.Z < Ankle.Z ? Ball : Ankle);
			}
			const TArray<FTransform>& CS = Mesh->GetComponentSpaceTransforms();
			for (TPair<TArray<TArray<FQuat>>*, const TArray<int32>*> Set : { MakeTuple(&Track.CoreRotations, &Track.Core), MakeTuple(&Track.ArmRotations, &Track.Arm) })
			{
				TArray<FQuat>& Frame = Set.Key->AddDefaulted_GetRef();
				for (const int32 B : *Set.Value)
				{
					Frame.Add(CS.IsValidIndex(B) ? CS[B].GetRotation() : FQuat::Identity);
				}
			}
			for (const TCHAR* Name : JerkBones)
			{
				if (Mesh->GetBoneIndex(FName(Name)) != INDEX_NONE)
				{
					Track.World.FindOrAdd(Name).Add(Mesh->GetBoneQuaternion(FName(Name), EBoneSpaces::WorldSpace));
				}
			}
		}

		double Percentile(TArray<double> Values, double P)
		{
			if (Values.IsEmpty())
			{
				return 0.0;
			}
			Values.Sort();
			return Values[FMath::Clamp(FMath::FloorToInt(P * (Values.Num() - 1)), 0, Values.Num() - 1)];
		}

		FBodyMetrics Measure(const FTrack& Track, const TArray<FString>& Phases, const FString& Name)
		{
			FBodyMetrics M;
			M.Name = Name;
			if (!Track.Mesh)
			{
				return M;
			}
			M.bValid = true;
			M.CoreBones = Track.Core.Num();

			// Planted feet: within 3 cm of the ground (its 10th percentile height) for at least 0.05 s;
			// how far it travels over that spell, for its length, is its slide.
			double SlideCm = 0.0;
			TMap<FString, FVector2D> ByPhase;
			for (int32 F = 0; F < 2; ++F)
			{
				const TArray<FVector>& Path = Track.FootPath[F];
				TArray<double> Heights;
				for (const FVector& P : Path) { Heights.Add(P.Z); }
				M.FootHeights += FString::Printf(TEXT("%s%.1f..%.1f"), F == 0 ? TEXT("") : TEXT(" / "), Percentile(Heights, 0.1), Percentile(Heights, 0.9));
				for (const FIntPoint& Planted : PlantedSpells(Path, Dt))
				{
					const double Spell = (Planted.Y - Planted.X) * Dt;
					const double Moved = FVector::Dist2D(Path[Planted.Y], Path[Planted.X]);
					SlideCm += Moved;
					M.PlantedS += Spell;
					M.WorstSlideSpellCmps = FMath::Max(M.WorstSlideSpellCmps, Moved / Spell);
					ByPhase.FindOrAdd(Phases[Planted.X]) += FVector2D(Moved, Spell);
				}
			}
			M.SlideCmps = M.PlantedS > 0.0 ? SlideCm / M.PlantedS : 0.0;
			FVector2D Steady = FVector2D::ZeroVector;
			for (const TCHAR* Phase : { TEXT("stand"), TEXT("run"), TEXT("run2"), TEXT("run back"), TEXT("settle"), TEXT("settle2"), TEXT("shuffle"), TEXT("end"), TEXT("walk"), TEXT("jog") })
			{
				if (const FVector2D* Found = ByPhase.Find(Phase)) { Steady += *Found; }
			}
			M.SteadySlideCmps = Steady.Y > 0.0 ? Steady.X / Steady.Y : 0.0;
			TArray<FString> Parts;
			for (const TPair<FString, FVector2D>& Entry : ByPhase)
			{
				Parts.Add(FString::Printf(TEXT("%s %.0f"), *Entry.Key, Entry.Value.X / FMath::Max(Entry.Value.Y, 1e-3)));
			}
			M.SlideByPhase = FString::Join(Parts, TEXT(", "));

			// The biggest turn of a body (and an arm) bone between two frames, component space.
			auto Worst = [&Phases](const TArray<TArray<FQuat>>& Rotations, const TArray<FString>& Names, double& OutDeg, FString& OutAt, int32* OutFrame)
			{
				for (int32 I = WarmUpFrames + 1; I < Rotations.Num(); ++I)
				{
					for (int32 B = 0; B < Names.Num(); ++B)
					{
						const double Deg = FMath::RadiansToDegrees(Rotations[I][B].AngularDistance(Rotations[I - 1][B]));
						if (Deg > OutDeg)
						{
							OutDeg = Deg;
							OutAt = Names[B] + TEXT(", ") + Phases[I];
							if (OutFrame)
							{
								*OutFrame = I;
							}
						}
					}
				}
			};
			Worst(Track.CoreRotations, Track.CoreNames, M.WorstBoneTurnDeg, M.WorstBoneTurnAt, &M.WorstBoneTurnFrame);
			Worst(Track.ArmRotations, Track.ArmNames, M.WorstArmTurnDeg, M.WorstArmTurnAt, nullptr);

			// Angular jerk, world space: the angular velocity between frames, differenced twice.
			for (const TPair<FString, TArray<FQuat>>& Bone : Track.World)
			{
				const TArray<FQuat>& Q = Bone.Value;
				TArray<FVector> Omega;
				for (int32 I = WarmUpFrames + 1; I < Q.Num(); ++I)
				{
					FQuat Delta = Q[I] * Q[I - 1].Inverse();
					Delta.EnforceShortestArcWith(FQuat::Identity);
					FVector Axis;
					double Angle;
					Delta.ToAxisAndAngle(Axis, Angle);
					Omega.Add(Axis * FMath::RadiansToDegrees(Angle) / Dt);
				}
				TArray<FVector> Alpha;
				for (int32 I = 1; I < Omega.Num(); ++I) { Alpha.Add((Omega[I] - Omega[I - 1]) / Dt); }
				TArray<double> Jerk;
				double AlphaSq = 0.0;
				for (const FVector& A : Alpha) { AlphaSq += A.SizeSquared(); }
				double JerkSq = 0.0;
				for (int32 I = 1; I < Alpha.Num(); ++I)
				{
					const double J = ((Alpha[I] - Alpha[I - 1]) / Dt).Size();
					Jerk.Add(J);
					JerkSq += J * J;
				}
				M.AccelDegPerS2.Add(Bone.Key, Alpha.IsEmpty() ? 0.0 : FMath::Sqrt(AlphaSq / Alpha.Num()));
				M.JerkDegPerS3.Add(Bone.Key, FVector2D(Jerk.IsEmpty() ? 0.0 : FMath::Sqrt(JerkSq / Jerk.Num()), Percentile(Jerk, 0.95)));
			}
			return M;
		}

		/** The angular velocities (deg/s) of a world rotation track, from the frame after warm-up. */
		TArray<FVector> AngularVelocities(const TArray<FQuat>& Q)
		{
			TArray<FVector> Omega;
			for (int32 I = WarmUpFrames + 1; I < Q.Num(); ++I)
			{
				FQuat Delta = Q[I] * Q[I - 1].Inverse();
				Delta.EnforceShortestArcWith(FQuat::Identity);
				Omega.Add(FMath::RadiansToDegrees(1.0) * Delta.ToRotationVector() / Dt);
			}
			return Omega;
		}

		TMap<FString, FVector> Follow(const FTrack& Driver, const FTrack& Shown)
		{
			TMap<FString, FVector> Out;
			for (const TCHAR* Bone : JerkBones)
			{
				const TArray<FQuat>* A = Driver.World.Find(Bone);
				const TArray<FQuat>* B = Shown.World.Find(Bone);
				if (!A || !B || A->Num() != B->Num() || A->Num() <= WarmUpFrames + 4)
				{
					continue;
				}
				// The MetaHuman's bone in the placeholder's bone frame is constant if it follows exactly.
				const FQuat Rest = (*A)[WarmUpFrames].Inverse() * (*B)[WarmUpFrames];
				double Sq = 0.0;
				double Worst = 0.0;
				int32 N = 0;
				for (int32 I = WarmUpFrames; I < A->Num(); ++I)
				{
					const FQuat Rel = (*A)[I].Inverse() * (*B)[I];
					const double Deg = FMath::RadiansToDegrees(Rel.AngularDistance(Rest));
					Sq += Deg * Deg;
					Worst = FMath::Max(Worst, Deg);
					++N;
				}
				const TArray<FVector> WA = AngularVelocities(*A);
				const TArray<FVector> WB = AngularVelocities(*B);
				int32 BestLag = 0;
				double BestErr = TNumericLimits<double>::Max();
				for (int32 Lag = 0; Lag <= 3; ++Lag)
				{
					double Err = 0.0;
					for (int32 I = 3; I + Lag < WB.Num(); ++I)
					{
						Err += (WB[I + Lag] - WA[I]).SizeSquared();
					}
					if (Err < BestErr)
					{
						BestErr = Err;
						BestLag = Lag;
					}
				}
				Out.Add(Bone, FVector(FMath::Sqrt(Sq / FMath::Max(N, 1)), Worst, BestLag));
			}
			return Out;
		}

		USkeletalMeshComponent* ShownMetaHuman(APadelPlayerPawn* Pawn)
		{
			AActor* MetaHuman = Pawn->GetAnimation()->GetMetaHuman();
			if (!MetaHuman)
			{
				return nullptr;
			}
			TInlineComponentArray<USkeletalMeshComponent*> Parts(MetaHuman);
			for (USkeletalMeshComponent* Part : Parts)
			{
				if (Part->GetAnimInstance() && Part->GetAnimInstance()->GetClass()->GetName() == TEXT("PadelMetaHumanAnimInstance"))
				{
					return Part;
				}
			}
			return nullptr;
		}
	}

	TArray<FIntPoint> PlantedSpells(const TArray<FVector>& Path, double Dt)
	{
		// Planted: within 3 cm of the ground (the path's 10th percentile height) for at least 0.05 s.
		TArray<double> Heights;
		for (const FVector& P : Path) { Heights.Add(P.Z); }
		const double Floor = Detail::Percentile(Heights, 0.1) + 3.0;
		TArray<FIntPoint> Spells;
		int32 From = INDEX_NONE;
		for (int32 I = 0; I <= Path.Num(); ++I)
		{
			const bool bDown = I < Path.Num() && Path[I].Z < Floor;
			if (bDown && From == INDEX_NONE) { From = I; }
			if (!bDown && From != INDEX_NONE)
			{
				if ((I - 1 - From) * Dt >= 0.05) { Spells.Add(FIntPoint(From, I - 1)); }
				From = INDEX_NONE;
			}
		}
		return Spells;
	}

	double PlantedSlideCmps(const TArray<FVector> (&FootPaths)[2], double Dt, double* OutPlantedS)
	{
		double Moved = 0.0;
		double Planted = 0.0;
		for (const TArray<FVector>& Path : FootPaths)
		{
			for (const FIntPoint& Spell : PlantedSpells(Path, Dt))
			{
				Moved += FVector::Dist2D(Path[Spell.Y], Path[Spell.X]);
				Planted += (Spell.Y - Spell.X) * Dt;
			}
		}
		if (OutPlantedS) { *OutPlantedS = Planted; }
		return Planted > 0.0 ? Moved / Planted : 0.0;
	}

	FRun Run(FAutomationTestBase& Test, EDrive Drive)
	{
		return Run(Test, Drive, FRunOptions());
	}

	FRun Run(FAutomationTestBase& Test, EDrive Drive, const FRunOptions& Options)
	{
		using namespace Detail;
		FRun Out;
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
		if (!Director || !Director->StartMatch(Setup))
		{
			Out.SkipReason = TEXT("the match didn't start");
			Test.AddError(Out.SkipReason);
			return Out;
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
			Out.SkipReason = TEXT("no motion matching databases on this machine (run animation_setup.py)");
			return Out;
		}
#if WITH_EDITOR
		for (const UPoseSearchDatabase* Database : Anim->LocomotionSet.MotionMatchingDatabases)
		{
			using namespace UE::PoseSearch;
			const EAsyncBuildIndexResult Result = FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database,
				ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
			if (Result != EAsyncBuildIndexResult::Success)
			{
				Out.SkipReason = FString::Printf(TEXT("%s has no search index"), *Database->GetName());
				Test.AddError(Out.SkipReason);
				return Out;
			}
		}
#endif
		const bool bFull = Drive == EDrive::FullStack;
		const FPadelAnimData Data = FPadelAnimData::Load();
		TUniquePtr<Padel::Animation::FPlayerAnimModel> Model;
		if (bFull)
		{
			if (!Data.bOk)
			{
				Out.SkipReason = TEXT("Data/Animation.json didn't load");
				Test.AddError(Out.SkipReason);
				return Out;
			}
			Model = MakeUnique<Padel::Animation::FPlayerAnimModel>(&Data.Settings, &Data.Clips, nullptr);
		}
		USkeletalMeshComponent* Shown = ShownMetaHuman(Pawn);

		Anim->RawSwingWeight = 0.0f;
		Anim->ContactWarpWeight = 0.0f;
		Anim->ArmCarryAlpha = 0.0f;
		Anim->Body = FPadelBodyDynamics();
		Anim->Stance = FPadelBodyDynamics();
		if (Options.bWarpsOff)
		{
			// Zeroed tuning turns each part off (FPadelLocomotionWarpTuning); the node copies the set every update.
			Anim->LocomotionSet.LocomotionWarp = FPadelLocomotionWarpTuning();
		}
		const bool bRootMotion = Options.bRootMotion && !bFull;

		FTrack Tracks[2];
		InitTrack(Tracks[0], Driver);
		if (Shown)
		{
			InitTrack(Tracks[1], Shown);
		}

		const double Ground = Pawn->GetActorLocation().Z;
		FScript S;
		S.Position = FVector(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Ground);
		S.FacingYawDeg = Pawn->GetActorRotation().Yaw;
		const bool bLayerVideo = Options.Path != EPath::Classic;
		if (bLayerVideo)
		{
			// Start near the back of the player's half so the 8 m forward leg stops short of the net,
			// offset sideways so the walk, jog and shuffles stay centred.
			const FVector SideDir = FRotator(0.0, S.FacingYawDeg + 90.0, 0.0).Vector();
			const double Sign = S.Position.X >= 0.0 ? 1.0 : -1.0;
			S.Position = FVector(Sign * 920.0, 0.0, Ground) - SideDir * 80.0;
		}
		if (Options.OnSetup)
		{
			FSetup Recorder;
			Recorder.World = World;
			Recorder.Pawn = Pawn;
			Recorder.Driver = Driver;
			Recorder.Shown = Shown;
			Recorder.StartYawDeg = S.FacingYawDeg;
			FScript Dry = S;
			if (bLayerVideo)
			{
				auto DryStep = [&Dry, &Recorder](const TCHAR*, Padel::EMovementMode)
				{
					Dry.Position += Dry.Velocity * Dt;
					Recorder.Path.Add(Dry.Position);
				};
				if (Options.Path == EPath::LayerVideo)
				{
					LayerVideoPath(Dry, Dry.FacingYawDeg, DryStep);
				}
				else
				{
					BasePath(Options.Path, Dry, Dry.FacingYawDeg, DryStep);
				}
			}
			else
			{
				Recorder.Path.Add(Dry.Position);
			}
			Options.OnSetup(Recorder);
		}
		// Root motion (FRunOptions::bRootMotion): the pawn's own place and facing, and the clip it last read.
		FVector RootPosition = S.Position;
		double RootYaw = S.FacingYawDeg;
		const UAnimSequenceBase* RootClip = nullptr;
		double RootClipTime = 0.0;
		const FVector Ahead = FRotator(0.0, S.FacingYawDeg, 0.0).Vector();
		const FVector Side = FRotator(0.0, S.FacingYawDeg + 90.0, 0.0).Vector();
		const double RunSpeed = 400.0;
		const double StartYaw = S.FacingYawDeg;
		const double TeamSign = S.Position.X >= 0.0 ? 1.0 : -1.0;
		// A ball to watch on the other side of the net, as in a rally.
		const Padel::Court::FVec3 BallPosition{ -TeamSign * 6.0, 1.0, 1.8 };
		Padel::EMovementMode Mode = Padel::EMovementMode::Adjust;

		TArray<FVector> Past;
		int64 PlayingSum = 0;
		int32 LockedFrames[2] = { 0, 0 };
		double StalledSpellS = 0.0;
		auto Tick = [&](const FString& Phase)
		{
			S.Position += S.Velocity * Dt;
			World->TimeSeconds += Dt;
			TArray<FVector> Trajectory;
			Padel::Animation::FPlayerAnimFrame Frame;
			if (bFull)
			{
				Padel::Animation::FPlayerAnimInput In;
				In.Position = Padel::ToMetres(S.Position);
				In.Position.Z = 0.0;
				In.Velocity = Padel::ToMetres(S.Velocity);
				In.TeamSign = TeamSign;
				In.ForehandSign = 1.0;
				In.Mode = Mode;
				In.bRallyLive = true;
				In.bBallVisible = true;
				In.BallPosition = BallPosition;
				Frame = Model->Step(In, Dt);
				Pawn->SetActorLocationAndRotation(S.Position, FRotator(0.0, Frame.FacingYawDeg, 0.0));
				for (int32 Index = 0; Index < Frame.TrajectoryCount; ++Index)
				{
					FVector Point = Padel::ToWorld(Frame.Trajectory[Index]);
					Point.Z = Ground;
					Trajectory.Add(Point);
				}
			}
			else
			{
				const FTransform SimPose(FRotator(0.0, S.FacingYawDeg, 0.0), S.Position);
				const FTransform PawnPose = bRootMotion ? FTransform(FRotator(0.0, RootYaw, 0.0), RootPosition) : SimPose;
				// Root motion: the path as the pawn would see it from where its clips took it.
				const FTransform SimToPawn = SimPose.Inverse() * PawnPose;
				Pawn->SetActorLocationAndRotation(PawnPose.GetLocation(), PawnPose.Rotator());
				// The path's own trajectory: the past as it happened, the future carried on at this velocity.
				Past.Add(S.Position);
				for (const float T : Anim->TrajectoryTimes)
				{
					const int32 Back = FMath::Min(Past.Num() - 1, FMath::RoundToInt(-T / Dt));
					const FVector Point = T < 0.0f ? Past[Past.Num() - 1 - Back] : S.Position + S.Velocity * T;
					Trajectory.Add(bRootMotion ? SimToPawn.TransformPosition(Point) : Point);
				}
				Frame.SpeedMps = S.Velocity.Size2D() / 100.0;
				Frame.Locomotion = Drive == EDrive::ReadyStance ? Padel::Animation::ELocomotionState::Ready
					: Frame.SpeedMps < 0.3 ? Padel::Animation::ELocomotionState::Idle : Padel::Animation::ELocomotionState::Run;
				const double TravelYaw = FMath::RadiansToDegrees(FMath::Atan2(S.Velocity.Y, S.Velocity.X));
				Frame.DirectionDeg = FMath::FindDeltaAngleDegrees(S.FacingYawDeg, TravelYaw);
			}
			Anim->ApplyFrame(Frame, nullptr, nullptr, nullptr, Trajectory);
			if (!bFull)
			{
				Anim->Body = FPadelBodyDynamics();
				Anim->Stance = FPadelBodyDynamics();
			}
			Driver->TickAnimation(static_cast<float>(Dt), false);
			if (bRootMotion)
			{
				// Move the pawn by what the playing clip's root did over this update (component space,
				// the mannequin's root bone), so the clip carries the body instead of the path.
				const FPadelMotionMatcher& Picked = Anim->GetPoseNode().GetMotionMatcher();
				const UAnimSequenceBase* Clip = Picked.CurrentClip();
				FTransform Delta = FTransform::Identity;
				if (Clip)
				{
					const double Now = Picked.CurrentTime();
					const FAnimExtractContext Extract;
					const double Step = Picked.CurrentPlayRate() * Dt;
					// A new pick, or the same clip picked again at another time (a jump, or a clip that ran out).
					const bool bJumped = Clip == RootClip && (Now - RootClipTime > 2.0 * Step + 1e-3
						|| (Now < RootClipTime && !(Picked.CurrentLoops() && RootClipTime + 1.5 * Step >= Clip->GetPlayLength())));
					if (Clip != RootClip || bJumped)
					{
						Delta = Clip->ExtractRootMotionFromRange(FMath::Max(0.0, Now - Step), Now, Extract);
					}
					else if (Now >= RootClipTime)
					{
						Delta = Clip->ExtractRootMotionFromRange(RootClipTime, Now, Extract);
					}
					else if (Picked.CurrentLoops())
					{
						const FTransform ToEnd = Clip->ExtractRootMotionFromRange(RootClipTime, Clip->GetPlayLength(), Extract);
						const FTransform FromStart = Clip->ExtractRootMotionFromRange(0.0, Now, Extract);
						Delta = FromStart * ToEnd;
					}
					RootClip = Clip;
					RootClipTime = Now;
				}
				FVector Move = Driver->GetComponentQuat().RotateVector(Delta.GetTranslation());
				Move.Z = 0.0;
				RootPosition += Move;
				RootYaw += Delta.GetRotation().Rotator().Yaw;
				Pawn->SetActorLocationAndRotation(RootPosition, FRotator(0.0, RootYaw, 0.0));
			}
			Driver->RefreshBoneTransforms();
			if (Shown)
			{
				// The MetaHuman ticks only when rendered: evaluate it here as the retarget would on screen.
				Shown->TickAnimation(static_cast<float>(Dt), false);
				Shown->RefreshBoneTransforms();
			}
			const FPadelMotionMatcher& Matcher = Anim->GetPoseNode().GetMotionMatcher();
			const UAnimSequenceBase* Playing = Matcher.CurrentClip();
			const bool bRanOut = Playing && !Matcher.CurrentLoops() && Matcher.CurrentTime() >= Playing->GetPlayLength() - 1e-3;
			const double PathSpeed = S.Velocity.Size2D();
			const bool bStalled = Playing && PathSpeed > 50.0 && (bRanOut || Matcher.ClipRootVelocity().Size2D() < PathSpeed / 3.0);
			if (bStalled)
			{
				Out.StalledMovingS += Dt;
				StalledSpellS += Dt;
				if (StalledSpellS > Out.WorstStalledSpellS)
				{
					Out.WorstStalledSpellS = StalledSpellS;
					Out.WorstStalledAt = Playing->GetName() + TEXT(", ") + Phase;
				}
			}
			else
			{
				StalledSpellS = 0.0;
			}
			Out.ClipPerFrame.Add(Playing ? FString::Printf(TEXT("%s %.2f/%.2f x%.2f, clip %.2f m/s, path %.2f m/s"), *Playing->GetName(), Matcher.CurrentTime(),
				Playing->GetPlayLength(), Matcher.CurrentPlayRate(), Matcher.ClipRootVelocity().Size2D() / 100.0, PathSpeed / 100.0) : FString());
			Out.Phases.Add(Phase);
			Out.PickPerFrame.Add(Matcher.CurrentDatabase());
			Out.MaxPlaying = FMath::Max(Out.MaxPlaying, Matcher.NumPlaying());
			PlayingSum += Matcher.NumPlaying();
			for (int32 F = 0; F < 2; ++F)
			{
				LockedFrames[F] += Anim->GetPoseNode().GetLocomotionWarp().IsLocked(F) ? 1 : 0;
			}
			Record(Tracks[0]);
			if (Shown)
			{
				Record(Tracks[1]);
			}
			if (Options.OnFrame)
			{
				Options.OnFrame(Out.Phases.Num() - 1, Phase);
			}
		};

		using Padel::EMovementMode;
		if (bLayerVideo)
		{
			auto Step = [&Mode, &Tick](const TCHAR* Phase, EMovementMode InMode)
			{
				Mode = InMode;
				Tick(Phase);
			};
			if (Options.Path == EPath::LayerVideo)
			{
				LayerVideoPath(S, StartYaw, Step);
			}
			else
			{
				BasePath(Options.Path, S, StartYaw, Step);
			}
		}
		else
		{
			Mode = EMovementMode::Adjust;
			for (int32 I = 0; I < 60; ++I) { Tick(TEXT("stand")); }
			Mode = EMovementMode::Run;
			while (S.Velocity.Size2D() < RunSpeed - 1.0) { Accelerate(S, Ahead * RunSpeed, 800.0); Tick(TEXT("start")); }
			const FVector RunFrom = S.Position;
			while (FVector::Dist2D(S.Position, RunFrom) < 300.0) { Tick(TEXT("run")); }
			while (S.Velocity.Size2D() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Tick(TEXT("stop")); }
			S.Velocity = FVector::ZeroVector;
			Mode = EMovementMode::Adjust;
			for (int32 I = 0; I < 60; ++I) { Tick(TEXT("settle")); }
			// A 180° reversal out of a run: accelerate ahead, then turn back through zero.
			Mode = EMovementMode::Run;
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
			Mode = EMovementMode::Adjust;
			for (int32 I = 0; I < 30; ++I) { Tick(TEXT("settle2")); }
			// A side shuffle, square to the net.
			Mode = EMovementMode::Shuffle;
			for (int32 I = 0; I < 75; ++I) { Accelerate(S, Side * 200.0, 800.0); Tick(TEXT("shuffle")); }
			while (S.Velocity.Size2D() > 1.0) { Accelerate(S, FVector::ZeroVector, 1000.0); Tick(TEXT("stop3")); }
			Mode = EMovementMode::Adjust;
			for (int32 I = 0; I < 45; ++I) { Tick(TEXT("end")); }
		}

		const FPadelMotionMatcher& Matcher = Anim->GetPoseNode().GetMotionMatcher();
		Out.bRan = true;
		Out.bMotionMatched = Anim->GetPoseNode().IsMotionMatching();
		Out.DurationS = Out.Phases.Num() * Dt;
		Out.SwitchesPerS = Out.DurationS > 0.0 ? Matcher.NumPicks() / Out.DurationS : 0.0;
		Out.StrandedS = Matcher.StrandedS();
		Out.ReadySearches = Matcher.NumReadySearches();
		Out.ReadyAccepted = Matcher.NumReadyAccepted();
		Out.MeanReadyCost = Matcher.MeanReadyBestCost();
		Out.LowestReadyCost = Matcher.LowestReadyBestCost();
		for (int32 F = 0; F < 2; ++F)
		{
			Out.LockedShare[F] = Out.Phases.IsEmpty() ? 0.0 : static_cast<double>(LockedFrames[F]) / Out.Phases.Num();
		}
		Out.MeanPlaying = Out.Phases.IsEmpty() ? 0.0 : static_cast<double>(PlayingSum) / Out.Phases.Num();
		Out.Placeholder = Measure(Tracks[0], Out.Phases, TEXT("placeholder"));
		Out.MetaHuman = Measure(Tracks[1], Out.Phases, TEXT("MetaHuman"));
		if (Shown)
		{
			Out.FollowRmsMaxLag = Follow(Tracks[0], Tracks[1]);
		}
		TArray<FString> Seen;
		for (int32 I = 0; I < Out.PickPerFrame.Num(); ++I)
		{
			const FString Pick = Out.PickPerFrame[I].ToString() + TEXT(" (") + Out.Phases[I] + TEXT(")");
			if (Seen.IsEmpty() || Seen.Last() != Pick) { Seen.Add(Pick); }
		}
		Out.Picks = FString::Join(Seen, TEXT(", "));
		return Out;
	}

	void Report(FAutomationTestBase& Test, const FString& Label, const FRun& Run)
	{
		if (!Run.bRan)
		{
			return;
		}
		Test.AddInfo(FString::Printf(TEXT("[%s] %.2f clip switches/s over %.1f s; poses playing at once: max %d, mean %.2f; feet locked %.0f%% / %.0f%%; footwork-first searches %d, footwork played in %d (best cost mean %.3f, lowest %.3f); a clip stranded %.2f s"),
			*Label, Run.SwitchesPerS, Run.DurationS, Run.MaxPlaying, Run.MeanPlaying, Run.LockedShare[0] * 100.0, Run.LockedShare[1] * 100.0,
			Run.ReadySearches, Run.ReadyAccepted, Run.MeanReadyCost, Run.LowestReadyCost, Run.StrandedS));
		for (const FBodyMetrics* Body : { &Run.Placeholder, &Run.MetaHuman })
		{
			if (!Body->bValid)
			{
				continue;
			}
			TArray<FString> Jerk;
			for (const TCHAR* Bone : Detail::JerkBones)
			{
				if (const FVector2D* Found = Body->JerkDegPerS3.Find(Bone))
				{
					Jerk.Add(FString::Printf(TEXT("%s rms %.0fk p95 %.0fk (accel rms %.0f)"), Bone, Found->X / 1000.0, Found->Y / 1000.0, Body->AccelDegPerS2.FindRef(Bone)));
				}
			}
			Test.AddInfo(FString::Printf(TEXT("[%s %s] angular jerk deg/s^3: %s"), *Label, *Body->Name, *FString::Join(Jerk, TEXT("; "))));
			Test.AddInfo(FString::Printf(TEXT("[%s %s] foot slide %.1f cm/s over %.1f s planted (steady %.1f, worst spell %.0f); by phase: %s; foot heights %s"),
				*Label, *Body->Name, Body->SlideCmps, Body->PlantedS, Body->SteadySlideCmps, Body->WorstSlideSpellCmps, *Body->SlideByPhase, *Body->FootHeights));
			Test.AddInfo(FString::Printf(TEXT("[%s %s] largest bone turn in a frame %.1f deg (%s); arms %.1f deg (%s)"), *Label, *Body->Name,
				Body->WorstBoneTurnDeg, *Body->WorstBoneTurnAt, Body->WorstArmTurnDeg, *Body->WorstArmTurnAt));
		}
		if (!Run.FollowRmsMaxLag.IsEmpty())
		{
			TArray<FString> Parts;
			for (const TPair<FString, FVector>& Bone : Run.FollowRmsMaxLag)
			{
				Parts.Add(FString::Printf(TEXT("%s rms %.1f max %.1f lag %.0f"), *Bone.Key, Bone.Value.X, Bone.Value.Y, Bone.Value.Z));
			}
			Test.AddInfo(FString::Printf(TEXT("[%s] MetaHuman follow (deg, frames): %s"), *Label, *FString::Join(Parts, TEXT("; "))));
		}
		Test.AddInfo(FString::Printf(TEXT("[%s] moving on a clip that barely moves: %.2f s, longest %.2f s (%s)"), *Label, Run.StalledMovingS, Run.WorstStalledSpellS, *Run.WorstStalledAt));
		Test.AddInfo(FString::Printf(TEXT("[%s] picks: %s"), *Label, *Run.Picks));
	}
}

#endif
