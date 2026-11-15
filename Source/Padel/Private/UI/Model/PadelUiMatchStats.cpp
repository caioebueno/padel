#include "UI/Model/PadelUiMatchStats.h"

#include "UI/Model/PadelUiCodes.h"

#include <algorithm>

namespace Padel::Ui
{
	void FMatchStats::OnServeStarted()
	{
		HitsThisPoint = 0;
		bGlassSinceHit = false;
		bGlassThisPoint = false;
	}

	void FMatchStats::OnBallHit(int PlayerId, std::string_view ShotType, std::string_view TimingGrade)
	{
		++HitsThisPoint;
		const int Team = TeamOfPlayer(PlayerId);
		if (Team >= 0)
		{
			++Shots[NormaliseCode(ShotType)][Team];
			if (bGlassSinceHit)
			{
				++OffGlassShots[Team];
				bGlassThisPoint = true;
			}
		}
		++TimingGrades[NormaliseCode(TimingGrade)];
		bGlassSinceHit = false;
	}

	void FMatchStats::OnBallBounced(std::string_view Surface)
	{
		if (NormaliseCode(Surface) == "GLASS")
		{
			bGlassSinceHit = true;
		}
	}

	void FMatchStats::OnPointEnded(int WinningTeam, std::string_view ReasonCode)
	{
		const std::string Reason = NormaliseCode(ReasonCode);
		if (WinningTeam != TeamA && WinningTeam != TeamB)
		{
			return;
		}
		const int Loser = OtherTeam(WinningTeam);
		++PointsWon[WinningTeam];
		++Endings[Reason][WinningTeam];
		if (IsWinnerReason(Reason))
		{
			++Winners[WinningTeam];
		}
		else
		{
			++Errors[Loser];
		}
		if (Reason == "DOUBLE_FAULT")
		{
			++DoubleFaults[Loser];
		}
		RallyLengths.push_back(HitsThisPoint);
		if (bGlassThisPoint)
		{
			++PointsWithGlass;
		}
		OnServeStarted();
	}

	void FMatchStats::OnMatchWon(int Team, const std::vector<std::array<int, 2>>& InFinalSets)
	{
		MatchWinner = Team;
		FinalSets = InFinalSets;
	}

	int FMatchStats::TotalPoints() const
	{
		return PointsWon[0] + PointsWon[1];
	}

	double FMatchStats::AverageRally() const
	{
		if (RallyLengths.empty())
		{
			return 0.0;
		}
		int Total = 0;
		for (const int Length : RallyLengths)
		{
			Total += Length;
		}
		return static_cast<double>(Total) / static_cast<double>(RallyLengths.size());
	}

	double FMatchStats::GlassPointShare() const
	{
		const int Total = TotalPoints();
		return Total == 0 ? 0.0 : static_cast<double>(PointsWithGlass) / static_cast<double>(Total);
	}

	std::vector<FStatRow> FMatchStats::SummaryRows() const
	{
		return {
			{"Points won", PointsWon[0], PointsWon[1]},
			{"Winners", Winners[0], Winners[1]},
			{"Errors", Errors[0], Errors[1]},
			{"Double faults", DoubleFaults[0], DoubleFaults[1]},
			{"Shots off the glass", OffGlassShots[0], OffGlassShots[1]},
		};
	}

	std::vector<FStatRow> FMatchStats::EndingRows() const
	{
		std::vector<FStatRow> Rows;
		for (const auto& [Code, Counts] : Endings)
		{
			Rows.push_back({ReasonLabel(Code), Counts[0], Counts[1]});
		}
		// Stable on the map's alphabetical order, so ties always list the same way.
		std::stable_sort(Rows.begin(), Rows.end(), [](const FStatRow& A, const FStatRow& B)
		{
			return A.TeamA + A.TeamB > B.TeamA + B.TeamB;
		});
		return Rows;
	}
}
