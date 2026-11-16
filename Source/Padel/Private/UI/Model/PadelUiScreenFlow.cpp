#include "UI/Model/PadelUiScreenFlow.h"

#include <algorithm>

namespace Padel::Ui
{
	namespace
	{
		constexpr const char* FlowScreenNames[] = {
			"Title", "MainMenu", "MatchSetup", "Versus", "Match", "Paused", "Controls", "Settings", "Stats",
			"Practice", "PracticePaused",
		};
		static_assert(std::size(FlowScreenNames) == static_cast<size_t>(EUiScreen::Count));

		bool IsGameplayFlow(EUiScreen Screen)
		{
			return Screen == EUiScreen::Match || Screen == EUiScreen::Practice;
		}
	}

	const char* ScreenName(EUiScreen Screen)
	{
		const size_t Index = static_cast<size_t>(Screen);
		return Index < std::size(FlowScreenNames) ? FlowScreenNames[Index] : "";
	}

	bool IsMenuScreen(EUiScreen Screen)
	{
		// Practice shows its intro overlay as a menu; while drilling, the HUD hides the
		// menu root the same way it does for a match.
		return Screen != EUiScreen::Match && Screen != EUiScreen::Count;
	}

	bool ReturnsToOpener(EUiScreen Screen)
	{
		return Screen == EUiScreen::Controls || Screen == EUiScreen::Settings;
	}

	EUiBackAction BackAction(EUiScreen Screen)
	{
		switch (Screen)
		{
		case EUiScreen::MainMenu:
			return EUiBackAction::QuitGameDialog;
		case EUiScreen::MatchSetup:
		case EUiScreen::Practice:
			return EUiBackAction::MainMenu;
		case EUiScreen::Paused:
		case EUiScreen::PracticePaused:
			return EUiBackAction::Resume;
		case EUiScreen::Controls:
		case EUiScreen::Settings:
			return EUiBackAction::Opener;
		default:
			// Title has nowhere to go; Versus is skipped with A; Stats ignores B so a
			// stray press can't throw the stats away (spec 3).
			return EUiBackAction::None;
		}
	}

	EUiTransition FScreenFlow::GoTo(EUiScreen To, EUiNavKind Kind, const std::string& FocusOnLeave)
	{
		const EUiScreen From = CurrentScreen;
		if (!FocusOnLeave.empty())
		{
			Remembered[From] = FocusOnLeave;
		}
		if (ReturnsToOpener(To) && !ReturnsToOpener(From))
		{
			Openers.push_back(From);
		}
		else if (!ReturnsToOpener(To))
		{
			Openers.clear();
		}

		ArrivalFocus.clear();
		if (Kind == EUiNavKind::Back)
		{
			const auto It = Remembered.find(To);
			if (It != Remembered.end())
			{
				ArrivalFocus = It->second;
			}
		}
		else
		{
			// A fresh open starts on the default focus (spec 5.4).
			Remembered.erase(To);
		}
		CurrentScreen = To;

		if (From == To)
		{
			return EUiTransition::None;
		}
		const bool bFades = IsGameplayFlow(From) || IsGameplayFlow(To) || To == EUiScreen::Paused
			|| To == EUiScreen::PracticePaused || From == EUiScreen::Paused || From == EUiScreen::PracticePaused;
		if (bFades)
		{
			return EUiTransition::Fade;
		}
		return Kind == EUiNavKind::Back ? EUiTransition::SlideFromLeft : EUiTransition::SlideFromRight;
	}

	EUiScreen FScreenFlow::ReturnToOpener(const std::string& FocusOnLeave)
	{
		const EUiScreen Target = Openers.empty() ? EUiScreen::MainMenu : Openers.back();
		GoTo(Target, EUiNavKind::Back, FocusOnLeave);
		return Target;
	}

	EUiScreen FScreenFlow::BackTarget() const
	{
		switch (BackAction(CurrentScreen))
		{
		case EUiBackAction::MainMenu:
			return EUiScreen::MainMenu;
		case EUiBackAction::Opener:
			return Openers.empty() ? EUiScreen::MainMenu : Openers.back();
		case EUiBackAction::Resume:
			return CurrentScreen == EUiScreen::PracticePaused ? EUiScreen::Practice : EUiScreen::Match;
		default:
			return EUiScreen::Count;
		}
	}

	bool FScreenFlow::CanOpenPause(bool bGoldenPromptOpen, bool bEndCardShowing) const
	{
		return IsGameplayFlow(CurrentScreen) && !bGoldenPromptOpen && !bEndCardShowing;
	}
}
