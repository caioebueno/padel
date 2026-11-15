#pragma once

#include "CoreMinimal.h"
#include "Gameplay/PadelGameMode.h"
#include "PadelPracticeGameMode.generated.h"

/**
 * Ball machine practice: the placeholder court and camera from APadelGameMode, plus
 * APadelPracticeDirector (the human's player, the machine and the stand-in ball) and
 * the shot debug overlay. Start it with a game mode override on the URL:
 *   UnrealEditor Padel.uproject "/Engine/Maps/Entry?game=/Script/Padel.PadelPracticeGameMode" -game
 * or set it as the GameMode Override in a level's World Settings.
 */
UCLASS()
class PADEL_API APadelPracticeGameMode : public APadelGameMode
{
	GENERATED_BODY()

public:
	APadelPracticeGameMode();

	virtual void StartPlay() override;
};
