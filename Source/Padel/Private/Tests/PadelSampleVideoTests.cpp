// The movement restart's gate video (docs/decisions/0014-sample-locomotion.md, step 1): one
// pawn on our court, driven along the layer videos' path by the full stack (PadelCore-style
// movement through the animation model), animated by the Game Animation Sample's locomotion
// (Padel.Animation.SampleLocomotion 1), filmed with the same follow camera as the sample
// reference (~/Documents/PadelData/restart-step1). Once with the MetaHuman, once with the
// sample mannequin that drives it.
//
// Records only with -PadelSampleVideo=<folder> and a renderer (Tools/RecordSampleVideo.sh);
// otherwise it reports that it skipped. Numbers: <folder>/sample.md.
#include "Misc/AutomationTest.h"

#include "PadelMotionProbe.h"

#include "Animation/PadelPlayerAnimComponent.h"
#include "AssetCompilingManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentStreaming.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Presentation/PadelBall.h"
#include "Presentation/PadelLookDevStage.h"
#include "Presentation/PadelVenueLighting.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PadelSampleVideo
{
	using namespace PadelMotionProbe;

	enum class EShow : uint8 { MetaHuman, Mannequin };

	/** Our court and the venue's daylight, and a camera following the pawn from a fixed three-quarter angle. */
	struct FFilm
	{
		FString Dir;
		int32 Width = 1280;
		int32 Height = 720;
		EShow Show = EShow::MetaHuman;
		bool bOk = false;
		FString Problem;
		TObjectPtr<UTextureRenderTarget2D> Target;
		USceneCaptureComponent2D* Capture = nullptr;
		ASceneCapture2D* Camera = nullptr;
		APadelPlayerPawn* Pawn = nullptr;
		APadelLookDevStage* StageActor = nullptr;
		double FloorZ = 0.0;
		FVector Out = FVector::ZeroVector;
		TArray<FColor> Pixels;

		~FFilm()
		{
			if (Target)
			{
				Target->RemoveFromRoot();
			}
		}

		void Setup(const FSetup& In)
		{
			UWorld* World = In.World;
			Pawn = In.Pawn;
			FloorZ = In.Driver->GetComponentLocation().Z;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			APadelVenueLighting* Lighting = World->SpawnActor<APadelVenueLighting>(APadelVenueLighting::StaticClass(), FTransform::Identity, Params);
			if (Lighting)
			{
				Lighting->ApplyPreset(TEXT("OutdoorDay"));
			}
			StageActor = World->SpawnActor<APadelLookDevStage>(APadelLookDevStage::StaticClass(), FTransform::Identity, Params);
			if (StageActor)
			{
				StageActor->Assemble(Lighting ? Lighting->GetActiveVenue() : Padel::Presentation::Lighting::EVenue::Outdoor);
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				const bool bOtherPawn = Actor->IsA<APadelPlayerPawn>() && Actor != In.Pawn;
				const bool bOthers = Actor->GetOwner() && Actor->GetOwner()->IsA<APadelPlayerPawn>() && Actor->GetOwner() != In.Pawn;
				if (bOtherPawn || bOthers || Actor->IsA<APadelBall>())
				{
					Actor->SetActorHiddenInGame(true);
					if (!Actor->GetAttachParentActor())
					{
						Actor->SetActorLocation(FVector(0.0, 0.0, -100000.0));
					}
				}
			}
			AActor* MetaHuman = In.Pawn->GetAnimation()->GetMetaHuman();
			const bool bMetaHuman = Show == EShow::MetaHuman;
			if (bMetaHuman && !MetaHuman)
			{
				Problem = TEXT("no MetaHuman is shown on this machine");
				return;
			}
			In.Driver->SetVisibility(!bMetaHuman);
			if (MetaHuman)
			{
				MetaHuman->SetActorHiddenInGame(!bMetaHuman);
			}
			TInlineComponentArray<UStaticMeshComponent*> Statics(In.Pawn);
			for (UStaticMeshComponent* Part : Statics)
			{
				if (Part->GetFName() == FName(TEXT("Racket")))
				{
					Part->SetHiddenInGame(true);
				}
			}
			// The sample reference's camera: 35 degrees round from side-on, 5.6 m back, 1.55 m up.
			const FVector Ahead = FRotator(0.0, In.StartYawDeg, 0.0).Vector();
			const FVector Side = FRotator(0.0, In.StartYawDeg - 90.0, 0.0).Vector();
			Out = (Side * FMath::Cos(FMath::DegreesToRadians(35.0)) + Ahead * FMath::Sin(FMath::DegreesToRadians(35.0))).GetSafeNormal();

			Camera = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, Params);
			if (!Camera)
			{
				Problem = TEXT("the scene capture didn't spawn");
				return;
			}
			Target = NewObject<UTextureRenderTarget2D>();
			Target->AddToRoot();
			Target->RenderTargetFormat = RTF_RGBA8_SRGB;
			Target->InitAutoFormat(Width, Height);
			Target->UpdateResourceImmediate(true);
			Capture = Camera->GetCaptureComponent2D();
			Capture->TextureTarget = Target;
			Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Capture->FOVAngle = 40.0f;
			Capture->bCaptureEveryFrame = false;
			Capture->bCaptureOnMovement = false;
			Capture->bAlwaysPersistRenderingState = true;
			Capture->ShowFlags.SetMotionBlur(false);
			// The camera stands outside the court, as in the sample reference: the glass, the fence
			// and the stands between it and the player are left out of its view (they still cast shadows).
			if (StageActor)
			{
				TInlineComponentArray<UStaticMeshComponent*> Parts(StageActor);
				for (UStaticMeshComponent* Part : Parts)
				{
					const FString Mesh = Part->GetStaticMesh() ? Part->GetStaticMesh()->GetName() : FString();
					if (Mesh.Contains(TEXT("Glass")) || Mesh.Contains(TEXT("Frame")) || Mesh.Contains(TEXT("CourtMesh")) || Mesh.Contains(TEXT("Stands")) || Mesh.Contains(TEXT("Boards")))
					{
						Capture->HideComponent(Part);
					}
				}
			}
			Follow();
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

		void Follow()
		{
			const FVector At = Pawn->GetActorLocation();
			const FVector LookAt(At.X, At.Y, FloorZ + 85.0);
			const FVector Eye = LookAt + Out * 560.0 + FVector(0.0, 0.0, 70.0);
			Camera->SetActorLocationAndRotation(Eye, (LookAt - Eye).Rotation());
		}

		void Frame(int32 Index)
		{
			if (!bOk)
			{
				return;
			}
			Follow();
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

	FString Body(const FBodyMetrics& B)
	{
		if (!B.bValid)
		{
			return TEXT("not measured");
		}
		return FString::Printf(TEXT("slide %.1f cm/s whole path, %.1f steady (by phase: %s); pelvis / spine_03 / head jerk p95 %.0fk / %.0fk / %.0fk; worst bone turn %.1f deg (%s)"),
			B.SlideCmps, B.SteadySlideCmps, *B.SlideByPhase, B.JerkDegPerS3.FindRef(TEXT("pelvis")).Y / 1000.0, B.JerkDegPerS3.FindRef(TEXT("spine_03")).Y / 1000.0,
			B.JerkDegPerS3.FindRef(TEXT("head")).Y / 1000.0, B.WorstBoneTurnDeg, *B.WorstBoneTurnAt);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPadelSampleVideoTest, "Padel.Animation.SampleVideo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPadelSampleVideoTest::RunTest(const FString& Parameters)
{
	using namespace PadelSampleVideo;
	FString OutDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("PadelSampleVideo="), OutDir))
	{
		AddInfo(TEXT("Skipped: records only with -PadelSampleVideo=<folder> (Tools/RecordSampleVideo.sh)."));
		return true;
	}
	IConsoleVariable* Switch = IConsoleManager::Get().FindConsoleVariable(TEXT("Padel.Animation.SampleLocomotion"));
	if (!Switch)
	{
		AddError(TEXT("No Padel.Animation.SampleLocomotion console variable"));
		return false;
	}
	const int32 Was = Switch->GetInt();
	Switch->Set(1, ECVF_SetByCode);
	ON_SCOPE_EXIT { Switch->Set(Was, ECVF_SetByCode); };
	IFileManager::Get().MakeDirectory(*OutDir, true);
	const bool bRender = FApp::CanEverRender();
	FString Report = TEXT("# Sample locomotion in Padel: the layer videos' path, full stack\n\n");
	for (const EShow Show : { EShow::MetaHuman, EShow::Mannequin })
	{
		const TCHAR* Name = Show == EShow::MetaHuman ? TEXT("metahuman") : TEXT("mannequin");
		FFilm Film;
		Film.Dir = FPaths::Combine(OutDir, Name);
		Film.Show = Show;
		FRunOptions Options;
		Options.Path = EPath::LayerVideo;
		if (bRender)
		{
			Options.OnSetup = [&Film](const FSetup& Setup) { Film.Setup(Setup); };
			Options.OnFrame = [&Film](int32 Frame, const FString&) { Film.Frame(Frame); };
		}
		const FRun Run = PadelMotionProbe::Run(*this, EDrive::FullStack, Options);
		if (!Run.bRan)
		{
			AddWarning(FString::Printf(TEXT("%s: didn't run (%s)"), Name, *Run.SkipReason));
			continue;
		}
		if (bRender && !Film.bOk)
		{
			AddWarning(FString::Printf(TEXT("%s: not filmed (%s)"), Name, *Film.Problem));
		}
		PadelMotionProbe::Report(*this, Name, Run);
		if (Show == EShow::MetaHuman)
		{
			TArray<FString> Lines;
			for (int32 I = 0; I < Run.ClipPerFrame.Num(); I += 5)
			{
				Lines.Add(FString::Printf(TEXT("%d %s: %s"), I, *Run.Phases[I], *Run.ClipPerFrame[I]));
			}
			FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")), *FPaths::Combine(OutDir, TEXT("frames.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
		Report += FString::Printf(TEXT("## %s run\n\n- %d frames (%.1f s), %.2f clip switches/s\n- mannequin driver: %s\n- MetaHuman: %s\n- databases picked: %s\n\n"),
			Name, Run.Phases.Num(), Run.DurationS, Run.SwitchesPerS, *Body(Run.Placeholder), *Body(Run.MetaHuman), *Run.Picks);
	}
	FFileHelper::SaveStringToFile(Report, *FPaths::Combine(OutDir, TEXT("sample.md")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return true;
}

#endif
