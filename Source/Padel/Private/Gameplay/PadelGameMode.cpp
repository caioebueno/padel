#include "Gameplay/PadelGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Court/PadelCourt.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/PadelPlayerController.h"
#include "PadelLog.h"
#include "Presentation/PadelBroadcastCamera.h"
#include "UI/PadelHUD.h"

APadelGameMode::APadelGameMode()
{
	PlayerControllerClass = APadelPlayerController::StaticClass();
	// Menus, HUD and the match flow (Agent 7).
	HUDClass = APadelHUD::StaticClass();
	// No pawn yet: the player watches from the broadcast camera until Agent 3 adds the player.
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

		// Warm overhead light (GDD 10) plus fill. Movable, since nothing is baked.
		if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
			FTransform(FRotator(-60.0, 25.0, 0.0)), Params))
		{
			UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(6.0f);
			Light->SetLightColor(FLinearColor(1.0f, 0.95f, 0.88f));
		}
		if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity, Params))
		{
			USkyLightComponent* Light = Sky->GetLightComponent();
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(1.0f);
			Light->RecaptureSky();
		}
	}

	if (!TActorIterator<APadelBroadcastCamera>(World))
	{
		World->SpawnActor<APadelBroadcastCamera>(APadelBroadcastCamera::StaticClass(),
			APadelBroadcastCamera::GetDefaultTransform(), Params);
	}
}
