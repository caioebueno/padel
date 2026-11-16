// The AI pair end to end: read delay, one taker, partner commands, determinism.
#include "doctest.h"
#include "AI/AiTeamController.h"
#include "AI/AiTeamFrame.h"
#include "Support/AiTestData.h"

using namespace Padel::AI;
using doctest::Approx;

namespace
{
	FAiTeamSetup TeamA(int HumanSlot = -1, uint64_t Seed = 1)
	{
		FAiTeamSetup S;
		S.HalfSign = 1;
		S.Profiles = { TestData::Player(ESide::Right), TestData::Player(ESide::Left) };
		S.HumanSlot = HumanSlot;
		S.Seed = Seed;
		return S;
	}

	FAiWorldView View(double Time, const FAiBall& Ball)
	{
		FAiWorldView V;
		V.Time = Time;
		V.Ball = Ball;
		V.OwnPositions = { FAiVec{ 8.5, -2.25, 0.0 }, FAiVec{ 8.5, 2.25, 0.0 } };
		V.OpponentPositions = { FAiVec{ -8.5, 2.25, 0.0 }, FAiVec{ -8.5, -2.25, 0.0 } };
		return V;
	}

	const FAiBall Incoming = { { -6.0, 1.0, 1.0 }, { 13.0, -0.5, 3.0 }, {} };
}

TEST_CASE("Ai.Team: waits for the read delay after the opponent's contact (GDD 8)")
{
	const FAiTactics& T = TestData::Tactics();
	const FAiDifficulty& D = TestData::Difficulty("Medium");
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController Team(TeamA(), T, D, Model);
	Team.OnPointStarted(false);
	Team.OnBallHit(10.0, -1, EAiShot::Drive, Incoming.Position, { 7.0, 0.0, 0.0 });
	const double Delay = Team.GetProfile(0).ReadDelay(T, D);
	CHECK(Delay == Approx(D.ReadDelayS * ByRating(77, T.AnticipationDelayScaleAt1, T.AnticipationDelayScaleAt99)));
	Team.Tick(View(10.0 + Delay * 0.5, Incoming));
	CHECK(Team.GetTaker() == -1);
	Team.Tick(View(10.0 + Delay + 0.01, Incoming));
	CHECK(Team.GetTaker() >= 0);
}

TEST_CASE("Ai.Team: harder levels read sooner")
{
	const FAiTactics& T = TestData::Tactics();
	const FAiPlayerProfile P = TestData::Player(ESide::Right);
	CHECK(P.ReadDelay(T, TestData::Difficulty("Hard")) < P.ReadDelay(T, TestData::Difficulty("Medium")));
	CHECK(P.ReadDelay(T, TestData::Difficulty("Medium")) < P.ReadDelay(T, TestData::Difficulty("Easy")));
}

TEST_CASE("Ai.Team: exactly one player goes for the ball and sends one shot")
{
	const FAiTactics& T = TestData::Tactics();
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController Team(TeamA(), T, TestData::Difficulty("Medium"), Model);
	Team.OnPointStarted(false);
	Team.OnBallHit(0.0, -1, EAiShot::Drive, Incoming.Position, { 7.0, 0.0, 0.0 });
	FAiBall Ball = Incoming;
	int Shots = 0;
	double Time = 0.0;
	for (int Step = 0; Step < 400; ++Step)
	{
		Time += 1.0 / 120.0;
		Model.Step(Ball, 1.0 / 120.0);
		const auto Intents = Team.Tick(View(Time, Ball));
		for (const auto& Intent : Intents)
		{
			REQUIRE(Intent.has_value());
			Shots += Intent->Shot ? 1 : 0;
			CHECK(Intent->Move.Length() <= 1.0 + 1e-9);
		}
	}
	CHECK(Shots == 1);
}

TEST_CASE("Ai.Team: the opponents' own shots never trigger a read")
{
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController Team(TeamA(), TestData::Tactics(), TestData::Difficulty("Medium"), Model);
	Team.OnPointStarted(true);
	Team.OnBallHit(0.0, 1, EAiShot::Drive, { 8.0, 0.0, 1.0 }, { -7.0, 0.0, 0.0 });
	const auto Intents = Team.Tick(View(2.0, Incoming));
	CHECK(Team.GetTaker() == -1);
	CHECK_FALSE(Intents[0]->Shot.has_value());
}

TEST_CASE("Ai.Commands: only the human's partner takes commands, and they clear at the point's end")
{
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController AllAi(TeamA(), TestData::Tactics(), TestData::Difficulty("Medium"), Model);
	CHECK_FALSE(AllAi.GiveCommand(EPartnerCommand::GoNet));

	FAiTeamController WithHuman(TeamA(LeftSlot), TestData::Tactics(), TestData::Difficulty("Medium"), Model);
	WithHuman.OnPointStarted(false);
	CHECK(WithHuman.GiveCommand(EPartnerCommand::GoNet));
	CHECK(WithHuman.GetPair().GetCommand(RightSlot) == EPartnerCommand::GoNet);
	const auto Intents = WithHuman.Tick(View(0.0, Incoming));
	CHECK_FALSE(Intents[LeftSlot].has_value());  // the human's slot is not driven
	REQUIRE(Intents[RightSlot].has_value());
	CHECK(Intents[RightSlot]->Move.Y > 0.5);      // heading for the net
	WithHuman.OnPointEnded();
	CHECK(WithHuman.GetPair().GetCommand(RightSlot) == EPartnerCommand::None);
}

TEST_CASE("Ai.Team: difficulty's attribute scale applies to AI players only")
{
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController Team(TeamA(LeftSlot), TestData::Tactics(), TestData::Difficulty("Easy"), Model);
	CHECK(Team.GetProfile(RightSlot).Speed == 66);
	CHECK(Team.GetProfile(LeftSlot).Speed == 76);
}

TEST_CASE("Ai.Team: golden point receiver is the better returner (GDD 3.1)")
{
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamSetup S = TeamA();
	S.Profiles[LeftSlot].ReturnSkill = 90;
	FAiTeamController Team(S, TestData::Tactics(), TestData::Difficulty("Medium"), Model);
	CHECK(Team.ChooseGoldenPointReceiver() == LeftSlot);
}

TEST_CASE("Ai.Team: the same seed makes the same decisions (GDD 8 determinism)")
{
	const IAiBallModel& Model = TestData::EngineModel();
	auto Run = [&](uint64_t Seed) {
		FAiTeamController Team(TeamA(-1, Seed), TestData::Tactics(), TestData::Difficulty("Medium"), Model);
		Team.OnPointStarted(false);
		Team.OnBallHit(0.0, -1, EAiShot::Drive, Incoming.Position, { 7.0, 0.0, 0.0 });
		FAiBall Ball = Incoming;
		std::vector<double> Trace;
		for (int Step = 0; Step < 300; ++Step)
		{
			const double Time = (Step + 1) / 120.0;
			Model.Step(Ball, 1.0 / 120.0);
			for (const auto& Intent : Team.Tick(View(Time, Ball)))
			{
				Trace.push_back(Intent->Move.X);
				Trace.push_back(Intent->Move.Y);
				if (Intent->Shot)
				{
					Trace.push_back(Intent->Shot->Target.X);
					Trace.push_back(Intent->Shot->TimingOffsetS);
				}
			}
		}
		return Trace;
	};
	CHECK(Run(42) == Run(42));
	CHECK(Run(42) != Run(43));
}
