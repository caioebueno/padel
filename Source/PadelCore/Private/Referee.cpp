#include "Referee.h"

#include <cmath>

namespace Padel
{
	using namespace Court;

	namespace
	{
		// Unreal's axes are left-handed: facing +X, +Y is on your right. So a team on the
		// +X half (facing -X) has its right side at -Y, and the right side's Y sign for a
		// half is minus the half's X sign. Right box to right box is then diagonal.
		double SideYSign(int HalfSign, EServeSide Side)
		{
			return -static_cast<double>(HalfSign) * (Side == EServeSide::Right ? 1.0 : -1.0);
		}
	}

	void FReferee::BeginPoint(const FPointSetup& InSetup)
	{
		Setup = InSetup;
		CurrentPhase = ERefereePhase::AwaitingServe;
		ServeAttempt = 1;
		LastHitter = -1;
		bServeBounced = false;
		bLetPending = false;
		Reason = EPointReason::None;
	}

	void FReferee::StartFlight(int HitterId)
	{
		LastHitter = HitterId;
		bCrossed = false;
		bLegalBounce = false;
		Bounces = 0;
		bTouchedNet = false;
		WallsSinceBounce = 0;
		bOwnWall = false;
	}

	int FReferee::HitterHalf() const
	{
		return HalfSignOf(TeamOfPlayer(LastHitter));
	}

	int FReferee::TeamToPlay() const
	{
		if (!IsLive() || LastHitter < 0)
		{
			return -1;
		}
		return TeamIndex(OtherTeam(TeamOfPlayer(LastHitter)));
	}

	bool FReferee::IsLegalServeStance(const FVec3& Feet) const
	{
		const int Half = HalfSignOf(TeamOfPlayer(Setup.ServerId));
		const double Depth = Half * Feet.X;
		const double Across = SideYSign(Half, Setup.Side) * Feet.Y;
		// GDD 3.2: behind the service line and not touching it, between the centre line
		// and the side wall, inside the court.
		return Depth > ServiceLineX + LineWidth * 0.5 && Depth <= HalfLength
			&& Across > LineWidth * 0.5 && Across <= HalfWidth;
	}

	bool FReferee::InServiceBox(const FVec3& Position) const
	{
		const int ReceiverHalf = -HalfSignOf(TeamOfPlayer(Setup.ServerId));
		const double Depth = ReceiverHalf * Position.X;
		const double Across = SideYSign(ReceiverHalf, Setup.Side) * Position.Y;
		// Lines are in: the ball counts if it touches the line, half a line width either side.
		return Depth > 0.0 && Depth <= ServiceLineX + LineWidth * 0.5
			&& Across >= -LineWidth * 0.5 && Across <= HalfWidth;
	}

	void FReferee::OnServe(int ServerId, const FServeContact& Contact)
	{
		if (CurrentPhase != ERefereePhase::AwaitingServe || ServerId != Setup.ServerId)
		{
			return;
		}
		StartFlight(ServerId);
		CurrentPhase = ERefereePhase::ServeInFlight;
		bServeBounced = false;
		bLetPending = false;
		// GDD 3.2: bounce first, then underhand at or below the waist, from behind the line.
		if (!Contact.bBouncedFirst)
		{
			Fault(EServeFault::NoBounce);
		}
		else if (Contact.ContactPosition.Z > Contact.WaistHeight)
		{
			Fault(EServeFault::AboveWaist);
		}
		else if (!IsLegalServeStance(Contact.ServerFeet))
		{
			Fault(EServeFault::FootFault);
		}
	}

	void FReferee::OnServeMissed(int ServerId)
	{
		if (CurrentPhase != ERefereePhase::AwaitingServe || ServerId != Setup.ServerId)
		{
			return;
		}
		LastHitter = ServerId;
		CurrentPhase = ERefereePhase::ServeInFlight;
		Fault(EServeFault::MissedBall);
	}

	void FReferee::Fault(EServeFault FaultReason)
	{
		bServeBounced = false;
		bLetPending = false;
		if (ServeAttempt == 1)
		{
			ServeAttempt = 2;
			CurrentPhase = ERefereePhase::AwaitingServe;
			if (OnServeFault)
			{
				OnServeFault(FaultReason);
			}
			return;
		}
		if (OnServeFault)
		{
			OnServeFault(FaultReason);
		}
		// GDD 3.2: two faults, the point goes to the receivers.
		LastHitter = Setup.ServerId;
		EndPoint(TeamOfPlayer(Setup.ServerId), EPointReason::DoubleFault);
	}

	void FReferee::Let()
	{
		bServeBounced = false;
		bLetPending = false;
		CurrentPhase = ERefereePhase::AwaitingServe;
		if (OnServeLet)
		{
			OnServeLet();
		}
	}

	void FReferee::EndPoint(ETeam Loser, EPointReason PointReason)
	{
		Award(OtherTeam(Loser), PointReason);
	}

	void FReferee::Award(ETeam PointWinner, EPointReason PointReason)
	{
		if (CurrentPhase == ERefereePhase::Over || CurrentPhase == ERefereePhase::Idle)
		{
			return;
		}
		CurrentPhase = ERefereePhase::Over;
		bServeBounced = false;
		bLetPending = false;
		Winner = PointWinner;
		Reason = PointReason;
		if (OnPointEnded)
		{
			OnPointEnded(PointWinner, PointReason, LastHitter);
		}
	}

	void FReferee::OnHit(int PlayerId, const FVec3& BallPosition)
	{
		if (!IsLive() || !IsValidPlayer(PlayerId))
		{
			return;
		}
		const ETeam Team = TeamOfPlayer(PlayerId);
		const ETeam HitterTeam = TeamOfPlayer(LastHitter);
		if (CurrentPhase == ERefereePhase::ServeInFlight)
		{
			// GDD 3.2: the receiver must let the serve bounce. The server's side touching
			// its own serve again is a double hit.
			EndPoint(Team, Team == HitterTeam ? EPointReason::DoubleHit : EPointReason::VolleyedServe);
			return;
		}
		if (bLetPending)
		{
			// GDD 3.2: a net serve that lands in the box is replayed, whatever happens next
			// (unless it hits the mesh, which would have faulted before this hit).
			Let();
			return;
		}
		if (Team == HitterTeam)
		{
			// GDD 3.3: one touch per team.
			LastHitter = PlayerId;
			EndPoint(Team, EPointReason::DoubleHit);
			return;
		}
		// Hitting the ball before it has come over to your side is reaching over (GDD 3.3).
		if (!bCrossed || HalfOf(BallPosition.X) != static_cast<EHalf>(HalfSignOf(Team)))
		{
			EndPoint(Team, EPointReason::Crossed);
			return;
		}
		bServeBounced = false;
		StartFlight(PlayerId);
	}

	void FReferee::OnContact(const FBallContact& Contact)
	{
		if (CurrentPhase == ERefereePhase::ServeInFlight)
		{
			OnServeContact(Contact);
		}
		else if (CurrentPhase == ERefereePhase::Rally)
		{
			OnRallyContact(Contact);
		}
	}

	void FReferee::OnServeContact(const FBallContact& Contact)
	{
		const int ServerHalf = HitterHalf();
		const bool bOnServerHalf = static_cast<int>(Contact.Half) == ServerHalf;
		switch (Contact.Kind)
		{
		case EContactKind::CrossedNet:
			bCrossed = static_cast<int>(Contact.Half) != ServerHalf;
			return;
		case EContactKind::LeftCourt:
			Fault(EServeFault::Out);
			return;
		case EContactKind::Surface:
			break;
		}
		switch (Contact.Surface)
		{
		case ESurface::Net:
			bTouchedNet = true;
			return;
		case ESurface::Ground:
			if (!bCrossed || bOnServerHalf)
			{
				Fault(bTouchedNet ? EServeFault::IntoNet : EServeFault::WrongBox);
			}
			else if (!InServiceBox(Contact.Position))
			{
				Fault(EServeFault::WrongBox);
			}
			else
			{
				// A good bounce. The rally is on, but the first wall decides a mesh fault,
				// and a net touch is a let unless the mesh comes first (GDD 3.2).
				CurrentPhase = ERefereePhase::Rally;
				bLegalBounce = true;
				Bounces = 1;
				bServeBounced = true;
				bLetPending = bTouchedNet;
			}
			return;
		case ESurface::Glass:
		case ESurface::Mesh:
			Fault(EServeFault::WallBeforeBounce);
			return;
		case ESurface::Ceiling:
			Fault(EServeFault::Out);
			return;
		case ESurface::None:
			return;
		}
	}

	void FReferee::OnRallyContact(const FBallContact& Contact)
	{
		const ETeam HitterTeam = TeamOfPlayer(LastHitter);
		const int OwnHalf = HitterHalf();
		const bool bOnOwnHalf = static_cast<int>(Contact.Half) == OwnHalf;

		if (bServeBounced)
		{
			// GDD 3.2: after the serve's bounce, mesh first is a fault even on a let.
			const bool bWall = Contact.Kind == EContactKind::Surface
				&& (Contact.Surface == ESurface::Glass || Contact.Surface == ESurface::Mesh);
			if (bWall && Contact.Surface == ESurface::Mesh)
			{
				Fault(EServeFault::MeshAfterBounce);
				return;
			}
			const bool bDecides = bWall || Contact.Kind == EContactKind::LeftCourt
				|| Contact.Kind == EContactKind::CrossedNet
				|| (Contact.Kind == EContactKind::Surface && Contact.Surface != ESurface::Net);
			if (bDecides)
			{
				if (bLetPending)
				{
					Let();
					return;
				}
				bServeBounced = false;
			}
		}

		if (Contact.Kind == EContactKind::CrossedNet)
		{
			if (static_cast<int>(Contact.Half) != OwnHalf)
			{
				bCrossed = true;
			}
			else if (bLegalBounce)
			{
				// GDD 3.3: bounced in and came back over on its own. No reach-over in the slice.
				Award(HitterTeam, EPointReason::CameBackOver);
			}
			else
			{
				bCrossed = false;
			}
			return;
		}
		if (Contact.Kind == EContactKind::LeftCourt)
		{
			if (bLegalBounce)
			{
				// GDD 3.3: "by three" / "by four". Out-of-court play is post-slice, so the
				// point ends as soon as the ball is out.
				Award(HitterTeam, EPointReason::OutOfCourtWinner);
			}
			else
			{
				EndPoint(HitterTeam, bOwnWall && !bCrossed ? EPointReason::OwnSide : EPointReason::OutDirect);
			}
			return;
		}

		switch (Contact.Surface)
		{
		case ESurface::Net:
			if (!bLegalBounce)
			{
				bTouchedNet = true;
			}
			return;
		case ESurface::Ground:
			if (!Contact.bInCourt)
			{
				return;
			}
			if (!bCrossed || bOnOwnHalf)
			{
				// Only reachable before the legal bounce: afterwards a return to this half
				// has already ended the point as CameBackOver.
				EndPoint(HitterTeam, bTouchedNet && !bOwnWall ? EPointReason::IntoNet : EPointReason::OwnSide);
			}
			else if (!bLegalBounce)
			{
				bLegalBounce = true;
				Bounces = 1;
			}
			else
			{
				++Bounces;
				// GDD 3.3: the opponents let it bounce twice.
				Award(HitterTeam, EPointReason::DoubleBounce);
			}
			return;
		case ESurface::Glass:
		case ESurface::Mesh:
			if (!bCrossed)
			{
				// GDD 3.3: playing off your own walls is allowed (the contrapared).
				bOwnWall = true;
			}
			else if (!bLegalBounce)
			{
				EndPoint(HitterTeam, EPointReason::WallBeforeBounce);
			}
			else
			{
				++WallsSinceBounce;
			}
			return;
		case ESurface::Ceiling:
			if (bLegalBounce)
			{
				Award(HitterTeam, EPointReason::OutOfCourtWinner);
			}
			else
			{
				EndPoint(HitterTeam, EPointReason::OutDirect);
			}
			return;
		case ESurface::None:
			return;
		}
	}

	void FReferee::OnBodyHit(int PlayerId)
	{
		if (!IsLive() || !IsValidPlayer(PlayerId))
		{
			return;
		}
		const ETeam Team = TeamOfPlayer(PlayerId);
		if (CurrentPhase == ERefereePhase::ServeInFlight && Team == TeamOfPlayer(Setup.ServerId))
		{
			Fault(EServeFault::HitPartner);
			return;
		}
		// GDD 3.3: the team whose player the ball touched loses, in play or heading out.
		EndPoint(Team, EPointReason::BodyHit);
	}

	void FReferee::OnNetTouch(int PlayerId)
	{
		if (IsLive() && IsValidPlayer(PlayerId))
		{
			EndPoint(TeamOfPlayer(PlayerId), EPointReason::NetTouch);
		}
	}

	void FReferee::OnPlayerCrossed(int PlayerId)
	{
		if (IsLive() && IsValidPlayer(PlayerId))
		{
			EndPoint(TeamOfPlayer(PlayerId), EPointReason::Crossed);
		}
	}

	void FReferee::OnRacketThrown(int PlayerId)
	{
		if (IsLive() && IsValidPlayer(PlayerId))
		{
			EndPoint(TeamOfPlayer(PlayerId), EPointReason::RacketThrown);
		}
	}

	void FReferee::OnBallStuck()
	{
		if (!IsLive() || LastHitter < 0)
		{
			return;
		}
		if (CurrentPhase == ERefereePhase::ServeInFlight)
		{
			Fault(EServeFault::Out);
			return;
		}
		// GDD 3.3: the hitter wins if it bounced in first, otherwise loses.
		const ETeam HitterTeam = TeamOfPlayer(LastHitter);
		if (bLegalBounce)
		{
			Award(HitterTeam, EPointReason::StuckInFence);
		}
		else
		{
			EndPoint(HitterTeam, EPointReason::StuckInFence);
		}
	}
}
