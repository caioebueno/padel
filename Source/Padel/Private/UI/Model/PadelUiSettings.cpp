#include "UI/Model/PadelUiSettings.h"

#include <algorithm>

namespace Padel::Ui
{
	namespace
	{
		// Spec 7.7's tables: ranges and the bold defaults.
		constexpr std::array<FSettingInfo, static_cast<size_t>(ESetting::Count)> SettingsTable{{
			{ESetting::MasterVolume, ESettingsTab::Audio, ESettingControl::Slider, 0, 100, 80, "Set.Vol.Master"},
			{ESetting::EffectsVolume, ESettingsTab::Audio, ESettingControl::Slider, 0, 100, 100, "Set.Vol.Effects"},
			{ESetting::CrowdVolume, ESettingsTab::Audio, ESettingControl::Slider, 0, 100, 70, "Set.Vol.Crowd"},
			{ESetting::MenuVolume, ESettingsTab::Audio, ESettingControl::Slider, 0, 100, 70, "Set.Vol.Ui"},
			{ESetting::TextSize, ESettingsTab::Accessibility, ESettingControl::Selector, 0, 2, 0, "Set.TextSize"},
			{ESetting::ReduceMotion, ESettingsTab::Accessibility, ESettingControl::Toggle, 0, 1, 0, "Set.ReduceMotion"},
			{ESetting::ReduceTransparency, ESettingsTab::Accessibility, ESettingControl::Toggle, 0, 1, 0, "Set.ReduceTransparency"},
		}};

		size_t SettingsIndex(ESetting Setting)
		{
			return std::min(static_cast<size_t>(Setting), SettingsTable.size() - 1);
		}
	}

	const FSettingInfo& SettingInfo(ESetting Setting)
	{
		return SettingsTable[SettingsIndex(Setting)];
	}

	const char* SettingsTabName(ESettingsTab Tab)
	{
		switch (Tab)
		{
		case ESettingsTab::Audio:
			return "Audio";
		case ESettingsTab::Accessibility:
			return "Accessibility";
		case ESettingsTab::Count:
			break;
		}
		return "";
	}

	std::vector<ESetting> TabSettings(ESettingsTab Tab, bool bMenuSounds)
	{
		std::vector<ESetting> Rows;
		for (const FSettingInfo& Info : SettingsTable)
		{
			if (Info.Tab != Tab || (Info.Setting == ESetting::MenuVolume && !bMenuSounds))
			{
				continue;
			}
			Rows.push_back(Info.Setting);
		}
		return Rows;
	}

	FSettingsValues::FSettingsValues()
	{
		for (const FSettingInfo& Info : SettingsTable)
		{
			Values[SettingsIndex(Info.Setting)] = Info.Default;
		}
	}

	int FSettingsValues::Get(ESetting Setting) const
	{
		return Values[SettingsIndex(Setting)];
	}

	bool FSettingsValues::Set(ESetting Setting, int Value)
	{
		const FSettingInfo& Info = SettingInfo(Setting);
		const int Clamped = std::clamp(Value, Info.Min, Info.Max);
		int& Slot = Values[SettingsIndex(Setting)];
		if (Slot == Clamped)
		{
			return false;
		}
		Slot = Clamped;
		return true;
	}

	bool FSettingsValues::IsDefault(ESetting Setting) const
	{
		return Get(Setting) == SettingInfo(Setting).Default;
	}

	bool FSettingsValues::IsTabDefault(ESettingsTab Tab, bool bMenuSounds) const
	{
		for (ESetting Setting : TabSettings(Tab, bMenuSounds))
		{
			if (!IsDefault(Setting))
			{
				return false;
			}
		}
		return true;
	}

	bool FSettingsValues::ResetTab(ESettingsTab Tab)
	{
		bool bChanged = false;
		for (const FSettingInfo& Info : SettingsTable)
		{
			if (Info.Tab == Tab)
			{
				bChanged |= Set(Info.Setting, Info.Default);
			}
		}
		return bChanged;
	}

	void FSettingsValues::Sanitise()
	{
		for (const FSettingInfo& Info : SettingsTable)
		{
			int& Slot = Values[SettingsIndex(Info.Setting)];
			Slot = std::clamp(Slot, Info.Min, Info.Max);
		}
	}

	float FSettingsValues::Volume(ESetting Setting) const
	{
		const FSettingInfo& Info = SettingInfo(Setting);
		if (Info.Control != ESettingControl::Slider || Info.Max <= Info.Min)
		{
			return 0.0f;
		}
		return static_cast<float>(Get(Setting) - Info.Min) / static_cast<float>(Info.Max - Info.Min);
	}

	ETextSize FSettingsValues::GetTextSize() const
	{
		switch (Get(ESetting::TextSize))
		{
		case 1:
			return ETextSize::Large;
		case 2:
			return ETextSize::Largest;
		default:
			return ETextSize::Default;
		}
	}
}
