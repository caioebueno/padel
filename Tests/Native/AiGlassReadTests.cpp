// GDD 4 off the glass: the AI says when a player is set to take the ball after it comes
// off a wall, and which wall, so animation can turn them side-on to it.
#include "doctest.h"
#include "AI/AiBallPredictor.h"
#include "AI/AiTeamController.h"
#include "AI/AiTeamFrame.h"
#include "Support/AiTestData.h"

using namespace Padel::AI;
using Padel::EGlassWall;
using Padel::FPadelRng;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	// An opponent's drive that bounces deep on team A's half (+X) and comes off a wall.
	FAiBall DeepDrive(double Vy)
	{
		return { { -8.0, 0.0, 1.0 }, { 17.0, Vy, 4.5 }, {} };
	}

	EGlassWall FirstWallAfterBounce(const std::vector<FTrajectorySample>& Samples)
	{
		for (const FTrajectorySample& S : Samples)
		{
			if (S.bOffGlass)
			{
				return S.GlassWall;
			}
		}
		return EGlassWall::None;
	}

	FAiTeamSetup AllAi()
	{
		FAiTeamSetup S;
		S.HalfSign = 1;
		S.Profiles = { TestData::Player(ESide::Right), TestData::Player(ESide::Left) };
		S.Seed = 3;
		return S;
	}
}

TEST_CASE("Ai.GlassRead: the prediction names the wall the ball comes off")
{
	const FAiBallPredictor Predictor(TestData::EngineModel(), TestData::Tactics());
	FPadelRng Rng(1);

	const std::vector<FTrajectorySample> Straight = Predictor.Predict(DeepDrive(0.0), 0.0, Rng);
	CHECK(FirstWallAfterBounce(Straight) == EGlassWall::Back);

	// Angled hard to +Y it reaches the side glass first.
	const std::vector<FTrajectorySample> Wide = Predictor.Predict(DeepDrive(5.0), 0.0, Rng);
	CHECK(FirstWallAfterBounce(Wide) == EGlassWall::SidePlusY);
	const std::vector<FTrajectorySample> WideOther = Predictor.Predict(DeepDrive(-5.0), 0.0, Rng);
	CHECK(FirstWallAfterBounce(WideOther) == EGlassWall::SideMinusY);

	for (const FTrajectorySample& S : Straight)
	{
		if (!S.bOffGlass)
		{
			CHECK(S.GlassWall == EGlassWall::None);
		}
	}
}

TEST_CASE("Ai.GlassRead: only the taker set to play it off the glass is flagged")
{
	const IAiBallModel& Model = TestData::EngineModel();
	FAiTeamController Team(AllAi(), TestData::Tactics(), TestData::Difficulty("Medium"), Model);
	Team.OnPointStarted(false);
	// Both on the service line: the deep drive runs past them into the back glass.
	const std::array<FAiVec, 2> Own = { TeamFrame::ToCourt(1, FAiVec2{ 2.0, 6.95 }), TeamFrame::ToCourt(1, FAiVec2{ -2.0, 6.95 }) };
	Team.OnBallHit(0.0, -1, EAiShot::Drive, FAiVec{ -8.0, 0.0, 1.0 }, FAiVec{ 8.5, 0.0, 0.0 });

	FAiBall Ball = DeepDrive(0.0);
	bool bFlagged = false;
	for (int Step = 1; Step < 180; ++Step)
	{
		Model.Step(Ball, Dt);
		FAiWorldView View;
		View.Time = Step * Dt;
		View.Ball = Ball;
		View.OwnPositions = Own;
		View.OpponentPositions = { FAiVec{ -8.5, 2.25, 0.0 }, FAiVec{ -8.5, -2.25, 0.0 } };
		Team.Tick(View);
		const int Taker = Team.GetTaker();
		for (int Slot = 0; Slot < 2; ++Slot)
		{
			const FAiGlassRead Read = Team.GetGlassRead(Slot);
			if (Slot != Taker)
			{
				CHECK_FALSE(Read.bPlayingOffGlass);
				continue;
			}
			const bool bPlanOffGlass = Team.GetPlan(Slot) && Team.GetPlan(Slot)->bReachable && Team.GetPlan(Slot)->Sample.bOffGlass;
			CHECK(Read.bPlayingOffGlass == bPlanOffGlass);
			if (Read.bPlayingOffGlass)
			{
				CHECK(Read.Wall == EGlassWall::Back);
				bFlagged = true;
			}
		}
	}
	CHECK(bFlagged);

	// The pair's own shot ends it.
	Team.OnBallHit(1.6, 1, EAiShot::Lob, FAiVec{ 8.0, 0.0, 1.0 }, FAiVec{ -8.0, 0.0, 0.0 });
	CHECK_FALSE(Team.GetGlassRead(RightSlot).bPlayingOffGlass);
	CHECK_FALSE(Team.GetGlassRead(LeftSlot).bPlayingOffGlass);
}

TEST_CASE("Ai.GlassRead: a human's slot is never flagged")
{
	FAiTeamSetup S = AllAi();
	S.HumanSlot = RightSlot;
	FAiTeamController Team(S, TestData::Tactics(), TestData::Difficulty("Medium"), TestData::EngineModel());
	CHECK_FALSE(Team.GetGlassRead(RightSlot).bPlayingOffGlass);
	CHECK(Team.GetGlassRead(5).Wall == EGlassWall::None);
}
