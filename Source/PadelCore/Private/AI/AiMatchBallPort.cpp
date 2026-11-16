#include "AI/AiMatchBallPort.h"

#include <algorithm>
#include <cmath>

namespace Padel::AI
{
namespace AiMatchBallPortDetail
{
	int HalfSignAt(double X)
	{
		return X >= 0.0 ? 1 : -1;
	}

	Shots::FBallView View(double T, const FBallState& Ball, int Bounces, bool bOffGlass, bool bLive)
	{
		Shots::FBallView V;
		V.T = T;
		V.Position = Ball.Position;
		V.Velocity = Ball.Velocity;
		V.GroundBounces = Bounces;
		V.bOffGlass = bOffGlass;
		V.bLive = bLive;
		return V;
	}

	bool SameVec(const FVec3& A, const FVec3& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}

	// Exact: the cached path came from the same code with the same inputs.
	bool SameBall(const FBallState& A, const FBallState& B)
	{
		return SameVec(A.Position, B.Position) && SameVec(A.Velocity, B.Velocity) && SameVec(A.Spin, B.Spin)
			&& A.bRolling == B.bRolling && A.bInCourt == B.bInCourt && A.bTouchingNet == B.bTouchingNet;
	}
}
using namespace AiMatchBallPortDetail;

FAiMatchBallPort::FTrack FAiMatchBallPort::MatchTrack() const
{
	FTrack Track;
	const FBallState& Ball = Match.Ball();
	Track.Half = HalfSignAt(Ball.Position.X);
	Track.bLive = Match.IsLive() && Ball.bInCourt;
	const int ToPlay = Match.TeamToPlay();
	// The referee counts bounces on the half of the team to play; on the hitter's own
	// half (just after the hit) there are none yet.
	if (ToPlay >= 0 && Match.HalfSignOf(static_cast<ETeam>(ToPlay)) == Track.Half)
	{
		Track.GroundBounces = Match.BouncesOnPlayHalf();
		Track.bOffGlass = Match.IsOffOwnGlass();
	}
	return Track;
}

void FAiMatchBallPort::Apply(FTrack& Track, const FBallContact& Contact)
{
	switch (Contact.Kind)
	{
	case Padel::EContactKind::CrossedNet:
		Track.Half = Contact.Half == Court::EHalf::A ? 1 : -1;
		Track.GroundBounces = 0;
		Track.bOffGlass = false;
		break;
	case Padel::EContactKind::LeftCourt:
		Track.bLive = false;
		break;
	case Padel::EContactKind::Surface:
		if (Contact.Surface == Court::ESurface::Ground)
		{
			// The roll report counts too: a rolling ball has bounced twice (GDD 3.3).
			++Track.GroundBounces;
		}
		else if (Contact.Surface == Court::ESurface::Glass && Track.GroundBounces >= 1)
		{
			Track.bOffGlass = true;
		}
		break;
	}
}

std::vector<Shots::FBallView> FAiMatchBallPort::PredictFrom(long long Now, const FBallState& Start, const FTrack& Track,
	double HorizonS, double StepS) const
{
	const int Ticks = std::max(0, static_cast<int>(std::lround(HorizonS / SimTickSeconds)));
	const int Every = std::max(1, static_cast<int>(std::lround(StepS / SimTickSeconds)));

	const long long Offset = Now - Cache.BaseTick;
	const long long Size = static_cast<long long>(Cache.States.size());
	const bool bHit = Cache.bValid && Cache.bToss == bTossing && Offset >= 0 && Offset < Size
		&& (Offset + Ticks < Size || Cache.bEndsDead)
		&& SameBall(Cache.States[static_cast<size_t>(Offset)], Start) && SameTrack(Cache.Tracks[static_cast<size_t>(Offset)], Track);
	if (!bHit)
	{
		// Simulate twice the horizon, so the next ticks reuse it.
		const int Length = 2 * Ticks + 1;
		Cache.bValid = true;
		Cache.bToss = bTossing;
		Cache.BaseTick = Now;
		Cache.States.assign(1, Start);
		Cache.Tracks.assign(1, Track);
		FBallState Ball = Start;
		FTrack Next = Track;
		std::vector<FBallContact> Contacts;
		for (int Tick = 1; Tick < Length && Next.bLive && Next.GroundBounces <= 1; ++Tick)
		{
			Contacts.clear();
			// No RNG: prediction never consumes the match's randomness.
			Match.BallSim().Tick(Ball, nullptr, Contacts);
			for (const FBallContact& C : Contacts)
			{
				Apply(Next, C);
			}
			Cache.States.push_back(Ball);
			Cache.Tracks.push_back(Next);
		}
		Cache.bEndsDead = !Next.bLive || Next.GroundBounces > 1;
	}

	std::vector<Shots::FBallView> Out;
	const size_t Base = static_cast<size_t>(Now - Cache.BaseTick);
	for (int Tick = 1; Tick <= Ticks && Base + Tick < Cache.States.size(); ++Tick)
	{
		const FTrack& T = Cache.Tracks[Base + Tick];
		if (Tick % Every == 0 || !T.bLive || T.GroundBounces > 1)
		{
			Out.push_back(View(Tick * SimTickSeconds, Cache.States[Base + Tick], T.GroundBounces, T.bOffGlass, T.bLive));
		}
		if (!T.bLive || T.GroundBounces > 1)
		{
			break;  // past the second bounce nobody can play it
		}
	}
	return Out;
}

Shots::FBallView FAiMatchBallPort::Current() const
{
	if (bTossing)
	{
		return View(0.0, Toss, TossBounces, false, true);
	}
	const FTrack Track = MatchTrack();
	return View(0.0, Match.Ball(), Track.GroundBounces, Track.bOffGlass, Track.bLive);
}

int FAiMatchBallPort::LastHitterTeam() const
{
	if (bTossing)
	{
		return 0;
	}
	const int Team = Match.LastHitterTeam();
	return Team < 0 ? 0 : Match.HalfSignOf(static_cast<ETeam>(Team));
}

std::vector<Shots::FBallView> FAiMatchBallPort::Predict(double HorizonS, double StepS) const
{
	if (bTossing)
	{
		FTrack Track;
		Track.Half = HalfSignAt(Toss.Position.X);
		Track.GroundBounces = TossBounces;
		return PredictFrom(TossTicks, Toss, Track, HorizonS, StepS);
	}
	if (!Match.IsLive())
	{
		return {};
	}
	return PredictFrom(Match.TickCount(), Match.Ball(), MatchTrack(), HorizonS, StepS);
}

void FAiMatchBallPort::ApplyShot(const Shots::FShotIntent& Intent)
{
	if (bTossing)
	{
		bTossing = false;
		bLastAccepted = Match.Phase() == EMatchPhase::AwaitingServe && Intent.PlayerId == Match.ServerId();
		FServeContact Contact;
		Contact.ContactPosition = Toss.Position;
		Contact.WaistHeight = ServerWaistM;
		Contact.bBouncedFirst = TossBounces >= 1;
		Contact.ServerFeet = ServerFeet;
		Match.PlayServe(Intent.PlayerId, Contact, Intent.ShotType, Intent.TimingGrade, Intent.Target, Intent.SpeedMps, Intent.Spin);
		return;
	}
	bLastAccepted = Match.IsLive();
	Match.PlayShot(Intent.PlayerId, Intent.ShotType, Intent.TimingGrade, Intent.Target, Intent.SpeedMps, Intent.Spin, Intent.ApexHeightM);
}

void FAiMatchBallPort::ServeToss(const FVec3& At, int TeamSign)
{
	(void)TeamSign;
	if (Match.Phase() != EMatchPhase::AwaitingServe)
	{
		return;
	}
	bTossing = true;
	Toss = FBallState();
	Toss.Position = At;
	TossBounces = 0;
	TossTicks = 0;
	Cache.bValid = false;
}

void FAiMatchBallPort::SetServerStance(const FVec3& Feet, double WaistHeightM)
{
	ServerFeet = Feet;
	ServerWaistM = WaistHeightM;
}

void FAiMatchBallPort::Tick()
{
	if (!bTossing)
	{
		return;
	}
	if (Match.Phase() != EMatchPhase::AwaitingServe)
	{
		bTossing = false;
		return;
	}
	std::vector<FBallContact> Contacts;
	Match.BallSim().Tick(Toss, nullptr, Contacts);
	++TossTicks;
	for (const FBallContact& C : Contacts)
	{
		if (C.Kind == Padel::EContactKind::Surface && C.Surface == Court::ESurface::Ground)
		{
			++TossBounces;
		}
	}
	if (TossBounces >= 2 || Toss.bRolling)
	{
		// GDD 3.2: the server let the ball go: a missed serve is a fault.
		bTossing = false;
		Match.ServeMissed(Match.ServerId());
	}
}
}
