// GDD 12 item 2: AI-vs-AI soak on the real match (FMatchSimulation: ball, referee,
// score) with all four players swinging through the shot system's controller. Fixed
// seeds; no point may hang and every point must end with a GDD 3.3 reason.
//
// CI runs a short soak on every PR. The full 100 matches:
//   PADEL_SOAK_MATCHES=100 Tools/RunNativeTests.sh -tc="Ai.Soak*"
// PADEL_SOAK_DEBUG=1 prints every stuck point and whiff.
#include "doctest.h"
#include "AI/AiMatchRunner.h"
#include "ShotTestData.h"
#include "Support/AiTestData.h"
#include "TestSupport.h"

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

		void OnServeFault(Padel::EServeFault Reason) override { ++Stats->Faults[std::to_string(static_cast<int>(Reason))]; }
		void OnPointEnded(Padel::ETeam, Padel::EPointReason Reason, int) override
		{
			++Stats->Points;
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
			Runner.Start();

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
					return Stats;
				}
				Runner.Tick();
				if (Match.Phase() != EMatchPhase::Live && Match.Phase() != EMatchPhase::AwaitingServe)
				{
					LastHit = Match.Time();  // between points the clock is the runner's
				}
				for (int Id = 0; Id < Padel::NumPlayers; ++Id)
				{
					const Padel::Shots::FSwingEvents& E = Runner.LastEvents(Id);
					if (Runner.HitAccepted(Id))
					{
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
							std::printf("hit id=%d shot=%s off=%.2f z=%.2f speed=%.1f charge=%.2f\n", Id, Padel::ShotTypeName(E.Intent.ShotType), Off,
								E.Intent.Origin.Z, E.Intent.SpeedMps, E.Intent.Charge);
						}
					}
					else if (E.Outcome == Padel::Shots::ESwingOutcome::Whiff || E.Outcome == Padel::Shots::ESwingOutcome::ShadowSwing)
					{
						++Stats.Whiffs;
						if (std::getenv("PADEL_SOAK_DEBUG"))
						{
							const Padel::FBallState& Ball = Match.Ball();
							const Padel::FVec3& P = Runner.Player(Id).GetMotion().GetPosition();
							const Padel::ETeam T = Padel::TeamOfPlayer(Id);
							const auto& Plan = Runner.Team(T).GetPlan(Runner.SlotOf(Id));
							std::printf("whiff id=%d outcome=%d ball=(%.2f,%.2f,%.2f) player=(%.2f,%.2f) toIdeal=%.3f taker=%d plan=%d stand=(%.2f,%.2f) slack=%.2f\n", Id,
								static_cast<int>(E.Outcome), Ball.Position.X, Ball.Position.Y, Ball.Position.Z, P.X, P.Y,
								Runner.Player(Id).TimeToIdealS(), Runner.Team(T).GetTaker(), Plan.has_value(), Plan ? Plan->StandPoint.X : 0.0,
								Plan ? Plan->StandPoint.Y : 0.0, Plan ? Plan->Slack : 0.0);
						}
					}
				}
			}
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
			Total.Add(Game.Play(D, D, FirstSeed + static_cast<uint64_t>(Index)));
		}
		return Total;
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
		std::printf("\n[soak]   timing:");
		for (const auto& [Grade, Count] : S.Grades) std::printf(" %s=%d", Grade.c_str(), Count);
		std::printf("\n");
	}
}

TEST_CASE("Ai.Soak: Medium AI-vs-AI matches finish with no stuck points")
{
	const FSoakStats S = Soak("Medium", SoakMatches(4), 1000);
	Report("Medium", S);
	CHECK(S.StuckPoints == 0);
	CHECK(S.Unfinished == 0);
	CHECK(S.InvariantFailures == 0);
	CHECK(S.Points > 0);
	CHECK(S.ShotsPerPoint() > 2.0);
}

TEST_CASE("Ai.Soak: every difficulty plays without getting stuck")
{
	for (const char* Level : { "Easy", "Hard" })
	{
		const FSoakStats S = Soak(Level, 2, 2000);
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
}
