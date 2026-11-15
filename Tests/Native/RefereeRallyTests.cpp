// GDD 3.3: rally rules. The referee is fed hand-written contacts, so each rule is
// checked exactly, independent of the ball physics.
#include "doctest.h"

#include "Referee.h"

#include <optional>

using namespace Padel;
using namespace Padel::Court;

namespace
{
	// Team A defends +X. Player 0 (A) has just hit from its half; Team B must play.
	struct FRally
	{
		FReferee Ref;
		std::optional<ETeam> Winner;
		EPointReason Reason = EPointReason::None;
		int LastHitter = -1;
		int Points = 0;

		explicit FRally(int Hitter = 0)
		{
			Ref.OnPointEnded = [this](ETeam W, EPointReason R, int H)
			{
				Winner = W;
				Reason = R;
				LastHitter = H;
				++Points;
			};
			FPointSetup Setup;
			Setup.ServerId = 0;
			Ref.BeginPoint(Setup);
			// A good serve from A's right box to B's right box, bounced, then returned
			// by B and played back by A, so the rally is under way with Hitter to move.
			FServeContact Serve;
			Serve.ContactPosition = { 8.0, -2.5, 0.8 };
			Serve.ServerFeet = { 8.0, -2.5, 0.0 };
			Ref.OnServe(0, Serve);
			Cross(EHalf::B);
			Ground({ -4.0, 2.5, 0.0 });
			Ref.OnHit(2, { -8.0, 2.5, 1.0 });
			Cross(EHalf::A);
			Ground({ 6.0, 0.0, 0.0 });
			if (Hitter == 0)
			{
				Ref.OnHit(0, { 8.0, 0.0, 1.0 });
			}
		}

		void Cross(EHalf To)
		{
			FBallContact C;
			C.Kind = EContactKind::CrossedNet;
			C.Half = To;
			C.Position = { 0.0, 0.0, 1.2 };
			Ref.OnContact(C);
		}

		void Surface(ESurface S, const FVec3& P, bool bInCourt = true)
		{
			FBallContact C;
			C.Kind = EContactKind::Surface;
			C.Surface = S;
			C.Position = P;
			C.Half = HalfOf(P.X);
			C.bInCourt = bInCourt;
			Ref.OnContact(C);
		}

		void Ground(const FVec3& P) { Surface(ESurface::Ground, P); }

		void Left(const FVec3& P)
		{
			FBallContact C;
			C.Kind = EContactKind::LeftCourt;
			C.Position = P;
			C.Half = HalfOf(P.X);
			C.bInCourt = false;
			Ref.OnContact(C);
		}
	};
}

TEST_CASE("Rules.Rally: the setup leaves the ball in play with Team B to play")
{
	FRally R;
	CHECK(R.Ref.IsLive());
	CHECK_FALSE(R.Winner.has_value());
	CHECK(R.Ref.TeamToPlay() == TeamIndex(ETeam::B));
	CHECK(R.Ref.LastHitterId() == 0);
}

TEST_CASE("Rules.Rally: DOUBLE_BOUNCE, two bounces on the receivers' ground")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -5.0, 1.0, 0.0 });
	CHECK_FALSE(R.Winner.has_value());
	CHECK(R.Ref.BouncesOnPlayHalf() == 1);
	R.Ground({ -8.0, 1.0, 0.0 });
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::DoubleBounce);
	CHECK(R.LastHitter == 0);
}

TEST_CASE("Rules.Rally: INTO_NET, the shot hits the net and drops on its own side")
{
	FRally R;
	R.Surface(ESurface::Net, { 0.03, 0.0, 0.5 });
	R.Ground({ 0.8, 0.0, 0.0 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::IntoNet);
}

TEST_CASE("Rules.Rally: a net cord that carries over is still in play")
{
	FRally R;
	R.Surface(ESurface::Net, { 0.0, 0.0, 0.9 });
	R.Cross(EHalf::B);
	R.Ground({ -0.6, 0.0, 0.0 });
	CHECK_FALSE(R.Winner.has_value());
}

TEST_CASE("Rules.Rally: WALL_BEFORE_BOUNCE, the opponents' glass on the fly")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Surface(ESurface::Glass, { -10.0, 0.0, 1.5 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::WallBeforeBounce);
}

TEST_CASE("Rules.Rally: WALL_BEFORE_BOUNCE, the opponents' mesh on the fly")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Surface(ESurface::Mesh, { -2.0, 5.0, 1.5 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::WallBeforeBounce);
}

TEST_CASE("Rules.Rally: OUT_DIRECT, out of the court without bouncing")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Left({ -10.1, 0.0, 4.5 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::OutDirect);
}

TEST_CASE("Rules.Rally: OUT_DIRECT, through the exit without bouncing")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Left({ -5.0, 5.1, 1.0 });
	CHECK(R.Reason == EPointReason::OutDirect);
	CHECK(R.Winner == ETeam::B);
}

TEST_CASE("Rules.Rally: OUT_DIRECT, the ceiling before the bounce")
{
	FRally R;
	R.Surface(ESurface::Ceiling, { 2.0, 0.0, 8.0 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::OutDirect);
}

TEST_CASE("Rules.Rally: OWN_SIDE, the shot bounces on its own ground")
{
	FRally R;
	R.Ground({ 3.0, 0.0, 0.0 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::OwnSide);
}

TEST_CASE("Rules.Rally: OWN_SIDE, off its own glass and it doesn't come back over")
{
	FRally R;
	R.Surface(ESurface::Glass, { 10.0, 0.0, 1.0 });
	CHECK_FALSE(R.Winner.has_value());
	R.Ground({ 7.0, 0.0, 0.0 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::OwnSide);
}

TEST_CASE("Rules.Rally: OWN_SIDE, off its own mesh and out of the court")
{
	FRally R;
	R.Surface(ESurface::Mesh, { 2.0, 5.0, 1.5 });
	R.Left({ 2.0, 5.2, 3.2 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::OwnSide);
}

TEST_CASE("Rules.Rally: the contrapared, off its own glass and over the net, is good")
{
	FRally R;
	R.Surface(ESurface::Glass, { 10.0, 0.0, 1.0 });
	R.Cross(EHalf::B);
	R.Ground({ -6.0, 0.0, 0.0 });
	CHECK_FALSE(R.Winner.has_value());
	CHECK(R.Ref.BouncesOnPlayHalf() == 1);
}

TEST_CASE("Rules.Rally: after the bounce the ball may hit any wall, any number of times")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -7.0, 0.0, 0.0 });
	R.Surface(ESurface::Glass, { -10.0, 0.0, 1.0 });
	R.Surface(ESurface::Glass, { -9.0, 5.0, 1.2 });
	R.Surface(ESurface::Mesh, { -3.0, -5.0, 1.0 });
	CHECK_FALSE(R.Winner.has_value());
	CHECK(R.Ref.IsOffOwnGlass());
	// Team B plays it off the glass.
	R.Ref.OnHit(3, { -6.0, 0.0, 1.0 });
	CHECK_FALSE(R.Winner.has_value());
	CHECK(R.Ref.TeamToPlay() == TeamIndex(ETeam::A));
	CHECK_FALSE(R.Ref.IsOffOwnGlass());
}

TEST_CASE("Rules.Rally: the receivers may volley before the bounce")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ref.OnHit(2, { -2.0, 0.0, 1.2 });
	CHECK_FALSE(R.Winner.has_value());
	CHECK(R.Ref.LastHitterId() == 2);
}

TEST_CASE("Rules.Rally: BODY_HIT, the ball hits a receiver before bouncing")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ref.OnBodyHit(3);
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::BodyHit);
}

TEST_CASE("Rules.Rally: BODY_HIT, even a shot heading out counts against the body's team")
{
	FRally R;
	// Flying long, it would have been OUT_DIRECT, but it hits B's player first.
	R.Cross(EHalf::B);
	R.Ref.OnBodyHit(2);
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::BodyHit);
}

TEST_CASE("Rules.Rally: BODY_HIT, the hitter's own partner")
{
	FRally R;
	R.Ref.OnBodyHit(1);
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::BodyHit);
}

TEST_CASE("Rules.Rally: DOUBLE_HIT, the same player twice")
{
	FRally R;
	R.Ref.OnHit(0, { 7.0, 0.0, 1.0 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::DoubleHit);
}

TEST_CASE("Rules.Rally: DOUBLE_HIT, both partners touch it")
{
	FRally R;
	R.Ref.OnHit(1, { 6.0, 0.0, 1.0 });
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::DoubleHit);
	CHECK(R.LastHitter == 1);
}

TEST_CASE("Rules.Rally: NET_TOUCH, a player touches the net while the ball is in play")
{
	FRally R;
	R.Ref.OnNetTouch(2);
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::NetTouch);
}

TEST_CASE("Rules.Rally: CROSSED, a player crosses the net plane")
{
	FRally R;
	R.Ref.OnPlayerCrossed(1);
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::Crossed);
}

TEST_CASE("Rules.Rally: CROSSED, reaching over to hit before the ball comes across")
{
	FRally R;
	R.Ref.OnHit(2, { 0.3, 0.0, 1.2 });
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::Crossed);
}

TEST_CASE("Rules.Rally: RACKET_THROWN")
{
	FRally R;
	R.Ref.OnRacketThrown(3);
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::RacketThrown);
}

TEST_CASE("Rules.Rally: player faults don't count when the ball is dead")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -5.0, 0.0, 0.0 });
	R.Ground({ -8.0, 0.0, 0.0 });
	REQUIRE(R.Points == 1);
	R.Ref.OnNetTouch(0);
	R.Ref.OnBodyHit(0);
	CHECK(R.Points == 1);
	CHECK(R.Reason == EPointReason::DoubleBounce);
}

TEST_CASE("Rules.Rally: OUT_OF_COURT_WINNER, over the back wall after the bounce (by three)")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -6.0, 0.0, 0.0 });
	R.Left({ -10.1, 0.0, 4.2 });
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::OutOfCourtWinner);
}

TEST_CASE("Rules.Rally: OUT_OF_COURT_WINNER, through the exit after the bounce (by four)")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -3.0, 3.0, 0.0 });
	R.Left({ -5.0, 5.1, 1.5 });
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::OutOfCourtWinner);
}

TEST_CASE("Rules.Rally: OUT_OF_COURT_WINNER, the ceiling after the bounce")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -6.0, 0.0, 0.0 });
	R.Surface(ESurface::Ceiling, { -8.0, 0.0, 8.0 });
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::OutOfCourtWinner);
}

TEST_CASE("Rules.Rally: the ball comes back over on its own after the bounce, hitter wins")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -1.0, 0.0, 0.0 });
	R.Cross(EHalf::A);
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::CameBackOver);
}

TEST_CASE("Rules.Rally: the ball stuck after bouncing in is won by the hitter")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Ground({ -6.0, 0.0, 0.0 });
	R.Ref.OnBallStuck();
	CHECK(R.Winner == ETeam::A);
	CHECK(R.Reason == EPointReason::StuckInFence);
}

TEST_CASE("Rules.Rally: the ball stuck before bouncing in is lost by the hitter")
{
	FRally R;
	R.Surface(ESurface::Mesh, { 3.0, 5.0, 2.0 });
	R.Ref.OnBallStuck();
	CHECK(R.Winner == ETeam::B);
	CHECK(R.Reason == EPointReason::StuckInFence);
}

TEST_CASE("Rules.Rally: a point ends exactly once")
{
	FRally R;
	R.Cross(EHalf::B);
	R.Surface(ESurface::Glass, { -10.0, 0.0, 1.5 });
	R.Ground({ -8.0, 0.0, 0.0 });
	R.Ground({ -7.0, 0.0, 0.0 });
	R.Ref.OnBodyHit(2);
	CHECK(R.Points == 1);
	CHECK(R.Reason == EPointReason::WallBeforeBounce);
	CHECK_FALSE(R.Ref.IsLive());
}

TEST_CASE("Rules.Rally: halves follow the change of ends")
{
	// After an odd number of changes Team A defends -X and Team B defends +X.
	FReferee Ref;
	std::optional<ETeam> Winner;
	EPointReason Reason = EPointReason::None;
	Ref.OnPointEnded = [&](ETeam W, EPointReason Rs, int) { Winner = W; Reason = Rs; };
	FPointSetup Setup;
	Setup.ServerId = 2;
	Setup.TeamAEnd = -1;
	Ref.BeginPoint(Setup);
	// Facing the net from +X, the right side is -Y (Unreal's axes).
	FServeContact Serve;
	Serve.ContactPosition = { 8.0, -2.5, 0.8 };
	Serve.ServerFeet = { 8.0, -2.5, 0.0 };
	Ref.OnServe(2, Serve);
	REQUIRE(Ref.IsLive());
	FBallContact Cross;
	Cross.Kind = EContactKind::CrossedNet;
	Cross.Half = EHalf::B;
	Ref.OnContact(Cross);
	// Diagonal: Team A's right box on -X is at +Y.
	FBallContact Bounce;
	Bounce.Surface = ESurface::Ground;
	Bounce.Position = { -4.0, 2.0, 0.0 };
	Bounce.Half = EHalf::B;
	Ref.OnContact(Bounce);
	CHECK_FALSE(Winner.has_value());
	Bounce.Position = { -8.0, 2.0, 0.0 };
	Ref.OnContact(Bounce);
	CHECK(Winner == ETeam::B);
	CHECK(Reason == EPointReason::DoubleBounce);
}
