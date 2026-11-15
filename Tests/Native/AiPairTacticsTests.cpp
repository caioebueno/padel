// GDD 8 pair tactics and GDD 7 partner commands.
#include "doctest.h"
#include "AI/AiPairTactics.h"
#include "AI/AiTeamFrame.h"
#include "Support/AiTestData.h"

#include <cmath>

using namespace Padel::AI;
using doctest::Approx;

TEST_CASE("Ai.TeamFrame: a team's right is -Y times its half, depth runs to its own glass")
{
	// Team A on +X faces -X; Unreal is left-handed, so its right is -Y.
	CHECK(TeamFrame::ToLocal(1, { 8.0, -2.0, 0.0 }).X == Approx(2.0));
	CHECK(TeamFrame::ToLocal(1, { 8.0, -2.0, 0.0 }).Y == Approx(8.0));
	CHECK(TeamFrame::ToLocal(-1, { -8.0, 2.0, 0.0 }).X == Approx(2.0));
	for (int H : { 1, -1 })
	{
		const FAiVec P = { 3.0 * H, 1.5, 0.0 };
		const FAiVec Back = TeamFrame::ToCourt(H, TeamFrame::ToLocal(H, P));
		CHECK(Back.X == Approx(P.X));
		CHECK(Back.Y == Approx(P.Y));
		// "Up on the stick" is toward the net.
		CHECK(TeamFrame::CourtDirection(H, { 0.0, 1.0 }).X == Approx(-H));
	}
}

TEST_CASE("Ai.Pair: both back and both at the net use the GDD 8 depths and spacing")
{
	const FAiTactics& T = TestData::Tactics();
	FAiPairTactics Pair(T);
	auto Homes = Pair.HomePositions({ 0.0, 5.0 });
	CHECK(Homes[RightSlot].Y == Approx(T.BackDepthM));
	CHECK(Homes[LeftSlot].Y == Approx(T.BackDepthM));
	CHECK(Homes[RightSlot].X - Homes[LeftSlot].X == Approx(T.PartnerSpacingM));
	CHECK(Homes[RightSlot].X > 0.0);
	Pair.SetFormation(EFormation::BothNet);
	Homes = Pair.HomePositions({ 0.0, 5.0 });
	CHECK(Homes[RightSlot].Y == Approx(T.NetDepthM));
	CHECK(Homes[LeftSlot].Y == Approx(T.NetDepthM));
}

TEST_CASE("Ai.Pair: the pair shifts toward the ball together, never splits on its own")
{
	const FAiTactics& T = TestData::Tactics();
	FAiPairTactics Pair(T);
	for (double BallX : { -4.0, -1.0, 0.0, 2.5, 4.5 })
	{
		for (EFormation F : { EFormation::BothBack, EFormation::BothNet })
		{
			Pair.SetFormation(F);
			const auto Homes = Pair.HomePositions({ BallX, 6.0 });
			CHECK(Homes[RightSlot].Y == Approx(Homes[LeftSlot].Y));
			const double Centre = (Homes[RightSlot].X + Homes[LeftSlot].X) * 0.5;
			if (BallX != 0.0)
			{
				CHECK(Centre * BallX > 0.0);
			}
			CHECK(std::abs(Centre) <= T.MaxLateralShiftM + 1e-9);
		}
	}
}

TEST_CASE("Ai.Pair: formation follows the shots (GDD 8)")
{
	const FAiTactics& T = TestData::Tactics();
	FAiPairTactics Pair(T);
	Pair.OnOwnShot(EAiShot::Lob, 8.5, 8.6);
	CHECK(Pair.GetFormation() == EFormation::BothNet);  // a deep lob buys time to advance
	Pair.OnOpponentShot(EAiShot::Lob);
	CHECK(Pair.GetFormation() == EFormation::BothBack);  // a lob over them sends them back
	Pair.OnOwnShot(EAiShot::Chiquita, 8.0, 2.5);
	CHECK(Pair.GetFormation() == EFormation::BothNet);
	Pair.OnOwnShot(EAiShot::Drive, 9.0, 7.5);
	CHECK(Pair.GetFormation() == EFormation::BothBack);  // a drive from the back keeps them back
	Pair.OnOwnShot(EAiShot::Lob, 8.5, 4.0);
	CHECK(Pair.GetFormation() == EFormation::BothBack);  // a short lob doesn't
	Pair.OnOwnShot(EAiShot::Bandeja, 3.0, 8.0);
	CHECK(Pair.GetFormation() == EFormation::BothNet);   // the bandeja keeps the net
}

TEST_CASE("Ai.Pair: the AI partner follows a human's depth with hysteresis")
{
	const FAiTactics& T = TestData::Tactics();
	FAiPairTactics Pair(T);
	Pair.FollowHuman(3.0);
	CHECK(Pair.GetFormation() == EFormation::BothNet);
	Pair.FollowHuman(T.HumanNetDepthM + 0.1);  // inside the hysteresis band: no change
	CHECK(Pair.GetFormation() == EFormation::BothNet);
	Pair.FollowHuman(8.0);
	CHECK(Pair.GetFormation() == EFormation::BothBack);
	Pair.FollowHuman(T.HumanNetDepthM - 0.1);
	CHECK(Pair.GetFormation() == EFormation::BothBack);
}

TEST_CASE("Ai.Commands: go to the net, stay back and take the middle move that player")
{
	const FAiTactics& T = TestData::Tactics();
	FAiPairTactics Pair(T);
	Pair.SetCommand(LeftSlot, EPartnerCommand::GoNet);
	auto Homes = Pair.HomePositions({ 0.0, 5.0 });
	CHECK(Homes[LeftSlot].Y == Approx(T.NetDepthM));
	CHECK(Homes[RightSlot].Y == Approx(T.BackDepthM));

	Pair.SetFormation(EFormation::BothNet);
	Pair.SetCommand(LeftSlot, EPartnerCommand::StayBack);
	Homes = Pair.HomePositions({ 0.0, 5.0 });
	CHECK(Homes[LeftSlot].Y == Approx(T.BackDepthM));

	const auto Before = Pair.HomePositions({ 0.0, 5.0 });
	Pair.SetCommand(LeftSlot, EPartnerCommand::TakeMiddle);
	Homes = Pair.HomePositions({ 0.0, 5.0 });
	CHECK(std::abs(Homes[LeftSlot].X) < std::abs(Before[LeftSlot].X));

	// GDD 8: commands last until the end of the point.
	Pair.ClearCommands();
	CHECK(Pair.GetCommand(LeftSlot) == EPartnerCommand::None);
}

namespace
{
	FTakerQuery MiddleBall()
	{
		FTakerQuery Q;
		FInterceptPlan Plan;
		Plan.bReachable = true;
		Plan.Slack = 0.5;
		Q.Plans = { Plan, Plan };
		Q.PlayerLocal = { FAiVec2{ 2.25, 8.5 }, FAiVec2{ -2.25, 8.5 } };
		Q.Profiles = { TestData::Player(ESide::Right), TestData::Player(ESide::Left) };
		Q.BallLocalX = 0.1;
		Q.Teamwork = 99;
		return Q;
	}
}

TEST_CASE("Ai.Taker: the player on the ball's side takes it")
{
	FAiPairTactics Pair(TestData::Tactics());
	FAiRng Rng(3);
	FTakerQuery Q = MiddleBall();
	Q.BallLocalX = 3.0;
	CHECK(Pair.ChooseTaker(Q, Rng) == RightSlot);
	Q.BallLocalX = -3.0;
	CHECK(Pair.ChooseTaker(Q, Rng) == LeftSlot);
}

TEST_CASE("Ai.Taker: down the middle, the left player's forehand by default")
{
	FAiPairTactics Pair(TestData::Tactics());
	FAiRng Rng(3);
	FTakerQuery Q = MiddleBall();
	CHECK(Pair.ChooseTaker(Q, Rng) == LeftSlot);
	// A left-hander on the right with a right-hander... still the left by default;
	// two left-handers: the right player's forehand faces the middle.
	Q.Profiles[RightSlot].Handedness = EHand::Left;
	Q.Profiles[LeftSlot].Handedness = EHand::Left;
	CHECK(Pair.ChooseTaker(Q, Rng) == RightSlot);
}

TEST_CASE("Ai.Taker: the partner takes a middle ball when clearly better placed")
{
	const FAiTactics& T = TestData::Tactics();
	FAiPairTactics Pair(T);
	FAiRng Rng(3);
	FTakerQuery Q = MiddleBall();
	Q.Plans[RightSlot]->Slack = Q.Plans[LeftSlot]->Slack + T.BetterPlacedMarginS + 0.1;
	CHECK(Pair.ChooseTaker(Q, Rng) == RightSlot);
	// Only one can reach it: that one goes, wherever it is.
	Q = MiddleBall();
	Q.BallLocalX = -3.0;
	Q.Plans[LeftSlot]->bReachable = false;
	CHECK(Pair.ChooseTaker(Q, Rng) == RightSlot);
}

TEST_CASE("Ai.Taker: low Teamwork sometimes sends the wrong partner, high never does")
{
	FAiPairTactics Pair(TestData::Tactics());
	FAiRng Rng(11);
	int Wrong = 0;
	FTakerQuery Q = MiddleBall();
	Q.Teamwork = 99;
	for (int I = 0; I < 500; ++I)
	{
		Wrong += Pair.ChooseTaker(Q, Rng) != LeftSlot ? 1 : 0;
	}
	CHECK(Wrong == 0);
	Q.Teamwork = 1;
	for (int I = 0; I < 2000; ++I)
	{
		Wrong += Pair.ChooseTaker(Q, Rng) != LeftSlot ? 1 : 0;
	}
	CHECK(Wrong > 150);
	CHECK(Wrong < 450);
}

TEST_CASE("Ai.Commands: \"yours\" hands the next ball to the partner, once")
{
	FAiPairTactics Pair(TestData::Tactics());
	FAiRng Rng(3);
	FTakerQuery Q = MiddleBall();
	Q.BallLocalX = 3.0;  // the right player's ball
	Pair.SetCommand(LeftSlot, EPartnerCommand::Yours);
	CHECK(Pair.ChooseTaker(Q, Rng) == LeftSlot);
	Pair.ConsumeYours();
	CHECK(Pair.ChooseTaker(Q, Rng) == RightSlot);
}
