#include "UI/Model/PadelUiDemoMatch.h"

#include "UI/Model/PadelUiCodes.h"

#include <array>
#include <string_view>

namespace Padel::Ui
{
	namespace
	{
		// GDD 3.2 serving order A1, B1, A2, B2 (player ids 0, 2, 1, 3).
		constexpr std::array<int, 4> ServeOrder{0, 2, 1, 3};

		constexpr std::array<std::string_view, 7> RallyShots{
			"DRIVE", "VOLLEY", "LOB", "BANDEJA", "VIBORA", "SMASH", "CHIQUITA"};
		constexpr std::array<std::string_view, 5> Grades{"PERFECT", "GOOD", "GOOD", "EARLY", "LATE"};
		constexpr std::array<std::string_view, 3> WinnerEndings{"DOUBLE_BOUNCE", "DOUBLE_BOUNCE", "OUT_OF_COURT_WINNER"};
		constexpr std::array<std::string_view, 4> ErrorEndings{"INTO_NET", "OUT_DIRECT", "WALL_BEFORE_BOUNCE", "INTO_NET"};
	}

	FDemoMatch::FDemoMatch(const FDemoMatchOptions& InOptions, IUiEventSink& InSink)
		: Options(InOptions)
		, Sink(InSink)
		, RngState(InOptions.Seed == 0 ? 1u : InOptions.Seed)
	{
		Score.bGoldenPoint = Options.bGoldenPoint;
		Score.ServerId = ServerFor(0);
		Score.ServingTeam = TeamOfPlayer(Score.ServerId);
	}

	std::uint32_t FDemoMatch::Next()
	{
		// xorshift32: tiny, and identical on every compiler, unlike <random>'s distributions.
		RngState ^= RngState << 13;
		RngState ^= RngState >> 17;
		RngState ^= RngState << 5;
		return RngState;
	}

	int FDemoMatch::Pick(int Count)
	{
		return Count <= 0 ? 0 : static_cast<int>(Next() % static_cast<std::uint32_t>(Count));
	}

	int FDemoMatch::ServerFor(int GameIndex) const
	{
		return ServeOrder[static_cast<std::size_t>(GameIndex % 4)];
	}

	void FDemoMatch::Start()
	{
		Publish();
	}

	bool FDemoMatch::Step(double DeltaSeconds)
	{
		if (bOver)
		{
			return false;
		}
		Clock += DeltaSeconds;
		while (Clock >= Options.SecondsPerPoint && !bOver)
		{
			Clock -= Options.SecondsPerPoint;
			PlayPoint();
		}
		return !bOver;
	}

	void FDemoMatch::PlayPoint()
	{
		if (bOver)
		{
			return;
		}
		const int Server = Score.ServerId;
		const int ServingTeam = TeamOfPlayer(Server);
		Sink.OnServeStarted(Server);

		// About one point in twelve starts with a fault, one in forty is a double fault.
		const int ServeRoll = Pick(40);
		if (ServeRoll < 4)
		{
			Sink.OnServeFault("WRONG_BOX");
			if (ServeRoll == 0)
			{
				Sink.OnServeFault("MESH_AFTER_BOUNCE");
				Sink.OnPointEnded(OtherTeam(ServingTeam), "DOUBLE_FAULT", Server);
				AwardPoint(OtherTeam(ServingTeam));
				return;
			}
		}
		else if (ServeRoll == 39)
		{
			Sink.OnServeLet();
		}

		const int Hits = 1 + Pick(10);
		int Hitter = Server;
		int LastHitter = Server;
		for (int Hit = 0; Hit < Hits; ++Hit)
		{
			const int Team = TeamOfPlayer(Hitter);
			if (Hit > 0 && Pick(10) < 4)
			{
				// The ball came off this team's glass before they played it.
				const double Side = Team == TeamA ? 1.0 : -1.0;
				Sink.OnBallBounced("glass", Team, {Side * 10.0, (Pick(9) - 4) * 1.0, 1.2});
			}
			const std::string_view Shot = Hit == 0 ? std::string_view("SERVE_FLAT") : RallyShots[static_cast<std::size_t>(Pick(7))];
			Sink.OnBallHit(Hitter, Shot, Grades[static_cast<std::size_t>(Pick(5))], 15.0 + Pick(20));
			const double Far = Team == TeamA ? -1.0 : 1.0;
			Sink.OnBallBounced("ground", OtherTeam(Team), {Far * (2.0 + Pick(7)), (Pick(9) - 4) * 1.0, 0.0});
			LastHitter = Hitter;
			// Next hitter: a player of the other team.
			Hitter = OtherTeam(Team) * 2 + Pick(2);
		}

		const int LastTeam = TeamOfPlayer(LastHitter);
		int Winner = LastTeam;
		std::string_view Reason;
		if (Pick(2) == 0)
		{
			Reason = WinnerEndings[static_cast<std::size_t>(Pick(3))];
		}
		else
		{
			Reason = ErrorEndings[static_cast<std::size_t>(Pick(4))];
			Winner = OtherTeam(LastTeam);
		}
		Sink.OnPointEnded(Winner, Reason, LastHitter);
		AwardPoint(Winner);
	}

	void FDemoMatch::AwardPoint(int Team)
	{
		++PointCount[static_cast<std::size_t>(Team)];
		const int Mine = PointCount[static_cast<std::size_t>(Team)];
		const int Theirs = PointCount[static_cast<std::size_t>(OtherTeam(Team))];
		bool bGameOver = false;
		if (Score.bInTiebreak)
		{
			bGameOver = Mine >= 7 && Mine - Theirs >= 2;
			if (!bGameOver && (Mine + Theirs) % 6 == 0)
			{
				// GDD 3.1: change ends every 6 points in a tiebreak.
				Score.TeamAEnd = -Score.TeamAEnd;
				Sink.OnChangeOfEnds();
			}
		}
		else if (Options.bGoldenPoint)
		{
			bGameOver = Mine >= 4;
		}
		else
		{
			bGameOver = Mine >= 4 && Mine - Theirs >= 2;
		}

		if (bGameOver)
		{
			AwardGame(Team);
			return;
		}
		Publish();
		if (!Score.bInTiebreak && Options.bGoldenPoint && PointCount[0] == 3 && PointCount[1] == 3)
		{
			Sink.OnGoldenPointStarted(OtherTeam(Score.ServingTeam));
		}
	}

	void FDemoMatch::AwardGame(int Team)
	{
		PointCount = {0, 0};
		++Score.Games[static_cast<std::size_t>(Team)];
		++GamesPlayedInSet;
		++GamesPlayedInMatch;
		Sink.OnGameWon(Team);

		const int Mine = Score.Games[static_cast<std::size_t>(Team)];
		const int Theirs = Score.Games[static_cast<std::size_t>(OtherTeam(Team))];
		const bool bSetOver = (Mine >= 6 && Mine - Theirs >= 2) || (Score.bInTiebreak && Mine == 7);
		const bool bOddGames = GamesPlayedInSet % 2 == 1;
		Score.bInTiebreak = false;

		if (bSetOver)
		{
			Score.Sets.push_back(Score.Games);
			Score.Games = {0, 0};
			GamesPlayedInSet = 0;
			++SetsWon[static_cast<std::size_t>(Team)];
			Sink.OnSetWon(Team);
			if (SetsWon[static_cast<std::size_t>(Team)] >= Options.SetsToWin)
			{
				bOver = true;
				Score.bMatchOver = true;
				Score.Winner = Team;
				Sink.OnMatchWon(Team);
				Publish();
				return;
			}
		}
		else if (Score.Games[0] == 6 && Score.Games[1] == 6)
		{
			Score.bInTiebreak = true;
		}

		if (bOddGames)
		{
			// GDD 3.1: change ends after the 1st, 3rd, 5th... game of each set.
			Score.TeamAEnd = -Score.TeamAEnd;
			Sink.OnChangeOfEnds();
		}
		Score.ServerId = ServerFor(GamesPlayedInMatch);
		Score.ServingTeam = TeamOfPlayer(Score.ServerId);
		Publish();
	}

	void FDemoMatch::Publish()
	{
		const int Serving = Score.ServingTeam;
		const int Receiving = OtherTeam(Serving);
		if (Score.bInTiebreak)
		{
			Score.Points = {std::to_string(PointCount[0]), std::to_string(PointCount[1])};
		}
		else if (!Options.bGoldenPoint && PointCount[0] >= 3 && PointCount[1] >= 3 && PointCount[0] != PointCount[1])
		{
			const bool bALeads = PointCount[0] > PointCount[1];
			Score.Points = {bALeads ? "AD" : "40", bALeads ? "40" : "AD"};
		}
		else
		{
			Score.Points = {PointLabelFromCount(PointCount[0]), PointLabelFromCount(PointCount[1])};
		}
		const int ReceiverPoints = PointCount[static_cast<std::size_t>(Receiving)];
		const int ServerPoints = PointCount[static_cast<std::size_t>(Serving)];
		Score.bBreakPoint = !Score.bInTiebreak && !bOver && ReceiverPoints >= 3
			&& (ReceiverPoints > ServerPoints || (Options.bGoldenPoint && ReceiverPoints == ServerPoints));
		Score.bServeFromRight = (PointCount[0] + PointCount[1]) % 2 == 0;
		Sink.OnScoreChanged(Score);
	}
}
