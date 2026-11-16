// End-of-match stats (GDD 11 mode 1 "to the stats screen"), built only from referee
// events (GDD 3.4), so they're the same whoever plays: human, AI or the soak test.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include "UI/Model/PadelUiScore.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Padel::Ui
{
	using FTeamCounts = std::array<int, 2>;

	struct FStatRow
	{
		std::string Label;
		int TeamA = 0;
		int TeamB = 0;
		// Stable id for the screen's copy: the spec 8 key after "Stats.Row." for summary
		// rows ("PointsWon"), the GDD code for shot and ending rows ("BANDEJA").
		std::string Key;
		// Errors and double faults: the lower number is the better one (spec 7.8).
		bool bLowerIsBetter = false;
	};

	class FMatchStats
	{
	public:
		void OnServeStarted();
		// Codes are GDD names (see PadelUiCodes.h).
		void OnBallHit(int PlayerId, std::string_view ShotType, std::string_view TimingGrade);
		void OnBallBounced(std::string_view Surface);
		void OnPointEnded(int WinningTeam, std::string_view Reason);
		// Every score_changed (GDD 3.4): tracks tie-break points per set (spec 7.8, W8).
		void OnScoreChanged(const FScoreView& Score);
		void OnMatchWon(int Team, const std::vector<std::array<int, 2>>& FinalSets);

		FTeamCounts PointsWon{0, 0};
		// Credited to the team that won the point.
		FTeamCounts Winners{0, 0};
		// Credited to the team that lost the point.
		FTeamCounts Errors{0, 0};
		FTeamCounts DoubleFaults{0, 0};
		// GDD 5.3: any shot played after the ball came off the glass.
		FTeamCounts OffGlassShots{0, 0};
		// Spec 7.8 (W8): points won by a team that played at least one shot off the glass
		// in that point. Not the same as OffGlassShots, which counts shots.
		FTeamCounts GlassPointsWon{0, 0};
		// Points with at least one off-the-glass shot (GDD 12 item 4 wants 25% or more).
		int PointsWithGlass = 0;
		// Reason code -> points won that way, per winning team.
		std::map<std::string, FTeamCounts> Endings;
		std::map<std::string, FTeamCounts> Shots;
		std::map<std::string, int> TimingGrades;
		std::vector<int> RallyLengths;
		int MatchWinner = -1;
		std::vector<std::array<int, 2>> FinalSets;
		// One entry per finished set, oldest first: the tie-break points when the set went
		// to a tie-break (GDD 3.1, 6-6), nothing otherwise (spec 7.8, W8).
		std::vector<std::optional<FTeamCounts>> SetTiebreaks;

		int TotalPoints() const;
		double AverageRally() const;
		double GlassPointShare() const;
		// Smashes played per team (spec 7.8's summary row).
		FTeamCounts Smashes() const;
		// The tie-break of a finished set (0-based), if it had one.
		std::optional<FTeamCounts> TiebreakOfSet(int SetIndex) const;

		// Spec 7.8 SUMMARY: points won, winners, errors, smashes, points won off the glass,
		// double faults.
		std::vector<FStatRow> SummaryRows() const;
		// Spec 7.8 SHOTS: one row per shot type played, in GDD 4 order (both serves are
		// "Serve"), then "Shots off the glass".
		std::vector<FStatRow> ShotRows() const;
		// "How the points were won", most common first, labelled for the screen.
		std::vector<FStatRow> EndingRows() const;

	private:
		int HitsThisPoint = 0;
		bool bGlassSinceHit = false;
		bool bGlassThisPoint = false;
		std::array<bool, 2> GlassShotBy{false, false};
		// The score shown while the current point is played says whether it's a tie-break.
		bool bTiebreakNow = false;
		bool bTiebreakThisSet = false;
		FTeamCounts TiebreakPoints{0, 0};
	};
}
