// GDD 12 item 2: AI-vs-AI soak on the real match (FMatchSimulation: ball, referee,
// score) with all four players swinging through the shot system's controller. Fixed
// seeds; no point may hang and every point must end with a GDD 3.3 reason.
//
// CI runs a 24-match soak on every PR. The full 100 matches:
//   PADEL_SOAK_MATCHES=100 Tools/RunNativeTests.sh -tc="Ai.Soak*"
// PADEL_SOAK_DEBUG=1 prints every stuck point and whiff.
#include "doctest.h"
#include "AI/AiMatchRunner.h"
#include "AI/AiMovementMetrics.h"
#include "ShotTestData.h"
#include "Support/AiTestData.h"
#include "TestSupport.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

using namespace Padel::AI;

namespace
{
	using Padel::EMatchPhase;
	using Padel::FMatchSimulation;

	struct FSoakStats
	{
		int Matches = 0;
		int Unfinished = 0;
		int Points = 0;
		int StuckPoints = 0;
		int InvariantFailures = 0;
		int Shots = 0;
		int Whiffs = 0;
		int PointsWithGlassShot = 0;
		std::map<std::string, int> Reasons;
		std::map<std::string, int> Faults;
		std::map<std::string, int> ShotCounts;
		std::map<std::string, int> Grades;
		// Why the losing pair let a ball bounce twice.
		std::map<std::string, int> Unreached;
		// The shot that won a point the opponents couldn't reach.
		std::map<std::string, int> Winners;
		// How the players moved (realism plan A1).
		FAiMovementMetrics Movement;

		void Add(const FSoakStats& O)
		{
			Matches += O.Matches;
			Unfinished += O.Unfinished;
			Points += O.Points;
			StuckPoints += O.StuckPoints;
			InvariantFailures += O.InvariantFailures;
			Shots += O.Shots;
			Whiffs += O.Whiffs;
			PointsWithGlassShot += O.PointsWithGlassShot;
			for (const auto& [K, V] : O.Reasons) Reasons[K] += V;
			for (const auto& [K, V] : O.Faults) Faults[K] += V;
			for (const auto& [K, V] : O.ShotCounts) ShotCounts[K] += V;
			for (const auto& [K, V] : O.Grades) Grades[K] += V;
			for (const auto& [K, V] : O.Unreached) Unreached[K] += V;
			for (const auto& [K, V] : O.Winners) Winners[K] += V;
			Movement.Merge(O.Movement);
		}
		double ShotsPerPoint() const { return Points > 0 ? static_cast<double>(Shots) / Points : 0.0; }
		double GlassShare() const { return Points > 0 ? static_cast<double>(PointsWithGlassShot) / Points : 0.0; }
	};

	struct FSoakOptions
	{
		// GDD 12 asks for whole matches; one set keeps CI quick and still covers games,
		// golden points, tiebreaks and the change of ends.
		int SetsToWin = 1;
		// A point hangs when nobody has hit the ball for this long while it is in play
		// or waiting for the serve (a live rally has a shot every couple of seconds).
		double MaxSecondsWithoutHit = 20.0;
		double MaxMatchSeconds = 4.0 * 3600.0;
	};

	const char* GradeName(Padel::ETimingGrade Grade)
	{
		switch (Grade)
		{
		case Padel::ETimingGrade::Perfect: return "perfect";
		case Padel::ETimingGrade::Good: return "good";
		case Padel::ETimingGrade::Early: return "early";
		case Padel::ETimingGrade::Late: return "late";
		default: return "miss";
		}
	}

	// Counts what the match reports.
	struct FSoakListener : Padel::IMatchEventListener
	{
		FSoakStats* Stats = nullptr;
		bool bGlassShot = false;
		bool bMatchOver = false;
		Padel::Shots::FShotIntent LastShot;
		const FAiMatchRunner* Runner = nullptr;
		std::array<bool, 2> bWhiffed = { false, false };
		int ServerId = -1;

		void OnServeStarted(int Server) override { ServerId = Server; }
		void OnServeFault(Padel::EServeFault Reason) override { ++Stats->Faults[std::to_string(static_cast<int>(Reason))]; }
		void OnPointEnded(Padel::ETeam Winner, Padel::EPointReason Reason, int) override
		{
			++Stats->Points;
			Stats->Movement.EndPoint(ServerId);
			if (Reason == Padel::EPointReason::DoubleBounce && Runner)
			{
				const Padel::ETeam Loser = Padel::OtherTeam(Winner);
				const FAiTeamController& Team = Runner->Team(Loser);
				const int Taker = Team.GetTaker();
				const char* Why = bWhiffed[Padel::TeamIndex(Loser)] ? "whiffed"
					: Taker < 0 || !Team.GetPlan(Taker) ? "no_plan"
					: !Team.GetPlan(Taker)->bReachable ? "out_of_reach"
					: "reachable_no_hit";
				++Stats->Unreached[Why];
				++Stats->Winners[Padel::ShotTypeName(LastShot.ShotType)];
				if (std::getenv("PADEL_SOAK_DEBUG") && Taker >= 0)
				{
					int Id = -1;
					for (int I = 0; I < Padel::NumPlayers; ++I)
					{
						if (Padel::TeamOfPlayer(I) == Loser && Runner->SlotOf(I) == Taker)
						{
							Id = I;
						}
					}
					const Padel::Shots::FPlayerShotController& C = Runner->Player(Id);
					const auto& Plan = *Team.GetPlan(Taker);
					const Padel::FVec3& P = C.GetMotion().GetPosition();
					std::printf("unreached why=%s id=%d phase=%d ideal=%d toIdeal=%.2f pending=%d slack=%.2f stand=(%.2f,%.2f) player=(%.2f,%.2f) planball=(%.2f,%.2f,%.2f) bounces=%d\n",
						Why, Id, static_cast<int>(C.GetPhase()), C.HasIdealMoment(), C.TimeToIdealS(), Runner->HasPendingShot(Id), Plan.Slack,
						Plan.StandPoint.X, Plan.StandPoint.Y, P.X, P.Y, Plan.Sample.Position.X, Plan.Sample.Position.Y, Plan.Sample.Position.Z, Plan.Sample.GroundBounces);
				}
			}
			bWhiffed = { false, false };
			if (Reason == Padel::EPointReason::None)
			{
				++Stats->InvariantFailures;
			}
			++Stats->Reasons[Padel::PointReasonCode(Reason)];
			if (std::getenv("PADEL_SOAK_DEBUG") && (Reason == Padel::EPointReason::IntoNet || Reason == Padel::EPointReason::OwnSide))
			{
				const Padel::Shots::FShotIntent& I = LastShot;
				std::printf("%s shot=%s grade=%d origin=(%.2f,%.2f,%.2f) target=(%.2f,%.2f) speed=%.1f apex=%.2f spin=(%.0f,%.0f)\n",
					Padel::PointReasonCode(Reason), Padel::ShotTypeName(I.ShotType), static_cast<int>(I.TimingGrade), I.Origin.X, I.Origin.Y, I.Origin.Z, I.Target.X,
					I.Target.Y, I.SpeedMps, I.ApexHeightM, I.Spin.X, I.Spin.Y);
			}
			Stats->PointsWithGlassShot += bGlassShot ? 1 : 0;
			bGlassShot = false;
		}
		void OnMatchWon(Padel::ETeam) override { bMatchOver = true; }
	};

	struct FSoakData
	{
		Padel::Shots::FShotLibrary Library = ShotTestData::LoadLibrary();
		Padel::Shots::FMovementTuning Movement = ShotTestData::LoadMovement();
		Padel::Shots::FPlayerRatings Player = ShotTestData::LoadPlayer();
	};

	const FSoakData& Data()
	{
		static const FSoakData Loaded;
		return Loaded;
	}

	FAiMatchSetup MakeSetup(const FAiDifficulty& A, const FAiDifficulty& B, uint64_t Seed)
	{
		FAiMatchSetup Setup;
		for (int Id = 0; Id < Padel::NumPlayers; ++Id)
		{
			Setup.Players[Id] = Data().Player;
			// Players 0 and 2 start on the right (FMatchScore's default), so they prefer it.
			Setup.Players[Id].PreferredSide = Id % 2 == 0 ? Padel::Shots::ECourtSide::Right : Padel::Shots::ECourtSide::Left;
		}
		Setup.Difficulties = { &A, &B };
		Setup.Seed = Seed;
		return Setup;
	}

	// The ball is in play from the serve's contact to the point's end (Live).
	// LastEdgeStops carries each player's FPlayerMotion edge-stop count between ticks.
	void SampleMovement(const FMatchSimulation& Match, const FAiMatchRunner& Runner, std::array<int, Padel::NumPlayers>& LastEdgeStops,
		FAiMovementMetrics& Metrics)
	{
		std::array<FAiMovementSample, FAiMovementMetrics::NumTracked> Players;
		int Hitter = -1;
		for (int Id = 0; Id < Padel::NumPlayers; ++Id)
		{
			const Padel::Shots::FPlayerMotion& M = Runner.Player(Id).GetMotion();
			Players[Id] = { M.GetPosition(), M.GetVelocity(), M.GetEdgeStopCount() != LastEdgeStops[Id] };
			LastEdgeStops[Id] = M.GetEdgeStopCount();
			Hitter = Runner.HitAccepted(Id) ? Id : Hitter;
		}
		Metrics.Tick(Match.Phase() == EMatchPhase::Live, Players, Hitter);
	}

	// Plays one match to the end and returns what happened.
	struct FSoakMatch
	{
		int Winner = -1;
		std::vector<int> Hitters;

		FSoakStats Play(const FAiDifficulty& A, const FAiDifficulty& B, uint64_t Seed, const FSoakOptions& Options = {})
		{
			FSoakStats Stats;
			Stats.Matches = 1;
			Padel::FMatchConfig Config = Padel::FMatchConfig::OneSet();
			Config.SetsToWin = Options.SetsToWin;
			FMatchSimulation Match(Config, Padel::Test::Physics(), Seed);
			FSoakListener Listener;
			Listener.Stats = &Stats;
			Match.AddListener(&Listener);
			FAiMatchRunner Runner(Match, Data().Library, Data().Movement, TestData::Tactics(), MakeSetup(A, B, Seed));
			Listener.Runner = &Runner;
			Runner.Start();
			Stats.Movement.BeginMatch();
			std::array<int, Padel::NumPlayers> EdgeStops{};
			for (int Id = 0; Id < Padel::NumPlayers; ++Id)
			{
				EdgeStops[Id] = Runner.Player(Id).GetMotion().GetEdgeStopCount();
			}

			double LastHit = Match.Time();
			while (!Listener.bMatchOver)
			{
				if (Match.Time() - LastHit > Options.MaxSecondsWithoutHit || Match.Time() > Options.MaxMatchSeconds)
				{
					++Stats.StuckPoints;
					if (std::getenv("PADEL_SOAK_DEBUG"))
					{
						const Padel::FBallState& Ball = Match.Ball();
						std::printf("stuck seed=%llu phase=%d ball=(%.2f,%.2f,%.2f) v=(%.2f,%.2f,%.2f) server=%d\n",
							static_cast<unsigned long long>(Seed), static_cast<int>(Match.Phase()), Ball.Position.X, Ball.Position.Y,
							Ball.Position.Z, Ball.Velocity.X, Ball.Velocity.Y, Ball.Velocity.Z, Match.ServerId());
					}
					++Stats.Unfinished;
					Stats.Movement.EndMatch();
					return Stats;
				}
				Runner.Tick();
				if (Match.Phase() != EMatchPhase::Live && Match.Phase() != EMatchPhase::AwaitingServe)
				{
					LastHit = Match.Time();  // between points the clock is the runner's
				}
				SampleMovement(Match, Runner, EdgeStops, Stats.Movement);
				for (int Id = 0; Id < Padel::NumPlayers; ++Id)
				{
					const Padel::Shots::FSwingEvents& E = Runner.LastEvents(Id);
					if (Runner.HitAccepted(Id))
					{
						Listener.bWhiffed = { false, false };
						LastHit = Match.Time();
						Hitters.push_back(Id);
						++Stats.Shots;
						++Stats.ShotCounts[Padel::ShotTypeName(E.Intent.ShotType)];
						++Stats.Grades[GradeName(E.Intent.TimingGrade)];
						Listener.bGlassShot = Listener.bGlassShot || E.Intent.bWallShot;
						Listener.LastShot = E.Intent;
						if (std::getenv("PADEL_SOAK_DEBUG") && !Padel::IsServeShot(E.Intent.ShotType))
						{
							const Padel::Shots::FPlayerShotController& C = Runner.Player(Id);
							const double Off = std::min(Padel::Shots::DistanceXY(E.Intent.Origin, C.IdealContactPoint(1.0, 0.0)),
								Padel::Shots::DistanceXY(E.Intent.Origin, C.IdealContactPoint(-1.0, 0.0)));
							std::printf("hit id=%d shot=%s off=%.2f depth=%.2f z=%.2f speed=%.1f charge=%.2f\n", Id, Padel::ShotTypeName(E.Intent.ShotType), Off, std::abs(E.Intent.Origin.X),
								E.Intent.Origin.Z, E.Intent.SpeedMps, E.Intent.Charge);
						}
					}
					else if (E.Outcome == Padel::Shots::ESwingOutcome::Whiff || E.Outcome == Padel::Shots::ESwingOutcome::ShadowSwing)
					{
						++Stats.Whiffs;
						Listener.bWhiffed[Padel::TeamIndex(Padel::TeamOfPlayer(Id))] = true;
						if (std::getenv("PADEL_SOAK_DEBUG"))
						{
							const Padel::FVec3& P = Runner.Player(Id).GetMotion().GetPosition();
							const Padel::ETeam T = Padel::TeamOfPlayer(Id);
							const auto& Plan = Runner.Team(T).GetPlan(Runner.SlotOf(Id));
							const Padel::Shots::FPlayerShotController& C = Runner.Player(Id);
							const Padel::Shots::FBallView V = Runner.Ball().Current();
							const Padel::FVec3 Ideal = C.GetIdealBallPosition();
							const double Gap = Padel::Shots::DistanceXY(Ideal, P);
							const char* Why = V.GroundBounces > 1 ? "bounced_twice"
								: Ideal.Z > C.MaxContactHeight() ? "too_high"
								: Gap > C.ReachM() + Data().Library.Model.ReachSlackM ? "too_far" : "other";
							std::printf("whiff id=%d why=%s gap=%.2f reach=%.2f z=%.2f max=%.2f bounces=%d taker=%d plan=%d stand=(%.2f,%.2f) player=(%.2f,%.2f) ideal=(%.2f,%.2f) slack=%.2f\n",
								Id, Why, Gap, C.ReachM(), Ideal.Z, C.MaxContactHeight(), V.GroundBounces, Runner.Team(T).GetTaker(), Plan.has_value(),
								Plan ? Plan->StandPoint.X : 0.0, Plan ? Plan->StandPoint.Y : 0.0, P.X, P.Y, Ideal.X, Ideal.Y, Plan ? Plan->Slack : 0.0);
						}
					}
				}
			}
			Stats.Movement.EndMatch();
			Winner = Padel::TeamIndex(Match.Score().Winner());
			Match.RemoveListener(&Listener);
			return Stats;
		}
	};

	int SoakMatches(int Default)
	{
		const char* Env = std::getenv("PADEL_SOAK_MATCHES");
		return Env != nullptr ? std::max(1, std::atoi(Env)) : Default;
	}

	FSoakStats Soak(const char* Level, int Matches, uint64_t FirstSeed)
	{
		FSoakStats Total;
		const FAiDifficulty& D = TestData::Difficulty(Level);
		for (int Index = 0; Index < Matches; ++Index)
		{
			FSoakMatch Game;
			const uint64_t Seed = FirstSeed + static_cast<uint64_t>(Index);
			const FSoakStats One = Game.Play(D, D, Seed);
			if (std::getenv("PADEL_SOAK_PER_MATCH"))
			{
				std::printf("[soak]   %s seed %llu: %d points, %.2f shots/point, %d whiffs\n", Level, static_cast<unsigned long long>(Seed),
					One.Points, One.ShotsPerPoint(), One.Whiffs);
			}
			Total.Add(One);
		}
		return Total;
	}

	// Realism plan A1: each metric with its real-padel band (BIOMECHANICS_MOVEMENT.md
	// section 4). Reported, not asserted yet; "!" marks a value outside its band.
	// Acceleration is smoothed over 0.2 s and events last 0.3 s, like the GNSS/IMU
	// data the bands come from; the wall and net dead stops are counted on their own.
	void ReportMovement(const FAiMovementSummary& M)
	{
		const auto Line = [](const char* Name, double Value, const char* Unit, double Lo, double Hi, const char* Band)
		{
			const bool bOut = Value < Lo || Value > Hi;
			std::printf("[soak]     %-40s %8.2f %-6s [%s]%s\n", Name, Value, Unit, Band, bOut ? " !" : "");
		};
		constexpr double Any = 1e9;
		std::printf("[soak]   movement: %.0f s ball in play, %d player-matches, %d reactions\n", M.BallInPlaySeconds, M.PlayerMatches, M.ReactionSamples);
		Line("distance per point, median", M.DistancePerPointMedian, "m", 6.0, 12.0, "6 to 12");
		Line("distance per point, p95", M.DistancePerPointP95, "m", 0.0, 30.0, "under 30");
		Line("average speed, ball in play", M.AverageSpeed, "m/s", 0.9, 1.3, "0.9 to 1.3");
		Line("rally time below 2 m/s", 100.0 * M.SlowShare, "%", 65.0, 75.0, "65 to 75");
		Line("rally time above 4 m/s", 100.0 * M.FastShare, "%", 0.0, 5.0, "under 5");
		Line("peak speed per match, median", M.PeakSpeedMedian, "m/s", 4.0, 5.0, "4 to 5");
		Line("peak speed per match, max", M.PeakSpeedMax, "m/s", 0.0, 7.0, "7 or less");
		Line("peak acceleration per match, median", M.PeakAccelMedian, "m/s2", 4.0, 8.0, "4 to 8");
		Line("peak acceleration per match, max", M.PeakAccelMax, "m/s2", -Any, Any, "-");
		Line("accel > 1.12 m/s2 for 0.3 s, per minute", M.AccelEventsPerMinute, "/min", 15.0, 20.0, "about 15 to 20");
		Line("wall/net dead stops per match", M.EdgeStopsPerMatch, "", 0.0, 0.0, "0: real players slow down");
		Line("first foot movement, median", M.ReactionMedianMs, "ms", 250.0, 300.0, "250 to 300 (Hard)");
		Line("server distance per point", M.ServerDistancePerPoint, "m", M.ServerPartnerDistancePerPoint, Any, "above partner");
		Line("server's partner distance per point", M.ServerPartnerDistancePerPoint, "m", -Any, Any, "-");
		Line("points the server moved more", 100.0 * M.ServerFartherShare, "%", 50.0, 100.0, "over 50");
	}

	void Report(const char* Label, const FSoakStats& S)
	{
		std::printf("[soak] %s: %d matches (%d unfinished), %d points, %.2f shots/point, %.0f%% points with an off-the-glass shot, "
					"%d stuck, %d invariant failures, %d whiffs\n",
			Label, S.Matches, S.Unfinished, S.Points, S.ShotsPerPoint(), 100.0 * S.GlassShare(), S.StuckPoints, S.InvariantFailures,
			S.Whiffs);
		std::printf("[soak]   endings:");
		for (const auto& [Reason, Count] : S.Reasons) std::printf(" %s=%d", Reason.c_str(), Count);
		std::printf("\n[soak]   serve faults:");
		for (const auto& [Fault, Count] : S.Faults) std::printf(" %s=%d", Fault.c_str(), Count);
		std::printf("\n[soak]   shots:");
		for (const auto& [Shot, Count] : S.ShotCounts) std::printf(" %s=%d", Shot.c_str(), Count);
		std::printf("\n[soak]   double bounces:");
		for (const auto& [Why, Count] : S.Unreached) std::printf(" %s=%d", Why.c_str(), Count);
		std::printf("\n[soak]   winners:");
		for (const auto& [Shot, Count] : S.Winners) std::printf(" %s=%d", Shot.c_str(), Count);
		std::printf("\n[soak]   timing:");
		for (const auto& [Grade, Count] : S.Grades) std::printf(" %s=%d", Grade.c_str(), Count);
		std::printf("\n");
		ReportMovement(S.Movement.Summarize());
	}
}

TEST_CASE("Ai.Soak: Medium AI-vs-AI matches finish with no stuck points")
{
	// 24 matches from the fixed seeds 1000 to 1023. One match's shots per point spreads
	// with a standard deviation of about 1.2, so 4 matches left the mean within about
	// 0.6 of its true value and a change could fail the 12-shot cap on luck alone. With 24 it
	// is about 0.24. PADEL_SOAK_PER_MATCH=1 prints each match.
	const FSoakStats S = Soak("Medium", SoakMatches(24), 1000);
	Report("Medium", S);
	CHECK(S.StuckPoints == 0);
	CHECK(S.Unfinished == 0);
	CHECK(S.InvariantFailures == 0);
	CHECK(S.Points > 0);
	// GDD 12.4 feel targets on Medium (issue 20): rallies of 6 to 12 shots, a quarter of
	// the points with a shot off the glass, smashes in the mix, and GDD 8's "mostly
	// good, some perfect" timing.
	CHECK(S.ShotsPerPoint() >= 6.0);
	CHECK(S.ShotsPerPoint() <= 12.0);
	CHECK(S.GlassShare() >= 0.25);
	const auto Count = [](const std::map<std::string, int>& M, const char* Key)
	{
		const auto It = M.find(Key);
		return It != M.end() ? It->second : 0;
	};
	CHECK(Count(S.ShotCounts, "smash") > S.Points / 10);
	CHECK(Count(S.Grades, "good") > Count(S.Grades, "perfect"));
	CHECK(Count(S.Grades, "perfect") > 0);
}

TEST_CASE("Ai.Soak: every difficulty plays without getting stuck")
{
	for (const char* Level : { "Easy", "Hard" })
	{
		const FSoakStats S = Soak(Level, SoakMatches(2), 2000);
		Report(Level, S);
		CHECK(S.StuckPoints == 0);
		CHECK(S.InvariantFailures == 0);
	}
}

TEST_CASE("Ai.Soak: a match replays identically from its seed (GDD 8 determinism)")
{
	const FAiDifficulty& D = TestData::Difficulty("Medium");
	FSoakMatch A;
	FSoakMatch B;
	const FSoakStats SA = A.Play(D, D, 77);
	const FSoakStats SB = B.Play(D, D, 77);
	CHECK(A.Winner == B.Winner);
	CHECK(SA.Points == SB.Points);
	CHECK(A.Hitters == B.Hitters);
	CHECK(SA.Reasons == SB.Reasons);
	// Realism plan A1: the movement metrics replay too.
	const FAiMovementSummary MA = SA.Movement.Summarize();
	const FAiMovementSummary MB = SB.Movement.Summarize();
	CHECK(MA.BallInPlaySeconds > 0.0);
	CHECK(MA.DistancePerPointMedian == MB.DistancePerPointMedian);
	CHECK(MA.DistancePerPointP95 == MB.DistancePerPointP95);
	CHECK(MA.AverageSpeed == MB.AverageSpeed);
	CHECK(MA.PeakSpeedMax == MB.PeakSpeedMax);
	CHECK(MA.PeakAccelMedian == MB.PeakAccelMedian);
	CHECK(MA.AccelEventsPerMinute == MB.AccelEventsPerMinute);
	CHECK(MA.EdgeStopsPerMatch == MB.EdgeStopsPerMatch);
	CHECK(MA.ReactionSamples == MB.ReactionSamples);
	CHECK(MA.ReactionMedianMs == MB.ReactionMedianMs);
	CHECK(MA.ServerFartherShare == MB.ServerFartherShare);
}
