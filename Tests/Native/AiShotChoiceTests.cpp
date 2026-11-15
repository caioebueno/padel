// GDD 8 shot choice and GDD 5.1 timing.
#include "doctest.h"
#include "AI/AiShotChooser.h"
#include "AI/AiTeamFrame.h"
#include "Support/AiTestData.h"

#include <cmath>
#include <map>

using namespace Padel::AI;
using Padel::FPadelRng;

namespace
{
	// Team A (+X) plays; opponents on -X at the given depths.
	FShotContext Context(EContactKind Kind, double Depth, double Height, double OppDepth1, double OppDepth2)
	{
		FShotContext C;
		C.HalfSign = 1;
		C.Contact = { Depth, 0.5, Height };
		C.Kind = Kind;
		C.Slack = 0.5;
		C.Profile = TestData::Player(ESide::Right);
		C.Opponents = { { -OppDepth1, -2.0, 0.0 }, { -OppDepth2, 2.0, 0.0 } };
		return C;
	}

	std::map<EAiShot, int> Count(const FShotContext& C, const char* Level, int Runs = 1000)
	{
		FPadelRng Rng(5);
		std::map<EAiShot, int> Out;
		for (int I = 0; I < Runs; ++I)
		{
			++Out[ShotChooser::ChooseRallyShot(C, TestData::Tactics(), TestData::Difficulty(Level), Rng).Shot];
		}
		return Out;
	}
}

TEST_CASE("Ai.ShotChoice: opponents at the net and a low ball: lob or chiquita")
{
	auto Shots = Count(Context(EContactKind::Groundstroke, 6.0, 0.6, 2.5, 2.5), "Hard");
	CHECK(Shots[EAiShot::Lob] + Shots[EAiShot::Chiquita] > 700);
	CHECK(Shots[EAiShot::Lob] > Shots[EAiShot::Drive]);
	CHECK(Shots[EAiShot::Chiquita] > 0);
	// From the back the chiquita is rare: the ball isn't in front.
	auto Deep = Count(Context(EContactKind::Groundstroke, 8.6, 0.6, 2.5, 2.5), "Hard");
	CHECK(Deep[EAiShot::Chiquita] < Shots[EAiShot::Chiquita]);
}

TEST_CASE("Ai.ShotChoice: a high ball at the net: bandeja by default, smash when short and high")
{
	auto Shots = Count(Context(EContactKind::Overhead, 3.0, 2.0, 8.5, 8.5), "Medium");
	CHECK(Shots[EAiShot::Bandeja] > Shots[EAiShot::Vibora]);
	CHECK(Shots[EAiShot::Smash] == 0);  // not high enough to smash
	auto High = Count(Context(EContactKind::Overhead, 3.0, 2.6, 8.5, 8.5), "Hard");
	CHECK(High[EAiShot::Smash] > 200);
}

TEST_CASE("Ai.ShotChoice: left-side players play the víbora more")
{
	FShotContext Right = Context(EContactKind::Overhead, 3.5, 2.0, 8.5, 8.5);
	FShotContext Left = Right;
	Left.Profile.PreferredSide = ESide::Left;
	CHECK(Count(Left, "Medium")[EAiShot::Vibora] > 2 * Count(Right, "Medium")[EAiShot::Vibora]);
}

TEST_CASE("Ai.ShotChoice: pinned at the back glass: a lob or a safe drive")
{
	FShotContext C = Context(EContactKind::Groundstroke, 9.2, 0.9, 8.5, 8.5);
	C.bOffGlass = true;
	auto Shots = Count(C, "Medium");
	CHECK(Shots[EAiShot::Lob] + Shots[EAiShot::Drive] > 900);
	CHECK(Shots[EAiShot::Lob] > Count(Context(EContactKind::Groundstroke, 6.0, 0.9, 8.5, 8.5), "Medium")[EAiShot::Lob]);
}

TEST_CASE("Ai.ShotChoice: Easy plays the safe lob more than Hard")
{
	const FShotContext C = Context(EContactKind::Groundstroke, 7.0, 1.0, 8.5, 8.5);
	CHECK(Count(C, "Easy")[EAiShot::Lob] > Count(C, "Hard")[EAiShot::Lob]);
}

TEST_CASE("Ai.ShotChoice: against a split pair, the drive goes through the middle")
{
	const FAiTactics& T = TestData::Tactics();
	const FShotContext C = Context(EContactKind::Groundstroke, 7.0, 1.0, 2.5, 8.5);
	CHECK(ShotChooser::ReadOpponents(C, T) == EOpponentShape::Split);
	FPadelRng Rng(1);
	const FAiVec Target = ShotChooser::IntendedTarget(EAiShot::Drive, C, T, TestData::Difficulty("Hard"), Rng);
	// Opponents at Y = -2 and +2: the gap is the middle.
	CHECK(std::abs(Target.Y) < 0.5);
	CHECK(Target.X < 0.0);
}

TEST_CASE("Ai.ShotChoice: targets land on the opponents' half, lobs deep, chiquitas short")
{
	const FAiTactics& T = TestData::Tactics();
	const FShotContext C = Context(EContactKind::Groundstroke, 7.0, 0.8, 2.5, 2.5);
	FPadelRng Rng(2);
	for (int I = 0; I < 50; ++I)
	{
		const FAiVec Lob = ShotChooser::IntendedTarget(EAiShot::Lob, C, T, TestData::Difficulty("Medium"), Rng);
		const FAiVec Chiquita = ShotChooser::IntendedTarget(EAiShot::Chiquita, C, T, TestData::Difficulty("Medium"), Rng);
		CHECK(TeamFrame::Depth(-1, Lob) == doctest::Approx(T.LobDepthM));
		CHECK(TeamFrame::Depth(-1, Chiquita) == doctest::Approx(T.ChiquitaDepthM));
		CHECK(std::abs(Lob.Y) <= T.TargetSideLimitM);
	}
}

TEST_CASE("Ai.ShotChoice: the serve goes diagonally into the box (GDD 3.2)")
{
	const FAiTactics& T = TestData::Tactics();
	FPadelRng Rng(4);
	for (int H : { 1, -1 })
	{
		for (double ServerY : { -1.5, 1.5 })
		{
			const FAiShotDecision Serve = ShotChooser::ChooseServe(H, { 7.6 * H, ServerY, 0.0 }, T, TestData::Difficulty("Hard"), Rng);
			CHECK(IsServe(Serve.Shot));
			CHECK(Serve.Target.X * H < 0.0);          // over the net
			CHECK(Serve.Target.Y * ServerY < 0.0);    // diagonal
			CHECK(std::abs(Serve.Target.X) < Padel::Court::ServiceLineFromNet);
		}
	}
}

TEST_CASE("Ai.Timing: grades follow the difficulty and offsets sit in their windows")
{
	const FAiTactics& T = TestData::Tactics();
	for (const char* Level : { "Easy", "Medium", "Hard" })
	{
		const FAiDifficulty& D = TestData::Difficulty(Level);
		FPadelRng Rng(9);
		std::map<EAiTimingGrade, int> Grades;
		const int Runs = 5000;
		for (int I = 0; I < Runs; ++I)
		{
			EAiTimingGrade Grade;
			double Offset = 0.0;
			ShotChooser::SampleTiming(T, D, Rng, Grade, Offset);
			++Grades[Grade];
			const double A = std::abs(Offset);
			switch (Grade)
			{
			case EAiTimingGrade::Perfect: CHECK(A <= T.WindowPerfectS); break;
			case EAiTimingGrade::Good: CHECK(A <= T.WindowGoodS); CHECK(A >= T.WindowPerfectS); break;
			case EAiTimingGrade::Early: CHECK(Offset < 0.0); CHECK(A <= T.WindowEarlyLateS); break;
			case EAiTimingGrade::Late: CHECK(Offset > 0.0); CHECK(A <= T.WindowEarlyLateS); break;
			case EAiTimingGrade::Miss: CHECK(A >= T.WindowEarlyLateS); break;
			}
		}
		const double Total = D.TimingWeightPerfect + D.TimingWeightGood + D.TimingWeightEarlyLate + D.TimingWeightMiss;
		CHECK(Grades[EAiTimingGrade::Perfect] / double(Runs) == doctest::Approx(D.TimingWeightPerfect / Total).epsilon(0.15));
	}
}

TEST_CASE("Ai.ShotChoice: extra aim scatter grows from Hard to Easy (GDD 8)")
{
	const FAiTactics& T = TestData::Tactics();
	// A volley against a pair at the back has one option and a fixed intended target.
	const FShotContext C = Context(EContactKind::Volley, 3.0, 1.0, 8.5, 8.5);
	FPadelRng TargetRng(1);
	const FAiVec Intended = ShotChooser::IntendedTarget(EAiShot::Volley, C, T, TestData::Difficulty("Medium"), TargetRng);
	auto MeanMiss = [&](const char* Level) {
		FPadelRng Rng(8);
		double Sum = 0.0;
		for (int I = 0; I < 2000; ++I)
		{
			const FAiShotDecision D = ShotChooser::ChooseRallyShot(C, T, TestData::Difficulty(Level), Rng);
			CHECK(D.Shot == EAiShot::Volley);
			Sum += (D.Target - Intended).Length2D();
		}
		return Sum / 2000.0;
	};
	const double Easy = MeanMiss("Easy");
	const double Hard = MeanMiss("Hard");
	CHECK(Easy > Hard);
	CHECK(Easy / Hard == doctest::Approx(1.4 / 0.8).epsilon(0.1));
}
