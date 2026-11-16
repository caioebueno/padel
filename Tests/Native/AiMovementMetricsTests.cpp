// Realism plan A1: the soak's movement metrics on scripted movements with known
// answers (distance, peak speed, smoothed acceleration, lasting acceleration events,
// wall and net stops, first foot movement).
#include "doctest.h"
#include "AI/AiMovementMetrics.h"

using namespace Padel::AI;

namespace
{
	using FSamples = std::array<FAiMovementSample, FAiMovementMetrics::NumTracked>;
	constexpr double Dt = Padel::SimTickSeconds;

	// After player 2's contact at tick 0, player 0 waits 30 ticks, speeds up along +X at
	// 2 m/s² for 1 s (to 2 m/s), runs 2 s at 2 m/s, stops dead and stands for 30 ticks.
	// The others stand still. 420 ticks in play.
	void PlayScriptedPoint(FAiMovementMetrics& Metrics)
	{
		FSamples Players{};
		Metrics.BeginMatch();
		for (int Tick = 0; Tick < 420; ++Tick)
		{
			const double Speed = Tick < 30 ? 0.0 : Tick < 150 ? 2.0 * (Tick - 29) / 120.0 : Tick < 390 ? 2.0 : 0.0;
			Players[0].Velocity = { Speed, 0.0, 0.0 };
			Players[0].Position.X += Speed * Dt;
			Metrics.Tick(true, Players, Tick == 0 ? 2 : -1);
		}
		Metrics.EndPoint(0);
		Metrics.EndMatch();
	}

	// Ramp ticks 1..120 (2k/120 m/s each), then 240 ticks at 2 m/s.
	constexpr double RunDistance = (2.0 / 120.0) * (120.0 * 121.0 / 2.0) * Dt + 2.0 * 240.0 * Dt;
}

TEST_CASE("Ai.MovementMetrics: a scripted run gives its known distance, peaks and reaction")
{
	FAiMovementMetrics Metrics;
	PlayScriptedPoint(Metrics);
	const FAiMovementSummary S = Metrics.Summarize();

	CHECK(S.Points == 1);
	CHECK(S.PlayerMatches == 4);
	CHECK(S.BallInPlaySeconds == doctest::Approx(3.5));
	// Point distances are about 5.01, 0, 0, 0: the p95 is the runner's, the median a stander's.
	CHECK(S.DistancePerPointP95 == doctest::Approx(RunDistance));
	CHECK(S.DistancePerPointMedian == doctest::Approx(0.0));
	CHECK(S.PeakSpeedMax == doctest::Approx(2.0));
	CHECK(S.AverageSpeed == doctest::Approx((121.0 + 480.0) / (420.0 * 4.0)));
	// Below 2 m/s: player 0's 30 + 119 + 30 ticks and the standers' 3 x 420.
	CHECK(S.SlowShare == doctest::Approx(1439.0 / 1680.0));
	CHECK(S.FastShare == doctest::Approx(0.0));
	// Smoothed over 0.2 s the dead stop is 2 m/s in 0.2 s, 10 m/s², the run's peak.
	CHECK(S.PeakAccelMax == doctest::Approx(10.0));
	CHECK(S.PeakAccelMedian == doctest::Approx(0.0));
	// The 1 s ramp at 2 m/s² is one event; the stop is above 1.12 m/s² for only 0.2 s,
	// so it is none. Over 4 players x 3.5 s.
	CHECK(S.AccelEventsPerMinute == doctest::Approx(1.0 / (4.0 * 3.5 / 60.0)));
	CHECK(S.EdgeStopsPerMatch == 0.0);
	// Player 0 first passes 0.2 m/s on the 13th ramp tick, 42 ticks after the contact.
	CHECK(S.ReactionSamples == 1);
	CHECK(S.ReactionMedianMs == doctest::Approx(350.0));
	CHECK(S.ServerDistancePerPoint == doctest::Approx(RunDistance));
	CHECK(S.ServerPartnerDistancePerPoint == doctest::Approx(0.0));
	CHECK(S.ServerFartherShare == doctest::Approx(1.0));
}

TEST_CASE("Ai.MovementMetrics: acceleration is smoothed over 0.2 s and an event must last 0.3 s")
{
	SUBCASE("a steady 6 m/s² push is one event per player")
	{
		FAiMovementMetrics Metrics;
		FSamples Players{};
		Metrics.BeginMatch();
		for (int Tick = 0; Tick < 90; ++Tick)
		{
			for (FAiMovementSample& P : Players)
			{
				P.Velocity.Y = 0.05 * Tick;
			}
			Metrics.Tick(true, Players, -1);
		}
		Metrics.EndPoint(-1);
		Metrics.EndMatch();
		const FAiMovementSummary S = Metrics.Summarize();
		CHECK(S.PeakAccelMedian == doctest::Approx(6.0));
		CHECK(S.PeakSpeedMedian == doctest::Approx(0.05 * 89));
		CHECK(S.AccelEventsPerMinute == doctest::Approx(4.0 / (4.0 * 0.75 / 60.0)));
		// No server named: the comparison stays empty.
		CHECK(S.ServerFartherShare == 0.0);
	}
	SUBCASE("per-tick jitter averages out and short bursts don't count")
	{
		FAiMovementMetrics Metrics;
		FSamples Players{};
		Metrics.BeginMatch();
		for (int Tick = 0; Tick < 240; ++Tick)
		{
			// 2 m/s with a ±0.05 m/s wobble each tick: 12 m/s² per tick raw, ~0 smoothed.
			Players[0].Velocity.X = 2.0 + (Tick % 2 == 0 ? 0.05 : -0.05);
			// A 0.125 s burst at 4 m/s² every second: above the threshold, but too short.
			const int InSecond = Tick % 120;
			Players[1].Velocity.X = InSecond < 15 ? 4.0 * InSecond * Dt : 0.0;
			Metrics.Tick(true, Players, -1);
		}
		Metrics.EndPoint(-1);
		Metrics.EndMatch();
		const FAiMovementSummary S = Metrics.Summarize();
		CHECK(S.AccelEventsPerMinute == 0.0);
		CHECK(S.PeakAccelMax < 6.0);
	}
}

TEST_CASE("Ai.MovementMetrics: a wall or net dead stop is counted, not read as acceleration")
{
	FAiMovementMetrics Metrics;
	FSamples Players{};
	Metrics.BeginMatch();
	for (int Tick = 0; Tick < 120; ++Tick)
	{
		// Running at 3 m/s from the start, stopped by the wall at tick 60.
		Players[0].Velocity.X = Tick < 60 ? 3.0 : 0.0;
		Players[0].bEdgeStop = Tick == 60;
		Metrics.Tick(true, Players, -1);
	}
	Metrics.EndPoint(0);
	Metrics.EndMatch();
	const FAiMovementSummary S = Metrics.Summarize();
	CHECK(S.EdgeStopsPerMatch == 1.0);
	// Without the flag this would be a 15 m/s² stop.
	CHECK(S.PeakAccelMax == 0.0);
	CHECK(S.AccelEventsPerMinute == 0.0);
}

TEST_CASE("Ai.MovementMetrics: placing players between points is not movement")
{
	FAiMovementMetrics Metrics;
	FSamples Players{};
	Metrics.BeginMatch();
	Metrics.Tick(true, Players, -1);
	// The point is over; player 0 is put 8 m away for the next serve.
	Players[0].Position = { 8.0, 2.0, 0.0 };
	Metrics.Tick(false, Players, -1);
	Metrics.Tick(true, Players, -1);
	Metrics.EndPoint(0);
	Metrics.EndMatch();
	const FAiMovementSummary S = Metrics.Summarize();
	CHECK(S.DistancePerPointP95 == 0.0);
	CHECK(S.BallInPlaySeconds == doctest::Approx(2.0 * Dt));
}

TEST_CASE("Ai.MovementMetrics: merged runs equal one run over both, in any order")
{
	FAiMovementMetrics A;
	FAiMovementMetrics B;
	PlayScriptedPoint(A);
	PlayScriptedPoint(B);
	FAiMovementMetrics AB = A;
	AB.Merge(B);
	FAiMovementMetrics BA = B;
	BA.Merge(A);
	const FAiMovementSummary One = A.Summarize();
	const FAiMovementSummary SAB = AB.Summarize();
	const FAiMovementSummary SBA = BA.Summarize();
	CHECK(SAB.Points == 2);
	CHECK(SAB.PlayerMatches == 8);
	CHECK(SAB.ReactionSamples == 2);
	CHECK(SAB.AverageSpeed == doctest::Approx(One.AverageSpeed));
	CHECK(SAB.AccelEventsPerMinute == doctest::Approx(One.AccelEventsPerMinute));
	CHECK(SAB.DistancePerPointP95 == SBA.DistancePerPointP95);
	CHECK(SAB.ReactionMedianMs == SBA.ReactionMedianMs);
}
