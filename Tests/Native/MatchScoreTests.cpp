// GDD 3.1 and 3.2: scoring, serving order, serve sides and change of ends.
#include "doctest.h"

#include "MatchScore.h"

using namespace Padel;

namespace
{
	void Win(FMatchScore& S, ETeam Team, int Points)
	{
		for (int Index = 0; Index < Points; ++Index)
		{
			S.AwardPoint(Team);
		}
	}

	// Wins one game for Team (from 0-0 points), returning the last point's outcome.
	FPointOutcome WinGame(FMatchScore& S, ETeam Team)
	{
		FPointOutcome Last;
		for (int Index = 0; Index < 4; ++Index)
		{
			Last = S.AwardPoint(Team);
		}
		return Last;
	}

	void Games(FMatchScore& S, int A, int B)
	{
		// Alternate so neither side runs away with the set before the target score.
		while (A > 0 || B > 0)
		{
			if (A > 0) { WinGame(S, ETeam::A); --A; }
			if (B > 0) { WinGame(S, ETeam::B); --B; }
		}
	}
}

TEST_CASE("Rules.Score: points go 0, 15, 30, 40, game")
{
	FMatchScore S;
	CHECK(S.Snapshot().Points[0] == "0");
	S.AwardPoint(ETeam::A);
	CHECK(S.Snapshot().Points[0] == "15");
	S.AwardPoint(ETeam::A);
	CHECK(S.Snapshot().Points[0] == "30");
	S.AwardPoint(ETeam::A);
	CHECK(S.Snapshot().Points[0] == "40");
	const FPointOutcome Outcome = S.AwardPoint(ETeam::A);
	CHECK(Outcome.bGameWon);
	CHECK(Outcome.Team == ETeam::A);
	CHECK(S.Snapshot().Games[0] == 1);
	CHECK(S.Snapshot().Points[0] == "0");
}

TEST_CASE("Rules.Score: golden point, one deciding point at 40-40")
{
	FMatchScore S;
	Win(S, ETeam::A, 3);
	const FPointOutcome Deuce = [&]() { FPointOutcome O; for (int I = 0; I < 3; ++I) O = S.AwardPoint(ETeam::B); return O; }();
	CHECK(Deuce.bGoldenPointNext);
	CHECK(S.IsGoldenPoint());
	CHECK(S.Snapshot().bGoldenPoint);
	CHECK(S.Snapshot().bBreakPoint);
	const FPointOutcome Outcome = S.AwardPoint(ETeam::B);
	CHECK(Outcome.bGameWon);
	CHECK(Outcome.Team == ETeam::B);
}

TEST_CASE("Rules.Score: at the golden point the receiving pair chooses the receiver")
{
	FMatchScore S;
	Win(S, ETeam::A, 3);
	Win(S, ETeam::B, 3);
	// Team A serves the first game; Team B receives. Default: the normal side (right).
	CHECK(S.ServeSide() == EServeSide::Right);
	CHECK(S.ReceiverId() == 2);
	CHECK_FALSE(S.ChooseGoldenPointReceiver(1));
	CHECK(S.ChooseGoldenPointReceiver(3));
	CHECK(S.ReceiverId() == 3);
	CHECK(S.ServeSide() == EServeSide::Left);
	// The choice lasts only for that point.
	S.AwardPoint(ETeam::A);
	CHECK(S.ServeSide() == EServeSide::Right);
}

TEST_CASE("Rules.Score: advantage scoring when golden point is off")
{
	FMatchConfig Config;
	Config.bGoldenPoint = false;
	FMatchScore S(Config);
	Win(S, ETeam::A, 3);
	Win(S, ETeam::B, 3);
	CHECK_FALSE(S.IsGoldenPoint());
	CHECK(S.Snapshot().Points[0] == "40");
	S.AwardPoint(ETeam::A);
	CHECK(S.Snapshot().Points[0] == "AD");
	CHECK(S.Snapshot().Points[1] == "40");
	S.AwardPoint(ETeam::B);
	CHECK(S.Snapshot().Points[0] == "40");
	CHECK(S.Snapshot().Points[1] == "40");
	S.AwardPoint(ETeam::B);
	CHECK(S.Snapshot().Points[1] == "AD");
	CHECK(S.AwardPoint(ETeam::B).bGameWon);
	CHECK(S.Snapshot().Games[1] == 1);
}

TEST_CASE("Rules.Score: a set is first to 6 with a 2-game lead")
{
	FMatchScore S;
	Games(S, 5, 4);
	CHECK(WinGame(S, ETeam::A).bSetWon);
	CHECK(S.Snapshot().Sets.size() == 1);
	CHECK(S.Snapshot().Sets[0] == std::array<int, 2>{ 6, 4 });
	CHECK(S.Snapshot().Games == std::array<int, 2>{ 0, 0 });
}

TEST_CASE("Rules.Score: 6-5 is not a set, 7-5 is")
{
	FMatchScore S;
	Games(S, 5, 5);
	CHECK_FALSE(WinGame(S, ETeam::A).bSetWon);
	CHECK(S.Snapshot().Games == std::array<int, 2>{ 6, 5 });
	CHECK(WinGame(S, ETeam::A).bSetWon);
	CHECK(S.Snapshot().Sets[0] == std::array<int, 2>{ 7, 5 });
}

TEST_CASE("Rules.Score: a tiebreak at 6-6, first to 7 with a 2-point lead")
{
	FMatchScore S;
	Games(S, 6, 6);
	CHECK(S.InTiebreak());
	Win(S, ETeam::A, 6);
	Win(S, ETeam::B, 6);
	CHECK(S.Snapshot().Points[0] == "6");
	CHECK_FALSE(S.AwardPoint(ETeam::A).bSetWon);
	CHECK(S.Snapshot().Points[0] == "7");
	S.AwardPoint(ETeam::B);
	S.AwardPoint(ETeam::B);
	const FPointOutcome Outcome = S.AwardPoint(ETeam::B);
	CHECK(Outcome.bSetWon);
	CHECK(Outcome.Team == ETeam::B);
	CHECK(S.Snapshot().Sets[0] == std::array<int, 2>{ 6, 7 });
	CHECK_FALSE(S.InTiebreak());
}

TEST_CASE("Rules.Score: no golden point in a tiebreak")
{
	FMatchScore S;
	Games(S, 6, 6);
	Win(S, ETeam::A, 3);
	Win(S, ETeam::B, 3);
	CHECK_FALSE(S.IsGoldenPoint());
	CHECK_FALSE(S.AwardPoint(ETeam::A).bGameWon);
}

TEST_CASE("Rules.Score: best of 3, two sets win the match")
{
	FMatchScore S;
	Games(S, 6, 0);
	CHECK_FALSE(S.IsMatchOver());
	Games(S, 0, 6);
	CHECK_FALSE(S.IsMatchOver());
	Games(S, 5, 0);
	CHECK_FALSE(S.Snapshot().bMatchPoint);
	Win(S, ETeam::A, 3);
	CHECK(S.Snapshot().bMatchPoint);
	CHECK(S.Snapshot().bSetPoint);
	const FPointOutcome Outcome = S.AwardPoint(ETeam::A);
	CHECK(Outcome.bMatchWon);
	CHECK(S.IsMatchOver());
	CHECK(S.Winner() == ETeam::A);
	CHECK(S.Snapshot().SetsWon == std::array<int, 2>{ 2, 1 });
	// Nothing changes after the match.
	CHECK_FALSE(S.AwardPoint(ETeam::B).bGameWon);
}

TEST_CASE("Rules.Score: the one-set option ends after one set")
{
	FMatchScore S(FMatchConfig::OneSet());
	Games(S, 5, 3);
	CHECK(WinGame(S, ETeam::A).bMatchWon);
	CHECK(S.IsMatchOver());
}

TEST_CASE("Rules.Score: super tiebreak to 10 instead of the third set")
{
	FMatchConfig Config;
	Config.bSuperTiebreakFinal = true;
	FMatchScore S(Config);
	Games(S, 6, 0);
	Games(S, 0, 6);
	CHECK(S.InTiebreak());
	CHECK(S.Snapshot().bSuperTiebreak);
	Win(S, ETeam::B, 9);
	Win(S, ETeam::A, 9);
	CHECK_FALSE(S.IsMatchOver());
	S.AwardPoint(ETeam::B);
	CHECK_FALSE(S.IsMatchOver());
	S.AwardPoint(ETeam::B);
	CHECK(S.IsMatchOver());
	CHECK(S.Winner() == ETeam::B);
	CHECK(S.Snapshot().Sets.back() == std::array<int, 2>{ 0, 1 });
}

TEST_CASE("Rules.Score: change of ends after the 1st, 3rd, 5th... game of each set")
{
	FMatchScore S;
	CHECK(S.TeamAEnd() == 1);
	CHECK(WinGame(S, ETeam::A).bChangeOfEnds);
	CHECK(S.TeamAEnd() == -1);
	CHECK_FALSE(WinGame(S, ETeam::B).bChangeOfEnds);
	CHECK(WinGame(S, ETeam::A).bChangeOfEnds);
	CHECK(S.TeamAEnd() == 1);
	// 6-3 ends the set on 9 games: change, then again after the first game of set 2.
	Games(S, 4, 2);
	REQUIRE(S.Snapshot().Sets.size() == 1);
	CHECK(S.Snapshot().Sets[0] == std::array<int, 2>{ 6, 3 });
	CHECK(WinGame(S, ETeam::B).bChangeOfEnds);
}

TEST_CASE("Rules.Score: in a tiebreak, change ends every 6 points")
{
	FMatchScore S;
	Games(S, 6, 6);
	const int EndBefore = S.TeamAEnd();
	int Changes = 0;
	for (int Index = 0; Index < 12; ++Index)
	{
		if (S.AwardPoint(Index % 2 == 0 ? ETeam::A : ETeam::B).bChangeOfEnds)
		{
			++Changes;
			CHECK((Index + 1) % 6 == 0);
		}
	}
	CHECK(Changes == 2);
	CHECK(S.TeamAEnd() == EndBefore);
}

TEST_CASE("Rules.Serve: the serve alternates teams each game, order A1, B1, A2, B2")
{
	FMatchScore S;
	const int Expected[] = { 0, 2, 1, 3, 0, 2 };
	for (int Game = 0; Game < 6; ++Game)
	{
		CHECK(S.ServerId() == Expected[Game]);
		WinGame(S, Game % 2 == 0 ? ETeam::A : ETeam::B);
	}
}

TEST_CASE("Rules.Serve: the server keeps the serve for the whole game")
{
	FMatchScore S;
	for (int Point = 0; Point < 3; ++Point)
	{
		CHECK(S.ServerId() == 0);
		S.AwardPoint(Point % 2 == 0 ? ETeam::A : ETeam::B);
	}
	CHECK(S.ServerId() == 0);
}

TEST_CASE("Rules.Serve: first point from the right, then alternating")
{
	FMatchScore S;
	CHECK(S.ServeSide() == EServeSide::Right);
	CHECK(S.ReceiverId() == 2);
	S.AwardPoint(ETeam::A);
	CHECK(S.ServeSide() == EServeSide::Left);
	CHECK(S.ReceiverId() == 3);
	S.AwardPoint(ETeam::B);
	CHECK(S.ServeSide() == EServeSide::Right);
	// Finish the game from 15-15: the next game starts from the right again.
	Win(S, ETeam::A, 3);
	CHECK(S.ServeSide() == EServeSide::Right);
}

TEST_CASE("Rules.Serve: the right-side player receives on the right")
{
	FMatchScore S;
	S.SetRightSidePlayer(ETeam::B, 3);
	CHECK(S.ReceiverId() == 3);
	S.AwardPoint(ETeam::A);
	CHECK(S.ReceiverId() == 2);
}

TEST_CASE("Rules.Serve: a team may change its order at the start of a set only")
{
	FMatchScore S;
	CHECK(S.SetServingOrder(ETeam::A, 1));
	CHECK(S.ServerId() == 1);
	WinGame(S, ETeam::A);
	CHECK(S.ServerId() == 2);
	WinGame(S, ETeam::A);
	CHECK(S.ServerId() == 0);
	CHECK_FALSE(S.SetServingOrder(ETeam::B, 3));
}

TEST_CASE("Rules.Serve: tiebreak, next server takes point 1 from the right, then two each from the left")
{
	FMatchScore S;
	Games(S, 6, 6);
	// 12 games served: rotation is back to A1 for the tiebreak.
	CHECK(S.ServerId() == 0);
	CHECK(S.ServeSide() == EServeSide::Right);
	const int Servers[] = { 0, 2, 2, 1, 1, 3, 3, 0, 0 };
	const EServeSide Sides[] = { EServeSide::Right, EServeSide::Left, EServeSide::Right, EServeSide::Left, EServeSide::Right,
		EServeSide::Left, EServeSide::Right, EServeSide::Left, EServeSide::Right };
	for (int Point = 0; Point < 9; ++Point)
	{
		CHECK(S.ServerId() == Servers[Point]);
		CHECK(S.ServeSide() == Sides[Point]);
		S.AwardPoint(Point % 2 == 0 ? ETeam::A : ETeam::B);
	}
}

TEST_CASE("Rules.Serve: after a tiebreak the team that received first serves the next set")
{
	FMatchScore S;
	Games(S, 6, 6);
	REQUIRE(S.ServerId() == 0);
	Win(S, ETeam::A, 7);
	REQUIRE(S.Snapshot().Sets.size() == 1);
	CHECK(S.ServingTeam() == ETeam::B);
}

TEST_CASE("Rules.Score: break point and set point flags for the pressure model")
{
	FMatchScore S;
	Win(S, ETeam::B, 3);
	CHECK(S.Snapshot().bBreakPoint);
	CHECK_FALSE(S.Snapshot().bSetPoint);
	FMatchScore T;
	// 6 games played: A1 serves the 7th, at 5-1.
	Games(T, 5, 1);
	Win(T, ETeam::A, 3);
	CHECK_FALSE(T.Snapshot().bBreakPoint);
	CHECK(T.Snapshot().bSetPoint);
	CHECK_FALSE(T.Snapshot().bMatchPoint);
}

TEST_CASE("Rules.Score: Team B can serve first")
{
	FMatchConfig Config;
	Config.FirstServingTeam = ETeam::B;
	FMatchScore S(Config);
	CHECK(S.ServerId() == 2);
	CHECK(S.ReceiverId() == 0);
	WinGame(S, ETeam::B);
	CHECK(S.ServerId() == 0);
}
