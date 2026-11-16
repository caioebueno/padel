#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/Model/PadelUiMatchSetup.h"
#include "UI/Model/PadelUiSettings.h"
#include "PadelSettings.generated.h"

/** The volumes changed (W15 binds sound classes to it; nothing plays them yet). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPadelVolumesChanged, const Padel::Ui::FSettingsValues& /*Values*/);

/**
 * What the game keeps between runs (docs/ui/MENU_SPEC.md 11.3): the Settings screen's
 * values (7.7) and the last Quick Match setup (difficulty, format, golden point, player),
 * in GameUserSettings.ini. Nothing else is saved in the slice.
 *
 * Registered as the engine's GameUserSettingsClassName (Config/DefaultEngine.ini), so it
 * is GEngine->GetGameUserSettings(). Changes apply at once (Text size, Reduce motion and
 * Reduce transparency through PadelUi::SetOptions; volumes through OnVolumesChanged) and
 * are written when the Settings screen closes or a match starts.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class PADEL_API UPadelSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UPadelSettings(const FObjectInitializer& ObjectInitializer);

	/** The engine's settings object, or this class's default object if another class is registered. */
	static UPadelSettings* Get();

	/** The 7.7 values, clamped into range. */
	Padel::Ui::FSettingsValues GetMenuValues() const;
	/** Store and apply: menu options at once, volumes through OnVolumesChanged. Doesn't write the file. */
	void SetMenuValues(const Padel::Ui::FSettingsValues& Values);
	/** Push Text size, Reduce motion and Reduce transparency to the menus (start-up, after a load). */
	void ApplyMenuOptions() const;

	/** The saved Quick Match setup into Setup. False (Setup untouched) if none was saved yet. */
	bool LoadMatchSetup(Padel::Ui::FMatchSetup& Setup) const;
	void StoreMatchSetup(const Padel::Ui::FMatchSetup& Setup);

	/** Write GameUserSettings.ini. */
	void SaveMenuSettings();

	static FOnPadelVolumesChanged& OnVolumesChanged();

private:
	// Settings (spec 7.7). Defaults come from Padel::Ui::FSettingsValues.
	UPROPERTY(config)
	int32 MasterVolume = 0;

	UPROPERTY(config)
	int32 EffectsVolume = 0;

	UPROPERTY(config)
	int32 CrowdVolume = 0;

	UPROPERTY(config)
	int32 MenuVolume = 0;

	/** 0 Default, 1 Large, 2 Largest. */
	UPROPERTY(config)
	int32 TextSize = 0;

	UPROPERTY(config)
	bool bReduceMotion = false;

	UPROPERTY(config)
	bool bReduceTransparency = false;

	// The last Quick Match setup (spec 11.3).
	UPROPERTY(config)
	bool bHasMatchSetup = false;

	/** 0 Easy, 1 Medium, 2 Hard. */
	UPROPERTY(config)
	int32 SetupDifficulty = 1;

	UPROPERTY(config)
	int32 SetupSetsToWin = 2;

	UPROPERTY(config)
	bool bSetupGoldenPoint = true;

	UPROPERTY(config)
	int32 SetupHumanPlayerId = 1;
};
