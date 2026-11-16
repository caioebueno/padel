#include "Presentation/PadelLookDevGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Court/PadelCourtUnits.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "PadelLog.h"
#include "Presentation/PadelLookDevStage.h"
#include "Presentation/PadelVenueLighting.h"

namespace
{
	APadelLookDevGameMode* LookDevMode(UWorld* World)
	{
		APadelLookDevGameMode* Mode = World ? World->GetAuthGameMode<APadelLookDevGameMode>() : nullptr;
		if (!Mode)
		{
			UE_LOG(LogPadel, Warning, TEXT("Padel.LookDev.* needs the look-dev game mode (open a map with ?game=LookDev)."));
		}
		return Mode;
	}

	// Console commands rather than input actions: this is a dev tool, and the input
	// bindings are a shared contract (CLAUDE.md).
	FAutoConsoleCommandWithWorldAndArgs PresetCommand(
		TEXT("Padel.LookDev.Preset"),
		TEXT("Apply a lighting preset from Data/Lighting.json: Padel.LookDev.Preset OutdoorDay"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (APadelLookDevGameMode* Mode = LookDevMode(World))
			{
				Mode->SetPreset(Args.IsEmpty() ? FString() : Args[0]);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs NextPresetCommand(
		TEXT("Padel.LookDev.NextPreset"),
		TEXT("Apply the next lighting preset in Data/Lighting.json."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (APadelLookDevGameMode* Mode = LookDevMode(World))
			{
				Mode->NextPreset();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CameraCommand(
		TEXT("Padel.LookDev.Camera"),
		TEXT("Look through a Data/LookDev.json camera, by name or index: Padel.LookDev.Camera NetSide"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (APadelLookDevGameMode* Mode = LookDevMode(World))
			{
				Mode->SetCamera(Args.IsEmpty() ? FString(TEXT("0")) : Args[0]);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs NextCameraCommand(
		TEXT("Padel.LookDev.NextCamera"),
		TEXT("Look through the next Data/LookDev.json camera."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (APadelLookDevGameMode* Mode = LookDevMode(World))
			{
				Mode->NextCamera();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ReportCommand(
		TEXT("Padel.LookDev.Report"),
		TEXT("Log which court parts and venue meshes are in place, and the lighting preset."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (APadelLookDevGameMode* Mode = LookDevMode(World))
			{
				Mode->Report();
			}
		}));
}

APadelLookDevGameMode::APadelLookDevGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
}

void APadelLookDevGameMode::StartPlay()
{
	// Before Super::StartPlay, so the lighting's BeginPlay (which applies the first preset
	// and tells the stage which venue to build) finds everything bound.
	SpawnStage();
	Super::StartPlay();
	ApplyCamera(CameraIndex);
}

void APadelLookDevGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (NewPlayer && Camera)
	{
		NewPlayer->SetViewTarget(Camera);
	}
}

void APadelLookDevGameMode::SpawnStage()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Stage = World->SpawnActor<APadelLookDevStage>(APadelLookDevStage::StaticClass(), FTransform::Identity, Params);
	Lighting = World->SpawnActor<APadelVenueLighting>(APadelVenueLighting::StaticClass(), FTransform::Identity, Params);
	Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}
	if (Lighting)
	{
		Lighting->OnPresetApplied.AddUObject(this, &APadelLookDevGameMode::OnPresetApplied);
	}
}

void APadelLookDevGameMode::OnPresetApplied(const FString& PresetName)
{
	if (Stage && Lighting)
	{
		Stage->Assemble(Lighting->GetActiveVenue());
	}
}

bool APadelLookDevGameMode::SetPreset(const FString& Name)
{
	return Lighting && Lighting->ApplyPreset(Name);
}

void APadelLookDevGameMode::NextPreset()
{
	if (Lighting)
	{
		Lighting->ApplyNextPreset();
	}
}

bool APadelLookDevGameMode::SetCamera(const FString& Key)
{
	if (!Stage)
	{
		return false;
	}
	const TArray<FPadelLookDevCamera>& Cameras = Stage->GetCameras();
	if (Key.IsNumeric())
	{
		const int32 Index = FCString::Atoi(*Key);
		if (Cameras.IsValidIndex(Index))
		{
			ApplyCamera(Index);
			return true;
		}
	}
	for (int32 Index = 0; Index < Cameras.Num(); ++Index)
	{
		if (Cameras[Index].Name.Equals(Key, ESearchCase::IgnoreCase))
		{
			ApplyCamera(Index);
			return true;
		}
	}
	UE_LOG(LogPadel, Warning, TEXT("No look-dev camera %s in Data/LookDev.json."), *Key);
	return false;
}

void APadelLookDevGameMode::NextCamera()
{
	if (Stage && !Stage->GetCameras().IsEmpty())
	{
		ApplyCamera((CameraIndex + 1) % Stage->GetCameras().Num());
	}
}

void APadelLookDevGameMode::ApplyCamera(int32 Index)
{
	if (!Stage || !Camera || !Stage->GetCameras().IsValidIndex(Index))
	{
		return;
	}
	CameraIndex = Index;
	const FPadelLookDevCamera& Bookmark = Stage->GetCameras()[Index];
	const FVector Location = Padel::MetresToWorld(Bookmark.LocationM);
	const FVector Target = Padel::MetresToWorld(Bookmark.LookAtM);
	Camera->SetActorLocationAndRotation(Location, UKismetMathLibrary::FindLookAtRotation(Location, Target));
	Camera->GetCameraComponent()->SetFieldOfView(Bookmark.FovDeg);
	if (APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		Player->SetViewTarget(Camera);
	}
	UE_LOG(LogPadel, Log, TEXT("Look-dev camera %d: %s."), Index, *Bookmark.Name);
}

void APadelLookDevGameMode::Report() const
{
	if (Lighting)
	{
		UE_LOG(LogPadel, Log, TEXT("Lighting preset %s (presets: %s)."), *Lighting->GetActivePresetName(),
			*FString::Join(Lighting->GetPresetNames(), TEXT(", ")));
	}
	if (Stage)
	{
		Stage->LogReport();
	}
}
