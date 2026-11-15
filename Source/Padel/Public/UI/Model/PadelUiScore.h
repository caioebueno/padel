// The score as the HUD shows it (GDD 3.1, GDD 10 "compact score bug").
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).
// FScoreView mirrors the score snapshot Agent 2 promised for score_changed (GDD 3.4):
// points as display strings, games, finished sets, who serves, and the pressure flags.

#pragma once

#include <array>
#include <string>
#include <vector>

namespace Padel::Ui
{
	struct FScoreView
	{
		// "0", "15", "30", "40", "AD", "" (the side without advantage) or tiebreak numbers.
		std::array<std::string, 2> Points{"0", "0"};
		std::array<int, 2> Games{0, 0};
		// Finished sets, oldest first.
		std::vector<std::array<int, 2>> Sets;
		int ServingTeam = 0;
		int ServerId = 0;
		int ReceiverId = 2;
		bool bServeFromRight = true;
		bool bInTiebreak = false;
		bool bGoldenPoint = true;
		// +1 when Team A is on the +X half, -1 after a change of ends.
		int TeamAEnd = 1;
		bool bBreakPoint = false;
		bool bSetPoint = false;
		bool bMatchPoint = false;
		bool bMatchOver = false;
		int Winner = -1;
	};

	inline constexpr const char* GoldenPointTag = "GOLDEN POINT";
	inline constexpr const char* TiebreakTag = "TIEBREAK";

	// Games per set for one team, finished sets then the current one: {6, 3, 2}.
	std::vector<int> SetColumns(const FScoreView& View, int Team);

	// True at 40-40 with golden point on: the next point decides the game.
	bool IsGoldenPointNow(const FScoreView& View);

	// The tag under the score bug: MATCH POINT > SET POINT > BREAK POINT > GOLDEN POINT
	// > TIEBREAK, or "" when nothing is at stake.
	std::string PressureTag(const FScoreView& View);

	// "6-4 3-2 30-15": sets, current games, points. Used by tests and logs.
	std::string SummaryLine(const FScoreView& View);

	// The final result from finished sets, e.g. "6-4 3-6 7-6".
	std::string FinalLine(const std::vector<std::array<int, 2>>& Sets);

	// Display string for a regular game's point count (0..3), for the demo driver.
	std::string PointLabelFromCount(int Count);
}
