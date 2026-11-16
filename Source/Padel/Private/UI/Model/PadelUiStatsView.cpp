#include "UI/Model/PadelUiStatsView.h"

#include "UI/Model/PadelUiCodes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Padel::Ui
{
	std::vector<FStatRow> StatsTabRows(const FMatchStats& Stats, EStatsTab Tab)
	{
		switch (Tab)
		{
		case EStatsTab::Summary:
			return Stats.SummaryRows();
		case EStatsTab::Shots:
			return Stats.ShotRows();
		case EStatsTab::Points:
			return Stats.EndingRows();
		case EStatsTab::Count:
			break;
		}
		return {};
	}

	int LeadingTeam(const FStatRow& Row)
	{
		if (Row.TeamA == Row.TeamB)
		{
			return -1;
		}
		const bool bAHigher = Row.TeamA > Row.TeamB;
		return (bAHigher != Row.bLowerIsBetter) ? TeamA : TeamB;
	}

	double BarFraction(const FStatRow& Row, int Team)
	{
		const int Larger = std::max(Row.TeamA, Row.TeamB);
		if (Larger <= 0)
		{
			return 0.0;
		}
		const int Value = Team == TeamA ? Row.TeamA : Row.TeamB;
		return std::clamp(static_cast<double>(Value) / static_cast<double>(Larger), 0.0, 1.0);
	}

	int SetCount(const FMatchStats& Stats)
	{
		return static_cast<int>(Stats.FinalSets.size());
	}

	FSetCell SetCell(const FMatchStats& Stats, int SetIndex, int Team)
	{
		FSetCell Cell;
		if (SetIndex < 0 || SetIndex >= SetCount(Stats) || (Team != TeamA && Team != TeamB))
		{
			return Cell;
		}
		const std::array<int, 2>& Set = Stats.FinalSets[static_cast<size_t>(SetIndex)];
		Cell.Games = Set[static_cast<size_t>(Team)];
		Cell.bWonSet = Set[static_cast<size_t>(Team)] > Set[static_cast<size_t>(OtherTeam(Team))];
		if (const std::optional<FTeamCounts> Tiebreak = Stats.TiebreakOfSet(SetIndex))
		{
			Cell.TiebreakPoints = (*Tiebreak)[static_cast<size_t>(Team)];
		}
		return Cell;
	}

	std::string RallyText(const FMatchStats& Stats)
	{
		char Buffer[32];
		std::snprintf(Buffer, sizeof(Buffer), "%.1f", Stats.AverageRally());
		return Buffer;
	}

	int GlassPercent(const FMatchStats& Stats)
	{
		return static_cast<int>(std::lround(Stats.GlassPointShare() * 100.0));
	}

	float FStatsTimeline::BarsStart() const
	{
		// The last column starts at (n - 1) stagger and fades in over one more.
		return static_cast<float>(std::max(SetColumns, 0)) * ColumnStagger;
	}

	float FStatsTimeline::RowStart(int Row) const
	{
		// Spec 9: at most StaggerMax items stagger; the rest enter with the last of them.
		const int Step = std::clamp(Row, 0, std::max(StaggerMax - 1, 0));
		return BarsStart() + static_cast<float>(Step) * RowStagger;
	}

	float FStatsTimeline::Duration() const
	{
		if (SetColumns <= 0 && Rows <= 0)
		{
			return 0.0f;
		}
		if (Rows <= 0)
		{
			return BarsStart();
		}
		return RowStart(Rows - 1) + BarGrow;
	}

	float FStatsTimeline::ColumnAlpha(int Column, float T) const
	{
		if (ColumnStagger <= 0.0f)
		{
			return 1.0f;
		}
		const float Start = static_cast<float>(Column) * ColumnStagger;
		return std::clamp((T - Start) / ColumnStagger, 0.0f, 1.0f);
	}

	float FStatsTimeline::BarProgress(int Row, float T) const
	{
		if (BarGrow <= 0.0f)
		{
			return T >= RowStart(Row) ? 1.0f : 0.0f;
		}
		return std::clamp((T - RowStart(Row)) / BarGrow, 0.0f, 1.0f);
	}

	void FStatsConfirmGate::Begin(float LockSeconds, float AnimationSeconds)
	{
		Elapsed = 0.0f;
		Lock = std::max(LockSeconds, 0.0f);
		Animation = std::max(AnimationSeconds, 0.0f);
		bAnimationFinished = Animation <= 0.0f;
	}

	EStatsConfirm FStatsConfirmGate::Press()
	{
		if (IsAnimating())
		{
			FinishAnimation();
			return EStatsConfirm::FinishAnimation;
		}
		return Elapsed < Lock ? EStatsConfirm::Ignored : EStatsConfirm::Accept;
	}
}
