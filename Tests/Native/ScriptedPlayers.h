// A test-only "player" for FMatchSimulation: serves into the box and returns every ball
// it can reach at a comfortable height, at targets picked from its own seeded RNG. It
// stands in for gameplay and the AI so the native tests can play real rallies and whole
// matches through the real physics and rules.
#pragma once

#include "MatchSimulation.h"

#include <string>
#include <vector>

namespace Padel::Test
{
	struct FEventLog : IMatchEventListener
	{
		std::vector<std::string> Events;
		std::vector<EPointReason> Reasons;
		std::vector<EServeFault> Faults;
		std::vector<int> Hitters;
		int Points = 0;
		int Lets = 0;
		int GoldenPoints = 0;
		bool bMatchWon = false;
		FScoreSnapshot LastScore;

		void OnServeStarted(int) override { Events.push_back("serve_started"); }
		void OnServeFault(EServeFault R) override { Events.push_back("serve_fault"); Faults.push_back(R); }
		void OnServeLet() override { Events.push_back("serve_let"); ++Lets; }
		void OnBallHit(int Player, EShotType, ETimingGrade, double, const FVec3&) override { Events.push_back("ball_hit"); Hitters.push_back(Player); }
		void OnBallBounced(Court::ESurface S, Court::EHalf, const FVec3&) override { Events.push_back(std::string("ball_bounced:") + Court::SurfaceName(S)); }
		void OnBallLeftCourt(const FVec3&) override { Events.push_back("ball_left_court"); }
		void OnPointEnded(ETeam, EPointReason R, int) override { Events.push_back("point_ended"); Reasons.push_back(R); ++Points; }
		void OnScoreChanged(const FScoreSnapshot& S) override { Events.push_back("score_changed"); LastScore = S; }
		void OnGameWon(ETeam) override { Events.push_back("game_won"); }
		void OnSetWon(ETeam) override { Events.push_back("set_won"); }
		void OnMatchWon(ETeam) override { Events.push_back("match_won"); bMatchWon = true; }
		void OnChangeOfEnds() override { Events.push_back("change_of_ends"); }
		void OnGoldenPointStarted(ETeam) override { Events.push_back("golden_point_started"); ++GoldenPoints; }

		int Count(const std::string& Name) const
		{
			int N = 0;
			for (const std::string& E : Events)
			{
				N += E == Name ? 1 : 0;
			}
			return N;
		}
	};

	class FScriptedPlayers
	{
	public:
		explicit FScriptedPlayers(FMatchSimulation& InMatch, uint64_t Seed = 1) : Match(InMatch), Rng(Seed) {}

		// Shots after which the next shot is deliberately put into the net (0 = never).
		int ErrorAfterShots = 0;
		// Pick a new ErrorAfterShots from 1 to 6 for every point, so the error falls to
		// either team (a fixed one always hands the point to the server).
		bool bRandomErrors = false;
		// Prefer to let the ball come off the back glass before returning it.
		bool bWaitForGlass = false;
		double DriveSpeed = 19.0;
		int ShotsThisPoint = 0;

		// Call once per tick, before Match.Tick().
		void Act()
		{
			const EMatchPhase Phase = Match.Phase();
			if (Phase == EMatchPhase::AwaitingServe)
			{
				ShotsThisPoint = 0;
				if (bRandomErrors)
				{
					ErrorAfterShots = 1 + static_cast<int>(Rng.NextU64() % 6);
				}
				const FServeContact Contact = Match.DefaultServeContact();
				FVec3 Target = Match.ServiceBoxCentre();
				Target.X *= Rng.Range(0.7, 1.3);
				Match.PlayServe(Match.ServerId(), Contact, EShotType::ServeFlat, ETimingGrade::Good, Target, 13.0, { -30.0, 0.0, 0.0 });
				return;
			}
			if (Phase != EMatchPhase::Live)
			{
				return;
			}
			const int Team = Match.TeamToPlay();
			if (Team < 0 || Match.BouncesOnPlayHalf() < 1)
			{
				return;
			}
			const FBallState& Ball = Match.Ball();
			const int Half = Match.HalfSignOf(static_cast<ETeam>(Team));
			const bool bOnHalf = Ball.Position.X * Half > 0.5;
			const bool bReachable = Ball.Velocity.Z < 0.0 && Ball.Position.Z < 1.4 && Ball.Position.Z > 0.25;
			const bool bDeep = Ball.Position.X * Half > 8.6;
			if (!bOnHalf || !bReachable)
			{
				return;
			}
			if (bWaitForGlass && ShotsThisPoint > 0 && !Match.IsOffOwnGlass() && Ball.Velocity.X * Half > 0.0 && !bDeep)
			{
				return;
			}
			const int Player = Team * 2 + (Ball.Position.Y * -Half > 0.0 ? 0 : 1);
			FVec3 Target{ -Half * Rng.Range(5.5, 8.5), Rng.Range(-3.5, 3.5), 0.0 };
			double Speed = DriveSpeed * Rng.Range(0.85, 1.1);
			if (ErrorAfterShots > 0 && ShotsThisPoint >= ErrorAfterShots)
			{
				// A deliberate error: a fast flat ball at the bottom of the net.
				Target = { -Half * 0.8, 0.0, 0.0 };
				Speed = 25.0;
			}
			++ShotsThisPoint;
			Match.PlayShot(Player, EShotType::Drive, ETimingGrade::Good, Target, Speed, { 40.0, 0.0, 0.0 });
		}

	private:
		FMatchSimulation& Match;
		FPadelRng Rng;
	};
}
