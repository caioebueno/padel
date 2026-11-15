#include "MatchSimulation.h"

#include <algorithm>

namespace Padel
{
	using namespace Court;

	FMatchSimulation::FMatchSimulation(const FMatchConfig& Config, const FBallPhysicsParams& Physics, uint64_t Seed)
		: Sim(Physics)
		, MatchScore(Config)
		, MatchRng(Seed)
	{
		Ref.OnServeFault = [this](EServeFault Reason) { HandleServeFault(Reason); };
		Ref.OnServeLet = [this]() { HandleServeLet(); };
		Ref.OnPointEnded = [this](ETeam Winner, EPointReason Reason, int LastHitter) { HandlePointEnded(Winner, Reason, LastHitter); };
	}

	void FMatchSimulation::AddListener(IMatchEventListener* Listener)
	{
		if (Listener && std::find(Listeners.begin(), Listeners.end(), Listener) == Listeners.end())
		{
			Listeners.push_back(Listener);
		}
	}

	void FMatchSimulation::RemoveListener(IMatchEventListener* Listener)
	{
		Listeners.erase(std::remove(Listeners.begin(), Listeners.end(), Listener), Listeners.end());
	}

	void FMatchSimulation::StartMatch()
	{
		CurrentPhase = EMatchPhase::PointOver;
		const FScoreSnapshot Snapshot = MatchScore.Snapshot();
		Notify([&](IMatchEventListener& L) { L.OnScoreChanged(Snapshot); });
	}

	void FMatchSimulation::BeginPoint()
	{
		if (CurrentPhase != EMatchPhase::PointOver && CurrentPhase != EMatchPhase::NotStarted)
		{
			return;
		}
		FPointSetup Setup;
		Setup.ServerId = MatchScore.ServerId();
		Setup.Side = MatchScore.ServeSide();
		Setup.TeamAEnd = MatchScore.TeamAEnd();
		Ref.BeginPoint(Setup);
		CurrentPhase = EMatchPhase::AwaitingServe;
		HoldBallForServe();
		const int Server = Setup.ServerId;
		Notify([&](IMatchEventListener& L) { L.OnServeStarted(Server); });
	}

	FServeContact FMatchSimulation::DefaultServeContact(double ContactHeight, double WaistHeight) const
	{
		const int Server = MatchScore.ServerId();
		const int Half = MatchScore.HalfSignOf(TeamOfPlayer(Server));
		// Unreal's axes: facing the net from the +X half, the right side is -Y (see Referee.cpp).
		const double YSign = -Half * (MatchScore.ServeSide() == EServeSide::Right ? 1.0 : -1.0);
		FServeContact Contact;
		Contact.ServerFeet = { Half * (ServiceLineX + 1.5), YSign * HalfWidth * 0.5, 0.0 };
		Contact.ContactPosition = { Half * (ServiceLineX + 1.2), YSign * HalfWidth * 0.5, ContactHeight };
		Contact.WaistHeight = WaistHeight;
		Contact.bBouncedFirst = true;
		return Contact;
	}

	FVec3 FMatchSimulation::ServiceBoxCentre() const
	{
		const int Server = MatchScore.ServerId();
		const int ReceiverHalf = -MatchScore.HalfSignOf(TeamOfPlayer(Server));
		const double YSign = -ReceiverHalf * (MatchScore.ServeSide() == EServeSide::Right ? 1.0 : -1.0);
		return { ReceiverHalf * ServiceLineX * 0.5, YSign * HalfWidth * 0.5, 0.0 };
	}

	void FMatchSimulation::HoldBallForServe()
	{
		const FServeContact Contact = DefaultServeContact();
		BallState = FBallState();
		BallState.Position = Contact.ContactPosition;
	}

	void FMatchSimulation::Launch(int PlayerId, EShotType ShotType, ETimingGrade Timing, const FVec3& Velocity, const FVec3& Spin)
	{
		BallState.Velocity = Velocity;
		BallState.Spin = Spin;
		BallState.bRolling = false;
		BallState.bTouchingNet = false;
		SinceHit = 0.0;
		SlowFor = 0.0;
		const double Speed = Size(Velocity);
		Notify([&](IMatchEventListener& L) { L.OnBallHit(PlayerId, ShotType, Timing, Speed, Spin); });
	}

	bool FMatchSimulation::Serve(int ServerId, const FServeContact& Contact, EShotType ShotType, ETimingGrade Timing,
		const FVec3& Velocity, const FVec3& Spin)
	{
		if (CurrentPhase != EMatchPhase::AwaitingServe || ServerId != MatchScore.ServerId())
		{
			return false;
		}
		BallState = FBallState();
		BallState.Position = Contact.ContactPosition;
		CurrentPhase = EMatchPhase::Live;
		Launch(ServerId, ShotType, Timing, Velocity, Spin);
		// May fault at once (bounce, waist, stance), which puts us back to AwaitingServe.
		Ref.OnServe(ServerId, Contact);
		return true;
	}

	FLaunchResult FMatchSimulation::PlayServe(int ServerId, const FServeContact& Contact, EShotType ShotType, ETimingGrade Timing,
		const FVec3& Target, double Speed, const FVec3& SpinLocal)
	{
		FLaunchRequest Request;
		Request.Origin = Contact.ContactPosition;
		Request.Target = Target;
		Request.Speed = Speed;
		Request.SpinLocal = SpinLocal;
		const FLaunchResult Result = SolveLaunch(Sim, Request);
		Serve(ServerId, Contact, ShotType, Timing, Result.Velocity, Result.Spin);
		return Result;
	}

	void FMatchSimulation::ServeMissed(int ServerId)
	{
		if (CurrentPhase == EMatchPhase::AwaitingServe)
		{
			Ref.OnServeMissed(ServerId);
		}
	}

	bool FMatchSimulation::HitBall(int PlayerId, EShotType ShotType, ETimingGrade Timing, const FVec3& Velocity, const FVec3& Spin)
	{
		if (CurrentPhase != EMatchPhase::Live || !IsValidPlayer(PlayerId))
		{
			return false;
		}
		Launch(PlayerId, ShotType, Timing, Velocity, Spin);
		Ref.OnHit(PlayerId, BallState.Position);
		return true;
	}

	FLaunchResult FMatchSimulation::PlayShot(int PlayerId, EShotType ShotType, ETimingGrade Timing, const FVec3& Target,
		double Speed, const FVec3& SpinLocal, double ApexHeight)
	{
		FLaunchRequest Request;
		Request.Origin = BallState.Position;
		Request.Target = Target;
		Request.Speed = Speed;
		Request.SpinLocal = SpinLocal;
		Request.ApexHeight = ApexHeight;
		const FLaunchResult Result = SolveLaunch(Sim, Request);
		HitBall(PlayerId, ShotType, Timing, Result.Velocity, Result.Spin);
		return Result;
	}

	void FMatchSimulation::ReportBodyHit(int PlayerId) { Ref.OnBodyHit(PlayerId); }
	void FMatchSimulation::ReportNetTouch(int PlayerId) { Ref.OnNetTouch(PlayerId); }
	void FMatchSimulation::ReportPlayerCrossed(int PlayerId) { Ref.OnPlayerCrossed(PlayerId); }
	void FMatchSimulation::ReportRacketThrown(int PlayerId) { Ref.OnRacketThrown(PlayerId); }

	bool FMatchSimulation::ChooseGoldenPointReceiver(int PlayerId)
	{
		if (CurrentPhase != EMatchPhase::PointOver && !(CurrentPhase == EMatchPhase::AwaitingServe && Ref.ServeNumber() == 1))
		{
			return false;
		}
		if (!MatchScore.ChooseGoldenPointReceiver(PlayerId))
		{
			return false;
		}
		if (CurrentPhase == EMatchPhase::AwaitingServe)
		{
			// Already set up: move the serve to the chosen side.
			CurrentPhase = EMatchPhase::PointOver;
			BeginPoint();
		}
		return true;
	}

	void FMatchSimulation::Tick()
	{
		++Ticks;
		if (CurrentPhase != EMatchPhase::Live && CurrentPhase != EMatchPhase::PointOver && CurrentPhase != EMatchPhase::MatchOver)
		{
			return;
		}
		// After the point the ball keeps flying for the presentation, but only live
		// contacts reach the referee.
		TickContacts.clear();
		Sim.Tick(BallState, &MatchRng, TickContacts);
		for (const FBallContact& Contact : TickContacts)
		{
			if (Contact.Kind == EContactKind::Surface && !Contact.bRolling)
			{
				Notify([&](IMatchEventListener& L) { L.OnBallBounced(Contact.Surface, Contact.Half, Contact.Position); });
			}
			else if (Contact.Kind == EContactKind::LeftCourt)
			{
				Notify([&](IMatchEventListener& L) { L.OnBallLeftCourt(Contact.Position); });
			}
			if (CurrentPhase == EMatchPhase::Live)
			{
				Ref.OnContact(Contact);
			}
		}
		if (CurrentPhase != EMatchPhase::Live)
		{
			return;
		}
		// Watchdog (GDD 3.3 "stuck"; GDD 12: zero stuck points in the soak test).
		const FBallPhysicsParams& P = Sim.GetParams();
		SinceHit += SimTickSeconds;
		SlowFor = Size(BallState.Velocity) < P.StuckSpeed ? SlowFor + SimTickSeconds : 0.0;
		if (SlowFor >= P.StuckSeconds || SinceHit >= P.MaxFlightSeconds)
		{
			Ref.OnBallStuck();
		}
	}

	void FMatchSimulation::HandleServeFault(EServeFault Reason)
	{
		Notify([&](IMatchEventListener& L) { L.OnServeFault(Reason); });
		if (Ref.Phase() == ERefereePhase::AwaitingServe)
		{
			// Second serve.
			CurrentPhase = EMatchPhase::AwaitingServe;
			HoldBallForServe();
			const int Server = MatchScore.ServerId();
			Notify([&](IMatchEventListener& L) { L.OnServeStarted(Server); });
		}
	}

	void FMatchSimulation::HandleServeLet()
	{
		Notify([&](IMatchEventListener& L) { L.OnServeLet(); });
		CurrentPhase = EMatchPhase::AwaitingServe;
		HoldBallForServe();
		const int Server = MatchScore.ServerId();
		Notify([&](IMatchEventListener& L) { L.OnServeStarted(Server); });
	}

	void FMatchSimulation::HandlePointEnded(ETeam Winner, EPointReason Reason, int LastHitter)
	{
		Notify([&](IMatchEventListener& L) { L.OnPointEnded(Winner, Reason, LastHitter); });
		const FPointOutcome Outcome = MatchScore.AwardPoint(Winner);
		CurrentPhase = Outcome.bMatchWon ? EMatchPhase::MatchOver : EMatchPhase::PointOver;
		// GDD 3.4 order: game, set, match, ends, score, then the golden point prompt.
		if (Outcome.bGameWon)
		{
			Notify([&](IMatchEventListener& L) { L.OnGameWon(Outcome.Team); });
		}
		if (Outcome.bSetWon)
		{
			Notify([&](IMatchEventListener& L) { L.OnSetWon(Outcome.Team); });
		}
		if (Outcome.bMatchWon)
		{
			Notify([&](IMatchEventListener& L) { L.OnMatchWon(Outcome.Team); });
		}
		if (Outcome.bChangeOfEnds)
		{
			Notify([](IMatchEventListener& L) { L.OnChangeOfEnds(); });
		}
		const FScoreSnapshot Snapshot = MatchScore.Snapshot();
		Notify([&](IMatchEventListener& L) { L.OnScoreChanged(Snapshot); });
		if (Outcome.bGoldenPointNext)
		{
			const ETeam Receiving = MatchScore.ReceivingTeam();
			Notify([&](IMatchEventListener& L) { L.OnGoldenPointStarted(Receiving); });
		}
	}
}
