// GDD 2 and 5.3: shot spin at real-world magnitudes, and what it does to the flight and
// the bounces. The spins are read from Data/Shots.json, so a retune is checked here.
#include "doctest.h"
#include "ShotTestData.h"
#include "TestSupport.h"

#include "BallSim.h"
#include "LaunchSolver.h"

#include <algorithm>
#include <cmath>

using namespace Padel;
using namespace Padel::Court;
using namespace Padel::Shots;
using doctest::Approx;

namespace
{
	const FShotLibrary& Library()
	{
		static const FShotLibrary Lib = ShotTestData::LoadLibrary();
		return Lib;
	}

	// The shot's spin for an average player (rating scale 1), in the shot's own frame.
	FVec3 ShotSpin(EShotType Type)
	{
		const FShotParams& P = Library().Get(Type);
		return { P.BaseSpinTop, P.BaseSpinSide, 0.0 };
	}

	FBallState Ball(FVec3 Position, FVec3 Velocity, FVec3 SpinLocal = {})
	{
		FBallState S;
		S.Position = Position;
		S.Velocity = Velocity;
		S.Spin = SpinLocalToWorld(SpinLocal, Velocity);
		return S;
	}

	struct FFlight
	{
		bool bHit = false;
		FVec3 Contact;
		// The state just after the contact.
		FBallState After;
		double MaxZ = 0.0;
	};

	// Flies until the first contact with Surface.
	FFlight FlyTo(const FBallSim& Sim, FBallState S, ESurface Surface, double MaxSeconds = 4.0)
	{
		FFlight F;
		for (double Time = 0.0; Time < MaxSeconds && !F.bHit; Time += SimTickSeconds)
		{
			std::vector<FBallContact> Contacts;
			Sim.Tick(S, nullptr, Contacts);
			F.MaxZ = std::max(F.MaxZ, S.Position.Z);
			for (const FBallContact& C : Contacts)
			{
				if (!F.bHit && C.Kind == EContactKind::Surface && C.Surface == Surface)
				{
					F.bHit = true;
					F.Contact = C.Position;
				}
			}
		}
		F.After = S;
		return F;
	}

	// The highest point of the hop that follows a ground bounce.
	double HopPeak(const FBallSim& Sim, FBallState S)
	{
		double Peak = S.Position.Z;
		for (double Time = 0.0; Time < 2.0 && S.Velocity.Z >= 0.0; Time += SimTickSeconds)
		{
			std::vector<FBallContact> Contacts;
			Sim.Tick(S, nullptr, Contacts);
			Peak = std::max(Peak, S.Position.Z);
		}
		return Peak;
	}

	// How much of its spin, about the same axis, a ball keeps through its first contact
	// with Surface: 1 is all of it, below 0 is turned the other way.
	double SpinKept(const FBallSim& Sim, const FBallState& S, ESurface Surface)
	{
		const FFlight F = FlyTo(Sim, S, Surface, 1.0);
		return F.bHit ? Dot(F.After.Spin, S.Spin) / Dot(S.Spin, S.Spin) : -2.0;
	}
}

TEST_CASE("Ball.Spin: the Magnus coefficient matches the standard lift model")
{
	// Lift coefficient C_L ≈ 0.6 × spin ratio (rω/v) for a ball this size, so the Magnus
	// acceleration is ½ρA·0.6·r·|ω||v| / m: about 0.0007 for our ball in air at 1.2 kg/m³.
	// The model is linear while rω/v stays under about 0.3, which the slice shots do.
	const double AirDensity = 1.2;
	const double Area = 3.14159265358979 * BallRadius * BallRadius;
	const double Expected = 0.5 * AirDensity * Area * 0.6 * BallRadius / BallMass;
	CHECK(Test::Physics().MagnusCoefficient == Approx(Expected).epsilon(0.15));
}

TEST_CASE("Ball.Spin: shot spins are in the real range and turn the right way")
{
	const FShotModel& Model = Library().Model;
	// Real padel strokes spin at about 600 to 2,500 rpm (60 to 260 rad/s), blocks and
	// touch shots less. Check the whole rating range, base × SpinRatingMin..Max (GDD 5.3).
	for (int Index = 0; Index < ShotTypeCount; ++Index)
	{
		const EShotType Type = static_cast<EShotType>(Index);
		if (Type == EShotType::Mishit)
		{
			continue;
		}
		CAPTURE(ShotTypeName(Type));
		const FVec3 Spin = ShotSpin(Type);
		const double Total = std::hypot(Spin.X, Spin.Y);
		CHECK(Total * Model.SpinRatingMin >= 25.0);
		CHECK(Total * Model.SpinRatingMax <= 270.0);
	}
	// The strokes whose spin is their point: heavy spin even for a weak player.
	for (EShotType Type : { EShotType::Drive, EShotType::Bandeja, EShotType::Vibora })
	{
		CAPTURE(ShotTypeName(Type));
		const FVec3 Spin = ShotSpin(Type);
		CHECK(std::hypot(Spin.X, Spin.Y) * Model.SpinRatingMin >= 60.0);
	}
	// GDD 4's spin column: topspin (+) or slice (-).
	CHECK(ShotSpin(EShotType::Drive).X > 0.0);
	CHECK(ShotSpin(EShotType::Smash).X >= 0.0);
	CHECK(ShotSpin(EShotType::ServeKick).X > 0.0);
	CHECK(ShotSpin(EShotType::Volley).X <= 0.0);
	CHECK(ShotSpin(EShotType::Bandeja).X < 0.0);
	CHECK(ShotSpin(EShotType::Vibora).X < 0.0);
	CHECK(ShotSpin(EShotType::Vibora).Y != 0.0);
	CHECK(ShotSpin(EShotType::Chiquita).X <= 0.0);
}

TEST_CASE("Ball.Spin: a topspin drive dips more than a flat one off the same launch")
{
	const FBallSim Sim(Test::Physics());
	// An 80 km/h drive from the back, 0.9 m up, rising at about 10 degrees.
	const FVec3 V{ -22.0, 0.0, 4.0 };
	const FFlight Flat = FlyTo(Sim, Ball({ 8.0, 0.0, 0.9 }, V), ESurface::Ground);
	const FFlight Top = FlyTo(Sim, Ball({ 8.0, 0.0, 0.9 }, V, ShotSpin(EShotType::Drive)), ESurface::Ground);
	REQUIRE(Flat.bHit);
	REQUIRE(Top.bHit);
	// Lands at least a metre shorter (more net margin for the same depth), and lower.
	CHECK(Top.Contact.X > Flat.Contact.X + 1.0);
	CHECK(Top.MaxZ < Flat.MaxZ - 0.05);
}

TEST_CASE("Ball.Spin: a slice bandeja comes in flatter and stays lower off the ground and the back glass")
{
	const FBallSim Sim(Test::Physics());
	// The same bandeja, played to the same deep target at the same pace: the slice lifts,
	// so it is launched lower, arrives flatter and stays low after the bounce (GDD 4).
	auto Play = [&](const FVec3& SpinLocal)
	{
		FLaunchRequest R;
		R.Origin = { 6.0, 1.0, 2.3 };
		R.Target = { -8.0, 1.0, 0.0 };
		R.Speed = 18.0;
		R.SpinLocal = SpinLocal;
		const FLaunchResult L = SolveLaunch(Sim, R);
		CHECK(L.bOk);
		FBallState S;
		S.Position = R.Origin;
		S.Velocity = L.Velocity;
		S.Spin = L.Spin;
		const FFlight Landing = FlyTo(Sim, S, ESurface::Ground);
		CHECK(Landing.bHit);
		return Landing.After;
	};
	const FBallState Flat = Play({});
	const FBallState Slice = Play(ShotSpin(EShotType::Bandeja));
	CHECK(HopPeak(Sim, Slice) < HopPeak(Sim, Flat) - 0.2);

	const FFlight FlatGlass = FlyTo(Sim, Flat, ESurface::Glass);
	const FFlight SliceGlass = FlyTo(Sim, Slice, ESurface::Glass);
	REQUIRE(FlatGlass.bHit);
	REQUIRE(SliceGlass.bHit);
	// Off the back glass the slice comes away lower: the bandeja's point.
	CHECK(SliceGlass.Contact.Z < FlatGlass.Contact.Z);
	CHECK(SliceGlass.After.Velocity.Z < FlatGlass.After.Velocity.Z - 0.5);
}

TEST_CASE("Ball.Spin: the víbora's side spin curves it toward the side glass")
{
	const FBallSim Sim(Test::Physics());
	const FVec3 Spin = ShotSpin(EShotType::Vibora);
	// An 86 km/h víbora from the net, aimed straight down the court. Positive side spin
	// curves left of the direction of travel (LaunchSolver.h); ShotResolver flips it for
	// a left-hander (GDD 5.3), so both curve the same way relative to the body.
	const FVec3 V{ -24.0, 0.0, -2.0 };
	const FFlight Right = FlyTo(Sim, Ball({ 6.0, 0.0, 2.3 }, V, Spin), ESurface::Ground);
	const FFlight Left = FlyTo(Sim, Ball({ 6.0, 0.0, 2.3 }, V, { Spin.X, -Spin.Y, 0.0 }), ESurface::Ground);
	REQUIRE(Right.bHit);
	REQUIRE(Left.bHit);
	// Travelling -X, the left of travel is -Y. A visible curve over a 12 m flight.
	CHECK(Right.Contact.Y < -0.2);
	CHECK(Left.Contact.Y == Approx(-Right.Contact.Y).epsilon(0.01));
}

TEST_CASE("Ball.Spin: the glass keeps more spin than the grass, and topspin climbs the back glass")
{
	const FBallSim Sim(Test::Physics());
	// The same impact on both: a 15 m/s slice ball at 30 degrees to the surface, sliding
	// along it the way the slice turns (forward on the grass, up the back glass; the
	// same spin vector, the geometry turned 90 degrees). GDD 2: the glass keeps its spin
	// turning the same way, the grass grips and turns it over. Friction takes a fixed
	// amount of spin per impact, so real-sized spins survive the glass where the old
	// 25 rad/s ones were turned over by it too.
	const FVec3 Slice = ShotSpin(EShotType::Bandeja);
	const double Grass = SpinKept(Sim, Ball({ 0.0, 0.0, 1.0 }, { 13.0, 0.0, -7.5 }, Slice), ESurface::Ground);
	const double Glass = SpinKept(Sim, Ball({ 9.0, 0.0, 1.0 }, { 7.5, 0.0, 13.0 }, Slice), ESurface::Glass);
	CHECK(Glass > 0.25);
	CHECK(Grass < 0.0);

	// A topspin smash climbs off the back glass more than a flat one: what lets a smash
	// come back over the net (GDD 3.3).
	const FVec3 V{ 18.0, 0.0, 2.0 };
	const FFlight Flat = FlyTo(Sim, Ball({ 8.5, 0.0, 1.0 }, V), ESurface::Glass, 1.0);
	const FFlight Top = FlyTo(Sim, Ball({ 8.5, 0.0, 1.0 }, V, ShotSpin(EShotType::Smash)), ESurface::Glass, 1.0);
	REQUIRE(Flat.bHit);
	REQUIRE(Top.bHit);
	CHECK(Top.After.Velocity.Z > Flat.After.Velocity.Z + 0.3);
}
