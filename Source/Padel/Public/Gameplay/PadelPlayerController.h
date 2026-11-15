#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PadelPlayerController.generated.h"

class UPadelInputConfig;

/**
 * Adds IMC_Match for the local player and views the match through the broadcast
 * camera. Binding actions to movement and shots is the controls task (Agent 3):
 * bind to GetInputConfig()->GetAction(...) in SetupInputComponent or the pawn.
 */
UCLASS()
class PADEL_API APadelPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UPadelInputConfig* GetInputConfig() const { return InputConfig; }

private:
	UPROPERTY()
	TObjectPtr<UPadelInputConfig> InputConfig;
};
