// GDD 6.1 Stamina and 6.3 fatigue (realism plan M3, BIOMECHANICS_MOVEMENT 2.13, 5.5): hard
// efforts and long rallies drain stamina, low stamina takes a little top speed and
// acceleration, and the gap between points brings it back.
#include "doctest.h"
#include "AI/AiMovementMetrics.h"
#include "ShotTestData.h"
#include "Shots/HardEffortCounter.h"
#include "Shots/MotionCurve.h"
#include "Shots/PlayerMotion.h"

#include <array>
#include <cstdlib>
#include <vector>

using namespace ShotTestData;
using doctest::Approx;

namespace
{
	constexpr double Dt = Padel::SimTickSeconds;

	// One tick of a rally (or of the gap after it), the way the shot controller drives it.
	void Tick(FPlayerMotion& M, const FVec2& Dir, bool bLive)
	{
		M.SetRallyLive(bLive);
		M.Step(Dir, false, Dt);
	}

	// A rally of Seconds: the player runs to a ball, stops, and turns back, every Leg
	// seconds, across the court (each start and each stop is a sharp effort).
	void Rally(FPlayerMotion& M, double Seconds, double Leg, double Rest)
	{
		const int Steps = static_cast<int>(Seconds / Dt);
		const int LegSteps = static_cast<int>(Leg / Dt);
		const int RestSteps = static_cast<int>(Rest / Dt);
		const FVec2 Dirs[] = { { 1.0, 0.0 }, { 0.0, 1.0 }, { -1.0, 0.0 }, { 0.0, -1.0 } };
		for (int I = 0; I < Steps; ++I)
		{
			const int Cycle = I % (LegSteps + RestSteps);
			const FVec2 Dir = Cycle < LegSteps ? Dirs[(I / (LegSteps + RestSteps)) % 4] : FVec2{};
			Tick(M, Dir, true);
		}
	}

	void Gap(FPlayerMotion& M, double Seconds)
	{
		for (int I = 0; I < static_cast<int>(Seconds / Dt); ++I)
		{
			Tick(M, {}, false);
		}
	}

	FPlayerRatings WithStamina(int Rating)
	{
		FPlayerRatings R;
		R.Stamina = Rating;
		return R;
	}

	double Regen(const FMovementTuning& T, const FPlayerRatings& R)
	{
		return Padel::Shots::Lerp(T.RegenAt1, T.RegenAt99, R.Unit(Padel::Shots::ERating::Stamina));
	}
}

TEST_CASE("Shots.Fatigue: hard efforts are counted the way the soak counts acceleration events")
{
	// Same smoothing window and duration as realism plan A1's metrics...
	const Padel::AI::FAiMovementMetricsConfig A1;
	CHECK(FHardEffortCounter::SmoothingS == A1.AccelSmoothingS);
	CHECK(FHardEffortCounter::MinDurationS == A1.AccelMinDurationS);
	// ...and the same events on the same velocity trace, at A1's threshold: starts, stops
	// and a wall stop in the middle.
	FMovementTuning T = LoadMovement();
	T.WallMarginM = -100.0;
	T.NetMarginM = -100.0;
	const FPlayerRatings R;
	FPlayerMotion M(&R, &T, 1.0, Vec(9.0, 0.0, 0.0));
	FHardEffortCounter Counter;
	Padel::AI::FAiMovementMetrics Metrics;
	Metrics.BeginMatch();
	int Counted = 0;
	const int Steps = static_cast<int>(20.0 / Dt);
	for (int I = 0; I < Steps; ++I)
	{
		const int Phase = (I / 90) % 4;
		M.Step(Phase == 0 ? FVec2{ 1.0, 0.0 } : Phase == 2 ? FVec2{ -0.5, 0.5 } : FVec2{}, false, Dt);
		const bool bEdgeStop = I == Steps / 2;
		Counted += Counter.Tick(M.GetVelocity(), bEdgeStop, A1.AccelEventMps2) ? 1 : 0;
		std::array<Padel::AI::FAiMovementSample, Padel::AI::FAiMovementMetrics::NumTracked> Samples;
		Samples.fill({ M.GetPosition(), M.GetVelocity(), bEdgeStop });
		Metrics.Tick(true, Samples, -1);
	}
	Metrics.EndPoint(-1);
	Metrics.EndMatch();
	const Padel::AI::FAiMovementSummary S = Metrics.Summarize();
	const double PlayerMinutes = S.BallInPlaySeconds * Padel::AI::FAiMovementMetrics::NumTracked / 60.0;
	CHECK(Counted > 10);
	CHECK(S.AccelEventsPerMinute * PlayerMinutes == Approx(Counted * Padel::AI::FAiMovementMetrics::NumTracked));
}

TEST_CASE("Shots.Fatigue: a 30 s rally of hard efforts tires the player a little, and the gap restores them")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings R;
	FPlayerMotion M(&R, &T, 1.0, Vec(8.0, 0.0, 0.0));
	const double FreshTop = M.TopSpeed(false);
	const double FreshPush = MotionCurve::InitialAccel(FreshTop, M.TimeToTopSpeed());
	Rally(M, 30.0, 0.9, 0.5);
	CHECK(M.GetHardEffortCount() >= 20);
	CHECK(M.GetRallyTimeS() == Approx(30.0).epsilon(0.01));
	const double Tired = M.GetStamina();
	INFO("stamina after the rally: " << Tired);
	CHECK(Tired < 0.8);
	// GDD 6.3: a little top speed and acceleration go, never more than the data's cap.
	const double Top = M.TopSpeed(false);
	CHECK(Top < FreshTop * 0.99);
	CHECK(Top >= FreshTop * (1.0 - T.FatigueTopSpeedLossAtZero));
	CHECK(Top == Approx(FreshTop * (1.0 - T.FatigueTopSpeedLossAtZero * (1.0 - Tired))));
	CHECK(MotionCurve::InitialAccel(Top, M.TimeToTopSpeed()) / FreshPush == Approx(Top / FreshTop));

	// The point ends: the between-point rest brings it all back before the next serve
	// (the match's gap is AiTactics' PointGapS, 1.5 s).
	Gap(M, 1.5);
	CHECK(M.GetStamina() == Approx(1.0));
	CHECK(M.TopSpeed(false) == Approx(FreshTop));
}

TEST_CASE("Shots.Fatigue: a normal 10 s rally barely changes stamina")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings R;
	FPlayerMotion M(&R, &T, 1.0, Vec(8.0, 0.0, 0.0));
	// A few moves to the ball and back, with time set between them.
	Rally(M, 10.0, 0.7, 1.5);
	CHECK(M.GetHardEffortCount() >= 3);
	INFO("stamina after the rally: " << M.GetStamina());
	CHECK(M.GetStamina() >= 0.95);
}

TEST_CASE("Shots.Fatigue: the rally drain starts at FatigueRallyStartS")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings R;
	FPlayerMotion M(&R, &T, 1.0, Vec(8.0, 0.0, 0.0));
	// Standing still: no efforts, only the clock.
	Rally(M, T.FatigueRallyStartS - 0.5, 0.0, 1.0);
	CHECK(M.GetStamina() == 1.0);
	Rally(M, 5.5, 0.0, 1.0);
	// Five seconds past the start, at the drain less the regen it fights.
	const double Drain = Padel::Shots::Lerp(T.FatigueDrainPerRallySAt1, T.FatigueDrainPerRallySAt99, R.Unit(Padel::Shots::ERating::Stamina));
	REQUIRE(Drain > Regen(T, R));
	CHECK(M.GetStamina() == Approx(1.0 - (Drain - Regen(T, R)) * 5.0).epsilon(0.002));
	CHECK(M.GetHardEffortCount() == 0);
}

TEST_CASE("Shots.Fatigue: a higher Stamina rating tires more slowly (GDD 6.1)")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings Low = WithStamina(1);
	const FPlayerRatings High = WithStamina(99);
	FPlayerMotion A(&Low, &T, 1.0, Vec(8.0, 0.0, 0.0));
	FPlayerMotion B(&High, &T, 1.0, Vec(8.0, 0.0, 0.0));
	Rally(A, 30.0, 0.9, 0.5);
	Rally(B, 30.0, 0.9, 0.5);
	// (The tired player runs a touch slower, so a count can differ by an effort or two.)
	CHECK(std::abs(A.GetHardEffortCount() - B.GetHardEffortCount()) <= 2);
	CHECK(A.GetStamina() < B.GetStamina());
	CHECK(T.FatigueDrainPerHardAccelAt1 > T.FatigueDrainPerHardAccelAt99);
	CHECK(T.FatigueDrainPerRallySAt1 > T.FatigueDrainPerRallySAt99);
}

TEST_CASE("Shots.Fatigue: between points nothing drains, and without a rally there is no rally clock")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings R;
	// The gap: players are walked to their serve spots, which isn't effort.
	FPlayerMotion Gapped(&R, &T, 1.0, Vec(8.0, 0.0, 0.0));
	Gapped.SetStamina(0.5);
	for (int I = 0; I < static_cast<int>(10.0 / Dt); ++I)
	{
		Tick(Gapped, (I / 90) % 2 == 0 ? FVec2{ 1.0, 0.0 } : FVec2{ -1.0, 0.0 }, false);
	}
	CHECK(Gapped.GetHardEffortCount() == 0);
	// Practice with no match (nobody calls SetRallyLive): hard efforts still count, but
	// there is no rally to run long and no point to end.
	FPlayerMotion Practice(&R, &T, 1.0, Vec(8.0, 0.0, 0.0));
	for (int I = 0; I < static_cast<int>(40.0 / Dt); ++I)
	{
		Practice.Step({}, false, Dt);
	}
	CHECK(Practice.GetStamina() == 1.0);
	CHECK(Practice.GetRallyTimeS() == 0.0);
}

TEST_CASE("Shots.Fatigue: the rest is credited once per point, at the regen rate")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings R;
	FPlayerMotion M(&R, &T, 1.0, Vec(8.0, 0.0, 0.0));
	Tick(M, {}, true);
	M.SetStamina(0.2);
	Tick(M, {}, false);
	// The point-end credit, then one tick of the usual regen.
	CHECK(M.GetStamina() == Approx(0.2 + Regen(T, R) * (T.FatigueBetweenPointRestS + Dt)));
	const double After = M.GetStamina();
	Tick(M, {}, false);
	CHECK(M.GetStamina() == Approx(After + Regen(T, R) * Dt));
}
