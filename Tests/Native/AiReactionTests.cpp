// GDD 8 reaction (movement doc 3.2 and 5.4, realism plan A5): after the opponent's contact
// an AI player can't move on the new ball until it has read it, and a player who
// split-stepped also waits for the landing before the first step.
#include "doctest.h"
#include "AI/AiTeamController.h"
#include "AI/AiTeamFrame.h"
#include "Support/AiTestData.h"
#include <cmath>

using namespace Padel::AI;
using doctest::Approx;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	FAiTeamSetup AllAi()
	{
		FAiTeamSetup S;
		S.HalfSign = 1;
		S.Profiles = { TestData::Player(ESide::Right), TestData::Player(ESide::Left) };
		S.Seed = 11;
		return S;
	}

	// A drive from the opponents' baseline (-X) across to team A's right side (+X).
	FAiBall Drive()
	{
		return { { -8.0, 0.0, 1.0 }, { 16.0, -2.0, 3.5 }, {} };
	}

	FAiWorldView View(double Time, const FAiBall& Ball, const std::array<FAiVec, 2>& Own)
	{
		FAiWorldView V;
		V.Time = Time;
		V.Ball = Ball;
		V.OwnPositions = Own;
		V.OpponentPositions = { FAiVec{ -8.5, 2.25, 0.0 }, FAiVec{ -8.5, -2.25, 0.0 } };
		return V;
	}

	double Length(const FAiVec2& V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }
}

TEST_CASE("Ai.Reaction: the split-step landing is 0.15 s in AiTactics.json")
{
	CHECK(TestData::Tactics().FirstStepExtraS == Approx(0.15));
}

TEST_CASE("Ai.Reaction: a split-stepper stands until the read plus the landing, then moves")
{
	const FAiTactics& T = TestData::Tactics();
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController Team(AllAi(), T, TestData::Difficulty("Hard"), Model);
	Team.OnPointStarted(false);

	// Both back, a little off-centre, so the player has somewhere to go.
	const std::array<FAiVec, 2> Own = { TeamFrame::ToCourt(1, FAiVec2{ 1.0, 7.5 }), TeamFrame::ToCourt(1, FAiVec2{ -2.5, 8.0 }) };
	Team.OnBallHit(0.0, -1, EAiShot::Drive, FAiVec{ -8.0, 0.0, 1.0 }, FAiVec{ 7.0, -3.0, 0.0 });
	Team.OnSplitStep(RightSlot);

	const double RightStep = Team.GetFirstStepAt(RightSlot);
	const double LeftStep = Team.GetFirstStepAt(LeftSlot);
	REQUIRE(LeftStep > 0.0);
	// The non-splitter steps at the read; the splitter one landing later.
	CHECK(RightStep - LeftStep == Approx(T.FirstStepExtraS));

	FAiBall Ball = Drive();
	double FirstMove = -1.0;
	bool bPlannedWithLanding = false;
	for (int Step = 1; Step < 120 && FirstMove < 0.0; ++Step)
	{
		const double Time = Step * Dt;
		Model.Step(Ball, Dt);
		const auto Intents = Team.Tick(View(Time, Ball, Own));
		REQUIRE(Intents[RightSlot].has_value());
		if (Team.GetPlan(RightSlot) && Time < RightStep)
		{
			// The plan made at the read already counts the landing still to come.
			bPlannedWithLanding = Team.GetPlan(RightSlot)->TimeNeeded >= RightStep - Time - 1e-9;
		}
		if (Length(Intents[RightSlot]->Move) > 1e-6)
		{
			FirstMove = Time;
		}
	}
	CHECK(FirstMove >= RightStep - 1e-9);
	CHECK(FirstMove < RightStep + 2.0 * Dt);
	CHECK(bPlannedWithLanding);
}

TEST_CASE("Ai.Reaction: the first step waits longer on Easy than on Hard")
{
	const FAiTactics& T = TestData::Tactics();
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController Hard(AllAi(), T, TestData::Difficulty("Hard"), Model);
	FAiTeamController Medium(AllAi(), T, TestData::Difficulty("Medium"), Model);
	FAiTeamController Easy(AllAi(), T, TestData::Difficulty("Easy"), Model);
	for (FAiTeamController* Team : { &Hard, &Medium, &Easy })
	{
		Team->OnPointStarted(false);
		Team->OnBallHit(0.0, -1, EAiShot::Drive, FAiVec{ -8.0, 0.0, 1.0 }, FAiVec{ 7.0, -3.0, 0.0 });
		Team->OnSplitStep(RightSlot);
	}
	// Hard keeps its edge in reading the ball, not in a superhuman first step: a
	// world-class first step is about 250 to 300 ms (movement doc 3.2).
	CHECK(Hard.GetFirstStepAt(RightSlot) >= 0.2);
	CHECK(Hard.GetFirstStepAt(RightSlot) < Medium.GetFirstStepAt(RightSlot));
	CHECK(Medium.GetFirstStepAt(RightSlot) < Easy.GetFirstStepAt(RightSlot));
}

TEST_CASE("Ai.Reaction: the pair's own shot clears the hold")
{
	FAiTeamController Team(AllAi(), TestData::Tactics(), TestData::Difficulty("Medium"), TestData::EngineModel());
	Team.OnPointStarted(false);
	Team.OnBallHit(0.0, -1, EAiShot::Drive, FAiVec{ -8.0, 0.0, 1.0 }, FAiVec{ 7.0, -3.0, 0.0 });
	Team.OnSplitStep(LeftSlot);
	REQUIRE(Team.GetFirstStepAt(LeftSlot) > 0.0);
	Team.OnBallHit(0.6, 1, EAiShot::Drive, FAiVec{ 7.0, -3.0, 1.0 }, FAiVec{ -7.0, 0.0, 0.0 });
	CHECK(Team.GetFirstStepAt(LeftSlot) < 0.0);
	CHECK(Team.GetFirstStepAt(RightSlot) < 0.0);
	Team.OnSplitStep(LeftSlot);  // no opponent ball in flight: nothing to wait for
	CHECK(Team.GetFirstStepAt(LeftSlot) < 0.0);
}
