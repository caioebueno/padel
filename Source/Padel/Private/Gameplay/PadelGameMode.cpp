#include "Gameplay/PadelGameMode.h"

#include "Court/PadelCourt.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerController.h"
#include "PadelLog.h"
#include "Presentation/PadelBroadcastCamera.h"
#include "Presentation/PadelLookDevStage.h"
#include "Presentation/PadelVenueLighting.h"
#include "UI/PadelHUD.h"

APadelGameMode::APadelGameMode()
{
	PlayerControllerClass = APadelPlayerController::StaticClass();
	// Menus, HUD and the match flow (Agent 7).
	HUDClass = APadelHUD::StaticClass();
	// No default pawn: APadelMatchDirector spawns the four players when a match starts.
	DefaultPawnClass = nullptr;
}

void APadelGameMode::StartPlay()
{
	// Spawn before Super::StartPlay so actors exist when BeginPlay is dispatched.
	SpawnPlaceholderStage();
	Super::StartPlay();
}

void APadelGameMode::SpawnPlaceholderStage()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The venue's floodlights, sky and broadcast post-process (GDD 10, Data/Lighting.json).
	APadelVenueLighting* Lighting = nullptr;
	if (TActorIterator<APadelVenueLighting> It(World); It)
	{
		Lighting = *It;
	}
	else
	{
		Lighting = World->SpawnActor<APadelVenueLighting>(APadelVenueLighting::StaticClass(), FTransform::Identity, Params);
	}

	// The court: a level that brings its own keeps it. Otherwise the imported court and venue
	// meshes from Data/LookDev.json, the same set the look-dev level judges, and only when
	// none are imported yet the placeholder boxes (the stage spawns APadelCourt itself).
	if (!TActorIterator<APadelCourt>(World) && !TActorIterator<APadelLookDevStage>(World))
	{
		CourtStage = World->SpawnActor<APadelLookDevStage>(APadelLookDevStage::StaticClass(), FTransform::Identity, Params);
		if (CourtStage)
		{
			CourtStage->Assemble(Lighting ? Lighting->GetActiveVenue() : Padel::Presentation::Lighting::EVenue::Indoor);
			if (Lighting)
			{
				// Indoor and outdoor presets show different venue pieces.
				Lighting->OnPresetApplied.AddUObject(this, &APadelGameMode::OnLightingPresetApplied);
			}
		}
	}

	// The real match: the HUD's Start match asks this actor for one.
	if (bSpawnMatchDirector && !TActorIterator<APadelMatchDirector>(World))
	{
		World->SpawnActor<APadelMatchDirector>(APadelMatchDirector::StaticClass(), FTransform::Identity, Params);
	}

	if (!TActorIterator<APadelBroadcastCamera>(World))
	{
		World->SpawnActor<APadelBroadcastCamera>(APadelBroadcastCamera::StaticClass(),
			APadelBroadcastCamera::GetDefaultTransform(), Params);
	}
}

void APadelGameMode::OnLightingPresetApplied(const FString& PresetName)
{
	UWorld* World = GetWorld();
	if (CourtStage && World)
	{
		if (TActorIterator<APadelVenueLighting> It(World); It)
		{
			CourtStage->Assemble(It->GetActiveVenue());
		}
	}
}
