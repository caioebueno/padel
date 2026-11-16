// Realism plan A1: the soak's movement metrics on a scripted movement with known
// answers (distance, peak speed, acceleration events, first foot movement).
#include "doctest.h"
#include "AI/AiMovementMetrics.h"

using namespace Padel::AI;

namespace
{
	using FSamples = std::array<FAiMovementSample, FAiMovementMetrics::NumTracked>;

	// Player 0 waits 30 ticks after player 2's contact, runs along +X at 3 m/s for 240
	// ticks (6 m), then stops dead for 30 ticks. The others stand still. 300 ticks in play.
	void PlayScriptedPoint(FAiMovementMetrics& Metrics)
	{
		const double Dt = Padel::SimTickSeconds;
		FSamples Players{};
		Metrics.BeginMatch();
		for (int Tick = 0; Tick < 300; ++Tick)
		{
			const bool bRunning = Tick >= 30 && Tick < 270;
			Players[0].Velocity = { bRunning ? 3.0 : 0.0, 0.0, 0.0 };
			Players[0].Position.X += Players[0].Velocity.X * Dt;
			Metrics.Tick(true, Players, Tick == 0 ? 2 : -1);
		}
		Metrics.EndPoint(0);
		Metrics.EndMatch();
	}
}

TEST_CASE("Ai.MovementMetrics: a scripted run gives its known distance, peak and reaction")
{
	FAiMovementMetrics Metrics;
	PlayScriptedPoint(Metrics);
	const FAiMovementSummary S = Metrics.Summarize();

	CHECK(S.Points == 1);
	CHECK(S.PlayerMatches == 4);
	CHECK(S.BallInPlaySeconds == doctest::Approx(2.5));
	// Point distances are 6, 0, 0, 0: the p95 is the runner's, the median a stander's.
	CHECK(S.DistancePerPointP95 == doctest::Approx(6.0));
	CHECK(S.DistancePerPointMedian == doctest::Approx(0.0));
	CHECK(S.PeakSpeedMax == doctest::Approx(3.0));
	// 240 of 1200 player-ticks at 3 m/s.
	CHECK(S.AverageSpeed == doctest::Approx(0.6));
	CHECK(S.SlowShare == doctest::Approx(0.8));
	CHECK(S.FastShare == doctest::Approx(0.0));
	// The start and the stop are one event each (not one per tick above the threshold),
	// over 4 players x 2.5 s = 1/6 of a player-minute.
	CHECK(S.AccelEventsPerMinute == doctest::Approx(12.0));
	// Peaks are 360 m/s² (the instant start) and three 0s, so the median is a stander's.
	CHECK(S.PeakAccelMedian == doctest::Approx(0.0));
	// Only player 0 reacted to player 2's contact (player 1 never moved; 2 and 3 hit it).
	CHECK(S.ReactionSamples == 1);
	CHECK(S.ReactionMedianMs == doctest::Approx(250.0));
	CHECK(S.ServerDistancePerPoint == doctest::Approx(6.0));
	CHECK(S.ServerPartnerDistancePerPoint == doctest::Approx(0.0));
	CHECK(S.ServerFartherShare == doctest::Approx(1.0));
}

TEST_CASE("Ai.MovementMetrics: the peak acceleration is the largest tick-to-tick change")
{
	FAiMovementMetrics Metrics;
	FSamples Players{};
	Metrics.BeginMatch();
	// Every player speeds up by 0.05 m/s per tick: 6 m/s².
	for (int Tick = 0; Tick < 60; ++Tick)
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
	CHECK(S.PeakSpeedMedian == doctest::Approx(0.05 * 59));
	// A sustained push above 1.12 m/s² is one event per player, not 59.
	CHECK(S.AccelEventsPerMinute == doctest::Approx(4.0 / (4.0 * 0.5 / 60.0)));
	// No server named: the comparison stays empty.
	CHECK(S.ServerFartherShare == 0.0);
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
	CHECK(S.BallInPlaySeconds == doctest::Approx(2.0 * Padel::SimTickSeconds));
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
