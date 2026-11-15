// One match: the ball, the referee and the score, stepped at a fixed 120 Hz with the
// match's seeded RNG (CLAUDE.md). This is the object gameplay, the AI and the soak test
// drive; it reports GDD 3.4 events to its listeners.

#pragma once

#include "BallSim.h"
#include "LaunchSolver.h"
#include "MatchEventListener.h"
#include "MatchScore.h"
#include "Referee.h"

#include <vector>

namespace Padel
{
	enum class EMatchPhase : int
	{
		NotStarted,
		// Waiting for the server (first or second serve). The ball is held.
		AwaitingServe,
		// The ball is in play.
		Live,
		// The point is decided; call BeginPoint for the next one.
		PointOver,
		MatchOver,
	};

	class FMatchSimulation
	{
	public:
		FMatchSimulation(const FMatchConfig& Config, const FBallPhysicsParams& Physics, uint64_t Seed);

		FMatchSimulation(const FMatchSimulation&) = delete;
		FMatchSimulation& operator=(const FMatchSimulation&) = delete;

		void AddListener(IMatchEventListener* Listener);
		void RemoveListener(IMatchEventListener* Listener);

		// Sends the opening score. Then call BeginPoint.
		void StartMatch();
		// Sets up the next point and sends OnServeStarted.
		void BeginPoint();

		// --- The serve ---
		// The server struck the serve with this velocity and world spin.
		bool Serve(int ServerId, const FServeContact& Contact, EShotType ShotType, ETimingGrade Timing,
			const FVec3& Velocity, const FVec3& Spin);
		// Solves the launch to Target and serves it.
		FLaunchResult PlayServe(int ServerId, const FServeContact& Contact, EShotType ShotType, ETimingGrade Timing,
			const FVec3& Target, double Speed, const FVec3& SpinLocal);
		void ServeMissed(int ServerId);

		// --- Shots in the rally, from the ball's current position ---
		bool HitBall(int PlayerId, EShotType ShotType, ETimingGrade Timing, const FVec3& Velocity, const FVec3& Spin);
		// GDD 5: the shot system's target, speed and spin; the launch is solved here.
		FLaunchResult PlayShot(int PlayerId, EShotType ShotType, ETimingGrade Timing, const FVec3& Target,
			double Speed, const FVec3& SpinLocal, double ApexHeight = 0.0);

		// --- Player faults (GDD 3.3), reported by gameplay ---
		void ReportBodyHit(int PlayerId);
		void ReportNetTouch(int PlayerId);
		void ReportPlayerCrossed(int PlayerId);
		void ReportRacketThrown(int PlayerId);

		// GDD 3.1: the receiving pair picks who receives the golden point. Valid after
		// OnGoldenPointStarted and before the serve.
		bool ChooseGoldenPointReceiver(int PlayerId);

		// Advances one fixed 120 Hz tick.
		void Tick();

		// --- Queries ---
		EMatchPhase Phase() const { return CurrentPhase; }
		const FBallState& Ball() const { return BallState; }
		const FMatchScore& Score() const { return MatchScore; }
		FMatchScore& MutableScore() { return MatchScore; }
		const FReferee& Referee() const { return Ref; }
		const FBallSim& BallSim() const { return Sim; }
		FPadelRng& Rng() { return MatchRng; }
		double Time() const { return Ticks * SimTickSeconds; }
		long long TickCount() const { return Ticks; }

		bool IsLive() const { return CurrentPhase == EMatchPhase::Live; }
		bool IsServe() const { return Ref.IsServe(); }
		int TeamToPlay() const { return IsLive() ? Ref.TeamToPlay() : -1; }
		int BouncesOnPlayHalf() const { return Ref.BouncesOnPlayHalf(); }
		bool IsOffOwnGlass() const { return Ref.IsOffOwnGlass(); }
		int LastHitterTeam() const { return Ref.LastHitterTeam(); }
		int LastHitterId() const { return Ref.LastHitterId(); }
		int ServerId() const { return MatchScore.ServerId(); }
		// The X sign of the half a team defends now.
		int HalfSignOf(ETeam Team) const { return MatchScore.HalfSignOf(Team); }

		// Where the server stands and the ball is held before a serve: behind the service
		// line, halfway between the centre line and the side wall (a convenience for the
		// AI, tests and the ball machine; any legal stance works).
		FServeContact DefaultServeContact(double ContactHeight = 0.8, double WaistHeight = 1.0) const;
		// The middle of the receiver's service box, on the ground.
		FVec3 ServiceBoxCentre() const;

	private:
		template <typename FFunc>
		void Notify(FFunc&& Func)
		{
			// Copy: a listener may remove itself while handling an event.
			const std::vector<IMatchEventListener*> Copy = Listeners;
			for (IMatchEventListener* Listener : Copy)
			{
				Func(*Listener);
			}
		}

		void HandleServeFault(EServeFault Reason);
		void HandleServeLet();
		void HandlePointEnded(ETeam Winner, EPointReason Reason, int LastHitter);
		void Launch(int PlayerId, EShotType ShotType, ETimingGrade Timing, const FVec3& Velocity, const FVec3& Spin);
		void HoldBallForServe();

		FBallSim Sim;
		FMatchScore MatchScore;
		FReferee Ref;
		FPadelRng MatchRng;
		FBallState BallState;
		EMatchPhase CurrentPhase = EMatchPhase::NotStarted;
		std::vector<IMatchEventListener*> Listeners;
		std::vector<FBallContact> TickContacts;
		long long Ticks = 0;
		double SinceHit = 0.0;
		double SlowFor = 0.0;
	};
}
