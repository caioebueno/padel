// End-of-match stats (GDD 11 mode 1 "to the stats screen"), built only from referee
// events (GDD 3.4), so they're the same whoever plays: human, AI or the soak test.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <array>
#include <map>
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
	};

	class FMatchStats
	{
	public:
		void OnServeStarted();
		// Codes are GDD names (see PadelUiCodes.h).
		void OnBallHit(int PlayerId, std::string_view ShotType, std::string_view TimingGrade);
		void OnBallBounced(std::string_view Surface);
		void OnPointEnded(int WinningTeam, std::string_view Reason);
		void OnMatchWon(int Team, const std::vector<std::array<int, 2>>& FinalSets);

		FTeamCounts PointsWon{0, 0};
		// Credited to the team that won the point.
		FTeamCounts Winners{0, 0};
		// Credited to the team that lost the point.
		FTeamCounts Errors{0, 0};
		FTeamCounts DoubleFaults{0, 0};
		// GDD 5.3: any shot played after the ball came off the glass.
		FTeamCounts OffGlassShots{0, 0};
		// Points with at least one off-the-glass shot (GDD 12 item 4 wants 25% or more).
		int PointsWithGlass = 0;
		// Reason code -> points won that way, per winning team.
		std::map<std::string, FTeamCounts> Endings;
		std::map<std::string, FTeamCounts> Shots;
		std::map<std::string, int> TimingGrades;
		std::vector<int> RallyLengths;
		int MatchWinner = -1;
		std::vector<std::array<int, 2>> FinalSets;

		int TotalPoints() const;
		double AverageRally() const;
		double GlassPointShare() const;

		// Points won, winners, errors, double faults, shots off the glass.
		std::vector<FStatRow> SummaryRows() const;
		// "How the points were won", most common first, labelled for the screen.
		std::vector<FStatRow> EndingRows() const;

	private:
		int HitsThisPoint = 0;
		bool bGlassSinceHit = false;
		bool bGlassThisPoint = false;
	};
}
