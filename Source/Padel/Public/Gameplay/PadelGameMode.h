#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PadelGameMode.generated.h"

/**
 * Match game mode. It makes any map playable: if the level has no court, it spawns
 * the placeholder court, lights and the broadcast camera, and it always makes sure
 * there is one APadelMatchDirector, which plays the real match when the HUD starts one.
 */
UCLASS()
class PADEL_API APadelGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APadelGameMode();

	virtual void StartPlay() override;

protected:
	/** False in modes that run their own simulation (practice). */
	UPROPERTY(EditDefaultsOnly, Category = "Padel")
	bool bSpawnMatchDirector = true;

private:
	void SpawnPlaceholderStage();
};
