// GDD 8 prediction and interception.
#include "doctest.h"
#include "AI/AiInterceptPlanner.h"
#include "Support/AiTestData.h"

#include <cmath>

using namespace Padel::AI;
using Padel::FPadelRng;
using doctest::Approx;

namespace
{
	// A ball hit from Team B's half (-X) toward Team A (+X), landing around X = 7.
	FAiBall IncomingDrive()
	{
		return { { -6.0, 0.0, 1.0 }, { 13.0, 0.0, 3.0 }, {} };
	}
}

TEST_CASE("Ai.Prediction: stops at the second bounce and counts bounces per half")
{
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBallPredictor Predictor(Model, TestData::Tactics());
	FPadelRng Rng(1);
	const std::vector<FTrajectorySample> Samples = Predictor.Predict(IncomingDrive(), 0.0, Rng);
	REQUIRE_FALSE(Samples.empty());
	CHECK(Samples.front().Half == -1);
	CHECK(Samples.back().GroundBounces == 2);
	for (const FTrajectorySample& S : Samples)
	{
		CHECK(S.GroundBounces <= 2);
		if (S.Half == -1)
		{
			CHECK(S.GroundBounces == 0);
		}
	}
	// It crossed the net and bounced on Team A's half.
	CHECK(Samples.back().Half == 1);
}

TEST_CASE("Ai.Prediction: rebounds off the back glass are flagged and kept")
{
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBallPredictor Predictor(Model, TestData::Tactics());
	FPadelRng Rng(1);
	// Deep and fast: bounces near the back glass, then hits it.
	const FAiBall Deep = { { -5.0, 0.0, 1.0 }, { 16.0, 0.0, 2.5 }, {} };
	const std::vector<FTrajectorySample> Samples = Predictor.Predict(Deep, 0.0, Rng);
	bool bSawGlass = false;
	for (const FTrajectorySample& S : Samples)
	{
		bSawGlass = bSawGlass || S.bOffGlass;
	}
	CHECK(bSawGlass);
}

TEST_CASE("Ai.Prediction: Wall Play noise moves only the rebound and is seeded")
{
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBallPredictor Predictor(Model, TestData::Tactics());
	const FAiBall Deep = { { -5.0, 0.0, 1.0 }, { 16.0, 0.0, 2.5 }, {} };
	FPadelRng Clean(1);
	FPadelRng NoisyA(7);
	FPadelRng NoisyB(7);
	const auto Exact = Predictor.Predict(Deep, 0.0, Clean);
	const auto A = Predictor.Predict(Deep, 0.5, NoisyA);
	const auto B = Predictor.Predict(Deep, 0.5, NoisyB);
	REQUIRE(Exact.size() == A.size());
	REQUIRE(A.size() == B.size());
	bool bMoved = false;
	bool bWallSeen = false;
	for (size_t I = 0; I < A.size(); ++I)
	{
		CHECK(A[I].Position.X == B[I].Position.X);
		bWallSeen = bWallSeen || A[I].bOffGlass;
		if (!bWallSeen)
		{
			CHECK(A[I].Position.X == Exact[I].Position.X);
		}
		bMoved = bMoved || (A[I].Position - Exact[I].Position).Length2D() > 1e-6;
	}
	CHECK(bMoved);
}

TEST_CASE("Ai.Intercept: time to cover follows the acceleration model")
{
	CHECK(InterceptPlanner::TimeToCover(0.0, 6.0, 0.6) == Approx(0.0));
	// Accelerating phase covers 0.5 * 6 * 0.6 = 1.8 m in 0.6 s.
	CHECK(InterceptPlanner::TimeToCover(1.8, 6.0, 0.6) == Approx(0.6));
	CHECK(InterceptPlanner::TimeToCover(7.8, 6.0, 0.6) == Approx(1.6));
}

TEST_CASE("Ai.Intercept: picks a reachable point on the own half before the second bounce")
{
	const FAiTactics& T = TestData::Tactics();
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBallPredictor Predictor(Model, T);
	FPadelRng Rng(1);
	const auto Samples = Predictor.Predict(IncomingDrive(), 0.0, Rng);
	FInterceptQuery Query;
	Query.PlayerPosition = { 8.5, 1.0, 0.0 };
	Query.HalfSign = 1;
	const auto Plan = InterceptPlanner::Plan(Samples, Query, TestData::Player(ESide::Right), T);
	REQUIRE(Plan.has_value());
	CHECK(Plan->bReachable);
	CHECK(Plan->Sample.Half == 1);
	CHECK(Plan->Sample.GroundBounces < 2);
	CHECK(Plan->Sample.Position.Z >= T.MinContactHeightM);
	CHECK(Plan->Slack >= 0.0);
	// The ball meets the player at the shot model's ideal contact point (GDD 5.2).
	const double Ideal = std::sqrt(T.ContactSideM * T.ContactSideM + T.ContactForwardM * T.ContactForwardM);
	CHECK((Plan->StandPoint - Plan->Sample.Position).Length2D() == Approx(Ideal).epsilon(0.05));
}

TEST_CASE("Ai.Intercept: the serve return waits for the bounce (GDD 3.2)")
{
	const FAiTactics& T = TestData::Tactics();
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBallPredictor Predictor(Model, T);
	FPadelRng Rng(1);
	const auto Samples = Predictor.Predict(IncomingDrive(), 0.0, Rng);
	FInterceptQuery Query;
	Query.PlayerPosition = { 3.0, 0.5, 0.0 };  // at the net: would volley it
	Query.HalfSign = 1;
	const auto Volley = InterceptPlanner::Plan(Samples, Query, TestData::Player(ESide::Right), T);
	REQUIRE(Volley.has_value());
	CHECK(Volley->Kind == EContactKind::Volley);
	Query.bMustBounce = true;
	const auto Return = InterceptPlanner::Plan(Samples, Query, TestData::Player(ESide::Right), T);
	REQUIRE(Return.has_value());
	CHECK(Return->Sample.GroundBounces == 1);
}

TEST_CASE("Ai.Intercept: an unreachable ball is still chased, flagged as late")
{
	const FAiTactics& T = TestData::Tactics();
	const IAiBallModel& Model = TestData::EngineModel();
	const FAiBallPredictor Predictor(Model, T);
	FPadelRng Rng(1);
	// A short ball across the court from the player, with a long read delay.
	const FAiBall Wide = { { -4.0, 2.0, 1.0 }, { 9.0, 1.0, 2.5 }, {} };
	const auto Samples = Predictor.Predict(Wide, 0.0, Rng);
	FInterceptQuery Query;
	Query.PlayerPosition = { 9.5, -4.5, 0.0 };
	Query.HalfSign = 1;
	Query.StartDelayS = 1.5;
	const auto Plan = InterceptPlanner::Plan(Samples, Query, TestData::Player(ESide::Left), T);
	REQUIRE(Plan.has_value());
	CHECK_FALSE(Plan->bReachable);
	CHECK(Plan->Slack < 0.0);
}
