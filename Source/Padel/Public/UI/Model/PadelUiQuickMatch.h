// The Quick Match setup and Versus screens' rules (docs/ui/MENU_SPEC.md 7.3, 7.4): the
// setup's focus map, how its controls map onto FMatchSetup, which cards go where, and the
// Versus card's clock (auto-advance, skip, loading indicator, start timeout).
//
// UI model: plain C++20, no Unreal headers.

#pragma once

#include "UI/Model/PadelUiMatchSetup.h"
#include "UI/Model/PadelUiNavigation.h"

#include <array>

namespace Padel::Ui
{
	// Focus map ids on the setup screen (spec 7.3).
	inline constexpr const char* SetupDifficultyId = "Setup.Difficulty";
	inline constexpr const char* SetupFormatId = "Setup.Format";
	inline constexpr const char* SetupGoldenPointId = "Setup.GoldenPoint";
	inline constexpr const char* SetupStartId = "Setup.Start";

	// Spec 7.3's table: one wrapping vertical list, default Start match, left/right belong
	// to the focused control's value.
	FFocusMap SetupFocusMap();

	// The selector's index is the difficulty (Easy, Medium, Hard).
	int DifficultyIndex(const FMatchSetup& Setup);
	void SetDifficultyIndex(FMatchSetup& Setup, int Index);
	inline constexpr int DifficultyCount = 3;

	// The segmented control's index: 0 = 1 set, 1 = best of 3 (spec 7.3, in that order).
	int FormatIndex(const FMatchSetup& Setup);
	void SetFormatIndex(FMatchSetup& Setup, int Index);

	// The large cards, left to right. Fixed, so X only crosses the badges over and the
	// cards never move (spec 7.3): the default human (Ventara, GDD 6.4) first.
	std::array<int, 2> SetupPairOrder();
	// The small cards, left to right: Team B in id order.
	std::array<int, 2> SetupOpponentOrder();

	// The Versus card's pair line (spec 7.4): the human's name first, then the partner;
	// Team B in id order.
	std::array<int, 2> VersusPairOrder(int Team, const FMatchSetup& Setup);

	// The Versus card's clock (spec 7.4, 6.16, 7.12), stepped by the screen's tick.
	//
	// - The card stays VersusSeconds, then starts the match if it's ready.
	// - A skips at once, but only once the match is ready.
	// - If the match isn't ready when the card's time is up, the card stays, and after
	//   LoadingAfter more seconds the loading indicator shows.
	// - Past StartTimeout with no match, the screen offers to go back to the menu.
	// - The match is started once, whichever comes first.
	struct FVersusClock
	{
		float CardSeconds = 2.5f;
		float LoadingAfter = 0.3f;
		float StartTimeout = 20.0f;

		float Elapsed = 0.0f;
		bool bStarted = false;
		bool bTimeoutShown = false;

		void Tick(float DeltaSeconds) { Elapsed += DeltaSeconds; }

		bool CanSkip(bool bMatchReady) const { return bMatchReady && !bStarted; }
		bool ShouldAutoStart(bool bMatchReady) const { return bMatchReady && !bStarted && Elapsed >= CardSeconds; }
		bool ShowsLoading(bool bMatchReady) const { return !bMatchReady && !bStarted && Elapsed >= CardSeconds + LoadingAfter; }
		// True once, the first time the timeout passes.
		bool TakeTimeout(bool bMatchReady);
		void MarkStarted() { bStarted = true; }
	};
}
