// Phase 0 of the movement plan: side-by-side videos that add one animation layer at a time, so
// a playtester can say which layer is the first to look wrong. Diagnosis only: every switch here
// is a probe option (PadelMotionProbe.h FRunOptions), nothing in the game changes.
//
//   A  clip root motion: the motion matched clips move the body with their own root motion
//   B  clips following the path, locomotion only, no warps, placeholder mannequin
//   C  B plus the locomotion warps (orientation, stride, foot lock)
//   D  C plus the body layers (the full stack: facing spring, lean, pelvis, stance, head look, split hop)
//   E  D on the MetaHuman, as in a match
//
// Records only when the editor runs with a renderer and -PadelLayerVideo=<folder> (Tools/RecordLayerVideos.sh);
// otherwise it reports that it skipped. Each variant writes <folder>/<letter>/frame_00000.png...
// at a fixed 60 Hz step from one fixed camera, and <folder>/metrics.md and metrics.json hold the
// probe's numbers.
#include "Misc/AutomationTest.h"

#include "PadelMotionProbe.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AssetCompilingManager.h"
#include "ContentStreaming.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "PadelLog.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Animation/PadelPlayerAnimComponent.h"
#include "Presentation/PadelBall.h"
#include "Presentation/PadelVenueLighting.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelLayerVideo
{
	using namespace PadelMotionProbe;

	enum class EBody : uint8 { Placeholder, MetaHuman };

	struct FLayerVariant
	{
		TCHAR Key;
		const TCHAR* Label;
		EDrive Drive;
		bool bWarpsOff;
		bool bRootMotion;
		EBody Body;
	};

	const FLayerVariant Variants[] = {
		{ TEXT('A'), TEXT("clip root motion"), EDrive::LocomotionOnly, true, true, EBody::Placeholder },
		{ TEXT('B'), TEXT("clips on the path, no warps"), EDrive::LocomotionOnly, true, false, EBody::Placeholder },
		{ TEXT('C'), TEXT("B + locomotion warps"), EDrive::LocomotionOnly, false, false, EBody::Placeholder },
		{ TEXT('D'), TEXT("C + body layers (full stack)"), EDrive::FullStack, false, false, EBody::Placeholder },
		{ TEXT('E'), TEXT("D on the MetaHuman"), EDrive::FullStack, false, false, EBody::MetaHuman },
	};

	/** One fixed camera rendering one body on a grid floor in the venue's daylight, a PNG per frame. */
	struct FRecorder
	{
		FString Dir;
		int32 Width = 1920;
		int32 Height = 1080;
		double FovDeg = 40.0;
		EBody Body = EBody::Placeholder;
		bool bOk = false;
		FString Problem;
		TObjectPtr<UTextureRenderTarget2D> Target;
		USceneCaptureComponent2D* Capture = nullptr;
		TArray<FColor> Pixels;
		FString CameraNote;
		APadelPlayerPawn* Pawn = nullptr;

		~FRecorder()
		{
			if (Target)
			{
				Target->RemoveFromRoot();
			}
		}

		void Setup(const FSetup& In)
		{
			UWorld* World = In.World;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			// Daylight from the venue's own preset (sun, sky, fog, the locked broadcast exposure),
			// without the floodlight heads (their shadows would cross the floor).
			FVector SunTravel = FVector(0.0, 0.0, -1.0);
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

			// A grid floor at the feet, so a sliding foot shows against the lines.
			const double FloorZ = In.Driver->GetComponentLocation().Z;
			FVector Centre = FVector::ZeroVector;
			FBox Box(ForceInit);
			for (const FVector& P : In.Path)
			{
				Box += FVector(P.X, P.Y, FloorZ);
			}
			Centre = Box.GetCenter();
			Pawn = In.Pawn;
			UE_LOG(LogPadel, Display, TEXT("LayerVideo path box %s .. %s, start yaw %.1f, %d frames"), *Box.Min.ToString(), *Box.Max.ToString(), In.StartYawDeg, In.Path.Num());
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
			Slab(Plane, FVector(Centre.X, Centre.Y, FloorZ), FVector(60.0, 60.0, 1.0), FLinearColor(0.18f, 0.2f, 0.22f));
			// Lines every 50 cm (every metre wider) over the path and 3 m round it.
			const FBox Lines = Box.ExpandBy(FVector(300.0, 300.0, 0.0));
			const FLinearColor LineColour(0.75f, 0.75f, 0.7f);
			for (double X = FMath::GridSnap(Lines.Min.X, 50.0); X <= Lines.Max.X; X += 50.0)
			{
				const double W = FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(X), 100.0)) ? 3.0 : 1.5;
				Slab(Cube, FVector(X, Centre.Y, FloorZ + 0.1), FVector(W / 100.0, Lines.GetSize().Y / 100.0, 0.002), LineColour);
			}
			for (double Y = FMath::GridSnap(Lines.Min.Y, 50.0); Y <= Lines.Max.Y; Y += 50.0)
			{
				const double W = FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Y), 100.0)) ? 3.0 : 1.5;
				Slab(Cube, FVector(Centre.X, Y, FloorZ + 0.1), FVector(Lines.GetSize().X / 100.0, W / 100.0, 0.002), LineColour);
			}

			// Only the recorded body: the other players, their MetaHumans and the ball are hidden.
			AActor* OwnMetaHuman = In.Pawn->GetAnimation()->GetMetaHuman();
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				const bool bOtherPawn = Actor->IsA<APadelPlayerPawn>() && Actor != In.Pawn;
				const bool bOthers = Actor->GetOwner() && Actor->GetOwner()->IsA<APadelPlayerPawn>() && Actor->GetOwner() != In.Pawn;
				if (bOtherPawn || bOthers || Actor->IsA<APadelBall>())
				{
					// Hidden and far away: hidden hair and clothes still cast shadows.
					Actor->SetActorHiddenInGame(true);
					if (!Actor->GetAttachParentActor())
					{
						Actor->SetActorLocation(FVector(0.0, 0.0, -100000.0));
					}
				}
			}
			const bool bMetaHuman = Body == EBody::MetaHuman;
			if (bMetaHuman && !OwnMetaHuman)
			{
				Problem = TEXT("no MetaHuman is shown on this machine");
				return;
			}
			In.Driver->SetVisibility(!bMetaHuman);
			if (OwnMetaHuman)
			{
				OwnMetaHuman->SetActorHiddenInGame(!bMetaHuman);
			}
			// The racket rides the MetaHuman's hand: shown with it, as in a match; not on the placeholder.
			TInlineComponentArray<UStaticMeshComponent*> Statics(In.Pawn);
			for (UStaticMeshComponent* Part : Statics)
			{
				if (Part->GetFName() == FName(TEXT("Racket")))
				{
					Part->SetHiddenInGame(!bMetaHuman);
				}
			}

			// One fixed camera for every variant: side-on at a three-quarter angle (toward the net
			// side, so the forward run comes toward it), slightly above, the whole path and body in frame.
			const FVector Ahead = FRotator(0.0, In.StartYawDeg, 0.0).Vector();
			const FVector Side = FRotator(0.0, In.StartYawDeg + 90.0, 0.0).Vector();
			// The camera on the sun's side of the path, so the body is lit from the front.
			const FVector Across = FVector::DotProduct(Side, -SunTravel) >= 0.0 ? Side : -Side;
			const FVector Out = (Across * FMath::Cos(FMath::DegreesToRadians(35.0)) + Ahead * FMath::Sin(FMath::DegreesToRadians(35.0))).GetSafeNormal();
			const FVector LookAt(Centre.X, Centre.Y, FloorZ + 80.0);
			const double TanH = FMath::Tan(FMath::DegreesToRadians(FovDeg * 0.5));
			const double TanV = TanH * Height / Width;
			FVector Eye = LookAt;
			FRotator View = FRotator::ZeroRotator;
			for (double Distance = 300.0; Distance < 4000.0; Distance += 20.0)
			{
				Eye = LookAt + Out * Distance + FVector(0.0, 0.0, 0.18 * Distance);
				View = (LookAt - Eye).Rotation();
				const FTransform Cam(View, Eye);
				bool bFits = true;
				for (const FVector& P : In.Path)
				{
					// The body's reach round its path point: half a metre each way, up to above the head.
					for (const FVector Offset : { FVector(50, 50, 0), FVector(-50, 50, 0), FVector(50, -50, 0), FVector(-50, -50, 0), FVector(0, 0, 200) })
					{
						const FVector L = Cam.InverseTransformPosition(FVector(P.X, P.Y, FloorZ) + Offset);
						if (L.X <= 1.0 || FMath::Abs(L.Y) / L.X > TanH * 0.85 || FMath::Abs(L.Z) / L.X > TanV * 0.85)
						{
							bFits = false;
						}
					}
				}
				if (bFits)
				{
					CameraNote = FString::Printf(TEXT("camera %.1f m from the path's centre, %.1f m up, looking down %.0f deg, %.0f deg horizontal field of view"),
						Distance / 100.0, (Eye.Z - FloorZ) / 100.0, -View.Pitch, FovDeg);
					break;
				}
			}

			ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform(View, Eye), Params);
			if (!Camera)
			{
				Problem = TEXT("the scene capture didn't spawn");
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
			// Temporal anti-aliasing, exposure and Lumen keep their history from frame to frame.
			Capture->bAlwaysPersistRenderingState = true;
			Capture->ShowFlags.SetMotionBlur(false);

			// Warm up: compile what the first frames need and let textures stream in and the lighting settle.
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

		void Frame(int32 Index)
		{
			if (!bOk)
			{
				return;
			}
			if (Index % 60 == 0 && Pawn)
			{
				UE_LOG(LogPadel, Display, TEXT("LayerVideo frame %d pawn %s"), Index, *Pawn->GetActorLocation().ToString());
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

	FString Fmt(double V, int32 Decimals = 1)
	{
		return Decimals <= 0 ? FString::Printf(TEXT("%.0f"), V) : Decimals == 1 ? FString::Printf(TEXT("%.1f"), V) : FString::Printf(TEXT("%.2f"), V);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelLayerVideoTest, "Padel.Animation.LayerVideo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelLayerVideoTest::RunTest(const FString& Parameters)
{
	using namespace PadelLayerVideo;
	FString OutDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("PadelLayerVideo="), OutDir))
	{
		AddInfo(TEXT("Skipped: records only with -PadelLayerVideo=<folder> and a renderer (Tools/RecordLayerVideos.sh)."));
		return true;
	}
	const bool bRender = FApp::CanEverRender();
	if (!bRender)
	{
		AddWarning(TEXT("No renderer (-nullrhi?): recording the probe numbers only, no frames."));
	}
	FString Only;
	FParse::Value(FCommandLine::Get(), TEXT("PadelLayerVideoVariants="), Only);
	int32 Width = 1920;
	int32 Height = 1080;
	FParse::Value(FCommandLine::Get(), TEXT("PadelLayerVideoWidth="), Width);
	FParse::Value(FCommandLine::Get(), TEXT("PadelLayerVideoHeight="), Height);
	IFileManager::Get().MakeDirectory(*OutDir, true);

	FString Table = TEXT("| Variant | What plays | Planted-foot slide, steady (cm/s) | Slide, whole path (cm/s) | Planted (s) | Clip switches/s | Pelvis jerk p95 (k deg/s^3) | spine_03 jerk p95 | Head jerk p95 | Worst bone turn in a frame (deg) | Worst arm turn (deg) |\n");
	Table += TEXT("|---|---|---|---|---|---|---|---|---|---|---|\n");
	FString Json = TEXT("[\n");
	FString Notes;
	for (const FLayerVariant& Variant : Variants)
	{
		if (!Only.IsEmpty() && !Only.Contains(FString(1, &Variant.Key)))
		{
			continue;
		}
		FRecorder Recorder;
		Recorder.Dir = FPaths::Combine(OutDir, FString(1, &Variant.Key));
		Recorder.Width = Width;
		Recorder.Height = Height;
		Recorder.Body = Variant.Body;
		FRunOptions Options;
		Options.Path = EPath::LayerVideo;
		Options.bWarpsOff = Variant.bWarpsOff;
		Options.bRootMotion = Variant.bRootMotion;
		if (bRender)
		{
			Options.OnSetup = [&Recorder](const FSetup& Setup) { Recorder.Setup(Setup); };
			Options.OnFrame = [&Recorder](int32 Frame, const FString&) { Recorder.Frame(Frame); };
		}
		const FRun Run = PadelMotionProbe::Run(*this, Variant.Drive, Options);
		const FString Label = FString(1, &Variant.Key);
		if (!Run.bRan)
		{
			AddWarning(FString::Printf(TEXT("%s skipped: %s"), *Label, *Run.SkipReason));
			continue;
		}
		if (bRender && !Recorder.bOk)
		{
			AddWarning(FString::Printf(TEXT("%s not recorded: %s"), *Label, *Recorder.Problem));
		}
		Report(*this, Label, Run);
		const FBodyMetrics& Body = Variant.Body == EBody::MetaHuman ? Run.MetaHuman : Run.Placeholder;
		if (!Body.bValid)
		{
			AddWarning(FString::Printf(TEXT("%s: no %s body measured"), *Label, Variant.Body == EBody::MetaHuman ? TEXT("MetaHuman") : TEXT("placeholder")));
			continue;
		}
		auto P95 = [&Body](const TCHAR* Bone) { const FVector2D* J = Body.JerkDegPerS3.Find(Bone); return J ? J->Y / 1000.0 : 0.0; };
		Table += FString::Printf(TEXT("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s (%s) | %s (%s) |\n"), *Label, Variant.Label,
			*Fmt(Body.SteadySlideCmps), *Fmt(Body.SlideCmps), *Fmt(Body.PlantedS), *Fmt(Run.SwitchesPerS, 2),
			*Fmt(P95(TEXT("pelvis")), 0), *Fmt(P95(TEXT("spine_03")), 0), *Fmt(P95(TEXT("head")), 0),
			*Fmt(Body.WorstBoneTurnDeg), *Body.WorstBoneTurnAt, *Fmt(Body.WorstArmTurnDeg), *Body.WorstArmTurnAt);
		Json += FString::Printf(TEXT("  {\"variant\": \"%s\", \"label\": \"%s\", \"steady_slide_cmps\": %.2f, \"slide_cmps\": %.2f, \"planted_s\": %.2f, \"switches_per_s\": %.3f, \"pelvis_jerk_p95\": %.0f, \"spine_03_jerk_p95\": %.0f, \"head_jerk_p95\": %.0f, \"worst_bone_turn_deg\": %.2f, \"worst_bone_turn_at\": \"%s\", \"worst_arm_turn_deg\": %.2f, \"worst_arm_turn_at\": \"%s\", \"feet_locked\": [%.3f, %.3f], \"frames\": %d, \"slide_by_phase\": \"%s\"},\n"),
			*Label, Variant.Label, Body.SteadySlideCmps, Body.SlideCmps, Body.PlantedS, Run.SwitchesPerS, P95(TEXT("pelvis")) * 1000.0,
			P95(TEXT("spine_03")) * 1000.0, P95(TEXT("head")) * 1000.0, Body.WorstBoneTurnDeg, *Body.WorstBoneTurnAt, Body.WorstArmTurnDeg,
			*Body.WorstArmTurnAt, Run.LockedShare[0], Run.LockedShare[1], Run.Phases.Num(), *Body.SlideByPhase);
		Notes += FString::Printf(TEXT("- %s: %d frames (%.1f s); slide by phase (cm/s): %s; feet locked %.0f%% / %.0f%%; %s\n"), *Label, Run.Phases.Num(), Run.DurationS,
			*Body.SlideByPhase, Run.LockedShare[0] * 100.0, Run.LockedShare[1] * 100.0, *Recorder.CameraNote);
		Notes += FString::Printf(TEXT("  - picks: %s\n"), *Run.Picks);
	}
	Json.RemoveFromEnd(TEXT(",\n"));
	Json += TEXT("\n]\n");
	FFileHelper::SaveStringToFile(Table + TEXT("\n") + Notes, *FPaths::Combine(OutDir, TEXT("metrics.md")));
	FFileHelper::SaveStringToFile(Json, *FPaths::Combine(OutDir, TEXT("metrics.json")));
	AddInfo(FString::Printf(TEXT("Layer video frames and numbers in %s"), *OutDir));
	return true;
}

#endif
