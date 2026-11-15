// GDD 5: the shot system gives target, speed and spin; the engine solves the launch.
#include "doctest.h"
#include "TestSupport.h"

#include "LaunchSolver.h"

#include <cmath>

using namespace Padel;
using namespace Padel::Court;
using doctest::Approx;

namespace
{
	FLaunchResult Solve(FVec3 Origin, FVec3 Target, double Speed, FVec3 Spin = {}, double Apex = 0.0)
	{
		const FBallSim Sim(Test::Physics());
		FLaunchRequest R;
		R.Origin = Origin;
		R.Target = Target;
		R.Speed = Speed;
		R.SpinLocal = Spin;
		R.ApexHeight = Apex;
		return SolveLaunch(Sim, R);
	}
}

TEST_CASE("Ball.Solver: drives land on target across the court")
{
	struct FCase { FVec3 Origin; FVec3 Target; double Speed; FVec3 Spin; };
	const FCase Cases[] = {
		{ { 8.0, 0.0, 1.0 }, { -8.0, 0.0, 0.0 }, 22.0, { 0.0, 0.0, 0.0 } },
		{ { 8.0, -3.0, 0.9 }, { -7.0, 3.5, 0.0 }, 20.0, { 150.0, 0.0, 0.0 } },
		{ { 7.0, 3.0, 1.2 }, { -5.0, -4.0, 0.0 }, 18.0, { -120.0, 0.0, 0.0 } },
		{ { -9.0, 2.0, 0.6 }, { 8.5, -2.0, 0.0 }, 24.0, { 100.0, 40.0, 0.0 } },
		{ { 3.0, 0.0, 2.6 }, { -9.0, 3.0, 0.0 }, 30.0, { 0.0, 0.0, 0.0 } },
	};
	for (const FCase& C : Cases)
	{
		const FLaunchResult R = Solve(C.Origin, C.Target, C.Speed, C.Spin);
		CHECK(R.bOk);
		CHECK(R.Miss < 0.05);
		CHECK(Size(R.Velocity) == Approx(C.Speed).epsilon(1e-6));
		CHECK(R.bClearsNet);
	}
}

TEST_CASE("Ball.Solver: the flat solve takes the lower arc")
{
	const FLaunchResult R = Solve({ 8.0, 0.0, 1.0 }, { -8.0, 0.0, 0.0 }, 22.0);
	REQUIRE(R.bOk);
	CHECK(R.Apex < 3.0);
	CHECK(R.FlightTime < 1.2);
}

TEST_CASE("Ball.Solver: the apex solve reaches the lob's height and lands on target")
{
	const FLaunchResult R = Solve({ -8.5, 1.0, 0.8 }, { 8.0, -2.0, 0.0 }, 0.0, { 80.0, 0.0, 0.0 }, 7.0);
	CHECK(R.bOk);
	CHECK(R.Apex == Approx(7.0).epsilon(0.03));
	CHECK(R.Miss < 0.05);
}

TEST_CASE("Ball.Solver: side spin is aimed off so the curve still lands on target")
{
	const FLaunchResult R = Solve({ 8.0, 0.0, 1.0 }, { -8.0, 0.0, 0.0 }, 20.0, { 0.0, 200.0, 0.0 });
	CHECK(R.bOk);
	// The curve is compensated by aiming off the straight line.
	CHECK(std::abs(R.Velocity.Y) > 0.05);
}

TEST_CASE("Ball.Solver: an out-of-range target at a low speed is reported, not faked")
{
	const FLaunchResult R = Solve({ 9.0, 0.0, 1.0 }, { -9.0, 0.0, 0.0 }, 6.0);
	CHECK_FALSE(R.bOk);
	CHECK(R.Miss > 1.0);
}

TEST_CASE("Ball.Solver: a flat shot too low for the net is flagged")
{
	const FLaunchResult R = Solve({ 1.5, 0.0, 0.2 }, { -1.0, 0.0, 0.0 }, 20.0);
	CHECK_FALSE(R.bClearsNet);
}

TEST_CASE("Ball.Solver: local spin maps to world axes")
{
	// Travelling +X with topspin: the axis is +Y (bottom of the ball moves backwards).
	const FVec3 Top = SpinLocalToWorld({ 100.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 });
	CHECK(Top.Y == Approx(100.0));
	CHECK(Top.X == Approx(0.0));
	const FVec3 Side = SpinLocalToWorld({ 0.0, 50.0, 0.0 }, { 0.0, 1.0, 0.0 });
	CHECK(Side.Z == Approx(50.0));
}
