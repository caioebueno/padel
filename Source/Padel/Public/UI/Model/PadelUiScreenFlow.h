// The menu screen flow (docs/ui/MENU_SPEC.md section 3): which screens exist, what Back
// does on each, where Controls and Settings return to, focus memory (5.4), and when
// Pause may open (7.5). UPadelMenuRoot drives the widgets from this.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <map>
#include <string>
#include <vector>

namespace Padel::Ui
{
	// Mirrored by the UENUM EPadelScreen in UI/PadelScreenTypes.h; keep the order.
	enum class EUiScreen
	{
		Title,
		MainMenu,
		MatchSetup,
		Versus,
		Match,
		Paused,
		Controls,
		Settings,
		Stats,
		Practice,
		PracticePaused,
		Count,
	};

	const char* ScreenName(EUiScreen Screen);

	// Match and Practice are the game itself; every other screen is a menu over it.
	bool IsMenuScreen(EUiScreen Screen);
	// Controls and Settings go back to whichever screen opened them.
	bool ReturnsToOpener(EUiScreen Screen);

	// What B (or Esc) does on a screen (spec 3, "Back on every screen").
	enum class EUiBackAction
	{
		None,           // Title, Versus, Stats, Match
		QuitGameDialog, // Main menu
		MainMenu,       // Match setup, Practice intro
		Resume,         // Pause and Practice pause
		Opener,         // Controls, Settings
	};

	EUiBackAction BackAction(EUiScreen Screen);

	enum class EUiNavKind
	{
		Forward, // deeper: slides in from the right, focus on the screen's default
		Back,    // returning: slides in from the left, focus where the player left it
	};

	enum class EUiTransition
	{
		None,
		SlideFromRight,
		SlideFromLeft,
		Fade, // Pause, the match, dialogs: they fade and scale, they don't slide (spec 9)
	};

	class FScreenFlow
	{
	public:
		EUiScreen Current() const { return CurrentScreen; }
		// The screen that opened Controls or Settings, or Count if none.
		EUiScreen Opener() const { return Openers.empty() ? EUiScreen::Count : Openers.back(); }

		// Move to a screen. FocusOnLeave is the element focused on the screen being left,
		// remembered so coming back restores it. Returns the transition to play.
		EUiTransition GoTo(EUiScreen To, EUiNavKind Kind, const std::string& FocusOnLeave = std::string());
		// B on Controls or Settings: back to the opener. Returns that screen.
		EUiScreen ReturnToOpener(const std::string& FocusOnLeave = std::string());
		// Where B leads from the current screen, or Count if B does nothing or opens a
		// dialog (the caller handles QuitGameDialog and Resume).
		EUiScreen BackTarget() const;

		// The element to focus on arrival: the remembered one when coming back, otherwise
		// empty (meaning the screen's default focus).
		const std::string& FocusOnArrival() const { return ArrivalFocus; }
		void Forget(EUiScreen Screen) { Remembered.erase(Screen); }

		// Spec 7.5: Start opens Pause only during play, and not over the golden point
		// prompt, the Versus card or the end-of-match card.
		bool CanOpenPause(bool bGoldenPromptOpen, bool bEndCardShowing) const;

	private:
		EUiScreen CurrentScreen = EUiScreen::Title;
		std::vector<EUiScreen> Openers;
		std::map<EUiScreen, std::string> Remembered;
		std::string ArrivalFocus;
	};
}
