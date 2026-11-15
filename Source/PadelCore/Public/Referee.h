// The referee (GDD 3.2, 3.3): follows the ball's contacts and the players' touches and
// decides every point. Driven only by events, so every rule can be tested with
// hand-written contacts as well as with real flights. It knows nothing about the score;
// FMatchSimulation tells it who serves, from which side, and which half each team defends.

#pragma once

#include "PadelCoreApi.h"

#include "BallContact.h"
#include "PadelTypes.h"

#include <functional>

namespace Padel
{
	// What the shot system knows at the moment of the serve (GDD 3.2).
	struct FServeContact
	{
		// Ball position at contact, metres.
		FVec3 ContactPosition;
		// The server's waist height, metres above the ground.
		double WaistHeight = 1.0;
		// The server bounced the ball on the ground before hitting it.
		bool bBouncedFirst = true;
		// Where the server stands (feet), metres.
		FVec3 ServerFeet;
	};

	struct FPointSetup
	{
		int ServerId = 0;
		EServeSide Side = EServeSide::Right;
		// +1 while Team A defends the +X half.
		int TeamAEnd = 1;
	};

	enum class ERefereePhase : int
	{
		// Between points, or before the match.
		Idle,
		// Waiting for the server to hit (first or second serve).
		AwaitingServe,
		// The serve is in the air and has not bounced yet.
		ServeInFlight,
		// The ball is in play.
		Rally,
		// The point is decided.
		Over,
	};

	class PADELCORE_API FReferee
	{
	public:
		std::function<void(EServeFault)> OnServeFault;
		std::function<void()> OnServeLet;
		std::function<void(ETeam Winner, EPointReason Reason, int LastHitterId)> OnPointEnded;

		void BeginPoint(const FPointSetup& Setup);

		// The serve was struck. Checks the bounce, waist height and stance first.
		void OnServe(int ServerId, const FServeContact& Contact);
		// GDD 3.2: missing the ball on the serve swing is a fault.
		void OnServeMissed(int ServerId);
		// A player hit the ball (not the serve).
		void OnHit(int PlayerId, const FVec3& BallPosition);
		void OnContact(const FBallContact& Contact);

		// GDD 3.3 player faults. They count only while the ball is in play.
		void OnBodyHit(int PlayerId);
		void OnNetTouch(int PlayerId);
		void OnPlayerCrossed(int PlayerId);
		void OnRacketThrown(int PlayerId);
		// GDD 3.3: the ball is stuck in the mesh or a gap (or stopped dead).
		void OnBallStuck();

		// --- Queries for gameplay and the AI ---
		ERefereePhase Phase() const { return CurrentPhase; }
		bool IsLive() const { return CurrentPhase == ERefereePhase::ServeInFlight || CurrentPhase == ERefereePhase::Rally; }
		bool IsServe() const { return CurrentPhase == ERefereePhase::AwaitingServe || CurrentPhase == ERefereePhase::ServeInFlight || bServeBounced; }
		int ServeNumber() const { return ServeAttempt; }
		int LastHitterId() const { return LastHitter; }
		// -1 before the first hit of a point.
		int LastHitterTeam() const { return LastHitter < 0 ? -1 : TeamIndex(TeamOfPlayer(LastHitter)); }
		// The team that must play the ball next, or -1 when nobody may (not live).
		int TeamToPlay() const;
		// Ground bounces on the half of the team to play since the last hit.
		int BouncesOnPlayHalf() const { return Bounces; }
		// The ball has touched a wall on the team to play's half since its legal bounce
		// (an off-the-glass return, GDD 4).
		bool IsOffOwnGlass() const { return WallsSinceBounce > 0; }
		int HalfSignOf(ETeam Team) const { return Team == ETeam::A ? Setup.TeamAEnd : -Setup.TeamAEnd; }

		ETeam LastWinner() const { return Winner; }
		EPointReason LastReason() const { return Reason; }

	private:
		void StartFlight(int HitterId);
		void Fault(EServeFault FaultReason);
		void Let();
		void EndPoint(ETeam Loser, EPointReason PointReason);
		void Award(ETeam PointWinner, EPointReason PointReason);
		void OnServeContact(const FBallContact& Contact);
		void OnRallyContact(const FBallContact& Contact);
		bool InServiceBox(const FVec3& Position) const;
		bool IsLegalServeStance(const FVec3& Feet) const;
		int HitterHalf() const;

		FPointSetup Setup;
		ERefereePhase CurrentPhase = ERefereePhase::Idle;
		int ServeAttempt = 1;
		int LastHitter = -1;
		bool bCrossed = false;
		bool bLegalBounce = false;
		int Bounces = 0;
		bool bTouchedNet = false;
		int WallsSinceBounce = 0;
		bool bOwnWall = false;
		// The serve has bounced in the box; the first wall after it decides mesh faults.
		bool bServeBounced = false;
		bool bLetPending = false;
		ETeam Winner = ETeam::A;
		EPointReason Reason = EPointReason::None;
	};
}
