#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PadelMatchAudio.generated.h"

class USoundBase;

/** The sounds the match events trigger (GDD 10 audio: glass and mesh must sound different). */
UENUM()
enum class EPadelMatchSound : uint8
{
	RacketHit,
	BounceGround,
	BounceGlass,
	BounceMesh,
	BounceNet,
	CrowdPoint,
	CrowdBigPoint,
	Whistle,
};

/**
 * Event-to-sound hookup. APadelHUD calls Play from the match events; the sounds
 * themselves are picked in a thin Blueprint subclass (CLAUDE.md: Blueprints only pick
 * assets). With no sound assigned it does nothing, so the slice runs silent until the
 * audio assets land.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class PADEL_API UPadelMatchAudio : public UObject
{
	GENERATED_BODY()

public:
	/** Plays at a world location (cm), or as a 2D sound when bAtLocation is false. */
	void Play(EPadelMatchSound Sound, const FVector& WorldLocation, bool bAtLocation = true) const;

	/** The sound for a GDD 3.4 surface name ("ground", "glass", "mesh", "net"). */
	static EPadelMatchSound SoundForSurface(const FString& Surface);

	UPROPERTY(EditDefaultsOnly, Category = "Padel|Audio")
	TMap<EPadelMatchSound, TObjectPtr<USoundBase>> Sounds;
};
