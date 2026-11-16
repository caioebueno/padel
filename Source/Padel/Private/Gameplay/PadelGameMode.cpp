#include "Gameplay/PadelGameMode.h"

#include "Court/PadelCourt.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/PadelMatchDirector.h"
#include "Gameplay/PadelPlayerController.h"
#include "PadelLog.h"
#include "Presentation/PadelBroadcastCamera.h"
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

	if (!TActorIterator<APadelCourt>(World))
	{
		World->SpawnActor<APadelCourt>(APadelCourt::StaticClass(), FTransform::Identity, Params);
		UE_LOG(LogPadel, Log, TEXT("No court in the level: spawned the placeholder court."));
	}

	// The venue's floodlights, sky and broadcast post-process (GDD 10, Data/Lighting.json).
	if (!TActorIterator<APadelVenueLighting>(World))
	{
		World->SpawnActor<APadelVenueLighting>(APadelVenueLighting::StaticClass(), FTransform::Identity, Params);
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
