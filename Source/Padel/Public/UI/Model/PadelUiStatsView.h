// What the post-match stats screen shows (docs/ui/MENU_SPEC.md 7.8), worked out without
// the engine: the rows of each tab, which number is highlighted, how long each bar is,
// the set columns with their tie-breaks, the footer numbers, the entry animation's
// timeline and the stray-press lock.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include "UI/Model/PadelUiMatchStats.h"

#include <optional>
#include <string>
#include <vector>

namespace Padel::Ui
{
	enum class EStatsTab
	{
		Summary,
		Shots,
		Points,
		Count,
	};

	std::vector<FStatRow> StatsTabRows(const FMatchStats& Stats, EStatsTab Tab);

	// The team whose number is drawn in Text.Primary: the higher one, or the lower where
	// lower is better (errors, double faults). -1 on a tie: both stay Text.Primary.
	int LeadingTeam(const FStatRow& Row);

	// A team's bar as a fraction of the half-width, both scaled to the larger of the two
	// numbers (spec 7.8). 0 when both are 0 (spec 7.12: an empty bar).
	double BarFraction(const FStatRow& Row, int Team);

	// One cell of the score panel: a team's games in a set.
	struct FSetCell
	{
		int Games = 0;
		// Full Text.Primary for the set's winner, Text.Secondary for the loser.
		bool bWonSet = false;
		// The team's own tie-break points, shown raised beside the games ("7⁷ 6⁵").
		std::optional<int> TiebreakPoints;
	};

	int SetCount(const FMatchStats& Stats);
	FSetCell SetCell(const FMatchStats& Stats, int SetIndex, int Team);

	// "Average rally {0} shots · {1}% of points had a shot off the glass" (spec 8).
	// Rally to one decimal ("8.4"); share as a whole percent.
	std::string RallyText(const FMatchStats& Stats);
	int GlassPercent(const FMatchStats& Stats);

	// Spec 7.8's entry animation: set columns fade in one after another, then the bars grow
	// from the centre, staggered per row (at most StaggerMax rows staggered; spec 9).
	struct FStatsTimeline
	{
		int SetColumns = 0;
		int Rows = 0;
		float ColumnStagger = 0.12f;
		float BarGrow = 0.4f;
		float RowStagger = 0.04f;
		int StaggerMax = 6;

		// Whole animation, seconds. 0 when there's nothing to animate.
		float Duration() const;
		// 0..1 opacity of a set column at time T.
		float ColumnAlpha(int Column, float T) const;
		// 0..1 linear progress of a row's bars at time T (the screen eases it).
		float BarProgress(int Row, float T) const;

	private:
		float BarsStart() const;
		float RowStart(int Row) const;
	};

	enum class EStatsConfirm
	{
		// An A during the entry animation: jump to its end, press nothing.
		FinishAnimation,
		// An A during the lock after the animation: nothing.
		Ignored,
		// A fresh A after the lock: the focused button acts.
		Accept,
	};

	// Spec 7.8's stray-press lock. A is also the shot button, so a player still pressing
	// A at match point mustn't start a rematch: Confirm waits until the entry animation
	// has finished and LockSeconds have passed since the screen appeared. A held A never
	// arrives here again (the menu root ignores key repeats), so only a fresh press acts.
	class FStatsConfirmGate
	{
	public:
		void Begin(float LockSeconds, float AnimationSeconds);
		void Tick(float DeltaSeconds) { Elapsed += DeltaSeconds; }
		void FinishAnimation() { bAnimationFinished = true; }

		bool IsAnimating() const { return !bAnimationFinished && Elapsed < Animation; }
		// Time into the entry animation, or its end once finished or skipped.
		float AnimationTime() const { return IsAnimating() ? Elapsed : Animation; }
		// True while the buttons show their Disabled look and A does nothing.
		bool IsLocked() const { return IsAnimating() || Elapsed < Lock; }
		// A fresh Confirm press (never a repeat).
		EStatsConfirm Press();

	private:
		float Elapsed = 0.0f;
		float Lock = 0.0f;
		float Animation = 0.0f;
		bool bAnimationFinished = false;
	};
}
