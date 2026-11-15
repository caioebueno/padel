#include "Gameplay/PadelPracticeGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/PadelPracticeDirector.h"
#include "Gameplay/PadelShotDebugHUD.h"

APadelPracticeGameMode::APadelPracticeGameMode()
{
	HUDClass = APadelShotDebugHUD::StaticClass();
	// The director spawns and possesses the player once the controller is ready.
	DefaultPawnClass = nullptr;
}

void APadelPracticeGameMode::StartPlay()
{
	if (UWorld* World = GetWorld(); World && !TActorIterator<APadelPracticeDirector>(World))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<APadelPracticeDirector>(APadelPracticeDirector::StaticClass(), FTransform::Identity, Params);
	}
	Super::StartPlay();
}
