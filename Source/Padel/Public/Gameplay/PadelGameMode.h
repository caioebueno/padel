#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PadelGameMode.generated.h"

class APadelLookDevStage;

/**
 * Match game mode. It makes any map playable: if the level has no court, it places the
 * imported court and venue meshes (APadelLookDevStage, with the placeholder court until they
 * are imported), the venue lighting and the broadcast camera, and it always makes sure
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
	void OnLightingPresetApplied(const FString& PresetName);

	/** The imported court and venue meshes (or the placeholder boxes until they're imported). */
	UPROPERTY(Transient)
	TObjectPtr<APadelLookDevStage> CourtStage;
};
