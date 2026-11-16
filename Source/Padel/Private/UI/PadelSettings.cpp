#include "UI/PadelSettings.h"

#include "Engine/Engine.h"
#include "UI/PadelUiTokens.h"

using Padel::Ui::ESetting;
using Padel::Ui::FSettingsValues;

UPadelSettings::UPadelSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One source for the defaults: the settings model (spec 7.7's bold values).
	const FSettingsValues Defaults;
	MasterVolume = Defaults.Get(ESetting::MasterVolume);
	EffectsVolume = Defaults.Get(ESetting::EffectsVolume);
	CrowdVolume = Defaults.Get(ESetting::CrowdVolume);
	MenuVolume = Defaults.Get(ESetting::MenuVolume);
	TextSize = Defaults.Get(ESetting::TextSize);
	bReduceMotion = Defaults.ReduceMotion();
	bReduceTransparency = Defaults.ReduceTransparency();

	const Padel::Ui::FMatchSetup Setup;
	SetupDifficulty = static_cast<int32>(Setup.Difficulty);
	SetupSetsToWin = Setup.SetsToWin;
	bSetupGoldenPoint = Setup.bGoldenPoint;
	SetupHumanPlayerId = Setup.HumanPlayerId;
}

UPadelSettings* UPadelSettings::Get()
{
	if (GEngine)
	{
		if (UPadelSettings* Settings = Cast<UPadelSettings>(GEngine->GetGameUserSettings()))
		{
			return Settings;
		}
	}
	// Without the DefaultEngine.ini registration (or before the engine is up) the class
	// default still holds and saves the values.
	return GetMutableDefault<UPadelSettings>();
}

FOnPadelVolumesChanged& UPadelSettings::OnVolumesChanged()
{
	static FOnPadelVolumesChanged Delegate;
	return Delegate;
}

FSettingsValues UPadelSettings::GetMenuValues() const
{
	FSettingsValues Values;
	Values.Set(ESetting::MasterVolume, MasterVolume);
	Values.Set(ESetting::EffectsVolume, EffectsVolume);
	Values.Set(ESetting::CrowdVolume, CrowdVolume);
	Values.Set(ESetting::MenuVolume, MenuVolume);
	Values.Set(ESetting::TextSize, TextSize);
	Values.Set(ESetting::ReduceMotion, bReduceMotion ? 1 : 0);
	Values.Set(ESetting::ReduceTransparency, bReduceTransparency ? 1 : 0);
	return Values;
}

void UPadelSettings::SetMenuValues(const FSettingsValues& Values)
{
	const FSettingsValues Before = GetMenuValues();
	MasterVolume = Values.Get(ESetting::MasterVolume);
	EffectsVolume = Values.Get(ESetting::EffectsVolume);
	CrowdVolume = Values.Get(ESetting::CrowdVolume);
	MenuVolume = Values.Get(ESetting::MenuVolume);
	TextSize = Values.Get(ESetting::TextSize);
	bReduceMotion = Values.ReduceMotion();
	bReduceTransparency = Values.ReduceTransparency();

	const bool bVolumes = Before.Get(ESetting::MasterVolume) != MasterVolume || Before.Get(ESetting::EffectsVolume) != EffectsVolume
		|| Before.Get(ESetting::CrowdVolume) != CrowdVolume || Before.Get(ESetting::MenuVolume) != MenuVolume;
	if (bVolumes)
	{
		// W15: a sound mix with one class per slider sets its volume here, and plays a
		// sound of that class at the new level (spec 7.7).
		OnVolumesChanged().Broadcast(Values);
	}
	ApplyMenuOptions();
}

void UPadelSettings::ApplyMenuOptions() const
{
	const FSettingsValues Values = GetMenuValues();
	PadelUi::FUiOptions Options = PadelUi::Options();
	const PadelUi::FUiOptions Old = Options;
	Options.TextSize = Values.GetTextSize();
	Options.bReduceMotion = Values.ReduceMotion();
	Options.bReduceTransparency = Values.ReduceTransparency();
	// SetOptions rebuilds the open screen, so only call it for a real change.
	if (Options.TextSize != Old.TextSize || Options.bReduceMotion != Old.bReduceMotion
		|| Options.bReduceTransparency != Old.bReduceTransparency)
	{
		PadelUi::SetOptions(Options);
	}
}

bool UPadelSettings::LoadMatchSetup(Padel::Ui::FMatchSetup& Setup) const
{
	if (!bHasMatchSetup)
	{
		return false;
	}
	Setup.Difficulty = static_cast<Padel::Ui::EDifficulty>(FMath::Clamp(SetupDifficulty, 0, 2));
	Setup.SetsToWin = SetupSetsToWin == 1 ? 1 : 2;
	Setup.bGoldenPoint = bSetupGoldenPoint;
	// Team A's players are ids 0 and 1 (FMatchSetup::PartnerId).
	Setup.HumanPlayerId = FMath::Clamp(SetupHumanPlayerId, 0, 1);
	return true;
}

void UPadelSettings::StoreMatchSetup(const Padel::Ui::FMatchSetup& Setup)
{
	bHasMatchSetup = true;
	SetupDifficulty = static_cast<int32>(Setup.Difficulty);
	SetupSetsToWin = Setup.SetsToWin;
	bSetupGoldenPoint = Setup.bGoldenPoint;
	SetupHumanPlayerId = Setup.HumanPlayerId;
}

void UPadelSettings::SaveMenuSettings()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// The class default saves to the same section; SaveSettings expects an instance.
		SaveConfig(CPF_Config, *GGameUserSettingsIni);
		return;
	}
	SaveSettings();
}
