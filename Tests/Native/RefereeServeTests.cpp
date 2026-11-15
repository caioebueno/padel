// GDD 3.2: serve rules, fed with hand-written contacts.
#include "doctest.h"

#include "Referee.h"

#include <optional>
#include <vector>

using namespace Padel;
using namespace Padel::Court;

namespace
{
	// Team A defends +X; player 0 serves from the right (-Y on the +X half) to Team B's
	// right box, which is on -X at +Y.
	struct FServeTest
	{
		FReferee Ref;
		std::vector<EServeFault> Faults;
		int Lets = 0;
		std::optional<ETeam> Winner;
		EPointReason Reason = EPointReason::None;

		explicit FServeTest(EServeSide Side = EServeSide::Right)
		{
			Ref.OnServeFault = [this](EServeFault F) { Faults.push_back(F); };
			Ref.OnServeLet = [this]() { ++Lets; };
			Ref.OnPointEnded = [this](ETeam W, EPointReason R, int) { Winner = W; Reason = R; };
			FPointSetup Setup;
			Setup.ServerId = 0;
			Setup.Side = Side;
			Ref.BeginPoint(Setup);
		}


		FServeContact Good(EServeSide Side = EServeSide::Right) const
		{
			const double Y = Side == EServeSide::Right ? -2.5 : 2.5;
			FServeContact C;
			C.ContactPosition = { 8.2, Y, 0.8 };
			C.ServerFeet = { 8.5, Y, 0.0 };
			C.WaistHeight = 1.0;
			C.bBouncedFirst = true;
			return C;
		}

		void Serve(const FServeContact& C) { Ref.OnServe(0, C); }
		void Serve() { Serve(Good()); }

		void Contact(EContactKind Kind, ESurface S, const FVec3& P, bool bInCourt = true)
		{
			FBallContact C;
			C.Kind = Kind;
			C.Surface = S;
			C.Position = P;
			C.Half = HalfOf(P.X);
			C.bInCourt = bInCourt;
			Ref.OnContact(C);
		}

		void Cross() { Contact(EContactKind::CrossedNet, ESurface::None, { -0.01, 0.0, 1.0 }); }
		void Ground(const FVec3& P) { Contact(EContactKind::Surface, ESurface::Ground, P); }
		void Net() { Contact(EContactKind::Surface, ESurface::Net, { 0.0, 0.0, 0.9 }); }
		void Wall(ESurface S, const FVec3& P) { Contact(EContactKind::Surface, S, P); }

		// In the right box: B's half (-X), +Y.
		void GoodBounce() { Cross(); Ground({ -4.0, 2.5, 0.0 }); }
	};
}

TEST_CASE("Rules.Serve: a good serve bounces in the diagonal box and stays in play")
{
	FServeTest T;
	T.Serve();
	CHECK(T.Ref.Phase() == ERefereePhase::ServeInFlight);
	T.GoodBounce();
	CHECK(T.Faults.empty());
	CHECK(T.Ref.Phase() == ERefereePhase::Rally);
	CHECK(T.Ref.TeamToPlay() == TeamIndex(ETeam::B));
}

TEST_CASE("Rules.Serve: after the bounce it may hit the glass")
{
	FServeTest T;
	T.Serve();
	T.GoodBounce();
	T.Wall(ESurface::Glass, { -10.0, 2.0, 1.0 });
	CHECK(T.Faults.empty());
	CHECK(T.Ref.IsLive());
	// An unreturned serve: second bounce, the server's team wins.
	T.Ground({ -8.5, 2.0, 0.0 });
	CHECK(T.Winner == ETeam::A);
	CHECK(T.Reason == EPointReason::DoubleBounce);
}

TEST_CASE("Rules.Serve: the box lines are in")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	// On the service line (6.95) and on the centre line.
	T.Ground({ -(ServiceLineX + 0.02), -0.02, 0.0 });
	CHECK(T.Faults.empty());
	CHECK(T.Ref.Phase() == ERefereePhase::Rally);
}

TEST_CASE("Rules.Serve: WRONG_BOX, straight down the line instead of diagonal")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	T.Ground({ -4.0, -2.5, 0.0 });
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::WrongBox);
	CHECK(T.Ref.Phase() == ERefereePhase::AwaitingServe);
	CHECK(T.Ref.ServeNumber() == 2);
}

TEST_CASE("Rules.Serve: WRONG_BOX, long past the service line")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	T.Ground({ -8.0, 2.5, 0.0 });
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::WrongBox);
}

TEST_CASE("Rules.Serve: from the left the box is the other diagonal")
{
	FServeTest T(EServeSide::Left);
	T.Serve(T.Good(EServeSide::Left));
	CHECK(T.Faults.empty());
	T.Cross();
	T.Ground({ -4.0, -2.5, 0.0 });
	CHECK(T.Faults.empty());
	CHECK(T.Ref.Phase() == ERefereePhase::Rally);
}

TEST_CASE("Rules.Serve: INTO_NET, the serve doesn't clear the net")
{
	FServeTest T;
	T.Serve();
	T.Net();
	T.Ground({ 1.0, -1.0, 0.0 });
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::IntoNet);
}

TEST_CASE("Rules.Serve: WALL_BEFORE_BOUNCE, the receivers' glass before the bounce")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	T.Wall(ESurface::Glass, { -10.0, 2.0, 1.0 });
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::WallBeforeBounce);
}

TEST_CASE("Rules.Serve: OUT, the serve leaves the court")
{
	FServeTest T;
	T.Serve();
	T.Contact(EContactKind::LeftCourt, ESurface::None, { -10.2, 0.0, 4.5 }, false);
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::Out);
}

TEST_CASE("Rules.Serve: MESH_AFTER_BOUNCE, the mesh first after the bounce")
{
	FServeTest T;
	T.Serve();
	T.GoodBounce();
	T.Wall(ESurface::Mesh, { -3.0, 5.0, 1.0 });
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::MeshAfterBounce);
}

TEST_CASE("Rules.Serve: glass then mesh after the bounce is fine")
{
	FServeTest T;
	T.Serve();
	T.GoodBounce();
	T.Wall(ESurface::Glass, { -9.0, 5.0, 1.0 });
	T.Wall(ESurface::Mesh, { -3.0, 5.0, 1.0 });
	CHECK(T.Faults.empty());
	CHECK(T.Ref.IsLive());
}

TEST_CASE("Rules.Serve: a let, net then the box, is replayed with the same serve")
{
	FServeTest T;
	T.Serve();
	T.Net();
	T.GoodBounce();
	CHECK(T.Lets == 0);
	T.Wall(ESurface::Glass, { -10.0, 2.0, 1.0 });
	CHECK(T.Lets == 1);
	CHECK(T.Faults.empty());
	CHECK(T.Ref.Phase() == ERefereePhase::AwaitingServe);
	CHECK(T.Ref.ServeNumber() == 1);
}

TEST_CASE("Rules.Serve: a let on the second serve keeps the second serve")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	T.Ground({ -8.0, 2.5, 0.0 });
	REQUIRE(T.Ref.ServeNumber() == 2);
	T.Serve();
	T.Net();
	T.GoodBounce();
	T.Ground({ -8.0, 2.5, 0.0 });
	CHECK(T.Lets == 1);
	CHECK(T.Ref.ServeNumber() == 2);
	CHECK_FALSE(T.Winner.has_value());
}

TEST_CASE("Rules.Serve: a let returned by the receiver is still a let")
{
	FServeTest T;
	T.Serve();
	T.Net();
	T.GoodBounce();
	T.Ref.OnHit(2, { -5.0, 2.5, 0.8 });
	CHECK(T.Lets == 1);
}

TEST_CASE("Rules.Serve: net, box, then the mesh is a fault, not a let")
{
	FServeTest T;
	T.Serve();
	T.Net();
	T.GoodBounce();
	T.Wall(ESurface::Mesh, { -3.0, 5.0, 1.0 });
	CHECK(T.Lets == 0);
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::MeshAfterBounce);
}

TEST_CASE("Rules.Serve: DOUBLE_FAULT, two faults give the point to the receivers")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	T.Ground({ -8.0, 2.5, 0.0 });
	T.Serve();
	T.Net();
	T.Ground({ 1.0, -1.0, 0.0 });
	CHECK(T.Faults.size() == 2);
	CHECK(T.Winner == ETeam::B);
	CHECK(T.Reason == EPointReason::DoubleFault);
	CHECK(T.Ref.Phase() == ERefereePhase::Over);
}

TEST_CASE("Rules.Serve: NO_BOUNCE, the server must bounce the ball first")
{
	FServeTest T;
	FServeContact C = T.Good();
	C.bBouncedFirst = false;
	T.Serve(C);
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::NoBounce);
}

TEST_CASE("Rules.Serve: ABOVE_WAIST, contact above the waist")
{
	FServeTest T;
	FServeContact C = T.Good();
	C.ContactPosition.Z = 1.3;
	T.Serve(C);
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::AboveWaist);
}

TEST_CASE("Rules.Serve: FOOT_FAULT, on or inside the service line, or the wrong side")
{
	{
		FServeTest T;
		FServeContact C = T.Good();
		C.ServerFeet.X = ServiceLineX;
		T.Serve(C);
		REQUIRE(T.Faults.size() == 1);
		CHECK(T.Faults[0] == EServeFault::FootFault);
	}
	{
		FServeTest T;
		FServeContact C = T.Good();
		C.ServerFeet.Y = 2.5;
		T.Serve(C);
		REQUIRE(T.Faults.size() == 1);
		CHECK(T.Faults[0] == EServeFault::FootFault);
	}
}

TEST_CASE("Rules.Serve: MISSED_BALL, missing the ball on the swing")
{
	FServeTest T;
	T.Ref.OnServeMissed(0);
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::MissedBall);
	CHECK(T.Ref.ServeNumber() == 2);
}

TEST_CASE("Rules.Serve: HIT_PARTNER, the serve hits the server's partner")
{
	FServeTest T;
	T.Serve();
	T.Ref.OnBodyHit(1);
	REQUIRE(T.Faults.size() == 1);
	CHECK(T.Faults[0] == EServeFault::HitPartner);
}

TEST_CASE("Rules.Serve: VOLLEYED_SERVE, the receiver takes it before the bounce")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	T.Ref.OnHit(2, { -5.0, 2.5, 1.2 });
	CHECK(T.Winner == ETeam::A);
	CHECK(T.Reason == EPointReason::VolleyedServe);
}

TEST_CASE("Rules.Serve: the serve hitting a receiver before the bounce loses them the point")
{
	FServeTest T;
	T.Serve();
	T.Cross();
	T.Ref.OnBodyHit(3);
	CHECK(T.Winner == ETeam::A);
	CHECK(T.Reason == EPointReason::BodyHit);
}

TEST_CASE("Rules.Serve: either receiver may return it after the bounce")
{
	FServeTest T;
	T.Serve();
	T.GoodBounce();
	T.Ref.OnHit(3, { -6.0, 2.0, 0.8 });
	CHECK_FALSE(T.Winner.has_value());
	CHECK(T.Ref.TeamToPlay() == TeamIndex(ETeam::A));
}

TEST_CASE("Rules.Serve: only the server can serve")
{
	FServeTest T;
	T.Ref.OnServe(1, T.Good());
	CHECK(T.Ref.Phase() == ERefereePhase::AwaitingServe);
	T.Ref.OnServeMissed(2);
	CHECK(T.Faults.empty());
}
