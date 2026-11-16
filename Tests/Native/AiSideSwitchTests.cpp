// GDD 8 side switch (movement doc 2.12, realism plan A4): a deep lob over one player at
// the net makes the partner cross behind to take it, and the pair plays on swapped sides
// until the point ends.
#include "doctest.h"
#include "AI/AiPairTactics.h"
#include "AI/AiTeamController.h"
#include "AI/AiTeamFrame.h"
#include "Support/AiTestData.h"

#include <cmath>

using namespace Padel::AI;
using Padel::FPadelRng;
using doctest::Approx;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	FAiTactics WithSwitch(double MinDepth)
	{
		FAiTactics T = TestData::Tactics();
		T.SideSwitchMinLobDepthM = MinDepth;
		return T;
	}

	// The pair at the net (team A, +X): right player at local X +1.75, left at -1.75.
	const std::array<FAiVec2, 2> NetLocal = { FAiVec2{ 1.75, 3.0 }, FAiVec2{ -1.75, 3.0 } };

	// Where a ball first touches anything, in team A's frame. A first bounce on team A's
	// ground is a landing; anything else (the net, the glass, the wrong half) is not.
	struct FFirstContact
	{
		bool bLanded = false;
		bool bLong = false;  // hit the glass or left the court before bouncing
		FAiVec2 Local;
	};

	FFirstContact FirstContact(const IAiBallModel& Model, FAiBall Ball)
	{
		for (int Step = 0; Step < 600; ++Step)
		{
			for (const FAiContact& C : Model.Step(Ball, Dt))
			{
				FFirstContact Out;
				Out.Local = TeamFrame::ToLocal(1, C.Position);
				Out.bLanded = C.Surface == Padel::Court::ESurface::Ground && C.Position.X > 0.0;
				Out.bLong = C.Surface != Padel::Court::ESurface::Ground && C.Surface != Padel::Court::ESurface::Net;
				return Out;
			}
			if (Model.IsDead(Ball))
			{
				return { false, true, {} };
			}
		}
		return { false, true, {} };
	}

	std::optional<FAiVec2> Landing(const IAiBallModel& Model, const FAiBall& Ball)
	{
		const FFirstContact C = FirstContact(Model, Ball);
		return C.bLanded ? std::optional<FAiVec2>(C.Local) : std::nullopt;
	}

	// An opponent's lob from (-8, Y0) that first bounces at about TargetDepth on team A's
	// half, found by bisection on the ball's forward speed (the engine model has drag).
	FAiBall Lob(const IAiBallModel& Model, double Y0, double TargetY, double TargetDepth)
	{
		double Lo = 3.0;
		double Hi = 20.0;
		FAiBall Ball;
		for (int I = 0; I < 40; ++I)
		{
			const double Vx = 0.5 * (Lo + Hi);
			const double Time = (8.0 + TargetDepth) / Vx;
			Ball = { { -8.0, Y0, 1.0 }, { Vx, (TargetY - Y0) / Time, 9.0 }, {} };
			const FFirstContact C = FirstContact(Model, Ball);
			const bool bTooLong = C.bLong || (C.bLanded && C.Local.Y > TargetDepth);
			(bTooLong ? Hi : Lo) = Vx;
		}
		return Ball;
	}

	FAiTeamSetup AllAi()
	{
		FAiTeamSetup S;
		S.HalfSign = 1;
		S.Profiles = { TestData::Player(ESide::Right), TestData::Player(ESide::Left) };
		S.Seed = 5;
		return S;
	}

	FAiWorldView NetView(double Time, const FAiBall& Ball, const std::array<FAiVec, 2>& Own)
	{
		FAiWorldView V;
		V.Time = Time;
		V.Ball = Ball;
		V.OwnPositions = Own;
		V.OpponentPositions = { FAiVec{ -8.5, 2.25, 0.0 }, FAiVec{ -8.5, -2.25, 0.0 } };
		return V;
	}

	struct FLobRun
	{
		bool bSwappedDuringFlight = false;
		int FirstTaker = -1;
		int Shots = 0;
		int ShotSlot = -1;
	};

	// Plays a lob at a pair standing at the net and moves the players as the AI asks.
	FLobRun PlayLob(FAiTeamController& Team, const IAiBallModel& Model, FAiBall Ball)
	{
		Team.OnPointStarted(false);
		Team.OnBallHit(0.0, 1, EAiShot::Volley, { 3.0, 0.0, 1.0 }, { -7.0, 0.0, 0.0 });  // own volley: both at the net
		REQUIRE(Team.GetPair().GetFormation() == EFormation::BothNet);
		Team.OnBallHit(0.1, -1, EAiShot::Lob, Ball.Position, { 8.5, 0.0, 0.0 });
		std::array<FAiVec, 2> Own;
		for (int Slot = 0; Slot < 2; ++Slot)
		{
			Own[Slot] = TeamFrame::ToCourt(1, NetLocal[Slot]);
		}
		FLobRun Run;
		double Time = 0.1;
		for (int Step = 0; Step < 480; ++Step)
		{
			Time += Dt;
			Model.Step(Ball, Dt);
			const auto Intents = Team.Tick(NetView(Time, Ball, Own));
			if (Run.FirstTaker < 0 && Team.GetTaker() >= 0)
			{
				Run.FirstTaker = Team.GetTaker();
			}
			Run.bSwappedDuringFlight = Run.bSwappedDuringFlight || Team.GetPair().IsSwapped();
			for (int Slot = 0; Slot < 2; ++Slot)
			{
				REQUIRE(Intents[Slot].has_value());
				if (Intents[Slot]->Shot)
				{
					++Run.Shots;
					Run.ShotSlot = Slot;
				}
				Own[Slot] = Own[Slot] + TeamFrame::CourtDirection(1, Intents[Slot]->Move) * (6.0 * Dt);
			}
		}
		return Run;
	}
}

TEST_CASE("Ai.SideSwitch: Data/AiTactics.json turns the switch on for lobs landing past 7 m")
{
	CHECK(TestData::Tactics().SideSwitchMinLobDepthM == Approx(7.0));
}

TEST_CASE("Ai.SideSwitch: a deep lob over one player at the net names that player")
{
	const FAiTactics T = WithSwitch(7.0);
	FAiPairTactics Pair(T);
	Pair.SetFormation(EFormation::BothNet);
	// Over the left player, landing deep on the left: the left player is lobbed.
	CHECK(Pair.LobbedSlot(EAiShot::Lob, { -1.6, 3.0 }, { -2.0, 8.5 }, NetLocal) == LeftSlot);
	// Over the left player, landing deep in the middle: still the left player.
	CHECK(Pair.LobbedSlot(EAiShot::Lob, { -1.6, 3.0 }, { 0.3, 8.5 }, NetLocal) == LeftSlot);
	// Over the right player, deep behind them.
	CHECK(Pair.LobbedSlot(EAiShot::Lob, { 1.5, 3.0 }, { 2.5, 8.8 }, NetLocal) == RightSlot);
	// Drifting across to the partner's own side: the partner's ball, no switch.
	CHECK(Pair.LobbedSlot(EAiShot::Lob, { -1.6, 3.0 }, { 2.5, 8.5 }, NetLocal) == -1);
	// Short of the threshold, not a lob, or the pair is back: no switch.
	CHECK(Pair.LobbedSlot(EAiShot::Lob, { -1.6, 3.0 }, { -2.0, 6.5 }, NetLocal) == -1);
	CHECK(Pair.LobbedSlot(EAiShot::Drive, { -1.6, 3.0 }, { -2.0, 8.5 }, NetLocal) == -1);
	Pair.SetFormation(EFormation::BothBack);
	CHECK(Pair.LobbedSlot(EAiShot::Lob, { -1.6, 8.5 }, { -2.0, 8.5 }, NetLocal) == -1);
	// SideSwitchMinLobDepthM = 0 never switches (today's behaviour on main).
	const FAiTactics Off = WithSwitch(0.0);
	FAiPairTactics PairOff(Off);
	PairOff.SetFormation(EFormation::BothNet);
	CHECK(PairOff.LobbedSlot(EAiShot::Lob, { -1.6, 3.0 }, { -2.0, 8.5 }, NetLocal) == -1);
}

TEST_CASE("Ai.SideSwitch: swapped, the players take each other's homes and side rules")
{
	const FAiTactics& T = TestData::Tactics();
	FAiPairTactics Pair(T);
	Pair.SetFormation(EFormation::BothBack);
	const auto Before = Pair.HomePositions({ 1.0, 5.0 });
	Pair.SetSwapped(true);
	const auto After = Pair.HomePositions({ 1.0, 5.0 });
	CHECK(After[RightSlot].X == Approx(Before[LeftSlot].X));
	CHECK(After[LeftSlot].X == Approx(Before[RightSlot].X));
	CHECK(Pair.SideOf(RightSlot) == LeftSlot);

	// The ball on the right side now belongs to the left-slot player, who stands there.
	FTakerQuery Q;
	FInterceptPlan Plan;
	Plan.bReachable = true;
	Plan.Slack = 0.5;
	Q.Plans = { Plan, Plan };
	Q.PlayerLocal = { FAiVec2{ -2.25, 8.5 }, FAiVec2{ 2.25, 8.5 } };
	Q.Profiles = { TestData::Player(ESide::Right), TestData::Player(ESide::Left) };
	Q.Teamwork = 99;
	Q.BallLocalX = 3.0;
	FPadelRng Rng(3);
	CHECK(Pair.ChooseTaker(Q, Rng) == LeftSlot);
	Q.BallLocalX = -3.0;
	CHECK(Pair.ChooseTaker(Q, Rng) == RightSlot);
	// Down the middle the forehand facing it is the right-slot player's now (on the left).
	Q.BallLocalX = 0.1;
	CHECK(Pair.ChooseTaker(Q, Rng) == RightSlot);
}

TEST_CASE("Ai.SideSwitch: a lob over the left player sends the right player across to take it, once")
{
	const FAiTactics T = WithSwitch(7.0);
	const IAiBallModel& Model = TestData::EngineModel();
	// From the opponents' left, over our left player (court Y +1.75) to land deep on our left.
	const FAiBall Ball = Lob(Model, -1.0, 2.0, 8.6);
	const std::optional<FAiVec2> L = Landing(Model, Ball);
	REQUIRE(L.has_value());
	REQUIRE(L->Y > 7.0);
	REQUIRE(L->X < 0.0);

	FAiTeamController Team(AllAi(), T, TestData::Difficulty("Medium"), Model);
	const FLobRun Run = PlayLob(Team, Model, Ball);
	CHECK(Run.bSwappedDuringFlight);
	CHECK(Run.FirstTaker == RightSlot);
	// GDD 3.3 and 8: one player of the pair plays the ball, exactly once.
	CHECK(Run.Shots == 1);
	CHECK(Run.ShotSlot == RightSlot);
	CHECK(Team.GetPair().IsSwapped());
	// The point ends: back to their own sides for the next one.
	Team.OnPointEnded();
	CHECK_FALSE(Team.GetPair().IsSwapped());
	Team.OnPointStarted(true);
	CHECK_FALSE(Team.GetPair().IsSwapped());
}

TEST_CASE("Ai.SideSwitch: with SideSwitchMinLobDepthM = 0 the same lob plays as on main")
{
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBall Ball = Lob(Model, -1.0, 2.0, 8.6);
	const FAiTactics Off = WithSwitch(0.0);
	FAiTeamController Team(AllAi(), Off, TestData::Difficulty("Medium"), Model);
	const FLobRun Run = PlayLob(Team, Model, Ball);
	CHECK_FALSE(Run.bSwappedDuringFlight);
	// Without the switch the lobbed player turns and chases it.
	CHECK(Run.FirstTaker == LeftSlot);
	CHECK(Run.Shots == 1);
}

TEST_CASE("Ai.SideSwitch: a pair with a human never switches on its own")
{
	const FAiTactics T = WithSwitch(7.0);
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBall Ball = Lob(Model, -1.0, 2.0, 8.6);
	FAiTeamSetup S = AllAi();
	S.HumanSlot = LeftSlot;
	FAiTeamController Team(S, T, TestData::Difficulty("Medium"), Model);
	Team.OnPointStarted(false);
	Team.OnBallHit(0.0, 1, EAiShot::Volley, { 3.0, 0.0, 1.0 }, { -7.0, 0.0, 0.0 });
	Team.OnBallHit(0.1, -1, EAiShot::Lob, Ball.Position, { 8.5, 0.0, 0.0 });
	FAiBall B = Ball;
	const std::array<FAiVec, 2> Own = { TeamFrame::ToCourt(1, NetLocal[0]), TeamFrame::ToCourt(1, NetLocal[1]) };
	for (int Step = 0; Step < 120; ++Step)
	{
		Model.Step(B, Dt);
		Team.Tick(NetView(0.1 + (Step + 1) * Dt, B, Own));
		CHECK_FALSE(Team.GetPair().IsSwapped());
	}
}
