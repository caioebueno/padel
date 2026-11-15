#include "Gameplay/PadelPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Gameplay/PadelInputConfig.h"
#include "Presentation/PadelBroadcastCamera.h"

void APadelPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	InputConfig = NewObject<UPadelInputConfig>(this);
	InputConfig->Build();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(InputConfig->GetMatchContext(), 0);
		}
	}

	if (TActorIterator<APadelBroadcastCamera> It(GetWorld()); It)
	{
		SetViewTarget(*It);
	}
}
