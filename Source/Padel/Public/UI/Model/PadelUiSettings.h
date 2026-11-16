// The Settings screen's values (docs/ui/MENU_SPEC.md 7.7): which settings exist in the
// slice, their tabs, ranges and defaults, and the rules for "is this tab at its defaults"
// (the X Reset hint shows only then) and resetting a tab. UPadelSettings saves these
// values; the Settings screen edits them.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include "UI/Model/PadelUiTheme.h"

#include <array>
#include <vector>

namespace Padel::Ui
{
	// Spec 7.7: Audio and Accessibility in the slice (Gameplay only if time allows).
	enum class ESettingsTab
	{
		Audio,
		Accessibility,
		Count,
	};

	enum class ESetting
	{
		MasterVolume,       // Set.Vol.Master
		EffectsVolume,      // Set.Vol.Effects ("Ball and court")
		CrowdVolume,        // Set.Vol.Crowd
		MenuVolume,         // Set.Vol.Ui ("Menu sounds"): only once menu sounds exist (W10)
		TextSize,           // Set.TextSize: 0 Default, 1 Large, 2 Largest
		ReduceMotion,       // Set.ReduceMotion: 0 or 1
		ReduceTransparency, // Set.ReduceTransparency: 0 or 1
		Count,
	};

	enum class ESettingControl
	{
		Slider,
		Selector,
		Toggle,
	};

	struct FSettingInfo
	{
		ESetting Setting = ESetting::MasterVolume;
		ESettingsTab Tab = ESettingsTab::Audio;
		ESettingControl Control = ESettingControl::Slider;
		int Min = 0;
		int Max = 100;
		int Default = 0;
		// The spec's id, e.g. "Set.Vol.Master"; also the element id on the Settings screen.
		const char* Id = "";
	};

	const FSettingInfo& SettingInfo(ESetting Setting);
	// Spec 7.7's tab names, as LOCTEXT keys use them ("Audio", "Accessibility").
	const char* SettingsTabName(ESettingsTab Tab);

	// The rows of a tab, top to bottom. Menu sounds is left out until menu sounds exist
	// (spec 7.7), so bMenuSounds says whether they do.
	std::vector<ESetting> TabSettings(ESettingsTab Tab, bool bMenuSounds);

	struct FSettingsValues
	{
		FSettingsValues();

		int Get(ESetting Setting) const;
		// Clamped to the setting's range. True if the value changed.
		bool Set(ESetting Setting, int Value);
		bool IsDefault(ESetting Setting) const;
		// Every row of the tab at its default: the X Reset hint is hidden (spec 6.1, 7.7).
		bool IsTabDefault(ESettingsTab Tab, bool bMenuSounds) const;
		// X Reset, after its dialog: every setting on the tab goes back to its default,
		// including a row that isn't shown. True if anything changed.
		bool ResetTab(ESettingsTab Tab);
		// Clamp every value into range (a hand-edited or old GameUserSettings.ini).
		void Sanitise();

		// A volume as 0..1 for a sound class (W15).
		float Volume(ESetting Setting) const;
		ETextSize GetTextSize() const;
		bool ReduceMotion() const { return Get(ESetting::ReduceMotion) != 0; }
		bool ReduceTransparency() const { return Get(ESetting::ReduceTransparency) != 0; }

		bool operator==(const FSettingsValues& Other) const { return Values == Other.Values; }
		bool operator!=(const FSettingsValues& Other) const { return Values != Other.Values; }

	private:
		std::array<int, static_cast<size_t>(ESetting::Count)> Values{};
	};
}
