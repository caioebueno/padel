// The whole match: ball, referee and score together, through real flights (GDD 3, 12).
#include "doctest.h"
#include "ScriptedPlayers.h"
#include "TestSupport.h"

#include <algorithm>

using namespace Padel;
using namespace Padel::Court;

namespace
{
	// Ticks until the point is over (or a safety limit). Returns the ticks used.
	int PlayPoint(FMatchSimulation& Match, Test::FScriptedPlayers& Players, int MaxTicks = 120 * 60)
	{
		int Ticks = 0;
		while (Ticks < MaxTicks && Match.Phase() != EMatchPhase::PointOver && Match.Phase() != EMatchPhase::MatchOver)
		{
			Players.Act();
			Match.Tick();
			++Ticks;
		}
		return Ticks;
	}
}

TEST_CASE("Match.Sim: a scripted rally plays out and ends on the deliberate error")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 11);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	Test::FScriptedPlayers Players(Match, 3);
	Players.ErrorAfterShots = 6;
	Match.StartMatch();
	Match.BeginPoint();
	PlayPoint(Match, Players);

	REQUIRE(Log.Points == 1);
	CHECK(Log.Faults.empty());
	// Serve plus six good shots, then the seventh into the net.
	CHECK(Log.Hitters.size() == 8);
	CHECK(Log.Reasons[0] == EPointReason::IntoNet);
	// The error was the 8th hit: player of the team that hit it loses.
	const ETeam Loser = TeamOfPlayer(Log.Hitters.back());
	CHECK(Match.Referee().LastWinner() == OtherTeam(Loser));
	// Hitters alternate teams.
	for (size_t Index = 1; Index < Log.Hitters.size(); ++Index)
	{
		CHECK(TeamOfPlayer(Log.Hitters[Index]) != TeamOfPlayer(Log.Hitters[Index - 1]));
	}
	CHECK(Log.Count("ball_bounced:ground") >= 7);
}

TEST_CASE("Match.Sim: returns off the back glass")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 5);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	Test::FScriptedPlayers Players(Match, 9);
	Players.ErrorAfterShots = 4;
	Players.bWaitForGlass = true;
	Players.DriveSpeed = 22.0;
	Match.StartMatch();
	Match.BeginPoint();
	PlayPoint(Match, Players);
	REQUIRE(Log.Points == 1);
	CHECK(Log.Count("ball_bounced:glass") >= 2);
	CHECK(Log.Hitters.size() >= 3);
}

TEST_CASE("Match.Sim: event order after a point (GDD 3.4)")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 1);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	Test::FScriptedPlayers Players(Match, 2);
	Players.ErrorAfterShots = 1;
	Match.StartMatch();
	CHECK(Log.Events.front() == "score_changed");
	for (int Point = 0; Point < 4 && Match.Phase() != EMatchPhase::MatchOver; ++Point)
	{
		Match.BeginPoint();
		PlayPoint(Match, Players);
	}
	// Each point: point_ended then score_changed; the game ends with game_won before them.
	const auto It = std::find(Log.Events.begin(), Log.Events.end(), "point_ended");
	REQUIRE(It != Log.Events.end());
	CHECK(*(It + 1) == "score_changed");
	CHECK(Log.Events[1] == "serve_started");
	CHECK(Log.Count("point_ended") == 4);
	CHECK(Log.Count("score_changed") == 5);
}

TEST_CASE("Match.Sim: a serve fault gives a second serve, two give the point away")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 4);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	Match.StartMatch();
	Match.BeginPoint();
	const FServeContact Contact = Match.DefaultServeContact();
	// Long: past the service line.
	FVec3 Long = Match.ServiceBoxCentre();
	Long.X *= 2.4;
	Match.PlayServe(Match.ServerId(), Contact, EShotType::ServeFlat, ETimingGrade::Good, Long, 14.0, {});
	for (int Tick = 0; Tick < 240 && Match.Phase() == EMatchPhase::Live; ++Tick)
	{
		Match.Tick();
	}
	REQUIRE(Log.Faults.size() == 1);
	CHECK(Log.Faults[0] == EServeFault::WrongBox);
	CHECK(Match.Phase() == EMatchPhase::AwaitingServe);
	CHECK(Match.Referee().ServeNumber() == 2);
	CHECK(Log.Count("serve_started") == 2);

	// Second serve above the waist: an immediate fault, and a double fault.
	FServeContact High = Contact;
	High.ContactPosition.Z = 1.5;
	Match.PlayServe(Match.ServerId(), High, EShotType::ServeFlat, ETimingGrade::Good, Match.ServiceBoxCentre(), 14.0, {});
	CHECK(Log.Faults.size() == 2);
	REQUIRE(Log.Points == 1);
	CHECK(Log.Reasons[0] == EPointReason::DoubleFault);
	CHECK(Log.LastScore.Points[1] == "15");
}

TEST_CASE("Match.Sim: a serve that clips the net and lands in the box is a let")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 4);
	Match.StartMatch();
	Match.BeginPoint();
	const FServeContact Contact = Match.DefaultServeContact(0.95, 1.0);
	// Hit straight at the tape: a slow ball, launched so it brushes the top of the net.
	const FVec3 Origin = Contact.ContactPosition;
	const FVec3 Box = Match.ServiceBoxCentre();
	// Search a launch that touches the net and still lands in the box.
	bool bLet = false;
	for (int Attempt = 0; Attempt < 150 && !bLet; ++Attempt)
	{
		FMatchSimulation Trial(FMatchConfig::OneSet(), Test::Physics(), 4);
		Test::FEventLog TrialLog;
		Trial.AddListener(&TrialLog);
		Trial.StartMatch();
		Trial.BeginPoint();
		const double Elev = Attempt * 0.004;
		const FVec3 Dir = SafeNormal(FVec3{ Box.X - Origin.X, Box.Y - Origin.Y, 0.0 });
		const FVec3 V = Dir * (11.0 * std::cos(Elev)) + UpVector * (11.0 * std::sin(Elev));
		Trial.Serve(Trial.ServerId(), Contact, EShotType::ServeFlat, ETimingGrade::Good, V, {});
		for (int Tick = 0; Tick < 360 && Trial.Phase() == EMatchPhase::Live; ++Tick)
		{
			Trial.Tick();
		}
		bLet = TrialLog.Lets == 1;
		if (bLet)
		{
			CHECK(TrialLog.Faults.empty());
			CHECK(Trial.Phase() == EMatchPhase::AwaitingServe);
			CHECK(Trial.Referee().ServeNumber() == 1);
		}
	}
	CHECK(bLet);
}

TEST_CASE("Match.Sim: the golden point lets the receivers choose, through the simulation")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 8);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	Match.StartMatch();
	FMatchScore& Score = Match.MutableScore();
	for (int Index = 0; Index < 3; ++Index)
	{
		Score.AwardPoint(ETeam::A);
		Score.AwardPoint(ETeam::B);
	}
	REQUIRE(Score.IsGoldenPoint());
	CHECK(Match.ChooseGoldenPointReceiver(3));
	Match.BeginPoint();
	CHECK(Match.Score().ReceiverId() == 3);
	CHECK(Match.Score().ServeSide() == EServeSide::Left);
	// The default serve stance follows the side, and is legal.
	Test::FScriptedPlayers Players(Match, 1);
	Players.ErrorAfterShots = 1;
	PlayPoint(Match, Players);
	CHECK(Log.Faults.empty());
	CHECK(Log.Count("game_won") == 1);
}

TEST_CASE("Match.Sim: the watchdog ends a point whose ball stops dead")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 8);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	Match.StartMatch();
	Match.BeginPoint();
	Match.PlayServe(Match.ServerId(), Match.DefaultServeContact(), EShotType::ServeFlat, ETimingGrade::Good,
		Match.ServiceBoxCentre(), 13.0, {});
	// Nobody returns it: it bounces twice and the point ends long before the watchdog.
	for (int Tick = 0; Tick < 120 * 30 && Match.Phase() == EMatchPhase::Live; ++Tick)
	{
		Match.Tick();
	}
	REQUIRE(Log.Points == 1);
	CHECK(Log.Reasons[0] == EPointReason::DoubleBounce);
}

TEST_CASE("Match.Sim: whole one-set matches finish, every point with one winner and a reason")
{
	for (uint64_t Seed = 1; Seed <= 3; ++Seed)
	{
		FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), Seed);
		Test::FEventLog Log;
		Match.AddListener(&Log);
		Test::FScriptedPlayers Players(Match, Seed * 77);
		Players.bRandomErrors = true;
		Match.StartMatch();
		int PointsStarted = 0;
		while (Match.Phase() != EMatchPhase::MatchOver && PointsStarted < 400)
		{
			Match.BeginPoint();
			++PointsStarted;
			const int Before = Log.Points;
			PlayPoint(Match, Players);
			CHECK(Log.Points == Before + 1);
		}
		CHECK(Log.bMatchWon);
		CHECK(Match.Score().IsMatchOver());
		for (EPointReason Reason : Log.Reasons)
		{
			CHECK(Reason != EPointReason::None);
			CHECK(Reason != EPointReason::StuckInFence);
		}
		const FScoreSnapshot S = Match.Score().Snapshot();
		CHECK(S.SetsWon[0] + S.SetsWon[1] == 1);
	}
}

TEST_CASE("Match.Sim: the same seed replays the same match")
{
	auto Play = [](uint64_t Seed)
	{
		FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), Seed);
		Test::FEventLog Log;
		Match.AddListener(&Log);
		Test::FScriptedPlayers Players(Match, 5);
		Players.ErrorAfterShots = 3;
		Match.StartMatch();
		for (int Point = 0; Point < 8; ++Point)
		{
			Match.BeginPoint();
			PlayPoint(Match, Players);
		}
		return std::make_pair(Log.Events, Match.Ball().Position);
	};
	const auto First = Play(21);
	const auto Second = Play(21);
	CHECK(First.first == Second.first);
	CHECK(First.second == Second.second);
}
