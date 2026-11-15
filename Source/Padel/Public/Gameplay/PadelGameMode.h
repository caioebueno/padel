#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PadelGameMode.generated.h"

/**
 * Match game mode. For now it makes any map playable: if the level has no court,
 * it spawns the placeholder court, lights and the broadcast camera, so pressing
 * Play on an empty level shows the court. Players, ball and rules come later.
 */
UCLASS()
class PADEL_API APadelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APadelGameMode();

	virtual void StartPlay() override;

private:
	void SpawnPlaceholderStage();
};
