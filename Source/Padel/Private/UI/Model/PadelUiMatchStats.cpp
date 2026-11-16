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
		GlassShotBy = {false, false};
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
				GlassShotBy[Team] = true;
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
		if (GlassShotBy[WinningTeam])
		{
			++GlassPointsWon[WinningTeam];
		}
		if (bTiebreakNow)
		{
			++TiebreakPoints[WinningTeam];
			bTiebreakThisSet = true;
		}
		OnServeStarted();
	}

	void FMatchStats::OnScoreChanged(const FScoreView& Score)
	{
		// point_ended, then set_won, then the new score (Agent 2's event order): when the
		// finished-set list grows, the tie-break points counted so far belong to that set.
		while (SetTiebreaks.size() < Score.Sets.size())
		{
			SetTiebreaks.push_back(bTiebreakThisSet ? std::optional<FTeamCounts>(TiebreakPoints) : std::nullopt);
			bTiebreakThisSet = false;
			TiebreakPoints = {0, 0};
		}
		bTiebreakNow = Score.bInTiebreak;
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

	FTeamCounts FMatchStats::Smashes() const
	{
		const auto Found = Shots.find("SMASH");
		return Found == Shots.end() ? FTeamCounts{0, 0} : Found->second;
	}

	std::optional<FTeamCounts> FMatchStats::TiebreakOfSet(int SetIndex) const
	{
		if (SetIndex < 0 || SetIndex >= static_cast<int>(SetTiebreaks.size()))
		{
			return std::nullopt;
		}
		return SetTiebreaks[static_cast<size_t>(SetIndex)];
	}

	std::vector<FStatRow> FMatchStats::SummaryRows() const
	{
		const FTeamCounts Smash = Smashes();
		return {
			{"Points won", PointsWon[0], PointsWon[1], "PointsWon"},
			{"Winners", Winners[0], Winners[1], "Winners"},
			{"Errors", Errors[0], Errors[1], "Errors", true},
			{"Smashes", Smash[0], Smash[1], "Smashes"},
			{"Points won off the glass", GlassPointsWon[0], GlassPointsWon[1], "GlassPoints"},
			{"Double faults", DoubleFaults[0], DoubleFaults[1], "DoubleFaults", true},
		};
	}

	std::vector<FStatRow> FMatchStats::ShotRows() const
	{
		// GDD 4 order. Both serves count as "Serve" on the screen (spec 7.8).
		static constexpr std::array<std::string_view, 9> Order{
			"DRIVE", "VOLLEY", "LOB", "BANDEJA", "VIBORA", "SMASH", "CHIQUITA", "SERVE", "MISHIT"};
		auto RowKey = [](const std::string& Code) -> std::string
		{
			return Code == "SERVE_FLAT" || Code == "SERVE_KICK" ? std::string("SERVE") : Code;
		};
		std::map<std::string, FTeamCounts> Merged;
		for (const auto& [Code, Counts] : Shots)
		{
			FTeamCounts& Into = Merged[RowKey(Code)];
			Into[0] += Counts[0];
			Into[1] += Counts[1];
		}
		std::vector<FStatRow> Rows;
		auto Take = [&Rows, &Merged](const std::string& Key)
		{
			const auto Found = Merged.find(Key);
			if (Found != Merged.end() && Found->second[0] + Found->second[1] > 0)
			{
				Rows.push_back({ShotLabel(Key), Found->second[0], Found->second[1], Key});
			}
			Merged.erase(Key);
		};
		for (const std::string_view Key : Order)
		{
			Take(std::string(Key));
		}
		// Shot codes the GDD doesn't list yet, alphabetically, so nothing played is hidden.
		while (!Merged.empty())
		{
			Take(Merged.begin()->first);
		}
		Rows.push_back({"Shots off the glass", OffGlassShots[0], OffGlassShots[1], "GlassShots"});
		return Rows;
	}

	std::vector<FStatRow> FMatchStats::EndingRows() const
	{
		std::vector<FStatRow> Rows;
		for (const auto& [Code, Counts] : Endings)
		{
			Rows.push_back({ReasonLabel(Code), Counts[0], Counts[1], Code});
		}
		// Stable on the map's alphabetical order, so ties always list the same way.
		std::stable_sort(Rows.begin(), Rows.end(), [](const FStatRow& A, const FStatRow& B)
		{
			return A.TeamA + A.TeamB > B.TeamA + B.TeamB;
		});
		return Rows;
	}
}
