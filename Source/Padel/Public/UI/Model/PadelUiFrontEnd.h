// The front end's own rules (docs/ui/MENU_SPEC.md 7.1 Title, 7.2 Main menu, 7.9 Practice):
// which keys count as "any button", the prompt's wording and pulse, the main menu's tile
// grid and focus map, and the practice control rows. The screens in UI/Screens draw these.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "UI/Model/PadelUiGlyphs.h"
#include "UI/Model/PadelUiNavigation.h"
#include "UI/Model/PadelUiScreenFlow.h"

namespace Padel::Ui
{
	// --- 7.1 Title ---------------------------------------------------------------

	// True if a key-down by Unreal key name continues from the title. Any key or button
	// does, but not the sticks (too easy to nudge), mouse movement, or a modifier held
	// on its own for a system shortcut (Alt+Tab, Cmd+Tab).
	bool CountsAsAnyButton(std::string_view KeyName);

	// "Press any button" when a pad is in use or plugged in, else "Press any key".
	bool TitleShowsPadPrompt(EPadFamily LastUsed, bool bGamepadAttached);

	// The prompt's alpha at Seconds into its loop: a sine between 1.0 and MinAlpha over
	// Period, starting at 1.0. Steady at 1.0 with Reduce motion (spec 9).
	float PulseAlpha(float Seconds, float Period, float MinAlpha, bool bReduceMotion);

	// How far the lime underline under STREET has drawn, 0..1 (ease-out cubic). Drawn at
	// once with Reduce motion.
	float UnderlineProgress(float Seconds, float Duration, bool bReduceMotion);

	// --- 7.2 Main menu -----------------------------------------------------------

	struct FMainMenuTile
	{
		const char* Id = "";
		int Column = 1;        // C1-C12
		int Span = 4;          // columns
		float Top = 0.0f;      // px at 1080p
		float Height = 0.0f;   // px
		bool bHero = false;
		bool bLocked = false;  // focusable, explains itself, A shows a toast (spec 5.3)
		EUiScreen Target = EUiScreen::Count; // where A goes; Count for a locked tile
	};

	// The six tiles in reading order (hero first), with spec 7.2's positions.
	const std::vector<FMainMenuTile>& MainMenuTiles();
	// Spec 7.2's focus map: a grid, no wrap. Default Quick Match.
	FFocusMap MainMenuFocusMap();

	// When item Index starts its entry, in seconds: Stagger apart, and items past
	// MaxStaggered enter with the last staggered one (spec 9, Motion.Stagger).
	float StaggerDelay(int Index, float Stagger, int MaxStaggered);

	// --- 7.9 Practice ------------------------------------------------------------

	enum class EPracticeControl
	{
		ChangeDrill,
		PauseMachine,
		FeedNow,
		Pause,
	};

	struct FPracticeControlRow
	{
		EPracticeControl Control = EPracticeControl::ChangeDrill;
		// Unreal key names, as bound in Gameplay/PadelInputConfig.cpp's binding table
		// (CmdMiddle/CmdYours, CmdNet, CmdBack, Pause).
		std::vector<const char*> PadKeys;
		std::vector<const char*> Keys;
	};

	// The intro's and the pause panel's control list, in the spec's order.
	const std::vector<FPracticeControlRow>& PracticeControlRows();

	// Practice pause's list (spec 7.9): Resume, Controls, Settings, Quit practice.
	const std::vector<std::string>& PracticePauseItems();
}
