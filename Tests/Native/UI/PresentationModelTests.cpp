// The demo match that drives the UI before the referee lands. The camera maths is in CameraRigTests.cpp.
#include "doctest.h"

#include "CourtSpec.h"
#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiDemoMatch.h"
#include "UI/Model/PadelUiEventSink.h"
#include "UI/Model/PadelUiMatchStats.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Padel::Ui;
using doctest::Approx;

namespace
{
	// Collects events in order and keeps stats, as the HUD would.
	class FRecordingSink final : public IUiEventSink
	{
	public:
		std::vector<std::string> Events;
		FMatchStats Stats;
		FScoreView LastScore;
		int GoldenPoints = 0;
		int MatchWinner = -1;

		void OnServeStarted(int) override { Events.push_back("serve"); Stats.OnServeStarted(); }
		void OnServeFault(std::string_view) override { Events.push_back("fault"); }
		void OnServeLet() override { Events.push_back("let"); }
		void OnBallHit(int PlayerId, std::string_view Shot, std::string_view Grade, double) override
		{
			Events.push_back("hit");
			Stats.OnBallHit(PlayerId, Shot, Grade);
		}
		void OnBallBounced(std::string_view Surface, int, const FUiVec3&) override { Stats.OnBallBounced(Surface); }
		void OnBallLeftCourt(const FUiVec3&) override {}
		void OnPointEnded(int Team, std::string_view Reason, int) override
		{
			Events.push_back("point");
			Stats.OnPointEnded(Team, Reason);
		}
		void OnScoreChanged(const FScoreView& Score) override { Events.push_back("score"); LastScore = Score; }
		void OnGameWon(int) override { Events.push_back("game"); }
		void OnSetWon(int) override { Events.push_back("set"); }
		void OnMatchWon(int Team) override
		{
			Events.push_back("match");
			MatchWinner = Team;
		}
		void OnChangeOfEnds() override { Events.push_back("ends"); }
		void OnGoldenPointStarted(int) override { ++GoldenPoints; }
	};
}

TEST_CASE("UI.Demo: plays a best-of-3 to the end, the same way for a seed")
{
	FDemoMatchOptions Options;
	Options.Seed = 42;
	FRecordingSink First;
	FDemoMatch Match(Options, First);
	Match.Start();
	int Points = 0;
	while (!Match.IsOver() && Points < 2000)
	{
		Match.PlayPoint();
		++Points;
	}
	REQUIRE(Match.IsOver());
	CHECK(First.MatchWinner >= 0);
	const FScoreView& Final = First.LastScore;
	CHECK(Final.bMatchOver);
	CHECK(Final.Winner == First.MatchWinner);
	// Two sets for the winner, in a best of 3.
	int SetsForWinner = 0;
	for (const auto& Set : Final.Sets)
	{
		const int Mine = Set[static_cast<std::size_t>(First.MatchWinner)];
		const int Theirs = Set[static_cast<std::size_t>(OtherTeam(First.MatchWinner))];
		CHECK(Mine != Theirs);
		CHECK(std::max(Mine, Theirs) >= 6);
		SetsForWinner += Mine > Theirs ? 1 : 0;
	}
	CHECK(SetsForWinner == 2);
	CHECK(First.Stats.TotalPoints() == Points);
	CHECK(First.Stats.PointsWithGlass > 0);

	FRecordingSink Second;
	FDemoMatch Again(Options, Second);
	Again.Start();
	while (!Again.IsOver())
	{
		Again.PlayPoint();
	}
	CHECK(Second.Events == First.Events);
}

TEST_CASE("UI.Demo: every point ends with exactly one point_ended, then the score")
{
	FDemoMatchOptions Options;
	Options.Seed = 7;
	Options.SetsToWin = 1;
	FRecordingSink Sink;
	FDemoMatch Match(Options, Sink);
	Match.Start();
	while (!Match.IsOver())
	{
		Match.PlayPoint();
	}
	int Serves = 0;
	int Points = 0;
	for (std::size_t I = 0; I < Sink.Events.size(); ++I)
	{
		if (Sink.Events[I] == "serve")
		{
			++Serves;
			CHECK(Serves == Points + 1);
		}
		if (Sink.Events[I] == "point")
		{
			++Points;
		}
	}
	CHECK(Serves == Points);
	CHECK(Sink.Events.back() == "score");
	CHECK(Sink.LastScore.Sets.size() == 1);
}

TEST_CASE("UI.Demo: golden point is announced at 40-40 and Step paces points")
{
	FDemoMatchOptions Options;
	Options.Seed = 3;
	Options.SecondsPerPoint = 2.0;
	FRecordingSink Sink;
	FDemoMatch Match(Options, Sink);
	Match.Start();
	Match.Step(1.9);
	CHECK(Sink.Stats.TotalPoints() == 0);
	Match.Step(0.2);
	CHECK(Sink.Stats.TotalPoints() == 1);
	while (!Match.IsOver())
	{
		Match.PlayPoint();
	}
	CHECK(Sink.GoldenPoints > 0);
	Match.ChooseGoldenPointReceiver(1);
	CHECK(Match.GetGoldenReceiver() == 1);
	CHECK_FALSE(Match.Step(10.0));
}
