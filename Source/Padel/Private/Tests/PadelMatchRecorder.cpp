// Records a stretch of a running match to PNG frames from two cameras: the broadcast camera
// (the player's view) and a side camera following one player from a three-quarter angle. For
// judging animation in real rallies (docs/decisions/0014-sample-locomotion.md); not a game feature.
//
// Console (development builds): Padel.Record.Clip <folder> <seconds> [player id, default 0]
// Run the game with -benchmark -fps=60 so the clip steps at a fixed 60 Hz; Tools/RecordMatchClip.sh
// starts a four-AI match, records and quits.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerPawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "Presentation/PadelLookDevStage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Tickable.h"

namespace PadelMatchRecorder
{
	class FRecorder : public FTickableGameObject
	{
	public:
		FRecorder(UWorld* InWorld, const FString& InDir, double InSeconds, int32 InPlayer)
			: World(InWorld), Dir(InDir), Seconds(InSeconds), Player(InPlayer)
		{
			for (int32 View = 0; View < 2; ++View)
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Cameras[View] = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, Params);
				Targets[View] = NewObject<UTextureRenderTarget2D>();
				Targets[View]->AddToRoot();
				Targets[View]->RenderTargetFormat = RTF_RGBA8_SRGB;
				Targets[View]->InitAutoFormat(1280, 720);
				Targets[View]->UpdateResourceImmediate(true);
				USceneCaptureComponent2D* Capture = Cameras[View]->GetCaptureComponent2D();
				Capture->TextureTarget = Targets[View];
				Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
				Capture->bCaptureEveryFrame = false;
				Capture->bCaptureOnMovement = false;
				Capture->bAlwaysPersistRenderingState = true;
				Capture->ShowFlags.SetMotionBlur(false);
				IFileManager::Get().MakeDirectory(*FPaths::Combine(Dir, View == 0 ? TEXT("broadcast") : TEXT("side")), true);
			}
			// The side camera stands at the court's side: the glass, fence and stands between it and
			// the player are left out of its view (they still cast shadows).
			for (TActorIterator<APadelLookDevStage> It(InWorld); It; ++It)
			{
				TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
				for (UStaticMeshComponent* Part : Parts)
				{
					const FString Mesh = Part->GetStaticMesh() ? Part->GetStaticMesh()->GetName() : FString();
					if (Mesh.Contains(TEXT("Glass")) || Mesh.Contains(TEXT("Frame")) || Mesh.Contains(TEXT("CourtMesh")) || Mesh.Contains(TEXT("Stands")) || Mesh.Contains(TEXT("Boards")))
					{
						Cameras[1]->GetCaptureComponent2D()->HideComponent(Part);
					}
				}
			}
			UE_LOG(LogPadel, Display, TEXT("Padel.Record.Clip: %.0f s into %s, side camera on player %d"), Seconds, *Dir, Player);
		}

		virtual ~FRecorder() override
		{
			for (UTextureRenderTarget2D* Target : Targets)
			{
				if (Target)
				{
					Target->RemoveFromRoot();
				}
			}
		}

		virtual void Tick(float DeltaTime) override
		{
			if (bDone || !World.IsValid())
			{
				return;
			}
			// The broadcast view: whatever the player's camera shows.
			if (APlayerController* Controller = UGameplayStatics::GetPlayerController(World.Get(), 0))
			{
				if (Controller->PlayerCameraManager)
				{
					const FMinimalViewInfo View = Controller->PlayerCameraManager->GetCameraCacheView();
					Cameras[0]->SetActorLocationAndRotation(View.Location, View.Rotation);
					Cameras[0]->GetCaptureComponent2D()->FOVAngle = View.FOV;
				}
			}
			// The side view: three-quarters on from the court's long side, following the player.
			APadelPlayerPawn* Pawn = nullptr;
			for (TActorIterator<APadelMatchDirector> It(World.Get()); It; ++It)
			{
				Pawn = It->GetPawn(Player);
			}
			if (Pawn)
			{
				const FVector At = Pawn->GetActorLocation();
				const FVector LookAt(At.X, At.Y, 100.0);
				const double Side = At.Y >= 0.0 ? 1.0 : -1.0;
				const FVector Eye = LookAt + FVector(FMath::Sign(At.X) * 250.0, Side * 520.0, 90.0);
				Cameras[1]->SetActorLocationAndRotation(Eye, (LookAt - Eye).Rotation());
				Cameras[1]->GetCaptureComponent2D()->FOVAngle = 45.0f;
			}
			for (int32 View = 0; View < 2; ++View)
			{
				Cameras[View]->GetCaptureComponent2D()->CaptureScene();
				FTextureRenderTargetResource* Resource = Targets[View]->GameThread_GetRenderTargetResource();
				if (Resource && Resource->ReadPixels(Pixels))
				{
					for (FColor& C : Pixels)
					{
						C.A = 255;
					}
					FImageUtils::SaveImageByExtension(*FPaths::Combine(Dir, View == 0 ? TEXT("broadcast") : TEXT("side"), FString::Printf(TEXT("frame_%05d.png"), Frame)),
						FImageView(Pixels.GetData(), 1280, 720));
				}
			}
			++Frame;
			if (Frame >= FMath::RoundToInt(Seconds * 60.0))
			{
				bDone = true;
				UE_LOG(LogPadel, Display, TEXT("Padel.Record.Clip: %d frames written, quitting"), Frame);
				FPlatformMisc::RequestExit(false);
			}
		}

		virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FPadelMatchRecorder, STATGROUP_Tickables); }
		virtual bool IsTickable() const override { return !bDone; }
		virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

	private:
		TWeakObjectPtr<UWorld> World;
		FString Dir;
		double Seconds = 30.0;
		int32 Player = 0;
		int32 Frame = 0;
		bool bDone = false;
		ASceneCapture2D* Cameras[2] = { nullptr, nullptr };
		UTextureRenderTarget2D* Targets[2] = { nullptr, nullptr };
		TArray<FColor> Pixels;
	};

	TUniquePtr<FRecorder> Active;

	FAutoConsoleCommandWithWorldAndArgs RecordCommand(
		TEXT("Padel.Record.Clip"),
		TEXT("Padel.Record.Clip <folder> <seconds> [player id]: write the broadcast view and a side view of one player as PNG frames, then quit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.IsEmpty())
			{
				return;
			}
			const double Seconds = Args.Num() > 1 ? FCString::Atod(*Args[1]) : 30.0;
			const int32 Player = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0;
			Active = MakeUnique<FRecorder>(World, Args[0], Seconds, Player);
		}));
}

#endif
