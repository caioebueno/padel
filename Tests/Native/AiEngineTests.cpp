// The AI on the real engine and shot system: the type bridge, the ball model on
// FBallSim, the match ball port behind the swing controllers, and the runner that
// plays four players through them (GDD 8).
#include "doctest.h"
#include "AI/AiEngineBallModel.h"
#include "AI/AiEngineBridge.h"
#include "AI/AiMatchBallPort.h"
#include "AI/AiMatchRunner.h"
#include "AI/AiTeamFrame.h"
#include "ScriptedPlayers.h"
#include "ShotTestData.h"
#include "Support/AiTestData.h"
#include "TestSupport.h"

using namespace Padel;
using namespace Padel::AI;

namespace
{
	FAiMatchSetup FourAi(uint64_t Seed)
	{
		FAiMatchSetup Setup;
		for (int Id = 0; Id < NumPlayers; ++Id)
		{
			Setup.Players[Id] = ShotTestData::LoadPlayer();
		}
		Setup.Difficulties = { &TestData::Difficulty("Medium"), &TestData::Difficulty("Medium") };
		Setup.Seed = Seed;
		return Setup;
	}
}

TEST_CASE("Ai.Engine: shot types map both ways")
{
	for (int Index = 0; Index <= static_cast<int>(EAiShot::ServeKick); ++Index)
	{
		const EAiShot Shot = static_cast<EAiShot>(Index);
		CHECK(Bridge::ToAi(Bridge::ToEngine(Shot)) == Shot);
		CHECK(std::string(AiShotName(Shot)) == ShotTypeName(Bridge::ToEngine(Shot)));
	}
	CHECK(Bridge::ToAi(EShotType::Mishit) == EAiShot::Drive);
}

TEST_CASE("Ai.Engine: the AI plans with the shot system's player row")
{
	Shots::FPlayerRatings R = ShotTestData::LoadPlayer();
	R.Handedness = Shots::EHandedness::Left;
	R.Anticipation = 91;
	const FAiPlayerProfile P = Bridge::ProfileFromRatings(R);
	CHECK(P.Speed == R.Speed);
	CHECK(P.WallPlay == R.WallPlay);
	CHECK(P.Anticipation == 91);
	CHECK(P.Handedness == EHand::Left);
	CHECK(P.PreferredSide == (R.PreferredSide == Shots::ECourtSide::Left ? ESide::Left : ESide::Right));

	// GDD 8 attribute scale, clamped to the rating range.
	const Shots::FPlayerRatings Up = Bridge::WithOffset(R, 10);
	CHECK(Up.Anticipation == 99);
	CHECK(Up.Speed == std::min(99, R.Speed + 10));
	CHECK(Bridge::WithOffset(R, -200).Drive == 1);
}

TEST_CASE("Ai.Engine: the AI's tuning agrees with the shot and movement data")
{
	// The AI stands where the shot model wants the ball and plans with the speeds the
	// player really moves at, so a plan it makes is one the controller can play.
	const FAiTactics& T = TestData::Tactics();
	const Shots::FShotLibrary Lib = ShotTestData::LoadLibrary();
	const Shots::FMovementTuning Move = ShotTestData::LoadMovement();
	CHECK(T.ContactSideM == doctest::Approx(Lib.Model.IdealContactSideM));
	CHECK(T.ContactForwardM == doctest::Approx(Lib.Model.IdealContactForwardM));
	CHECK(T.SpeedAt1Mps == doctest::Approx(Move.TopSpeedAt1Mps));
	CHECK(T.SpeedAt99Mps == doctest::Approx(Move.TopSpeedAt99Mps));
	CHECK(T.AccelTimeAt1S == doctest::Approx(Move.TimeToTopAt1S));
	CHECK(T.AccelTimeAt99S == doctest::Approx(Move.TimeToTopAt99S));
	CHECK(T.ReachSideAt1M == doctest::Approx(Lib.Model.ReachMinM));
	CHECK(T.ReachSideAt99M == doctest::Approx(Lib.Model.ReachMaxM));
	// The shot request must arrive before the longest backswing has to start.
	CHECK(T.SwingLeadS >= Lib.Model.ChargeTimeS);
}

TEST_CASE("Ai.Engine: the AI's move is the stick the player motion takes")
{
	const Shots::FPlayerRatings R = ShotTestData::LoadPlayer();
	const Shots::FMovementTuning Move = ShotTestData::LoadMovement();
	for (const int Half : { 1, -1 })
	{
		Shots::FPlayerMotion Motion(&R, &Move, Half, { Half * 6.0, 0.0, 0.0 });
		// Toward the net and to the right, in court space.
		const FAiVec Goal = TeamFrame::ToCourt(Half, { 2.0, 3.0 });
		FAiIntent Intent;
		const FAiVec Delta = Goal - Bridge::ToAi(Motion.GetPosition());
		Intent.Move = TeamFrame::MoveVector(Half, Delta * (1.0 / Delta.Length2D()));
		for (int Tick = 0; Tick < 120; ++Tick)
		{
			Motion.Step(Bridge::InputFrom(Intent).Move, false, SimTickSeconds);
		}
		const FAiVec2 Local = TeamFrame::ToLocal(Half, Bridge::ToAi(Motion.GetPosition()));
		CHECK(Local.Y < 6.0);  // closer to the net
		CHECK(Local.X > 0.0);  // on the right
	}
}

TEST_CASE("Ai.Engine: the ball model steps exactly like the engine")
{
	const FBallSim Sim(Test::Physics());
	const FAiEngineBallModel Model(Sim);
	FBallState Engine;
	Engine.Position = { -6.0, 1.0, 1.0 };
	Engine.Velocity = { 16.0, -1.0, 2.5 };
	Engine.Spin = { 0.0, 30.0, 0.0 };
	FAiBall Ai = Bridge::ToAi(Engine);
	std::vector<FBallContact> Contacts;
	int Bounces = 0;
	for (int Tick = 0; Tick < 360 && !Model.IsDead(Ai); ++Tick)
	{
		Sim.Tick(Engine, nullptr, Contacts);
		for (const FAiContact& C : Model.Step(Ai, SimTickSeconds))
		{
			Bounces += C.Surface == Court::ESurface::Ground ? 1 : 0;
		}
		CHECK(Ai.Position.X == Engine.Position.X);
		CHECK(Ai.Position.Z == Engine.Position.Z);
		CHECK(Ai.Velocity.Y == Engine.Velocity.Y);
	}
	CHECK(Bounces >= 1);
}

TEST_CASE("Ai.Engine: the port's prediction is the engine's path, cached or not")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 3);
	Test::FScriptedPlayers Players(Match, 3);
	Match.StartMatch();
	Match.BeginPoint();
	FAiMatchBallPort Port(Match);
	for (int Tick = 0; Tick < 40; ++Tick)
	{
		Players.Act();
		Match.Tick();
	}
	REQUIRE(Match.IsLive());
	// The same call twice, and once from a fresh port, give the same samples.
	const std::vector<Shots::FBallView> A = Port.Predict(1.5, SimTickSeconds);
	Players.Act();
	Match.Tick();
	const std::vector<Shots::FBallView> B = Port.Predict(1.5, SimTickSeconds);
	const std::vector<Shots::FBallView> Fresh = FAiMatchBallPort(Match).Predict(1.5, SimTickSeconds);
	REQUIRE(B.size() == Fresh.size());
	for (size_t I = 0; I < B.size(); ++I)
	{
		CHECK(B[I].Position.X == Fresh[I].Position.X);
		CHECK(B[I].GroundBounces == Fresh[I].GroundBounces);
	}
	// One tick on, the path is the old path one tick later.
	REQUIRE(A.size() > 2);
	CHECK(B[0].Position.X == A[1].Position.X);
	CHECK(B[0].Position.Z == A[1].Position.Z);
	// It follows the engine's own integrator.
	FBallState Copy = Match.Ball();
	std::vector<FBallContact> Contacts;
	Match.BallSim().Tick(Copy, nullptr, Contacts);
	CHECK(B[0].Position.X == Copy.Position.X);
}

TEST_CASE("Ai.Engine: a toss left to bounce twice is a missed serve (GDD 3.2)")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 5);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	Match.StartMatch();
	Match.BeginPoint();
	FAiMatchBallPort Port(Match);
	const FServeContact Stance = Match.DefaultServeContact();
	Port.ServeToss({ Stance.ContactPosition.X, Stance.ContactPosition.Y, 1.7 }, 1);
	REQUIRE(Port.IsTossing());
	CHECK(Port.LastHitterTeam() == 0);
	bool bSawBounce = false;
	for (int Tick = 0; Tick < 600 && Port.IsTossing(); ++Tick)
	{
		Port.Tick();
		Match.Tick();
		bSawBounce = bSawBounce || (Port.IsTossing() && Port.Current().GroundBounces == 1);
	}
	CHECK(bSawBounce);
	REQUIRE(Log.Faults.size() == 1);
	CHECK(Log.Faults[0] == EServeFault::MissedBall);
	Match.RemoveListener(&Log);
}

TEST_CASE("Ai.Engine: four AI players serve, return and finish a point on the real match")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 21);
	Test::FEventLog Log;
	Match.AddListener(&Log);
	const Shots::FShotLibrary Lib = ShotTestData::LoadLibrary();
	const Shots::FMovementTuning Move = ShotTestData::LoadMovement();
	FAiMatchRunner Runner(Match, Lib, Move, TestData::Tactics(), FourAi(21));
	Runner.Start();
	for (int Tick = 0; Tick < 120 * 60 && Log.Points == 0; ++Tick)
	{
		Runner.Tick();
	}
	REQUIRE(Log.Points == 1);
	REQUIRE_FALSE(Log.Hitters.empty());
	// GDD 3.2: the server struck it, from the toss, after the bounce.
	CHECK(Log.Hitters.front() == Match.Score().Snapshot().ServerId);
	CHECK(Log.Reasons.front() != EPointReason::None);
	// The next point starts by itself.
	for (int Tick = 0; Tick < 120 * 5 && Match.Phase() != EMatchPhase::AwaitingServe; ++Tick)
	{
		Runner.Tick();
	}
	CHECK(Match.Phase() == EMatchPhase::AwaitingServe);
	Match.RemoveListener(&Log);
}

TEST_CASE("Ai.Engine: with a human, the runner leaves the human's player alone")
{
	FMatchSimulation Match(FMatchConfig::OneSet(), Test::Physics(), 8);
	const Shots::FShotLibrary Lib = ShotTestData::LoadLibrary();
	const Shots::FMovementTuning Move = ShotTestData::LoadMovement();
	FAiMatchSetup Setup = FourAi(8);
	// Team B's right player is the human: Team A serves first, so they receive.
	Setup.HumanPlayerId = 2;
	FAiMatchRunner Runner(Match, Lib, Move, TestData::Tactics(), Setup);
	Runner.Start();
	CHECK(Runner.GiveCommand(EPartnerCommand::GoNet));
	CHECK_FALSE(Runner.IsAi(2));
	CHECK_FALSE(Runner.IsWaitingForGoldenReceiver());
	CHECK_FALSE(Runner.ChooseGoldenPointReceiver(2));
	Runner.Tick();
	const FVec3 Start = Runner.Player(2).GetMotion().GetPosition();
	for (int Tick = 0; Tick < 240; ++Tick)
	{
		Runner.Tick();
	}
	// No input: the human's player stands still while the AI serves and plays.
	const FVec3 End = Runner.Player(2).GetMotion().GetPosition();
	CHECK(End.X == doctest::Approx(Start.X));
	CHECK(End.Y == doctest::Approx(Start.Y));
}
