// The adapter from the real match simulation to the UI: codes, score view and a whole
// match played through it (GDD 3.4, 12 item 1).
#include "doctest.h"
#include "../ScriptedPlayers.h"
#include "../TestSupport.h"

#include "UI/Model/PadelUiCodes.h"
#include "UI/Model/PadelUiCoreAdapter.h"
#include "UI/Model/PadelUiMatchStats.h"
#include "UI/Model/PadelUiScore.h"

#include <string>
#include <vector>

using namespace Padel;

namespace
{
	struct FRecordingSink final : Ui::IUiEventSink
	{
		std::vector<std::string> Events;
		std::vector<std::string> Reasons;
		std::vector<std::string> Surfaces;
		std::vector<std::string> Shots;
		std::vector<std::string> Grades;
		Ui::FScoreView LastScore;
		Ui::FMatchStats Stats;
		int MatchWinner = -1;

		void OnServeStarted(int) override { Events.push_back("serve_started"); Stats.OnServeStarted(); }
		void OnServeFault(std::string_view Fault) override { Events.push_back("serve_fault:" + std::string(Fault)); }
		void OnServeLet() override { Events.push_back("serve_let"); }
		void OnBallHit(int Player, std::string_view Shot, std::string_view Grade, double) override
		{
			Events.push_back("ball_hit");
			Shots.emplace_back(Shot);
			Grades.emplace_back(Grade);
			Stats.OnBallHit(Player, Shot, Grade);
		}
		void OnBallBounced(std::string_view Surface, int, const Ui::FUiVec3&) override
		{
			Surfaces.emplace_back(Surface);
			Stats.OnBallBounced(Surface);
		}
		void OnBallLeftCourt(const Ui::FUiVec3&) override { Events.push_back("ball_left_court"); }
		void OnPointEnded(int Team, std::string_view Reason, int) override
		{
			Events.push_back("point_ended");
			Reasons.emplace_back(Reason);
			Stats.OnPointEnded(Team, Reason);
		}
		void OnScoreChanged(const Ui::FScoreView& Score) override { Events.push_back("score_changed"); LastScore = Score; }
		void OnGameWon(int) override { Events.push_back("game_won"); }
		void OnSetWon(int) override { Events.push_back("set_won"); }
		void OnMatchWon(int Team) override { Events.push_back("match_won"); MatchWinner = Team; }
		void OnChangeOfEnds() override { Events.push_back("change_of_ends"); }
		void OnGoldenPointStarted(int) override { Events.push_back("golden_point_started"); }
	};

	void PlayWholeMatch(FMatchSimulation& Match, Test::FScriptedPlayers& Players)
	{
		Match.StartMatch();
		int PointsStarted = 0;
		while (Match.Phase() != EMatchPhase::MatchOver && PointsStarted < 400)
		{
			Match.BeginPoint();
			++PointsStarted;
			int Ticks = 0;
			while (Ticks < 120 * 60 && Match.Phase() != EMatchPhase::PointOver && Match.Phase() != EMatchPhase::MatchOver)
			{
				Players.Act();
				Match.Tick();
				++Ticks;
			}
		}
	}
}

TEST_CASE("UI.CoreAdapter: the score snapshot maps field for field onto the score view")
{
	FScoreSnapshot S;
	S.Points = {"40", "AD"};
	S.Games = {5, 4};
	S.Sets = {{6, 3}};
	S.ServingTeam = ETeam::B;
	S.ServerId = 3;
	S.ReceiverId = 1;
	S.ServeSide = EServeSide::Left;
	S.TeamAEnd = -1;
	S.bBreakPoint = true;
	S.bSetPoint = true;

	Ui::FScoreView V = Ui::ToScoreView(S, false);
	CHECK(V.Points[0] == "40");
	CHECK(V.Points[1] == "AD");
	CHECK(V.Games[0] == 5);
	CHECK(V.Sets.size() == 1);
	CHECK(V.ServingTeam == 1);
	CHECK(V.ServerId == 3);
	CHECK(V.ReceiverId == 1);
	CHECK_FALSE(V.bServeFromRight);
	CHECK(V.TeamAEnd == -1);
	CHECK(V.bBreakPoint);
	CHECK(V.bSetPoint);
	CHECK_FALSE(V.bGoldenPoint);
	CHECK(V.Winner == -1);
	CHECK(Ui::PressureTag(V) == "SET POINT");

	// The snapshot's flag means "this point is golden", the view's means "the rule is on".
	S.Points = {"40", "40"};
	S.bGoldenPoint = true;
	CHECK(Ui::IsGoldenPointNow(Ui::ToScoreView(S, false)));
	S.bGoldenPoint = false;
	CHECK(Ui::ToScoreView(S, true).bGoldenPoint);

	S.bMatchOver = true;
	S.Winner = ETeam::B;
	CHECK(Ui::ToScoreView(S, true).Winner == 1);
}

TEST_CASE("UI.CoreAdapter: halves, setup and codes")
{
	CHECK(Ui::HalfIndex(Court::EHalf::A) == 0);
	CHECK(Ui::HalfIndex(Court::EHalf::B) == 1);

	Ui::FMatchSetup Setup;
	Setup.SetsToWin = 1;
	Setup.bGoldenPoint = false;
	const FMatchConfig Config = Ui::ToMatchConfig(Setup);
	CHECK(Config.SetsToWin == 1);
	CHECK_FALSE(Config.bGoldenPoint);

	FRecordingSink Sink;
	Ui::FCoreEventAdapter Adapter(Sink, true);
	Adapter.OnPointEnded(ETeam::B, EPointReason::WallBeforeBounce, 1);
	CHECK(Sink.Reasons.back() == "WALL_BEFORE_BOUNCE");
	CHECK(Ui::ReasonLabel(Sink.Reasons.back()) == "Glass on the fly");
	Adapter.OnBallHit(1, EShotType::Volley, ETimingGrade::Perfect, 20.0, {});
	CHECK(Ui::ShotLabel(Sink.Shots.back()) == "Volley");
	CHECK(Ui::NormaliseCode(Sink.Grades.back()) == "PERFECT");
	Adapter.OnBallBounced(Court::ESurface::Glass, Court::EHalf::B, {});
	CHECK(Sink.Surfaces.back() == "glass");
}

TEST_CASE("UI.CoreAdapter: a whole simulated match reaches the stats screen")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 4);
	FRecordingSink Sink;
	Ui::FCoreEventAdapter Adapter(Sink, true);
	Match.AddListener(&Adapter);
	Test::FScriptedPlayers Players(Match, 21);
	Players.bRandomErrors = true;
	PlayWholeMatch(Match, Players);
	Match.RemoveListener(&Adapter);

	REQUIRE(Match.Phase() == EMatchPhase::MatchOver);
	CHECK(Sink.LastScore.bMatchOver);
	CHECK(Sink.MatchWinner == Sink.LastScore.Winner);
	CHECK(Sink.Events.back() == "score_changed");
	CHECK(Sink.LastScore.Sets.size() == 1);
	CHECK_FALSE(Ui::FinalLine(Sink.LastScore.Sets).empty());

	// Every reason the simulation sends has a readable label, and the stats add up.
	for (const std::string& Reason : Sink.Reasons)
	{
		CAPTURE(Reason);
		CHECK(Ui::ReasonLabel(Reason).find('_') == std::string::npos);
	}
	CHECK(Sink.Stats.PointsWon[0] + Sink.Stats.PointsWon[1] == static_cast<int>(Sink.Reasons.size()));
	CHECK(Sink.Stats.Winners[0] + Sink.Stats.Winners[1] + Sink.Stats.Errors[0] + Sink.Stats.Errors[1]
		== static_cast<int>(Sink.Reasons.size()));
	for (const std::string& Shot : Sink.Shots)
	{
		CAPTURE(Shot);
		CHECK_FALSE(Ui::ShotLabel(Shot).empty());
	}
}
