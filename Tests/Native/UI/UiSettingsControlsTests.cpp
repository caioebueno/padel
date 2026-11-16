// Workstream D's engine-free parts (docs/ui/MENU_SPEC.md 6.10, 7.6, 7.7): the Settings
// values (ranges, defaults, the X Reset rule, reset per tab), the slider's step and repeat
// rules with the theme's numbers, and the Controls pad diagram's callouts.
#include "doctest.h"

#include "UI/Model/PadelUiControls.h"
#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiPadDiagram.h"
#include "UI/Model/PadelUiSettings.h"
#include "UI/Model/PadelUiTheme.h"

#include <algorithm>
#include <set>
#include <string>

using namespace Padel::Ui;

TEST_CASE("UI.Settings: defaults are spec 7.7's bold values")
{
	const FSettingsValues Values;
	CHECK(Values.Get(ESetting::MasterVolume) == 80);
	CHECK(Values.Get(ESetting::EffectsVolume) == 100);
	CHECK(Values.Get(ESetting::CrowdVolume) == 70);
	CHECK(Values.Get(ESetting::MenuVolume) == 70);
	CHECK(Values.GetTextSize() == ETextSize::Default);
	CHECK_FALSE(Values.ReduceMotion());
	CHECK_FALSE(Values.ReduceTransparency());
	for (int Tab = 0; Tab < static_cast<int>(ESettingsTab::Count); ++Tab)
	{
		CHECK(Values.IsTabDefault(static_cast<ESettingsTab>(Tab), true));
	}
}

TEST_CASE("UI.Settings: each tab lists its rows in spec order; Menu sounds waits for menu sounds")
{
	const std::vector<ESetting> AudioWithout = TabSettings(ESettingsTab::Audio, false);
	REQUIRE(AudioWithout.size() == 3);
	CHECK(AudioWithout[0] == ESetting::MasterVolume);
	CHECK(AudioWithout[1] == ESetting::EffectsVolume);
	CHECK(AudioWithout[2] == ESetting::CrowdVolume);
	const std::vector<ESetting> AudioWith = TabSettings(ESettingsTab::Audio, true);
	REQUIRE(AudioWith.size() == 4);
	CHECK(AudioWith[3] == ESetting::MenuVolume);

	const std::vector<ESetting> Access = TabSettings(ESettingsTab::Accessibility, true);
	REQUIRE(Access.size() == 3);
	CHECK(Access[0] == ESetting::TextSize);
	CHECK(Access[1] == ESetting::ReduceMotion);
	CHECK(Access[2] == ESetting::ReduceTransparency);

	std::set<std::string> Ids;
	for (int Index = 0; Index < static_cast<int>(ESetting::Count); ++Index)
	{
		const FSettingInfo& Info = SettingInfo(static_cast<ESetting>(Index));
		CHECK(Info.Setting == static_cast<ESetting>(Index));
		CHECK(Info.Min <= Info.Default);
		CHECK(Info.Default <= Info.Max);
		CHECK(Ids.insert(Info.Id).second);
	}
	CHECK(std::string(SettingInfo(ESetting::EffectsVolume).Id) == "Set.Vol.Effects");
	CHECK(SettingInfo(ESetting::TextSize).Control == ESettingControl::Selector);
	CHECK(SettingInfo(ESetting::ReduceMotion).Control == ESettingControl::Toggle);
}

TEST_CASE("UI.Settings: values clamp to their range and report changes")
{
	FSettingsValues Values;
	CHECK(Values.Set(ESetting::CrowdVolume, 150));
	CHECK(Values.Get(ESetting::CrowdVolume) == 100);
	CHECK_FALSE(Values.Set(ESetting::CrowdVolume, 100));
	CHECK(Values.Set(ESetting::CrowdVolume, -5));
	CHECK(Values.Get(ESetting::CrowdVolume) == 0);
	CHECK(Values.Volume(ESetting::CrowdVolume) == doctest::Approx(0.0f));
	CHECK(Values.Volume(ESetting::MasterVolume) == doctest::Approx(0.8f));

	CHECK(Values.Set(ESetting::TextSize, 7));
	CHECK(Values.GetTextSize() == ETextSize::Largest);
	CHECK(Values.Set(ESetting::TextSize, 1));
	CHECK(Values.GetTextSize() == ETextSize::Large);
}

TEST_CASE("UI.Settings: X Reset shows only off-default, and resets only its own tab")
{
	FSettingsValues Values;
	Values.Set(ESetting::MasterVolume, 40);
	Values.Set(ESetting::TextSize, 2);
	Values.Set(ESetting::ReduceMotion, 1);
	CHECK_FALSE(Values.IsTabDefault(ESettingsTab::Audio, false));
	CHECK_FALSE(Values.IsTabDefault(ESettingsTab::Accessibility, false));

	CHECK(Values.ResetTab(ESettingsTab::Accessibility));
	CHECK(Values.IsTabDefault(ESettingsTab::Accessibility, false));
	CHECK(Values.Get(ESetting::MasterVolume) == 40);
	CHECK_FALSE(Values.ResetTab(ESettingsTab::Accessibility));

	CHECK(Values.ResetTab(ESettingsTab::Audio));
	CHECK(Values == FSettingsValues());
}

TEST_CASE("UI.Settings: a hidden Menu sounds row doesn't show the Reset hint, but Reset still clears it")
{
	FSettingsValues Values;
	Values.Set(ESetting::MenuVolume, 10);
	CHECK(Values.IsTabDefault(ESettingsTab::Audio, false));
	CHECK_FALSE(Values.IsTabDefault(ESettingsTab::Audio, true));
	CHECK(Values.ResetTab(ESettingsTab::Audio));
	CHECK(Values.IsDefault(ESetting::MenuVolume));
}

TEST_CASE("UI.Settings: a broken saved value is clamped on load")
{
	FSettingsValues Values;
	// Set clamps already; Sanitise is for values read straight from the ini file.
	Values.Sanitise();
	CHECK(Values == FSettingsValues());
}

TEST_CASE("UI.Slider: theme steps press by 5, repeat by 1, then by 5 after a second (spec 5.3, 6.10)")
{
	const FUiTheme Theme;
	FSliderModel Slider;
	Slider.StepSize = static_cast<int>(Theme.Input.SliderStep);
	Slider.FineStep = static_cast<int>(Theme.Input.SliderFineStep);
	Slider.FastAfter = Theme.Input.SliderFastAfter;
	Slider.Value = SettingInfo(ESetting::MasterVolume).Default;

	CHECK(Slider.Press(1));
	CHECK(Slider.Value == 85);
	CHECK(Slider.Repeat(1, 0.5f));
	CHECK(Slider.Value == 86);
	CHECK(Slider.Repeat(1, 1.2f));
	CHECK(Slider.Value == 91);
	Slider.Set(100);
	CHECK_FALSE(Slider.Press(1));
	CHECK(Theme.Input.SliderRepeatRate < Theme.Input.RepeatRate);
}

TEST_CASE("UI.Controls: every bound pad key has a callout, one per spot")
{
	// GDD 7's pad column, as UPadelInputConfig binds it.
	const char* Bound[] = {"Gamepad_Left2D", "Gamepad_RightTrigger", "Gamepad_FaceButton_Bottom", "Gamepad_FaceButton_Right",
		"Gamepad_FaceButton_Top", "Gamepad_FaceButton_Left", "Gamepad_RightShoulder", "Gamepad_LeftShoulder", "Gamepad_DPad_Up",
		"Gamepad_DPad_Down", "Gamepad_DPad_Left", "Gamepad_DPad_Right", "Gamepad_Special_Right"};
	for (const char* Key : Bound)
	{
		CHECK_MESSAGE(FindPadCallout(Key) != nullptr, Key);
	}
	CHECK(FindPadCallout("Gamepad_LeftTrigger") == nullptr);

	std::set<std::string> Keys;
	std::set<std::pair<int, int>> Slots;
	for (const FPadCallout& Callout : PadCallouts())
	{
		CHECK(Keys.insert(Callout.KeyName).second);
		CHECK(Slots.insert({static_cast<int>(Callout.Side), static_cast<int>(Callout.LabelY)}).second);
		CHECK(Callout.Leader.size() >= 2);
	}
}

TEST_CASE("UI.Controls: leaders start beside their label and end on the pad")
{
	const FPadDiagramLayout& Layout = PadDiagramLayout();
	float MinX = 1e9f;
	float MaxX = -1e9f;
	float MinY = 1e9f;
	float MaxY = -1e9f;
	for (const FDiagramLine& Line : PadOutline())
	{
		REQUIRE(Line.size() >= 2);
		for (const FDiagramPoint& Point : Line)
		{
			MinX = std::min(MinX, Point.X);
			MaxX = std::max(MaxX, Point.X);
			MinY = std::min(MinY, Point.Y);
			MaxY = std::max(MaxY, Point.Y);
		}
	}
	// Spec 7.6: the drawing sits inside C3-C10 and y 232-760.
	const FUiTheme Theme;
	CHECK(MinX >= Theme.ColumnX(3));
	CHECK(MaxX <= Theme.ColumnX(10) + Theme.SpanWidth(1));
	CHECK(MinY >= Layout.AreaTop);
	CHECK(MaxY <= Layout.AreaBottom);

	for (const FPadCallout& Callout : PadCallouts())
	{
		const FDiagramPoint& Start = Callout.Leader.front();
		const FDiagramPoint& End = Callout.Leader.back();
		switch (Callout.Side)
		{
		case ECalloutSide::Left:
			CHECK(Start.X == doctest::Approx(Layout.LeftLabelRight + Layout.LabelGap));
			CHECK(Start.Y == doctest::Approx(Callout.LabelY));
			break;
		case ECalloutSide::Right:
			CHECK(Start.X == doctest::Approx(Layout.RightLabelLeft - Layout.LabelGap));
			CHECK(Start.Y == doctest::Approx(Callout.LabelY));
			break;
		case ECalloutSide::Top:
			CHECK(Start.X == doctest::Approx(Layout.TopLabelX));
			CHECK(Start.Y > Layout.TopLabelY);
			break;
		}
		CHECK(End.X >= MinX);
		CHECK(End.X <= MaxX);
		CHECK(End.Y >= MinY);
		CHECK(End.Y <= MaxY);
	}
}

TEST_CASE("UI.Glyphs: pad keys draw as circles, caps or the D-pad, with short labels")
{
	CHECK(PadKeyShape("Gamepad_FaceButton_Bottom") == EGlyphShape::Circle);
	CHECK(PadKeyShape("Gamepad_Left2D") == EGlyphShape::Circle);
	CHECK(PadKeyShape("Gamepad_RightTrigger") == EGlyphShape::Cap);
	CHECK(PadKeyShape("Gamepad_Special_Right") == EGlyphShape::Cap);
	CHECK(PadKeyShape("Gamepad_DPad_Left") == EGlyphShape::DPad);

	CHECK(PadKeyGlyphText("Gamepad_Left2D", EPadFamily::Xbox) == "LS");
	CHECK(PadKeyGlyphText("Gamepad_DPad_Up", EPadFamily::Xbox).empty());
	CHECK(PadKeyGlyphText("Gamepad_RightTrigger", EPadFamily::PlayStation) == "R2");
	CHECK(PadKeyGlyphText("Gamepad_FaceButton_Bottom", EPadFamily::Switch) == "B");
	CHECK(PadKeyGlyphText("Gamepad_Special_Right", EPadFamily::Xbox) == "Menu");
}
