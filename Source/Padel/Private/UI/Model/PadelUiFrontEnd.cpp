#include "UI/Model/PadelUiFrontEnd.h"

#include "UI/Model/PadelUiTheme.h"

#include <algorithm>
#include <cmath>

namespace Padel::Ui
{
	namespace
	{
		constexpr std::string_view FrontEndIgnoredPrefixes[] = {
			"Gamepad_LeftStick_",
			"Gamepad_RightStick_",
		};

		constexpr std::string_view FrontEndIgnoredKeys[] = {
			// Analog axes and mouse movement.
			"Gamepad_LeftX", "Gamepad_LeftY", "Gamepad_RightX", "Gamepad_RightY",
			"Gamepad_Left2D", "Gamepad_Right2D",
			"Gamepad_LeftTriggerAxis", "Gamepad_RightTriggerAxis",
			"MouseX", "MouseY", "Mouse2D", "MouseWheelAxis",
			// Modifiers alone: the first half of a system shortcut.
			"LeftAlt", "RightAlt", "LeftCommand", "RightCommand", "LeftControl", "RightControl",
			"LeftShift", "RightShift", "CapsLock",
			"Invalid", "None",
		};

		constexpr float FrontEndTwoPi = 6.28318530718f;
	}

	bool CountsAsAnyButton(std::string_view KeyName)
	{
		if (KeyName.empty())
		{
			return false;
		}
		for (std::string_view Prefix : FrontEndIgnoredPrefixes)
		{
			if (KeyName.substr(0, Prefix.size()) == Prefix)
			{
				return false;
			}
		}
		return std::find(std::begin(FrontEndIgnoredKeys), std::end(FrontEndIgnoredKeys), KeyName) == std::end(FrontEndIgnoredKeys);
	}

	bool TitleShowsPadPrompt(EPadFamily LastUsed, bool bGamepadAttached)
	{
		return LastUsed != EPadFamily::KeyboardMouse || bGamepadAttached;
	}

	float PulseAlpha(float Seconds, float Period, float MinAlpha, bool bReduceMotion)
	{
		if (bReduceMotion || Period <= 0.0f)
		{
			return 1.0f;
		}
		// Cosine so the loop starts fully visible: 1 at 0, MinAlpha at half the period.
		const float Phase = std::fmod(std::max(Seconds, 0.0f), Period) / Period;
		const float Wave = 0.5f + 0.5f * std::cos(Phase * FrontEndTwoPi);
		return MinAlpha + (1.0f - MinAlpha) * Wave;
	}

	float UnderlineProgress(float Seconds, float Duration, bool bReduceMotion)
	{
		if (bReduceMotion || Duration <= 0.0f)
		{
			return 1.0f;
		}
		return EaseOutCubic(Seconds / Duration);
	}

	const std::vector<FMainMenuTile>& MainMenuTiles()
	{
		// Spec 7.2's table: hero C1-C8 y 184-704; small tiles C9-C12 at y 184 and 456
		// (248 tall); the bottom row y 728-936 (208 tall).
		static const std::vector<FMainMenuTile> Tiles = {
			{"Main.QuickMatch", 1, 8, 184.0f, 520.0f, true, false, EUiScreen::MatchSetup},
			{"Main.Practice", 9, 4, 184.0f, 248.0f, false, false, EUiScreen::Practice},
			{"Main.Settings", 9, 4, 456.0f, 248.0f, false, false, EUiScreen::Settings},
			{"Main.Career", 1, 4, 728.0f, 208.0f, false, true, EUiScreen::Count},
			{"Main.TeamBuilder", 5, 4, 728.0f, 208.0f, false, true, EUiScreen::Count},
			{"Main.Controls", 9, 4, 728.0f, 208.0f, false, false, EUiScreen::Controls},
		};
		return Tiles;
	}

	FFocusMap MainMenuFocusMap()
	{
		FFocusMap Map;
		// Up, Down, Left, Right (spec 7.2). Grids don't wrap (5.3).
		Map.Add("Main.QuickMatch", {"", "Main.Career", "", "Main.Practice"});
		Map.Add("Main.Practice", {"", "Main.Settings", "Main.QuickMatch", ""});
		Map.Add("Main.Settings", {"Main.Practice", "Main.Controls", "Main.QuickMatch", ""});
		Map.Add("Main.Career", {"Main.QuickMatch", "", "", "Main.TeamBuilder"});
		Map.Add("Main.TeamBuilder", {"Main.QuickMatch", "", "Main.Career", "Main.Controls"});
		Map.Add("Main.Controls", {"Main.Settings", "", "Main.TeamBuilder", ""});
		Map.SetDefault("Main.QuickMatch");
		return Map;
	}

	float StaggerDelay(int Index, float Stagger, int MaxStaggered)
	{
		if (Index <= 0 || MaxStaggered <= 1)
		{
			return 0.0f;
		}
		return Stagger * static_cast<float>(std::min(Index, MaxStaggered - 1));
	}

	const std::vector<FPracticeControlRow>& PracticeControlRows()
	{
		static const std::vector<FPracticeControlRow> Rows = {
			{EPracticeControl::ChangeDrill, {"Gamepad_DPad_Left", "Gamepad_DPad_Right"}, {"Three", "Four"}},
			{EPracticeControl::PauseMachine, {"Gamepad_DPad_Up"}, {"One"}},
			{EPracticeControl::FeedNow, {"Gamepad_DPad_Down"}, {"Two"}},
			{EPracticeControl::Pause, {"Gamepad_Special_Right"}, {"Escape"}},
		};
		return Rows;
	}

	const std::vector<std::string>& PracticePauseItems()
	{
		static const std::vector<std::string> Items = {
			"PracticePause.Resume",
			"PracticePause.Controls",
			"PracticePause.Settings",
			"PracticePause.Quit",
		};
		return Items;
	}
}
