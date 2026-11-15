// The score tracker (GDD 3.1, 3.2): points, games, sets, tiebreaks, golden point,
// serving order, serve side and change of ends.

#pragma once

#include "MatchConfig.h"

#include <array>
#include <string>
#include <vector>

namespace Padel
{
	// What the HUD shows (GDD 3.4 score_changed).
	struct FScoreSnapshot
	{
		// Display strings: "0", "15", "30", "40", "AD", or tiebreak numbers.
		std::array<std::string, 2> Points;
		std::array<int, 2> RawPoints{};
		std::array<int, 2> Games{};
		std::array<int, 2> SetsWon{};
		// Finished sets, games per team (a tiebreak set reads 7-6).
		std::vector<std::array<int, 2>> Sets;
		int SetIndex = 0;
		bool bInTiebreak = false;
		bool bSuperTiebreak = false;
		bool bGoldenPoint = false;
		ETeam ServingTeam = ETeam::A;
		int ServerId = 0;
		int ReceiverId = 2;
		EServeSide ServeSide = EServeSide::Right;
		// +1 while Team A is on the +X half, -1 after an odd number of changes.
		int TeamAEnd = 1;
		// The receiving team wins the game with the next point (GDD 5.3 pressure).
		bool bBreakPoint = false;
		// Either team wins the set or the match with the next point.
		bool bSetPoint = false;
		bool bMatchPoint = false;
		bool bMatchOver = false;
		ETeam Winner = ETeam::A;
	};

	// Everything one point changed, in the order the events go out.
	struct FPointOutcome
	{
		bool bGameWon = false;
		bool bSetWon = false;
		bool bMatchWon = false;
		ETeam Team = ETeam::A;
		bool bChangeOfEnds = false;
		// The next point is a golden point (GDD 3.1).
		bool bGoldenPointNext = false;
	};

	class FMatchScore
	{
	public:
		explicit FMatchScore(const FMatchConfig& InConfig = FMatchConfig());

		FPointOutcome AwardPoint(ETeam Team);

		// --- Who serves and receives the next point ---
		int ServerId() const;
		ETeam ServingTeam() const { return TeamOfPlayer(ServerId()); }
		ETeam ReceivingTeam() const { return OtherTeam(ServingTeam()); }
		EServeSide ServeSide() const;
		int ReceiverId() const;

		// GDD 3.1: at a golden point the receiving pair picks which player receives.
		bool IsGoldenPoint() const;
		bool ChooseGoldenPointReceiver(int PlayerId);

		// GDD 3.2: a team may change its serving order at the start of each set.
		bool SetServingOrder(ETeam Team, int FirstServerId);
		// Which player of a team plays the right side (receives on the right).
		void SetRightSidePlayer(ETeam Team, int PlayerId);
		int RightSidePlayer(ETeam Team) const { return RightPlayer[TeamIndex(Team)]; }

		// +1 while Team A is on the +X half.
		int TeamAEnd() const { return TeamAEndSign; }
		// +1 or -1: the X sign of the half the team defends now.
		int HalfSignOf(ETeam Team) const { return Team == ETeam::A ? TeamAEndSign : -TeamAEndSign; }

		bool IsMatchOver() const { return bMatchOver; }
		ETeam Winner() const { return MatchWinner; }
		bool InTiebreak() const { return bTiebreak; }
		const FMatchConfig& Config() const { return Rules; }

		FScoreSnapshot Snapshot() const;

	private:
		bool IsStartOfSet() const;
		void StartNextGame();
		void WinGame(ETeam Team, FPointOutcome& Outcome);
		int TiebreakTarget() const;
		bool PointWinsGame(ETeam Team) const;
		bool PointWinsSet(ETeam Team) const;
		bool PointWinsMatch(ETeam Team) const;
		std::string PointText(int Team) const;

		FMatchConfig Rules;
		std::array<int, 2> Points{};
		std::array<int, 2> Games{};
		std::array<int, 2> SetsWon{};
		std::vector<std::array<int, 2>> FinishedSets;
		bool bTiebreak = false;
		bool bSuperTiebreak = false;
		// GDD 3.2: A1, B1, A2, B2, kept for the set.
		std::array<int, 4> Rotation{ 0, 2, 1, 3 };
		// Index in Rotation of the player serving this game (or the tiebreak's first server).
		int RotationIndex = 0;
		std::array<int, 2> RightPlayer{ 0, 2 };
		int TeamAEndSign = 1;
		int GoldenReceiver = -1;
		bool bMatchOver = false;
		ETeam MatchWinner = ETeam::A;
	};
}
