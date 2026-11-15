// GDD 2: ball flight, spin and the surface responses.
#include "doctest.h"
#include "TestSupport.h"

#include "BallSim.h"
#include "LaunchSolver.h"

#include <cmath>

using namespace Padel;
using namespace Padel::Court;
using doctest::Approx;

namespace
{
	struct FRun
	{
		FBallState State;
		std::vector<FBallContact> Contacts;
	};

	// Runs until the first contact of the wanted kind (or a timeout).
	FRun RunUntil(const FBallSim& Sim, FBallState State, ESurface Surface, double MaxSeconds = 5.0, FPadelRng* Rng = nullptr)
	{
		FRun Run;
		for (double T = 0.0; T < MaxSeconds; T += SimTickSeconds)
		{
			std::vector<FBallContact> Contacts;
			Sim.Tick(State, Rng, Contacts);
			bool bFound = false;
			for (const FBallContact& C : Contacts)
			{
				Run.Contacts.push_back(C);
				if (C.Kind == EContactKind::Surface && C.Surface == Surface)
				{
					bFound = true;
				}
			}
			if (bFound)
			{
				break;
			}
		}
		Run.State = State;
		return Run;
	}

	FBallState Ball(FVec3 Position, FVec3 Velocity, FVec3 Spin = {})
	{
		FBallState S;
		S.Position = Position;
		S.Velocity = Velocity;
		S.Spin = Spin;
		return S;
	}

	const FBallContact* FirstSurface(const std::vector<FBallContact>& Contacts)
	{
		for (const FBallContact& C : Contacts)
		{
			if (C.Kind == EContactKind::Surface)
			{
				return &C;
			}
		}
		return nullptr;
	}
}

TEST_CASE("Ball.Physics: Data/Physics.json loads and validates")
{
	const FBallPhysicsParams& P = Test::Physics();
	CHECK(P.Gravity == Approx(9.81));
	CHECK(P.Ground.Restitution == Approx(0.72));
	CHECK(P.Glass.Restitution == Approx(0.68));
	CHECK(P.Mesh.Restitution == Approx(0.30));
	CHECK(P.SubStepsPerTick >= 1);
}

TEST_CASE("Ball.Physics: a missing field is an error, not a silent default")
{
	FBallPhysicsParams P;
	std::string Error;
	CHECK_FALSE(ParseBallPhysicsJson("[{\"Name\":\"Default\",\"Gravity\":9.81}]", P, Error));
	CHECK(Error.find("DragCoefficient") != std::string::npos);
}

TEST_CASE("Ball.Sim: a dropped ball falls like gravity says (drag is small at low speed)")
{
	const FBallSim Sim(Test::Physics());
	FBallState S = Ball({ 5.0, 0.0, 2.0 }, {});
	std::vector<FBallContact> Contacts;
	const int Ticks = static_cast<int>(0.5 * SimTickRate);
	for (int Index = 0; Index < Ticks; ++Index)
	{
		Sim.Tick(S, nullptr, Contacts);
	}
	CHECK(S.Position.Z == Approx(2.0 - 0.5 * 9.81 * 0.25).epsilon(0.02));
	CHECK(Contacts.empty());
}

TEST_CASE("Ball.Sim: a vertical drop keeps the ground restitution")
{
	const FBallSim Sim(Test::Physics());
	FRun Run = RunUntil(Sim, Ball({ 5.0, 0.0, BallRadius + 0.001 }, { 0.0, 0.0, -5.0 }), ESurface::Ground);
	REQUIRE(FirstSurface(Run.Contacts));
	CHECK(Run.State.Velocity.Z == Approx(5.0 * 0.72).epsilon(0.03));
	CHECK(FirstSurface(Run.Contacts)->Half == EHalf::A);
}

TEST_CASE("Ball.Sim: topspin kicks forward off the grass, slice skids and slows")
{
	const FBallSim Sim(Test::Physics());
	const FVec3 V{ -15.0, 0.0, -6.0 };
	const FVec3 Forward{ -1.0, 0.0, 0.0 };
	auto AfterBounce = [&](double Topspin)
	{
		FBallState S = Ball({ 3.0, 0.0, BallRadius + 0.02 }, V, SpinLocalToWorld({ Topspin, 0.0, 0.0 }, Forward));
		return RunUntil(Sim, S, ESurface::Ground).State;
	};
	const FBallState Top = AfterBounce(250.0);
	const FBallState Flat = AfterBounce(0.0);
	const FBallState Slice = AfterBounce(-250.0);
	// Forward is -X: more negative X speed is a faster ball after the bounce.
	CHECK(-Top.Velocity.X > -Flat.Velocity.X + 0.5);
	CHECK(-Top.Velocity.X > -Slice.Velocity.X + 0.5);
	// The grass grips the slice: much of its backspin is spent, unlike the topspin ball.
	const FVec3 TopAxis = SpinLocalToWorld({ 1.0, 0.0, 0.0 }, Forward);
	CHECK(Dot(Slice.Spin, TopAxis) < Dot(Top.Spin, TopAxis));
}

TEST_CASE("Ball.Sim: topspin dips the flight, so it lands shorter than a flat ball")
{
	const FBallSim Sim(Test::Physics());
	const FVec3 V{ -18.0, 0.0, 4.0 };
	auto Land = [&](double Topspin)
	{
		FBallState S = Ball({ 8.0, 0.0, 1.0 }, V, SpinLocalToWorld({ Topspin, 0.0, 0.0 }, V));
		return RunUntil(Sim, S, ESurface::Ground);
	};
	const FRun Top = Land(300.0);
	const FRun Flat = Land(0.0);
	REQUIRE(FirstSurface(Top.Contacts));
	REQUIRE(FirstSurface(Flat.Contacts));
	CHECK(FirstSurface(Top.Contacts)->Position.X > FirstSurface(Flat.Contacts)->Position.X);
}

TEST_CASE("Ball.Sim: back wall is glass below 3 m and mesh from 3 to 4 m")
{
	const FBallSim Sim(Test::Physics());
	FRun Glass = RunUntil(Sim, Ball({ 8.0, 0.0, 1.5 }, { 20.0, 0.0, 0.0 }), ESurface::Glass, 1.0);
	REQUIRE(FirstSurface(Glass.Contacts));
	CHECK(FirstSurface(Glass.Contacts)->Surface == ESurface::Glass);
	CHECK(Glass.State.Velocity.X < 0.0);

	FRun Mesh = RunUntil(Sim, Ball({ 8.0, 0.0, 3.5 }, { 20.0, 0.0, 1.0 }), ESurface::Mesh, 1.0);
	REQUIRE(FirstSurface(Mesh.Contacts));
	CHECK(FirstSurface(Mesh.Contacts)->Surface == ESurface::Mesh);
}

TEST_CASE("Ball.Sim: the glass keeps more speed than the mesh")
{
	const FBallSim Sim(Test::Physics());
	const FRun Glass = RunUntil(Sim, Ball({ 8.0, 0.0, 1.5 }, { 20.0, 0.0, 1.5 }), ESurface::Glass, 1.0);
	const FRun Mesh = RunUntil(Sim, Ball({ 8.0, 0.0, 3.4 }, { 20.0, 0.0, 1.5 }), ESurface::Mesh, 1.0);
	CHECK(Size(Glass.State.Velocity) > Size(Mesh.State.Velocity) * 1.5);
}

TEST_CASE("Ball.Sim: side wall surfaces follow the court layout")
{
	const FBallSim Sim(Test::Physics());
	// 9 m from the net, 2.5 m up: the full-height glass panel.
	const FRun High = RunUntil(Sim, Ball({ 9.0, 3.0, 2.5 }, { 0.0, 15.0, 0.5 }), ESurface::Glass, 1.0);
	REQUIRE(FirstSurface(High.Contacts));
	CHECK(FirstSurface(High.Contacts)->Surface == ESurface::Glass);
	// 7 m from the net, 2.5 m up: above the 2 m step, so mesh.
	const FRun Step = RunUntil(Sim, Ball({ 7.0, 3.0, 2.5 }, { 0.0, 15.0, 0.5 }), ESurface::Mesh, 1.0);
	REQUIRE(FirstSurface(Step.Contacts));
	CHECK(FirstSurface(Step.Contacts)->Surface == ESurface::Mesh);
	// 2 m from the net: mesh all the way down.
	const FRun Near = RunUntil(Sim, Ball({ 2.0, 3.0, 1.0 }, { 0.0, 15.0, 0.5 }), ESurface::Mesh, 1.0);
	REQUIRE(FirstSurface(Near.Contacts));
	CHECK(FirstSurface(Near.Contacts)->Surface == ESurface::Mesh);
}

TEST_CASE("Ball.Sim: the exit is open, so the ball leaves the court")
{
	const FBallSim Sim(Test::Physics());
	FBallState S = Ball({ 5.0, 3.0, 1.0 }, { 0.0, 12.0, 1.0 });
	std::vector<FBallContact> Contacts;
	for (int Index = 0; Index < 60; ++Index)
	{
		Sim.Tick(S, nullptr, Contacts);
	}
	REQUIRE_FALSE(Contacts.empty());
	CHECK(Contacts.front().Kind == EContactKind::LeftCourt);
	CHECK_FALSE(S.bInCourt);
}

TEST_CASE("Ball.Sim: over the 4 m back fence the ball leaves the court")
{
	const FBallSim Sim(Test::Physics());
	FBallState S = Ball({ 8.0, 0.0, 4.6 }, { 15.0, 0.0, 2.0 });
	std::vector<FBallContact> Contacts;
	for (int Index = 0; Index < 60; ++Index)
	{
		Sim.Tick(S, nullptr, Contacts);
	}
	REQUIRE_FALSE(Contacts.empty());
	CHECK(Contacts.front().Kind == EContactKind::LeftCourt);
}

TEST_CASE("Ball.Sim: a low ball hits the net and drops back on its own side")
{
	const FBallSim Sim(Test::Physics());
	const FRun Run = RunUntil(Sim, Ball({ 3.0, 0.0, 0.5 }, { -25.0, 0.0, 0.0 }), ESurface::Ground, 2.0);
	const FBallContact* First = FirstSurface(Run.Contacts);
	REQUIRE(First);
	CHECK(First->Surface == ESurface::Net);
	CHECK(Run.State.Position.X > 0.0);
	for (const FBallContact& C : Run.Contacts)
	{
		CHECK(C.Kind != EContactKind::CrossedNet);
	}
}

TEST_CASE("Ball.Sim: a fast ball cannot tunnel through the net")
{
	const FBallSim Sim(Test::Physics());
	// 60 m/s moves 12 cm per sub-step at 480 Hz, more than the ball's diameter.
	const FRun Run = RunUntil(Sim, Ball({ 1.0, 0.0, 0.4 }, { -60.0, 0.0, 0.0 }), ESurface::Net, 0.5);
	REQUIRE(FirstSurface(Run.Contacts));
	CHECK(FirstSurface(Run.Contacts)->Surface == ESurface::Net);
	CHECK(Run.State.Position.X > 0.0);
}

TEST_CASE("Ball.Sim: a ball over the net reports the crossing and the new half")
{
	const FBallSim Sim(Test::Physics());
	const FRun Run = RunUntil(Sim, Ball({ 3.0, 0.0, 1.5 }, { -15.0, 0.0, 1.0 }), ESurface::Ground, 2.0);
	bool bCrossed = false;
	for (const FBallContact& C : Run.Contacts)
	{
		if (C.Kind == EContactKind::CrossedNet)
		{
			bCrossed = true;
			CHECK(C.Half == EHalf::B);
		}
	}
	CHECK(bCrossed);
	REQUIRE(FirstSurface(Run.Contacts));
	CHECK(FirstSurface(Run.Contacts)->Half == EHalf::B);
}

TEST_CASE("Ball.Sim: a ball that clips the tape slows and carries over")
{
	const FBallSim Sim(Test::Physics());
	const double Tape = NetHeightAt(0.0);
	const FRun Run = RunUntil(Sim, Ball({ 0.1, 0.0, Tape + BallRadius * 0.5 }, { -8.0, 0.0, 0.0 }), ESurface::Ground, 2.0);
	const FBallContact* First = FirstSurface(Run.Contacts);
	REQUIRE(First);
	CHECK(First->Surface == ESurface::Net);
	CHECK(Run.State.Position.X < 0.0);
}

TEST_CASE("Ball.Sim: the ceiling is a surface")
{
	const FBallSim Sim(Test::Physics());
	const FRun Run = RunUntil(Sim, Ball({ 5.0, 0.0, 7.0 }, { 0.0, 0.0, 10.0 }), ESurface::Ceiling, 1.0);
	REQUIRE(FirstSurface(Run.Contacts));
	CHECK(FirstSurface(Run.Contacts)->Surface == ESurface::Ceiling);
	CHECK(Run.State.Velocity.Z < 0.0);
}

TEST_CASE("Ball.Sim: the mesh is random, but the same seed gives the same bounce")
{
	const FBallSim Sim(Test::Physics());
	auto Bounce = [&](uint64_t Seed)
	{
		FPadelRng Rng(Seed);
		return RunUntil(Sim, Ball({ 8.0, 0.0, 3.4 }, { 20.0, 0.0, 1.5 }), ESurface::Mesh, 1.0, &Rng).State.Velocity;
	};
	CHECK(Bounce(7) == Bounce(7));
	CHECK_FALSE(Bounce(7) == Bounce(8));
	// Without an RNG (prediction) the mesh is deterministic and plain.
	const FRun Plain = RunUntil(Sim, Ball({ 8.0, 0.0, 3.4 }, { 20.0, 0.0, 1.5 }), ESurface::Mesh, 1.0);
	CHECK(Plain.State.Velocity.Y == Approx(0.0));
}

TEST_CASE("Ball.Sim: a dying bounce turns into a roll and reports a second ground contact")
{
	const FBallSim Sim(Test::Physics());
	const FRun Run = RunUntil(Sim, Ball({ 5.0, 0.0, BallRadius + 0.001 }, { 2.0, 0.0, -0.2 }), ESurface::Ground, 1.0);
	int Ground = 0;
	bool bRollReport = false;
	for (const FBallContact& C : Run.Contacts)
	{
		if (C.Surface == ESurface::Ground)
		{
			++Ground;
			bRollReport |= C.bRolling;
		}
	}
	CHECK(Ground == 2);
	CHECK(bRollReport);
	CHECK(Run.State.bRolling);
}

TEST_CASE("Ball.Sim: prediction leaves the state alone and matches stepping")
{
	const FBallSim Sim(Test::Physics());
	const FBallState Start = Ball({ 6.0, 1.0, 1.0 }, { -18.0, -1.0, 3.0 }, { 0.0, -100.0, 0.0 });
	std::vector<FBallContact> PredictedContacts;
	const std::vector<FBallSample> Samples = Sim.Predict(Start, 2.0, 0.1, &PredictedContacts);
	CHECK(Samples.size() == 21);
	CHECK(Samples.back().T == Approx(2.0));

	FBallState S = Start;
	std::vector<FBallContact> Contacts;
	for (int Index = 0; Index < 240; ++Index)
	{
		Sim.Tick(S, nullptr, Contacts);
	}
	CHECK(S.Position == Samples.back().Position);
	CHECK(Contacts.size() == PredictedContacts.size());
	CHECK(Samples.back().GroundBounces >= 1);
}

TEST_CASE("Ball.Sim: same inputs, same flight (determinism)")
{
	const FBallSim Sim(Test::Physics());
	auto Run = [&]()
	{
		FPadelRng Rng(42);
		FBallState S = Ball({ 9.0, -2.0, 1.0 }, { -22.0, 3.0, 6.0 }, { 0.0, 80.0, 20.0 });
		std::vector<FBallContact> Contacts;
		for (int Index = 0; Index < 600; ++Index)
		{
			Sim.Tick(S, &Rng, Contacts);
		}
		return S.Position;
	};
	CHECK(Run() == Run());
}
