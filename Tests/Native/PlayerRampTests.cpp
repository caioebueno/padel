// Human movement limits (BIOMECHANICS_MOVEMENT 2.6, 5.7): the push and the brake build over
// AccelRiseS instead of in one tick, a stop eases off and ends, and a reversal at a run
// brakes, plants and then pushes off. These are what let the motion-matched clips follow
// the player without skating.
#include "doctest.h"
#include "ShotTestData.h"
#include "Shots/AccelRamp.h"
#include "Shots/MotionCurve.h"
#include "Shots/PlayerMotion.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace ShotTestData;
using doctest::Approx;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	FMovementTuning OpenField()
	{
		FMovementTuning T = LoadMovement();
		T.WallMarginM = -100.0;
		T.NetMarginM = -100.0;
		return T;
	}

	// FPlayerMotion's jerk limit: the strongest push (the brake) over the rise time.
	double JerkLimit(const FPlayerMotion& M, const FMovementTuning& T)
	{
		return MotionCurve::InitialAccel(M.TopSpeed(false), M.TimeToTopSpeed()) * std::max(1.0, T.BrakeMult) / T.AccelRiseS;
	}

	struct FStick
	{
		FVec2 Dir;
		double Seconds;
	};

	// Steps the sticks in turn and returns the largest change of applied acceleration in a
	// tick, over the jerk limit's step (1 = at the limit).
	double WorstJerkShare(FPlayerMotion& M, const FMovementTuning& T, const std::vector<FStick>& Sticks)
	{
		const double Limit = JerkLimit(M, T) * Dt;
		FVec3 Before = M.GetAppliedAccel();
		double Worst = 0.0;
		for (const FStick& S : Sticks)
		{
			for (int I = 0; I < static_cast<int>(S.Seconds / Dt); ++I)
			{
				M.Step(S.Dir, false, Dt);
				const FVec3 After = M.GetAppliedAccel();
				Worst = std::max(Worst, LengthXY(Sub(After, Before)) / Limit);
				Before = After;
			}
		}
		return Worst;
	}
}

TEST_CASE("Shots.Ramp: the push builds over AccelRiseS instead of in one tick")
{
	FMovementTuning T = OpenField();
	REQUIRE(T.AccelRiseS > 0.0);
	const FPlayerRatings R = LoadPlayer();
	FPlayerMotion M(&R, &T, 1.0, Vec(9.0, 0.0, 0.0));
	const double A0 = MotionCurve::InitialAccel(M.TopSpeed(false), M.TimeToTopSpeed());
	M.Step({ 0.0, 1.0 }, false, Dt);
	// One tick of the jerk limit, not the whole push.
	CHECK(LengthXY(M.GetAppliedAccel()) == Approx(JerkLimit(M, T) * Dt));
	CHECK(LengthXY(M.GetAppliedAccel()) < 0.2 * A0);
	for (int I = 1; I < static_cast<int>(T.AccelRiseS / Dt) + 1; ++I)
	{
		M.Step({ 0.0, 1.0 }, false, Dt);
	}
	// Built by AccelRiseS (less the falloff at the little speed gained so far).
	CHECK(LengthXY(M.GetAppliedAccel()) == Approx(A0).epsilon(0.03));

	// AccelRiseS 0 is the old instant push.
	T.AccelRiseS = 0.0;
	FPlayerMotion Old(&R, &T, 1.0, Vec(9.0, 0.0, 0.0));
	Old.Step({ 0.0, 1.0 }, false, Dt);
	CHECK(LengthXY(Old.GetVelocity()) == Approx(A0 * Dt));
}

TEST_CASE("Shots.Ramp: starts, stops, cuts and reversals keep to the jerk limit")
{
	const FMovementTuning T = OpenField();
	for (const int Rating : { 1, 50, 99 })
	{
		CAPTURE(Rating);
		FPlayerRatings R;
		R.Speed = R.Acceleration = R.Agility = Rating;
		FPlayerMotion M(&R, &T, 1.0, Vec(9.0, 0.0, 0.0));
		const double Worst = WorstJerkShare(M, T,
			{ { { 0.0, 1.0 }, 1.2 }, { {}, 1.0 }, { { 0.0, 1.0 }, 1.0 }, { { 1.0, 0.0 }, 0.8 }, { { -1.0, 0.0 }, 1.0 }, { { 0.0, -1.0 }, 1.0 },
				{ { 0.0, 1.0 }, 1.0 }, { { 0.7, 0.7 }, 0.3 }, { { -0.7, 0.7 }, 0.3 }, { {}, 1.0 } });
		CHECK(Worst <= 1.0 + 1e-9);
	}
}

TEST_CASE("Shots.Ramp: a stop eases off and ends, without stepping back")
{
	const FMovementTuning T = OpenField();
	const FPlayerRatings R = LoadPlayer();
	FPlayerMotion M(&R, &T, 1.0, Vec(9.0, 0.0, 0.0));
	for (int I = 0; I < 120; ++I)
	{
		M.Step({ 0.0, 1.0 }, false, Dt);
	}
	const FVec3 Heading = Scale(M.GetVelocity(), 1.0 / LengthXY(M.GetVelocity()));
	const double Brake = MotionCurve::InitialAccel(M.TopSpeed(false), M.TimeToTopSpeed()) * T.BrakeMult;
	const double Speed = LengthXY(M.GetVelocity());
	int Ticks = 0;
	double LastAccel = 0.0;
	while (LengthXY(M.GetVelocity()) > 0.0 && Ticks < 240)
	{
		LastAccel = LengthXY(M.GetAppliedAccel());
		M.Step({}, false, Dt);
		// Never past standing, back the way the player came.
		CHECK(DotProduct(M.GetVelocity(), Heading) >= 0.0);
		++Ticks;
	}
	REQUIRE(LengthXY(M.GetVelocity()) == 0.0);
	// The brake had eased off by the end: a tick of the jerk limit, not a jolt.
	CHECK(LastAccel <= JerkLimit(M, T) * Dt * 2.0);
	// About the hard stop plus the build-up and the ease-off: well under a quarter of a
	// second more.
	CHECK(Ticks * Dt <= Speed / Brake + 2.0 * T.AccelRiseS + Dt);
	CHECK(Ticks * Dt >= Speed / Brake);
}

TEST_CASE("Shots.Pivot: a reversal at a run brakes, plants, then pushes off")
{
	const FMovementTuning T = OpenField();
	REQUIRE(T.PivotMinDeg > 0.0);
	const auto Reverse = [&T](const FPlayerRatings& R, const FVec2& Then, int& PlantedTicks, double& PlantedTopSpeed)
	{
		FPlayerMotion M(&R, &T, 1.0, Vec(9.0, 0.0, 0.0));
		for (int I = 0; I < 120; ++I)
		{
			M.Step({ 0.0, 1.0 }, false, Dt);
		}
		PlantedTicks = 0;
		PlantedTopSpeed = 0.0;
		bool bPivoted = false;
		for (int I = 0; I < 180; ++I)
		{
			M.Step(Then, false, Dt);
			bPivoted = bPivoted || M.IsPivoting();
			if (M.IsPivotPlanted())
			{
				++PlantedTicks;
				PlantedTopSpeed = std::max(PlantedTopSpeed, LengthXY(M.GetVelocity()));
			}
		}
		return bPivoted;
	};
	FPlayerRatings Agile;
	Agile.Agility = 99;
	FPlayerRatings Stiff;
	Stiff.Agility = 1;
	int AgileTicks = 0;
	int StiffTicks = 0;
	double Top = 0.0;
	CHECK(Reverse(Agile, { 0.0, -1.0 }, AgileTicks, Top));
	CHECK(Top <= T.PivotSpeedMps);
	CHECK(Reverse(Stiff, { 0.0, -1.0 }, StiffTicks, Top));
	CHECK(Top <= T.PivotSpeedMps);
	// GDD 6.1 Agility sets the plant.
	CHECK(AgileTicks * Dt == Approx(T.PivotPlantAt99S).epsilon(Dt / T.PivotPlantAt99S + 1e-9));
	CHECK(StiffTicks * Dt == Approx(T.PivotPlantAt1S).epsilon(Dt / T.PivotPlantAt1S + 1e-9));
	CHECK(AgileTicks < StiffTicks);
	// A cut to the side is no pivot.
	int Ticks = 0;
	CHECK_FALSE(Reverse(Stiff, { 1.0, 0.0 }, Ticks, Top));
	// At a walk there's nothing to plant against.
	FPlayerMotion Walk(&Stiff, &T, 1.0, Vec(9.0, 0.0, 0.0));
	for (int I = 0; I < 120; ++I)
	{
		Walk.Step({ 0.0, 0.1 }, false, Dt);
	}
	REQUIRE(LengthXY(Walk.GetVelocity()) <= T.SplitMaxSpeedMps);
	bool bPivoted = false;
	for (int I = 0; I < 60; ++I)
	{
		Walk.Step({ 0.0, -1.0 }, false, Dt);
		bPivoted = bPivoted || Walk.IsPivoting();
	}
	CHECK_FALSE(bPivoted);
	// PivotMinDeg 0 turns pivots off.
	FMovementTuning Off = T;
	Off.PivotMinDeg = 0.0;
	FPlayerMotion NoPivot(&Stiff, &Off, 1.0, Vec(9.0, 0.0, 0.0));
	for (int I = 0; I < 120; ++I)
	{
		NoPivot.Step({ 0.0, 1.0 }, false, Dt);
	}
	for (int I = 0; I < 60; ++I)
	{
		NoPivot.Step({ 0.0, -1.0 }, false, Dt);
		CHECK_FALSE(NoPivot.IsPivoting());
	}
}

TEST_CASE("Shots.Ramp: running into the glass or the net eases onto the line within the jerk limit")
{
	const FMovementTuning T = LoadMovement();
	FPlayerRatings R;
	R.Speed = R.Acceleration = R.Agility = 77;
	struct FCase
	{
		const char* Name;
		FVec3 Start;
		FVec2 Dir;
	};
	for (const FCase& C : { FCase{ "net", Vec(8.0, 0.0, 0.0), { 0.0, 1.0 } }, FCase{ "side glass", Vec(6.0, 4.0, 0.0), { 1.0, 0.0 } },
			 FCase{ "back glass", Vec(4.0, 0.0, 0.0), { 0.0, -1.0 } } })
	{
		CAPTURE(std::string(C.Name));
		FPlayerMotion M(&R, &T, 1.0, C.Start);
		CHECK(WorstJerkShare(M, T, { { C.Dir, 4.0 } }) <= 1.0 + 1e-9);
		CHECK(M.GetEdgeStopCount() == 0);
		CHECK(LengthXY(M.GetVelocity()) == 0.0);
	}
}

TEST_CASE("Shots.Ramp: the stop distance and stop speed match the stepped ramp")
{
	const double Rise = 0.08;
	const double Brake = 7.5;
	const double Jerk = Brake / Rise;
	for (const double Speed : { 0.5, 2.0, 4.0, 6.5 })
	{
		for (const double Accel : { -3.0, 0.0, 5.0 })
		{
			CAPTURE(Speed);
			CAPTURE(Accel);
			// AccelRamp::Step itself with nothing wanted, stepped to a stop.
			double VX = Speed;
			double VY = 0.0;
			AccelRamp::FState A{ Accel, 0.0 };
			double Travel = 0.0;
			for (int I = 0; I < 600 && VX > 0.0; ++I)
			{
				AccelRamp::Step(VX, VY, A, 0.0, 0.0, Brake, Rise, Jerk, Dt);
				Travel += std::max(0.0, VX) * Dt;
			}
			CHECK(AccelRamp::StopDistance(Speed, Accel, Brake, Rise, Jerk, Dt) == Approx(Travel));
			// A pushing start runs further than a braking one.
			CHECK(AccelRamp::StopDistance(Speed, 5.0, Brake, Rise, Jerk, Dt) > AccelRamp::StopDistance(Speed, -3.0, Brake, Rise, Jerk, Dt));
		}
	}
	// The stop speed for a distance stops inside it, from no push under way.
	for (const double Distance : { 0.05, 0.3, 1.0, 3.0 })
	{
		CAPTURE(Distance);
		const double V = AccelRamp::StopSpeed(Distance, Brake, Rise);
		CHECK(V > 0.0);
		CHECK(AccelRamp::StopDistance(V, 0.0, Brake, Rise, Jerk, Dt) <= Distance);
	}
	// No rise: the plain v^2 / 2a.
	CHECK(AccelRamp::StopSpeed(2.0, Brake, 0.0) == Approx(std::sqrt(2.0 * Brake * 2.0)));
}

TEST_CASE("Shots.Ramp: Movement.json's limits are validated")
{
	FMovementTuning T = LoadMovement();
	CHECK(T.Validate().empty());
	T.AccelRiseS = -0.1;
	CHECK_FALSE(T.Validate().empty());
	T = LoadMovement();
	T.PivotMinDeg = 200.0;
	CHECK_FALSE(T.Validate().empty());
	T = LoadMovement();
	T.PivotPlantAt1S = -0.1;
	CHECK_FALSE(T.Validate().empty());
}
