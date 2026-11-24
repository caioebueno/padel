// Phase 1 base check: do the clips themselves look human before any of our layers? Plays five
// Game Animation Sample clips straight (play rate 1, no search, no warps, the body moved by the
// clip's own root motion) on three bodies: the UE5 mannequin the clips were imported on (no
// retarget), our placeholder skeleton after the RTG retarget, and the MetaHuman retargeted from
// the placeholder as in a match. Then the same five moves through the motion matcher in the
// layer videos' A setup (root motion, no warps), to show what the search and play rate add.
// Measures planted-foot slide per body and compares the skeletons and clips (proportions,
// reference pose, foot height, knee bend, hip width, frame rate).
//
// Diagnosis only. Records only with -PadelBaseClips=<folder> and a renderer
// (Tools/RecordBaseClips.sh); otherwise it reports that it skipped.
#include "Misc/AutomationTest.h"

#include "PadelMotionProbe.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/PadelAnimData.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Animation/PadelPlayerAnimInstance.h"
#include "Animation/PadelPlayerPoseNode.h"
#include "AssetCompilingManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Animation/SkeletalMeshActor.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Presentation/PadelBall.h"
#include "Presentation/PadelVenueLighting.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UObject/SoftObjectPath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelBaseClips
{
	constexpr double Dt = 1.0 / 60.0;
	const TCHAR* const SourceDir = TEXT("/Game/Characters/GASP");
	const TCHAR* const RetargetedDir = TEXT("/Game/Padel/Animations/MotionMatching/Clips");
	const TCHAR* const MannyMesh = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");

	struct FClip
	{
		const TCHAR* Name;
		const TCHAR* Label;
		PadelMotionProbe::EPath Move;
	};
	const FClip Clips[] = {
		{ TEXT("M_Neutral_Walk_Loop_F"), TEXT("walk loop"), PadelMotionProbe::EPath::WalkSteady },
		{ TEXT("M_Neutral_Run_Loop_F"), TEXT("run loop"), PadelMotionProbe::EPath::RunSteady },
		{ TEXT("M_Neutral_Walk_Start_F_Lfoot"), TEXT("walk start"), PadelMotionProbe::EPath::WalkStart },
		{ TEXT("M_Neutral_Walk_Stop_F_Lfoot"), TEXT("walk stop"), PadelMotionProbe::EPath::WalkStop },
		{ TEXT("M_Neutral_Walk_Turn_R_090_Lfoot"), TEXT("walk 90 turn"), PadelMotionProbe::EPath::WalkTurn90 },
	};

	enum class EBody : uint8 { Manny, Placeholder, MetaHuman };
	const TCHAR* BodyName(EBody B) { return B == EBody::Manny ? TEXT("mannequin") : B == EBody::Placeholder ? TEXT("placeholder") : TEXT("MetaHuman"); }

	/** Daylight, a grey floor with lines every 50 cm, and one camera that follows a body from a fixed three-quarter angle. */
	struct FStage
	{
		FString Dir;
		int32 Width = 1280;
		int32 Height = 720;
		double FovDeg = 40.0;
		bool bOk = false;
		TObjectPtr<UTextureRenderTarget2D> Target;
		USceneCaptureComponent2D* Capture = nullptr;
		ASceneCapture2D* Camera = nullptr;
		TArray<FColor> Pixels;
		FVector Out = FVector(0.0, -1.0, 0.0);
		double FloorZ = 0.0;

		~FStage()
		{
			if (Target)
			{
				Target->RemoveFromRoot();
			}
		}

		void Build(UWorld* World, double InFloorZ, const FVector& Around, double Facing)
		{
			FloorZ = InFloorZ;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			FVector SunTravel(0.0, 0.0, -1.0);
			if (APadelVenueLighting* Lighting = World->SpawnActor<APadelVenueLighting>(APadelVenueLighting::StaticClass(), FTransform::Identity, Params))
			{
				Lighting->ApplyPreset(TEXT("OutdoorDay"));
				for (UStaticMeshComponent* Head : Lighting->GetFixtureHeads())
				{
					if (Head)
					{
						Head->SetHiddenInGame(true);
						Head->SetCastShadow(false);
					}
				}
				if (Lighting->GetSun())
				{
					SunTravel = Lighting->GetSun()->GetComponentRotation().Vector();
				}
			}
			UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
			UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
			UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			auto Slab = [&](UStaticMesh* Mesh, const FVector& Where, const FVector& Scale, const FLinearColor& Colour)
			{
				AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Where), Params);
				if (!Actor)
				{
					return;
				}
				UStaticMeshComponent* Part = Actor->GetStaticMeshComponent();
				Part->SetMobility(EComponentMobility::Movable);
				Part->SetStaticMesh(Mesh);
				Part->SetCastShadow(false);
				UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(Basic, Actor);
				Tint->SetVectorParameterValue(TEXT("Color"), Colour);
				Part->SetMaterial(0, Tint);
				Part->SetWorldScale3D(Scale);
			};
			Slab(Plane, FVector(Around.X, Around.Y, FloorZ), FVector(80.0, 80.0, 1.0), FLinearColor(0.35f, 0.36f, 0.38f));
			const double Half = 1500.0;
			for (double X = FMath::GridSnap(Around.X - Half, 50.0); X <= Around.X + Half; X += 50.0)
			{
				const double W = FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(X), 100.0)) ? 2.5 : 1.2;
				Slab(Cube, FVector(X, Around.Y, FloorZ + 0.1), FVector(W / 100.0, 2.0 * Half / 100.0, 0.002), FLinearColor(0.85f, 0.85f, 0.8f));
			}
			for (double Y = FMath::GridSnap(Around.Y - Half, 50.0); Y <= Around.Y + Half; Y += 50.0)
			{
				const double W = FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Y), 100.0)) ? 2.5 : 1.2;
				Slab(Cube, FVector(Around.X, Y, FloorZ + 0.1), FVector(2.0 * Half / 100.0, W / 100.0, 0.002), FLinearColor(0.85f, 0.85f, 0.8f));
			}
			// Side-on at a three-quarter angle to the body's starting facing, on the sun's side.
			const FVector Ahead = FRotator(0.0, Facing, 0.0).Vector();
			const FVector Side = FRotator(0.0, Facing + 90.0, 0.0).Vector();
			const FVector Across = FVector::DotProduct(Side, -SunTravel) >= 0.0 ? Side : -Side;
			Out = (Across * FMath::Cos(FMath::DegreesToRadians(35.0)) + Ahead * FMath::Sin(FMath::DegreesToRadians(35.0))).GetSafeNormal();

			Camera = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, Params);
			if (!Camera)
			{
				return;
			}
			Target = NewObject<UTextureRenderTarget2D>();
			Target->AddToRoot();
			Target->RenderTargetFormat = RTF_RGBA8_SRGB;
			Target->ClearColor = FLinearColor::Black;
			Target->InitAutoFormat(Width, Height);
			Target->UpdateResourceImmediate(true);
			Capture = Camera->GetCaptureComponent2D();
			Capture->TextureTarget = Target;
			Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Capture->FOVAngle = static_cast<float>(FovDeg);
			Capture->bCaptureEveryFrame = false;
			Capture->bCaptureOnMovement = false;
			Capture->bAlwaysPersistRenderingState = true;
			Capture->ShowFlags.SetMotionBlur(false);
			Follow(Around);
			for (int32 I = 0; I < 40; ++I)
			{
				Capture->CaptureScene();
				FlushRenderingCommands();
				if (I % 10 == 0)
				{
					if (GShaderCompilingManager)
					{
						GShaderCompilingManager->FinishAllCompilation();
					}
					FAssetCompilingManager::Get().FinishAllCompilation();
					IStreamingManager::Get().StreamAllResources(2.0f);
				}
			}
			IFileManager::Get().MakeDirectory(*Dir, true);
			bOk = true;
		}

		/** The camera keeps its angle and distance and slides with the body (its root on the floor). */
		void Follow(const FVector& Body)
		{
			if (!Camera)
			{
				return;
			}
			const FVector LookAt(Body.X, Body.Y, FloorZ + 95.0);
			const FVector Eye = LookAt + Out * 420.0 + FVector(0.0, 0.0, 60.0);
			Camera->SetActorLocationAndRotation(Eye, (LookAt - Eye).Rotation());
		}

		void Frame(int32 Index)
		{
			if (!bOk)
			{
				return;
			}
			Capture->CaptureScene();
			FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
			if (!Resource || !Resource->ReadPixels(Pixels))
			{
				return;
			}
			for (FColor& C : Pixels)
			{
				C.A = 255;
			}
			FImageUtils::SaveImageByExtension(*FPaths::Combine(Dir, FString::Printf(TEXT("frame_%05d.png"), Index)), FImageView(Pixels.GetData(), Width, Height));
		}
	};

	/** One body's track over a clip: foot contact points, knee bends and pelvis height. */
	struct FTrack
	{
		TArray<FVector> Feet[2];
		/** The ankles alone (a MetaHuman in shoes hides its toes), so every body can be compared on the same points. */
		TArray<FVector> Ankles[2];
		TArray<double> KneeDeg[2];
		TArray<double> PelvisCm;
		TArray<double> FootLowCm;

		void Record(const USkeletalMeshComponent* Mesh, double FloorZ, bool bAnkleOnly)
		{
			const TCHAR* AnkleBones[2] = { TEXT("foot_l"), TEXT("foot_r") };
			const TCHAR* Balls[2] = { TEXT("ball_l"), TEXT("ball_r") };
			const TCHAR* Thighs[2] = { TEXT("thigh_l"), TEXT("thigh_r") };
			const TCHAR* Calves[2] = { TEXT("calf_l"), TEXT("calf_r") };
			double Low = TNumericLimits<double>::Max();
			for (int32 F = 0; F < 2; ++F)
			{
				const FVector A = Mesh->GetSocketLocation(AnkleBones[F]);
				const FVector B = !bAnkleOnly && Mesh->GetBoneIndex(Balls[F]) != INDEX_NONE ? Mesh->GetSocketLocation(Balls[F]) : A;
				const FVector Contact = B.Z < A.Z ? B : A;
				Feet[F].Add(Contact);
				Ankles[F].Add(A);
				Low = FMath::Min(Low, Contact.Z - FloorZ);
				const FVector H = Mesh->GetSocketLocation(Thighs[F]);
				const FVector K = Mesh->GetSocketLocation(Calves[F]);
				const double Cos = FVector::DotProduct((H - K).GetSafeNormal(), (A - K).GetSafeNormal());
				KneeDeg[F].Add(180.0 - FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.0, 1.0))));
			}
			FootLowCm.Add(Low);
			PelvisCm.Add(Mesh->GetSocketLocation(TEXT("pelvis")).Z - FloorZ);
		}

		double Slide() const { return PadelMotionProbe::PlantedSlideCmps(Feet, Dt); }
		double AnkleSlide() const { return PadelMotionProbe::PlantedSlideCmps(Ankles, Dt); }

		static double Mean(const TArray<double>& V)
		{
			double Sum = 0.0;
			for (const double X : V) { Sum += X; }
			return V.IsEmpty() ? 0.0 : Sum / V.Num();
		}

		static double Percentile(TArray<double> V, double P)
		{
			if (V.IsEmpty()) { return 0.0; }
			V.Sort();
			return V[FMath::Clamp(FMath::FloorToInt(P * (V.Num() - 1)), 0, V.Num() - 1)];
		}

		FString Summary() const
		{
			TArray<double> Knees = KneeDeg[0];
			Knees.Append(KneeDeg[1]);
			return FString::Printf(TEXT("slide %.1f cm/s (ankles only %.1f); knee bend mean %.0f deg (max %.0f, min %.0f); pelvis %.1f cm up (mean); lowest foot point %.1f cm above the floor (5th percentile %.1f)"),
				Slide(), AnkleSlide(), Mean(Knees), Percentile(Knees, 1.0), Percentile(Knees, 0.0), Mean(PelvisCm), Percentile(FootLowCm, 0.0), Percentile(FootLowCm, 0.05));
		}
	};

	/** The per-frame mean absolute difference of two tracks' knee bends (degrees). */
	double KneeDifference(const FTrack& A, const FTrack& B)
	{
		double Sum = 0.0;
		int32 N = 0;
		for (int32 F = 0; F < 2; ++F)
		{
			for (int32 I = 0; I < FMath::Min(A.KneeDeg[F].Num(), B.KneeDeg[F].Num()); ++I)
			{
				Sum += FMath::Abs(A.KneeDeg[F][I] - B.KneeDeg[F][I]);
				++N;
			}
		}
		return N > 0 ? Sum / N : 0.0;
	}

	/** The clip's root motion between two playheads, wrapping round a looping clip. */
	FTransform RootMotion(const UAnimSequence* Clip, double From, double To, bool bLoop)
	{
		const double Length = Clip->GetPlayLength();
		if (!bLoop || To >= From)
		{
			return Clip->ExtractRootMotionFromRange(From, FMath::Min(To, Length), FAnimExtractContext());
		}
		const FTransform ToEnd = Clip->ExtractRootMotionFromRange(From, Length, FAnimExtractContext());
		const FTransform FromStart = Clip->ExtractRootMotionFromRange(0.0, To, FAnimExtractContext());
		return FromStart * ToEnd;
	}

	/** How long each clip plays: loops twice round, the others once and half a second held. */
	double Duration(const UAnimSequence* Clip, bool bLoop)
	{
		return bLoop ? 2.0 * Clip->GetPlayLength() : Clip->GetPlayLength() + 0.5;
	}

	/** The mannequin playing the source clip in a fresh world, straight from the sequence. */
	FTrack PlayManny(FAutomationTestBase& Test, const UAnimSequence* Clip, bool bLoop, FStage* Stage)
	{
		FTrack Track;
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MannyMesh);
		if (!Mesh || Mesh->GetSkeleton() != Clip->GetSkeleton())
		{
			Test.AddWarning(TEXT("No SKM_Manny_Simple on the clip's skeleton: the mannequin column is skipped"));
			return Track;
		}
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		ON_SCOPE_EXIT
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		};
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Faces +X like the probe's pawn rotated: the mannequin faces +Y in its component, so yaw -90.
		ASkeletalMeshActor* Actor = World->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(FRotator(0.0, -90.0, 0.0), FVector::ZeroVector), Params);
		USkeletalMeshComponent* Body = Actor->GetSkeletalMeshComponent();
		Body->SetMobility(EComponentMobility::Movable);
		Body->SetSkeletalMeshAsset(Mesh);
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Body->SetAnimation(const_cast<UAnimSequence*>(Clip));
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		// A root-locked clip plays in place and the actor carries its root motion; otherwise the root travels in the pose.
		const bool bMoveActor = Clip->bForceRootLock;
		if (Stage)
		{
			Stage->Build(World, 0.0, FVector::ZeroVector, 0.0);
		}
		const double Length = Clip->GetPlayLength();
		FVector Position = FVector::ZeroVector;
		double Yaw = -90.0;
		double Last = 0.0;
		int32 Frame = 0;
		for (double T = 0.0; T < Duration(Clip, bLoop) - 1e-6; T += Dt, ++Frame)
		{
			const double Head = bLoop ? FMath::Fmod(T, Length) : FMath::Min(T, Length);
			if (!bMoveActor && bLoop && Frame > 0 && Head < Last)
			{
				// The root travels in the pose: carry the actor on by a whole cycle so the loop doesn't jump back.
				const FTransform Cycle = Clip->ExtractRootMotionFromRange(0.0, Length, FAnimExtractContext());
				Position += Body->GetComponentQuat().RotateVector(Cycle.GetTranslation()) * FVector(1.0, 1.0, 0.0);
				Actor->SetActorLocation(Position);
			}
			if (bMoveActor && Frame > 0)
			{
				const FTransform Step = RootMotion(Clip, Last, Head, bLoop);
				Position += Body->GetComponentQuat().RotateVector(Step.GetTranslation()) * FVector(1.0, 1.0, 0.0);
				Yaw += Step.GetRotation().Rotator().Yaw;
				Actor->SetActorLocationAndRotation(Position, FRotator(0.0, Yaw, 0.0));
			}
			Last = Head;
			if (UAnimSingleNodeInstance* Single = Body->GetSingleNodeInstance())
			{
				Single->SetPlaying(false);
				Single->SetPosition(static_cast<float>(Head), false);
			}
			Body->TickAnimation(0.0f, false);
			Body->RefreshBoneTransforms();
			Track.Record(Body, 0.0, false);
			if (Stage)
			{
				Stage->Follow(Body->GetSocketLocation(TEXT("pelvis")));
				Stage->Frame(Frame);
			}
		}
		return Track;
	}

	/** The pawn of a fresh match, its placeholder driver and the shown MetaHuman body (or null). */
	struct FMatch
	{
		UWorld* World = nullptr;
		APadelMatchDirector* Director = nullptr;
		APadelPlayerPawn* Pawn = nullptr;
		USkeletalMeshComponent* Driver = nullptr;
		USkeletalMeshComponent* Shown = nullptr;
		UPadelPlayerAnimInstance* Anim = nullptr;

		bool Start()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			Director = World->SpawnActor<APadelMatchDirector>();
			Padel::Ui::FMatchSetup Setup;
			Setup.SetsToWin = 1;
			Setup.HumanPlayerId = -1;
			if (!Director || !Director->StartMatch(Setup))
			{
				return false;
			}
			Director->Tick(static_cast<float>(Dt));
			for (int32 Id = 0; Id < Padel::NumPlayers && !Anim; ++Id)
			{
				Pawn = Director->GetPawn(Id);
				Driver = Pawn && Pawn->GetAnimation()->HasSkeletalBody() ? Pawn->GetAnimation()->GetDriverBody() : nullptr;
				Anim = Driver ? Cast<UPadelPlayerAnimInstance>(Driver->GetAnimInstance()) : nullptr;
			}
			if (AActor* MetaHuman = Anim ? Pawn->GetAnimation()->GetMetaHuman() : nullptr)
			{
				TInlineComponentArray<USkeletalMeshComponent*> Parts(MetaHuman);
				for (USkeletalMeshComponent* Part : Parts)
				{
					if (Part->GetAnimInstance() && Part->GetAnimInstance()->GetClass()->GetName() == TEXT("PadelMetaHumanAnimInstance"))
					{
						Shown = Part;
					}
				}
			}
			return Anim != nullptr;
		}

		~FMatch()
		{
			if (Director)
			{
				Director->CloseMatch();
			}
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		/** Only Body shows; the other players, their MetaHumans and the ball are hidden and moved away. */
		void ShowOnly(EBody Body)
		{
			AActor* OwnMetaHuman = Pawn->GetAnimation()->GetMetaHuman();
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				const bool bOtherPawn = Actor->IsA<APadelPlayerPawn>() && Actor != Pawn;
				const bool bOthers = Actor->GetOwner() && Actor->GetOwner()->IsA<APadelPlayerPawn>() && Actor->GetOwner() != Pawn;
				if (bOtherPawn || bOthers || Actor->IsA<APadelBall>())
				{
					Actor->SetActorHiddenInGame(true);
					if (!Actor->GetAttachParentActor())
					{
						Actor->SetActorLocation(FVector(0.0, 0.0, -100000.0));
					}
				}
			}
			const bool bMetaHuman = Body == EBody::MetaHuman;
			Driver->SetVisibility(!bMetaHuman);
			if (OwnMetaHuman)
			{
				OwnMetaHuman->SetActorHiddenInGame(!bMetaHuman);
			}
			TInlineComponentArray<UStaticMeshComponent*> Statics(Pawn);
			for (UStaticMeshComponent* Part : Statics)
			{
				if (Part->GetFName() == FName(TEXT("Racket")))
				{
					Part->SetHiddenInGame(true);
				}
			}
		}
	};

	/**
	 * The retargeted clip pinned on the pawn's motion matcher (play rate 1, no search), warps and
	 * body layers off, the pawn moved by the clip's own root motion. Tracks the placeholder and
	 * the MetaHuman; Stage (if any) films Body.
	 */
	bool PlayPinned(FAutomationTestBase& Test, const UAnimSequence* Clip, bool bLoop, EBody Body, FStage* Stage, FTrack& OutPlaceholder, FTrack& OutMetaHuman, bool& bOutMetaHuman)
	{
		FMatch Match;
		if (!Match.Start() || Match.Anim->LocomotionSet.MotionMatchingDatabases.IsEmpty() || Match.Driver->GetSkeletalMeshAsset()->GetSkeleton() != Clip->GetSkeleton())
		{
			return false;
		}
		bOutMetaHuman = Match.Shown != nullptr;
		if (Body == EBody::MetaHuman && !Match.Shown)
		{
			return false;
		}
		UPadelPlayerAnimInstance* Anim = Match.Anim;
		USkeletalMeshComponent* Driver = Match.Driver;
		APadelPlayerPawn* Pawn = Match.Pawn;
		Anim->RawSwingWeight = 0.0f;
		Anim->ContactWarpWeight = 0.0f;
		Anim->ArmCarryAlpha = 0.0f;
		Anim->Body = FPadelBodyDynamics();
		Anim->Stance = FPadelBodyDynamics();
		Anim->LocomotionSet.LocomotionWarp = FPadelLocomotionWarpTuning();
		const UPoseSearchDatabase* Database = Anim->LocomotionSet.MotionMatchingDatabases[0];
		const double FloorZ = Driver->GetComponentLocation().Z;
		FVector Position = Pawn->GetActorLocation();
		double Yaw = Pawn->GetActorRotation().Yaw;
		if (Stage)
		{
			Match.ShowOnly(Body);
			Stage->Build(Match.World, FloorZ, Position, Yaw);
		}
		const double Length = Clip->GetPlayLength();
		TArray<FVector> Past;
		double Last = 0.0;
		int32 Frame = 0;
		for (double T = 0.0; T < Duration(Clip, bLoop) - 1e-6; T += Dt, ++Frame)
		{
			const double Head = bLoop ? FMath::Fmod(T, Length) : FMath::Min(T, Length);
			FVector Move = FVector::ZeroVector;
			if (Frame > 0)
			{
				const FTransform Step = RootMotion(Clip, Last, Head, bLoop);
				Move = Driver->GetComponentQuat().RotateVector(Step.GetTranslation()) * FVector(1.0, 1.0, 0.0);
				Position += Move;
				Yaw += Step.GetRotation().Rotator().Yaw;
			}
			Pawn->SetActorLocationAndRotation(Position, FRotator(0.0, Yaw, 0.0));
			Match.World->TimeSeconds += Dt;
			Past.Add(Position);
			TArray<FVector> Trajectory;
			for (const float Time : Anim->TrajectoryTimes)
			{
				const int32 Back = FMath::Min(Past.Num() - 1, FMath::RoundToInt(FMath::Max(-Time, 0.0f) / Dt));
				Trajectory.Add(Time < 0.0f ? Past[Past.Num() - 1 - Back] : Position + Move / Dt * Time);
			}
			Padel::Animation::FPlayerAnimFrame AnimFrame;
			AnimFrame.SpeedMps = Move.Size2D() / Dt / 100.0;
			AnimFrame.Locomotion = Padel::Animation::ELocomotionState::Run;
			AnimFrame.FacingYawDeg = Yaw;
			Anim->ApplyFrame(AnimFrame, nullptr, nullptr, nullptr, Trajectory);
			Anim->Body = FPadelBodyDynamics();
			Anim->Stance = FPadelBodyDynamics();
			FPadelMotionMatcher& Matcher = Anim->GetPoseNodeForTest().GetMotionMatcherForTest();
			if (Frame == 0)
			{
				// The first update builds the matcher's history and configuration; then the clip is pinned.
				Driver->TickAnimation(static_cast<float>(Dt), false);
				Matcher.PinForTest(Clip, Database, Head);
			}
			else if (bLoop && Head < Last)
			{
				// A pinned clip doesn't loop on its own: start it again where the loop came round.
				Matcher.PinForTest(Clip, Database, FMath::Max(0.0, Head - Dt));
			}
			Last = Head;
			Driver->TickAnimation(static_cast<float>(Dt), false);
			Driver->RefreshBoneTransforms();
			if (Match.Shown)
			{
				Match.Shown->TickAnimation(static_cast<float>(Dt), false);
				Match.Shown->RefreshBoneTransforms();
			}
			OutPlaceholder.Record(Driver, FloorZ, false);
			if (Match.Shown)
			{
				OutMetaHuman.Record(Match.Shown, FloorZ, true);
			}
			if (Stage)
			{
				Stage->Follow(Pawn->GetActorLocation());
				Stage->Frame(Frame);
			}
		}
		if (Anim->GetPoseNode().GetMotionMatcher().CurrentClip() != Clip)
		{
			Test.AddWarning(FString::Printf(TEXT("%s: the pinned clip didn't stay playing"), *Clip->GetName()));
		}
		return true;
	}

	/** Reference-pose numbers of a skeletal mesh, component space, cm and degrees. */
	FString Proportions(const USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return TEXT("not found");
		}
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		auto At = [&Ref](const TCHAR* Name) -> FVector
		{
			int32 Bone = Ref.FindBoneIndex(Name);
			if (Bone == INDEX_NONE)
			{
				return FVector(NAN);
			}
			FTransform T = FTransform::Identity;
			for (; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
			{
				T = T * Ref.GetRefBonePose()[Bone];
			}
			return T.GetLocation();
		};
		const FVector Pelvis = At(TEXT("pelvis")), ThighL = At(TEXT("thigh_l")), ThighR = At(TEXT("thigh_r")), Calf = At(TEXT("calf_l")), Foot = At(TEXT("foot_l")),
			Ball = At(TEXT("ball_l")), Head = At(TEXT("head")), Upper = At(TEXT("upperarm_l")), Lower = At(TEXT("lowerarm_l")), Hand = At(TEXT("hand_l"));
		const double Thigh = FVector::Dist(ThighL, Calf);
		const double Shin = FVector::Dist(Calf, Foot);
		const double KneeBend = 180.0 - FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct((ThighL - Calf).GetSafeNormal(), (Foot - Calf).GetSafeNormal()), -1.0, 1.0)));
		const FVector Arm = Lower - Upper;
		const double ArmDown = FMath::RadiansToDegrees(FMath::Atan2(-Arm.Z, Arm.Size2D()));
		const FVector Knee = Calf - 0.5 * (ThighL + Foot);
		return FString::Printf(TEXT("pelvis %.1f up; head %.1f up; hips %.1f wide; thigh %.1f, shin %.1f (leg %.1f, thigh/shin %.2f); ankle %.1f up, ball %.1f up; foot %.1f long; upper arm %.1f, forearm %.1f; arm %.0f deg below horizontal (T pose 0, A pose ~45); knee bend %.1f deg, knee points (%.2f, %.2f, %.2f); %d bones"),
			Pelvis.Z, Head.Z, FVector::Dist(ThighL, ThighR), Thigh, Shin, Thigh + Shin, Shin > 0.0 ? Thigh / Shin : 0.0, Foot.Z, Ball.Z, FVector::Dist2D(Foot, Ball),
			FVector::Dist(Upper, Lower), FVector::Dist(Lower, Hand), ArmDown, KneeBend, Knee.GetSafeNormal().X, Knee.GetSafeNormal().Y, Knee.GetSafeNormal().Z, Ref.GetNum());
	}

	FString ClipFacts(const UAnimSequence* Clip)
	{
		if (!Clip)
		{
			return TEXT("not found");
		}
		const FTransform Whole = Clip->ExtractRootMotionFromRange(0.0, Clip->GetPlayLength(), FAnimExtractContext());
		const FFrameRate Rate = Clip->GetSamplingFrameRate();
		return FString::Printf(TEXT("%.3f s, %d keys at %.2f fps; root motion %s, root lock %s; root travels %.1f cm and turns %.1f deg over the clip (%.1f cm/s); rate scale %.2f"),
			Clip->GetPlayLength(), Clip->GetNumberOfSampledKeys(), Rate.AsDecimal(), Clip->bEnableRootMotion ? TEXT("on") : TEXT("off"),
			Clip->bForceRootLock ? TEXT("on") : TEXT("off"), Whole.GetTranslation().Size2D(), Whole.GetRotation().Rotator().Yaw,
			Whole.GetTranslation().Size2D() / FMath::Max(Clip->GetPlayLength(), 1e-3), Clip->RateScale);
	}

	const UAnimSequence* Load(const TCHAR* Dir, const TCHAR* Name)
	{
		return Cast<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Dir, Name, Name)).TryLoad());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelBaseClipsTest, "Padel.Animation.BaseClips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelBaseClipsTest::RunTest(const FString& Parameters)
{
	using namespace PadelBaseClips;
	FString OutDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("PadelBaseClips="), OutDir))
	{
		AddInfo(TEXT("Skipped: records only with -PadelBaseClips=<folder> (Tools/RecordBaseClips.sh)."));
		return true;
	}
	const bool bRender = FApp::CanEverRender();
	int32 Width = 1280;
	int32 Height = 720;
	FParse::Value(FCommandLine::Get(), TEXT("PadelBaseClipsWidth="), Width);
	FParse::Value(FCommandLine::Get(), TEXT("PadelBaseClipsHeight="), Height);
	FString Only;
	FParse::Value(FCommandLine::Get(), TEXT("PadelBaseClipsOnly="), Only);
	IFileManager::Get().MakeDirectory(*OutDir, true);
	FString Report;

	// The skeletons.
	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMesh);
	const FPadelAnimData Data = FPadelAnimData::Load();
	USkeletalMesh* Placeholder = Data.TeamMeshes.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *Data.TeamMeshes[0]);
	USkeletalMesh* MetaHumanMesh = nullptr;
	{
		FMatch Match;
		if (Match.Start() && Match.Shown)
		{
			MetaHumanMesh = Match.Shown->GetSkeletalMeshAsset();
		}
	}
	Report += TEXT("## Skeletons (reference pose, cm)\n\n");
	Report += FString::Printf(TEXT("- mannequin (%s): %s\n"), Manny ? *Manny->GetName() : TEXT("-"), *Proportions(Manny));
	Report += FString::Printf(TEXT("- placeholder (%s): %s\n"), Placeholder ? *Placeholder->GetName() : TEXT("-"), *Proportions(Placeholder));
	Report += FString::Printf(TEXT("- MetaHuman (%s): %s\n\n"), MetaHumanMesh ? *MetaHumanMesh->GetName() : TEXT("-"), *Proportions(MetaHumanMesh));

	Report += TEXT("## Clips played straight (rate 1, no search, no warps, root motion moves the body)\n\n");
	Report += TEXT("| Clip | Mannequin slide (cm/s) | Placeholder slide | Ankles only: mannequin / placeholder / MetaHuman | Knee bend vs mannequin, placeholder (deg, mean per frame) | MetaHuman vs placeholder |\n|---|---|---|---|---|---|\n");
	FString Details;
	for (const FClip& Entry : Clips)
	{
		if (!Only.IsEmpty() && !FString(Entry.Name).Contains(Only))
		{
			continue;
		}
		const UAnimSequence* Source = Load(SourceDir, Entry.Name);
		const UAnimSequence* Retargeted = Load(RetargetedDir, Entry.Name);
		Details += FString::Printf(TEXT("### %s (%s)\n\n- source on the mannequin: %s\n- retargeted on the placeholder: %s\n"), Entry.Label, Entry.Name, *ClipFacts(Source), *ClipFacts(Retargeted));
		if (!Source || !Retargeted)
		{
			AddWarning(FString::Printf(TEXT("%s: source or retargeted clip missing"), Entry.Name));
			continue;
		}
		const bool bLoop = FString(Entry.Name).Contains(TEXT("_Loop"));
		FTrack MannyTrack, PlaceholderTrack, MetaHumanTrack, Unused;
		bool bMetaHuman = false;
		{
			FStage Stage;
			Stage.Dir = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_1mannequin"), Entry.Name));
			Stage.Width = Width;
			Stage.Height = Height;
			MannyTrack = PlayManny(*this, Source, bLoop, bRender ? &Stage : nullptr);
		}
		{
			FStage Stage;
			Stage.Dir = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_2placeholder"), Entry.Name));
			Stage.Width = Width;
			Stage.Height = Height;
			if (!PlayPinned(*this, Retargeted, bLoop, EBody::Placeholder, bRender ? &Stage : nullptr, PlaceholderTrack, MetaHumanTrack, bMetaHuman))
			{
				AddWarning(FString::Printf(TEXT("%s: the placeholder couldn't play it"), Entry.Name));
				continue;
			}
		}
		if (bRender && bMetaHuman)
		{
			FStage Stage;
			Stage.Dir = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_3metahuman"), Entry.Name));
			Stage.Width = Width;
			Stage.Height = Height;
			FTrack Again;
			PlayPinned(*this, Retargeted, bLoop, EBody::MetaHuman, &Stage, Again, Unused, bMetaHuman);
		}
		Report += FString::Printf(TEXT("| %s | %.1f | %.1f | %.1f / %.1f / %s | %.1f | %s |\n"), Entry.Label, MannyTrack.Slide(), PlaceholderTrack.Slide(),
			MannyTrack.AnkleSlide(), PlaceholderTrack.AnkleSlide(), bMetaHuman ? *FString::Printf(TEXT("%.1f"), MetaHumanTrack.AnkleSlide()) : TEXT("-"), KneeDifference(MannyTrack, PlaceholderTrack),
			bMetaHuman ? *FString::Printf(TEXT("%.1f"), KneeDifference(PlaceholderTrack, MetaHumanTrack)) : TEXT("-"));
		Details += FString::Printf(TEXT("- mannequin: %s\n- placeholder: %s\n"), *MannyTrack.Summary(), *PlaceholderTrack.Summary());
		if (bMetaHuman)
		{
			Details += FString::Printf(TEXT("- MetaHuman: %s\n"), *MetaHumanTrack.Summary());
		}
		Details += TEXT("\n");
	}

	// The same moves through the motion matcher, as in the layer videos' A (root motion, no warps).
	Report += TEXT("\n## The same moves through the motion matcher (A: the search picks, the clips' root motion moves the body, no warps)\n\n");
	Report += TEXT("| Move | Placeholder slide steady (cm/s) | Whole move | Clip switches/s | Picks |\n|---|---|---|---|---|\n");
	for (const FClip& Entry : Clips)
	{
		using namespace PadelMotionProbe;
		if (!Only.IsEmpty() && !FString(Entry.Name).Contains(Only))
		{
			continue;
		}
		FStage Stage;
		Stage.Dir = FPaths::Combine(OutDir, FString::Printf(TEXT("%s_4matched"), Entry.Name));
		Stage.Width = Width;
		Stage.Height = Height;
		APadelPlayerPawn* Pawn = nullptr;
		FRunOptions Options;
		Options.Path = Entry.Move;
		Options.bWarpsOff = true;
		Options.bRootMotion = true;
		if (bRender)
		{
			Options.OnSetup = [&Stage, &Pawn](const FSetup& Setup)
			{
				Pawn = Setup.Pawn;
				for (TActorIterator<AActor> It(Setup.World); It; ++It)
				{
					AActor* Actor = *It;
					const bool bOtherPawn = Actor->IsA<APadelPlayerPawn>() && Actor != Setup.Pawn;
					const bool bOthers = Actor->GetOwner() && Actor->GetOwner()->IsA<APadelPlayerPawn>() && Actor->GetOwner() != Setup.Pawn;
					if (bOtherPawn || bOthers || Actor->IsA<APadelBall>())
					{
						Actor->SetActorHiddenInGame(true);
						if (!Actor->GetAttachParentActor())
						{
							Actor->SetActorLocation(FVector(0.0, 0.0, -100000.0));
						}
					}
				}
				Setup.Driver->SetVisibility(true);
				if (AActor* MetaHuman = Setup.Pawn->GetAnimation()->GetMetaHuman())
				{
					MetaHuman->SetActorHiddenInGame(true);
				}
				TInlineComponentArray<UStaticMeshComponent*> Statics(Setup.Pawn);
				for (UStaticMeshComponent* Part : Statics)
				{
					if (Part->GetFName() == FName(TEXT("Racket")))
					{
						Part->SetHiddenInGame(true);
					}
				}
				Stage.Build(Setup.World, Setup.Driver->GetComponentLocation().Z, Setup.Path.IsEmpty() ? Setup.Pawn->GetActorLocation() : Setup.Path[0], Setup.StartYawDeg);
			};
			Options.OnFrame = [&Stage, &Pawn](int32 Frame, const FString&)
			{
				if (Pawn)
				{
					Stage.Follow(Pawn->GetActorLocation());
				}
				Stage.Frame(Frame);
			};
		}
		const FRun Run = PadelMotionProbe::Run(*this, EDrive::LocomotionOnly, Options);
		if (!Run.bRan)
		{
			AddWarning(FString::Printf(TEXT("%s: the matched run didn't run (%s)"), Entry.Label, *Run.SkipReason));
			continue;
		}
		PadelMotionProbe::Report(*this, Entry.Label, Run);
		TArray<FString> Played;
		for (const FString& Line : Run.ClipPerFrame)
		{
			FString Name;
			Line.Split(TEXT(" "), &Name, nullptr);
			if (!Name.IsEmpty() && (Played.IsEmpty() || Played.Last() != Name))
			{
				Played.Add(Name);
			}
		}
		TArray<double> Rates;
		for (const FString& Line : Run.ClipPerFrame)
		{
			FString Rest;
			if (Line.Split(TEXT(" x"), nullptr, &Rest))
			{
				Rates.Add(FCString::Atod(*Rest));
			}
		}
		Report += FString::Printf(TEXT("| %s | %.1f | %.1f | %.2f | %s |\n"), Entry.Label, Run.Placeholder.SteadySlideCmps, Run.Placeholder.SlideCmps, Run.SwitchesPerS, *FString::Join(Played, TEXT(" → ")));
		Details += FString::Printf(TEXT("### matched: %s\n\n- play rate min %.2f, median %.2f, max %.2f; slide by phase: %s\n\n"), Entry.Label,
			FTrack::Percentile(Rates, 0.0), FTrack::Percentile(Rates, 0.5), FTrack::Percentile(Rates, 1.0), *Run.Placeholder.SlideByPhase);
	}
	FFileHelper::SaveStringToFile(Report + TEXT("\n## Details\n\n") + Details, *FPaths::Combine(OutDir, TEXT("base.md")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	AddInfo(FString::Printf(TEXT("Base clip frames and numbers in %s"), *OutDir));
	return true;
}

#endif
