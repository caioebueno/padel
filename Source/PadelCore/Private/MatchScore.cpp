#include "MatchScore.h"

#include <cstdlib>

namespace Padel
{
	FMatchScore::FMatchScore(const FMatchConfig& InConfig) : Rules(InConfig)
	{
		if (Rules.FirstServingTeam == ETeam::B)
		{
			Rotation = { 2, 0, 3, 1 };
		}
	}

	int FMatchScore::TiebreakTarget() const
	{
		return bSuperTiebreak ? Rules.SuperTiebreakPoints : Rules.TiebreakPoints;
	}

	int FMatchScore::ServerId() const
	{
		if (!bTiebreak)
		{
			return Rotation[RotationIndex];
		}
		// GDD 3.2: the first tiebreak point by the next server, then two points each.
		const int Played = Points[0] + Points[1];
		const int Offset = Played == 0 ? 0 : 1 + (Played - 1) / 2;
		return Rotation[(RotationIndex + Offset) % 4];
	}

	EServeSide FMatchScore::ServeSide() const
	{
		if (IsGoldenPoint() && GoldenReceiver >= 0)
		{
			return GoldenReceiver == RightPlayer[TeamIndex(ReceivingTeam())] ? EServeSide::Right : EServeSide::Left;
		}
		// GDD 3.2: first point from the right, then alternate (tiebreaks too).
		return (Points[0] + Points[1]) % 2 == 0 ? EServeSide::Right : EServeSide::Left;
	}

	int FMatchScore::ReceiverId() const
	{
		const int Right = RightPlayer[TeamIndex(ReceivingTeam())];
		return ServeSide() == EServeSide::Right ? Right : PartnerOf(Right);
	}

	bool FMatchScore::IsGoldenPoint() const
	{
		return Rules.bGoldenPoint && !bTiebreak && !bMatchOver && Points[0] == 3 && Points[1] == 3;
	}

	bool FMatchScore::ChooseGoldenPointReceiver(int PlayerId)
	{
		if (!IsGoldenPoint() || !IsValidPlayer(PlayerId) || TeamOfPlayer(PlayerId) != ReceivingTeam())
		{
			return false;
		}
		GoldenReceiver = PlayerId;
		return true;
	}

	bool FMatchScore::IsStartOfSet() const
	{
		return Points[0] == 0 && Points[1] == 0 && Games[0] == 0 && Games[1] == 0;
	}

	bool FMatchScore::SetServingOrder(ETeam Team, int FirstServerId)
	{
		if (!IsStartOfSet() || !IsValidPlayer(FirstServerId) || TeamOfPlayer(FirstServerId) != Team)
		{
			return false;
		}
		// The team keeps its slots in the rotation; only who fills them changes.
		for (int Slot = 0; Slot < 4; ++Slot)
		{
			const int Index = (RotationIndex + Slot) % 4;
			if (TeamOfPlayer(Rotation[Index]) == Team)
			{
				Rotation[Index] = FirstServerId;
				Rotation[(Index + 2) % 4] = PartnerOf(FirstServerId);
				break;
			}
		}
		return true;
	}

	void FMatchScore::SetRightSidePlayer(ETeam Team, int PlayerId)
	{
		if (IsValidPlayer(PlayerId) && TeamOfPlayer(PlayerId) == Team)
		{
			RightPlayer[TeamIndex(Team)] = PlayerId;
		}
	}

	bool FMatchScore::PointWinsGame(ETeam Team) const
	{
		const int T = TeamIndex(Team);
		const int O = 1 - T;
		const int After = Points[T] + 1;
		if (bTiebreak)
		{
			return After >= TiebreakTarget() && After - Points[O] >= 2;
		}
		if (Rules.bGoldenPoint)
		{
			return After >= 4;
		}
		return After >= 4 && After - Points[O] >= 2;
	}

	bool FMatchScore::PointWinsSet(ETeam Team) const
	{
		if (!PointWinsGame(Team))
		{
			return false;
		}
		if (bTiebreak)
		{
			return true;
		}
		const int T = TeamIndex(Team);
		const int After = Games[T] + 1;
		return After >= Rules.GamesPerSet && After - Games[1 - T] >= 2;
	}

	bool FMatchScore::PointWinsMatch(ETeam Team) const
	{
		return PointWinsSet(Team) && SetsWon[TeamIndex(Team)] + 1 >= Rules.SetsToWin;
	}

	FPointOutcome FMatchScore::AwardPoint(ETeam Team)
	{
		FPointOutcome Outcome;
		Outcome.Team = Team;
		if (bMatchOver)
		{
			return Outcome;
		}
		const int T = TeamIndex(Team);
		if (PointWinsGame(Team))
		{
			WinGame(Team, Outcome);
		}
		else
		{
			++Points[T];
			// Advantage scoring: back to deuce.
			if (!bTiebreak && !Rules.bGoldenPoint && Points[0] >= 4 && Points[1] >= 4)
			{
				Points = { 3, 3 };
			}
			// GDD 3.1: change ends every 6 points in a tiebreak.
			if (bTiebreak && (Points[0] + Points[1]) % Rules.TiebreakChangeEvery == 0)
			{
				Outcome.bChangeOfEnds = true;
			}
		}
		if (Outcome.bChangeOfEnds)
		{
			TeamAEndSign = -TeamAEndSign;
		}
		Outcome.bGoldenPointNext = IsGoldenPoint();
		return Outcome;
	}

	void FMatchScore::WinGame(ETeam Team, FPointOutcome& Outcome)
	{
		const int T = TeamIndex(Team);
		Outcome.bGameWon = true;
		const bool bWasTiebreak = bTiebreak;
		++Games[T];
		const int GamesInSet = Games[0] + Games[1];

		const bool bSetWon = bWasTiebreak || (Games[T] >= Rules.GamesPerSet && Games[T] - Games[1 - T] >= 2);
		if (bSetWon)
		{
			Outcome.bSetWon = true;
			++SetsWon[T];
			FinishedSets.push_back(Games);
			if (SetsWon[T] >= Rules.SetsToWin)
			{
				Outcome.bMatchWon = true;
				bMatchOver = true;
				MatchWinner = Team;
			}
		}
		// GDD 3.1: change ends after the 1st, 3rd, 5th... game of each set; a set that
		// ends on an odd total also changes, and a tiebreak counts as one game.
		Outcome.bChangeOfEnds = !bMatchOver && GamesInSet % 2 == 1;

		Points = { 0, 0 };
		GoldenReceiver = -1;
		if (bMatchOver)
		{
			return;
		}
		if (bWasTiebreak)
		{
			// The team that received first in the tiebreak serves first in the next set.
			RotationIndex = (RotationIndex + 1) % 4;
		}
		else
		{
			StartNextGame();
		}
		bTiebreak = false;
		if (bSetWon)
		{
			Games = { 0, 0 };
			const int Deciding = Rules.SetsToWin - 1;
			if (Rules.bSuperTiebreakFinal && SetsWon[0] == Deciding && SetsWon[1] == Deciding)
			{
				bTiebreak = true;
				bSuperTiebreak = true;
			}
		}
		else if (Games[0] == Rules.GamesPerSet && Games[1] == Rules.GamesPerSet)
		{
			bTiebreak = true;
		}
	}

	void FMatchScore::StartNextGame()
	{
		RotationIndex = (RotationIndex + 1) % 4;
	}

	std::string FMatchScore::PointText(int Team) const
	{
		const int Mine = Points[Team];
		if (bTiebreak)
		{
			return std::to_string(Mine);
		}
		const int Theirs = Points[1 - Team];
		if (!Rules.bGoldenPoint && Mine >= 3 && Theirs >= 3)
		{
			return Mine > Theirs ? "AD" : "40";
		}
		static const char* Names[] = { "0", "15", "30", "40" };
		return Names[Mine < 4 ? Mine : 3];
	}

	FScoreSnapshot FMatchScore::Snapshot() const
	{
		FScoreSnapshot S;
		S.Points = { PointText(0), PointText(1) };
		S.RawPoints = Points;
		S.Games = Games;
		S.SetsWon = SetsWon;
		S.Sets = FinishedSets;
		S.SetIndex = static_cast<int>(FinishedSets.size());
		S.bInTiebreak = bTiebreak;
		S.bSuperTiebreak = bSuperTiebreak;
		S.bGoldenPoint = IsGoldenPoint();
		S.ServingTeam = ServingTeam();
		S.ServerId = ServerId();
		S.ReceiverId = ReceiverId();
		S.ServeSide = ServeSide();
		S.TeamAEnd = TeamAEndSign;
		S.bMatchOver = bMatchOver;
		S.Winner = MatchWinner;
		if (!bMatchOver)
		{
			S.bBreakPoint = !bTiebreak && PointWinsGame(ReceivingTeam());
			S.bSetPoint = PointWinsSet(ETeam::A) || PointWinsSet(ETeam::B);
			S.bMatchPoint = PointWinsMatch(ETeam::A) || PointWinsMatch(ETeam::B);
		}
		return S;
	}
}
