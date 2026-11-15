#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PadelShotDebugHUD.generated.h"

/**
 * Developer overlay for the shot system (Agent 3 brief: timing grade and target).
 * Text only, drawn in non-shipping builds; the target markers are drawn in the world
 * by APadelPracticeDirector. The player-facing timing cue (GDD 5.1) is the HUD's job (UI).
 */
UCLASS()
class PADEL_API APadelShotDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
