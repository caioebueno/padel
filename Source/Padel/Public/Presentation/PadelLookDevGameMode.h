#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PadelLookDevGameMode.generated.h"

class ACameraActor;
class APadelLookDevStage;
class APadelVenueLighting;

/**
 * The shared look-dev level, built in code so it can be reviewed and never goes stale
 * (docs/decisions/0006-lighting-and-venue.md). Open any map with
 * ?game=LookDev (Tools/RunLookDev.sh does it): it spawns the venue lighting, the stage
 * with all five court parts and the venue, and a camera on the look-dev bookmarks.
 *
 * Console: Padel.LookDev.Preset <name>, Padel.LookDev.NextPreset,
 *          Padel.LookDev.Camera <name or index>, Padel.LookDev.NextCamera, Padel.LookDev.Report
 */
UCLASS()
class PADEL_API APadelLookDevGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APadelLookDevGameMode();

	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	bool SetPreset(const FString& Name);
	void NextPreset();
	/** Looks through a bookmark by name, or by index when Key is a number. */
	bool SetCamera(const FString& Key);
	void NextCamera();
	void Report() const;

	APadelVenueLighting* GetLighting() const { return Lighting; }
	APadelLookDevStage* GetStage() const { return Stage; }
	ACameraActor* GetCamera() const { return Camera; }
	int32 GetCameraIndex() const { return CameraIndex; }

private:
	void SpawnStage();
	void ApplyCamera(int32 Index);
	void OnPresetApplied(const FString& PresetName);

	UPROPERTY(Transient)
	TObjectPtr<APadelVenueLighting> Lighting;

	UPROPERTY(Transient)
	TObjectPtr<APadelLookDevStage> Stage;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	int32 CameraIndex = 0;
};
