// GDD 6.1 (Speed, Acceleration) and 6.3 (sprint, split-step, home drift).
#include "doctest.h"
#include "ShotTestData.h"
#include "Shots/MotionCurve.h"
#include "Shots/PlayerMotion.h"

#include <algorithm>
#include <cmath>

using namespace ShotTestData;
using doctest::Approx;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	double RunFor(FPlayerMotion& Motion, const FVec2& Dir, bool bSprint, double Seconds)
	{
		const int Steps = static_cast<int>(Seconds / Dt);
		for (int I = 0; I < Steps; ++I)
		{
			Motion.Step(Dir, bSprint, Dt);
		}
		return LengthXY(Motion.GetVelocity());
	}
}

TEST_CASE("Shots.Motion: top speed is 4.5 m/s at Speed 1 and 6.5 m/s at 99")
{
	const FMovementTuning T = LoadMovement();
	FPlayerRatings P;
	P.Speed = 1;
	FPlayerMotion Slow(&P, &T, 1.0, Vec(9.0, 4.0, 0.0));
	CHECK(Slow.TopSpeed(false) == Approx(4.5));
	FPlayerRatings Q;
	Q.Speed = 99;
	FPlayerMotion Fast(&Q, &T, 1.0, Vec(9.0, 4.0, 0.0));
	CHECK(Fast.TopSpeed(false) == Approx(6.5));
	CHECK(Fast.TopSpeed(true) == Approx(6.5 * 1.15));
}

TEST_CASE("Shots.Motion: Acceleration sets the push from standing")
{
	const FMovementTuning T = LoadMovement();
	FPlayerRatings P;
	P.Acceleration = 99;
	FPlayerMotion Motion(&P, &T, 1.0, Vec(9.0, 4.5, 0.0));
	CHECK(Motion.TimeToTopSpeed() == Approx(T.TimeToTopAt99S));
	const double Top = Motion.TopSpeed(false);
	const double A0 = MotionCurve::InitialAccel(Top, Motion.TimeToTopSpeed());
	// The first tick pushes at the full initial acceleration (toward -Y, the player's right).
	CHECK(RunFor(Motion, { 1.0, 0.0 }, false, Dt) == Approx(A0 * Dt));
	CHECK(Motion.GetVelocity().Y < 0.0);
	CHECK(RunFor(Motion, { 1.0, 0.0 }, false, 0.3) < Top);
	CHECK(RunFor(Motion, { 1.0, 0.0 }, false, 1.5) == Approx(Top));
}

TEST_CASE("Shots.Motion: up on the stick moves toward the net for both teams")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings P;
	FPlayerMotion A(&P, &T, 1.0, Vec(9.0, 0.0, 0.0));
	FPlayerMotion B(&P, &T, -1.0, Vec(-9.0, 0.0, 0.0));
	RunFor(A, { 0.0, 1.0 }, false, 0.5);
	RunFor(B, { 0.0, 1.0 }, false, 0.5);
	CHECK(A.GetPosition().X < 9.0);
	CHECK(B.GetPosition().X > -9.0);
}

TEST_CASE("Shots.Motion: players stay on their own half")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings P;
	FPlayerMotion A(&P, &T, 1.0, Vec(2.0, 0.0, 0.0));
	RunFor(A, { 0.0, 1.0 }, true, 3.0);
	CHECK(A.GetPosition().X == Approx(T.NetMarginM));
	RunFor(A, { 1.0, 0.0 }, true, 3.0);
	CHECK(A.GetPosition().Y == Approx(-(5.0 - T.WallMarginM)));
}

TEST_CASE("Shots.Motion: sprint drains stamina, rest brings it back")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings P;
	FPlayerMotion M(&P, &T, 1.0, Vec(9.0, 4.5, 0.0));
	RunFor(M, { 1.0, 0.0 }, true, 1.0);
	CHECK(M.IsSprinting());
	const double After = M.GetStamina();
	CHECK(After < 1.0);
	RunFor(M, { 0.0, 0.0 }, false, 1.0);
	CHECK(M.GetStamina() > After);
}

TEST_CASE("Shots.Motion: split-step only when slow inside 150 ms before the contact")
{
	const FMovementTuning T = LoadMovement();
	const FPlayerRatings P;
	FPlayerMotion M(&P, &T, 1.0, Vec(8.0, 4.5, 0.0));
	// Standing still: splits.
	RunFor(M, { 0.0, 0.0 }, false, 0.2);
	CHECK(M.NotifyOpponentContact());
	CHECK(M.ConsumeSplitStep());
	CHECK_FALSE(M.ConsumeSplitStep());
	// Running flat out for a while: no split.
	RunFor(M, { 1.0, 0.0 }, false, 1.0);
	CHECK_FALSE(M.NotifyOpponentContact());
	// Stopped, then moving for 400 ms before the contact: outside the window. (M1: the
	// softer start passes the 1 m/s split limit later, so 300 ms is now inside it.)
	FPlayerMotion N(&P, &T, 1.0, Vec(8.0, 4.5, 0.0));
	RunFor(N, { 0.0, 0.0 }, false, 0.2);
	RunFor(N, { 1.0, 0.0 }, false, 0.4);
	const bool bSplit = N.NotifyOpponentContact();
	CHECK_FALSE(bSplit);
}

TEST_CASE("Shots.Motion: with no input the player drifts home, faster with Positioning")
{
	const FMovementTuning T = LoadMovement();
	FPlayerRatings Low;
	Low.Positioning = 1;
	FPlayerRatings High;
	High.Positioning = 99;
	FPlayerMotion A(&Low, &T, 1.0, Vec(9.0, 3.0, 0.0));
	FPlayerMotion B(&High, &T, 1.0, Vec(9.0, 3.0, 0.0));
	A.SetHome(Vec(8.0, 0.0, 0.0));
	B.SetHome(Vec(8.0, 0.0, 0.0));
	RunFor(A, { 0.0, 0.0 }, false, 0.5);
	RunFor(B, { 0.0, 0.0 }, false, 0.5);
	CHECK(B.GetPosition().Y < A.GetPosition().Y);
	CHECK(A.GetPosition().Y < 3.0);
	RunFor(B, { 0.0, 0.0 }, false, 5.0);
	CHECK(DistanceXY(B.GetPosition(), Vec(8.0, 0.0, 0.0)) <= T.HomeArriveM + 0.05);
}

// M1 (BIOMECHANICS_MOVEMENT 3.1, 4, 5.1): acceleration is strongest from standing and
// falls as speed rises, and no rating starts faster than a human sprinter.
namespace
{
	// The real tuning with the court limits pushed out of the way, so straight runs and
	// stops measure the curve and not the glass.
	FMovementTuning OpenField()
	{
		FMovementTuning T = LoadMovement();
		T.WallMarginM = -100.0;
		T.NetMarginM = -100.0;
		return T;
	}

	FPlayerRatings Rated(int Rating)
	{
		FPlayerRatings R;
		R.Speed = Rating;
		R.Acceleration = Rating;
		return R;
	}

	// Seconds to run Distance straight from standing, flat out (no sprint).
	double TimeToRun(const FPlayerRatings& R, const FMovementTuning& T, double Distance)
	{
		FPlayerMotion M(&R, &T, 1.0, Vec(9.5, 4.6, 0.0));
		const FVec3 Start = M.GetPosition();
		int Steps = 0;
		while (DistanceXY(M.GetPosition(), Start) < Distance && Steps < 2000)
		{
			M.Step({ 1.0, 0.0 }, false, Dt);
			++Steps;
		}
		return Steps * Dt;
	}

	struct FMoveResult
	{
		double PeakMps = 0.0;
		double TimeS = 0.0;
		double DistanceM = 0.0;
	};

	// One move from standing to a stop Distance away, the stick held at CapMps / top speed
	// and let go once the remaining distance is the braking distance.
	FMoveResult SingleMove(const FPlayerRatings& R, const FMovementTuning& T, double Distance, double CapMps)
	{
		FPlayerMotion M(&R, &T, 1.0, Vec(9.5, 4.6, 0.0));
		const FVec3 Start = M.GetPosition();
		const double Top = M.TopSpeed(false);
		const double Brake = MotionCurve::InitialAccel(Top, M.TimeToTopSpeed()) * T.BrakeMult;
		const double Gain = std::min(1.0, CapMps / Top);
		FMoveResult Out;
		bool bBraking = false;
		for (int Step = 0; Step < 1200; ++Step)
		{
			const double Speed = LengthXY(M.GetVelocity());
			const double Left = Distance - DistanceXY(M.GetPosition(), Start);
			bBraking = bBraking || Left <= Speed * Speed / (2.0 * Brake);
			M.Step({ bBraking ? 0.0 : Gain, 0.0 }, false, Dt);
			Out.PeakMps = std::max(Out.PeakMps, LengthXY(M.GetVelocity()));
			Out.TimeS += Dt;
			if (bBraking && LengthXY(M.GetVelocity()) < 1e-9)
			{
				break;
			}
		}
		Out.DistanceM = DistanceXY(M.GetPosition(), Start);
		return Out;
	}
}

TEST_CASE("Shots.Motion: acceleration falls as speed rises (M1)")
{
	const FMovementTuning T = OpenField();
	const FPlayerRatings R = Rated(77);
	FPlayerMotion M(&R, &T, 1.0, Vec(9.5, 4.6, 0.0));
	const double Top = M.TopSpeed(false);
	const double A0 = MotionCurve::InitialAccel(Top, M.TimeToTopSpeed());
	const double First = RunFor(M, { 1.0, 0.0 }, false, Dt) / Dt;
	CHECK(First == Approx(A0));
	// Run up to about 90% of top speed, then measure one tick.
	while (LengthXY(M.GetVelocity()) < 0.9 * Top)
	{
		M.Step({ 1.0, 0.0 }, false, Dt);
	}
	const double V = LengthXY(M.GetVelocity());
	const double Late = (RunFor(M, { 1.0, 0.0 }, false, Dt) - V) / Dt;
	CHECK(Late == Approx(A0 * (1.0 - T.AccelFalloff * V / Top)).epsilon(0.01));
	CHECK(Late < 0.8 * First);
}

TEST_CASE("Shots.Motion: braking keeps its full strength at speed (M1)")
{
	const FMovementTuning T = OpenField();
	const FPlayerRatings R = Rated(77);
	FPlayerMotion M(&R, &T, 1.0, Vec(9.5, 4.6, 0.0));
	RunFor(M, { 1.0, 0.0 }, false, 2.0);
	const double V = LengthXY(M.GetVelocity());
	const double A0 = MotionCurve::InitialAccel(M.TopSpeed(false), M.TimeToTopSpeed());
	CHECK((V - RunFor(M, { 0.0, 0.0 }, false, Dt)) / Dt == Approx(A0 * T.BrakeMult));
}

TEST_CASE("Shots.Motion: a player running one way can still turn hard (M1)")
{
	// The falloff counts only speed along the new direction, so a reversal at top speed
	// gets the full push, not none.
	const FMovementTuning T = OpenField();
	const FPlayerRatings R = Rated(77);
	FPlayerMotion M(&R, &T, 1.0, Vec(9.0, 0.0, 0.0));
	RunFor(M, { 1.0, 0.0 }, false, 2.0);
	const double Before = M.GetVelocity().Y;
	RunFor(M, { -1.0, 0.0 }, false, 0.1);
	const double A0 = MotionCurve::InitialAccel(M.TopSpeed(false), M.TimeToTopSpeed());
	CHECK((M.GetVelocity().Y - Before) / 0.1 > 0.95 * A0);
}

TEST_CASE("Shots.Motion: no rating starts faster than a human sprinter (M1)")
{
	const FMovementTuning T = OpenField();
	for (const int Rating : { 1, 77, 99 })
	{
		CAPTURE(Rating);
		const FPlayerRatings R = Rated(Rating);
		const FPlayerMotion M(&R, &T, 1.0, Vec(9.5, 4.6, 0.0));
		// BIOMECHANICS_MOVEMENT 3.1: human sprint starts reach about 6 to 9 m/s^2.
		CHECK(MotionCurve::InitialAccel(M.TopSpeed(false), M.TimeToTopSpeed()) <= 9.0);
	}
}

TEST_CASE("Shots.Motion: 5 m and 10 m from standing (M1)")
{
	// Before M1 (constant acceleration) these were 1.56 / 2.68 s at rating 1, 1.10 / 1.93 s
	// at 77 and 0.99 / 1.77 s at 99. Holding them within 5% needs at least 9.2 m/s^2 from
	// standing at 77, over the human cap above, so M1 lets them grow (the plan lead's call:
	// up to about 15%): about +4/+2% at 1, +14/+8% at 77 and +18/+10% at 99, the times of
	// a good racket athlete from a standing start. The stepped run must also follow the shared closed form.
	const FMovementTuning T = OpenField();
	struct FCase
	{
		int Rating;
		double Lo5, Hi5, Lo10, Hi10;
	};
	for (const FCase& C : { FCase{ 1, 1.55, 1.75, 2.65, 2.90 }, FCase{ 77, 1.20, 1.35, 2.00, 2.20 }, FCase{ 99, 1.10, 1.30, 1.85, 2.10 } })
	{
		CAPTURE(C.Rating);
		const FPlayerRatings R = Rated(C.Rating);
		const FPlayerMotion M(&R, &T, 1.0, Vec(9.5, 4.6, 0.0));
		const double T5 = TimeToRun(R, T, 5.0);
		const double T10 = TimeToRun(R, T, 10.0);
		CHECK(T5 >= C.Lo5);
		CHECK(T5 <= C.Hi5);
		CHECK(T10 >= C.Lo10);
		CHECK(T10 <= C.Hi10);
		for (const double D : { 5.0, 10.0 })
		{
			const double Closed = MotionCurve::TimeToCover(D, M.TopSpeed(false), M.TimeToTopSpeed(), T.AccelFalloff);
			CHECK(std::abs(TimeToRun(R, T, D) - Closed) <= 2.0 * Dt);
		}
	}
	CHECK(TimeToRun(Rated(99), T, 5.0) < TimeToRun(Rated(77), T, 5.0));
	CHECK(TimeToRun(Rated(77), T, 5.0) < TimeToRun(Rated(1), T, 5.0));
}

TEST_CASE("Shots.Motion: single moves from standing match the movement table (M1)")
{
	// BIOMECHANICS_MOVEMENT 4: peak speed and time of a 1, 2, 4 and 8 m move from standing.
	// Real players pace short moves, so the move here caps its speed at the top of the
	// table's band; the curve must then cover the distance within 20% of the table's
	// slowest time at ratings 77 and 99 (with the human acceleration cap and braking no
	// harder than starting, rating 77 lands about 0 to 15% slow).
	// A flat-out move peaks higher than the band; pacing it is the AI's job (A3). The 1 m
	// and 2 m rows assume a paced move: flat out, 1 m in 0.6 s at a 2 m/s peak would need
	// about 16 m/s^2, not physical under the human cap (plan lead, review of M1).
	const FMovementTuning T = OpenField();
	struct FRow
	{
		double DistanceM, PeakLo, PeakHi, TimeHi;
	};
	const FRow Rows[] = { { 1.0, 1.5, 2.5, 0.7 }, { 2.0, 2.5, 3.5, 0.9 }, { 4.0, 4.0, 4.5, 1.3 }, { 8.0, 5.0, 5.5, 1.9 } };
	for (const FRow& Row : Rows)
	{
		CAPTURE(Row.DistanceM);
		for (const int Rating : { 1, 77, 99 })
		{
			CAPTURE(Rating);
			const FMoveResult Paced = SingleMove(Rated(Rating), T, Row.DistanceM, Row.PeakHi);
			CHECK(Paced.DistanceM == Approx(Row.DistanceM).epsilon(0.03));
			CHECK(Paced.PeakMps <= Row.PeakHi + 1e-9);
			if (Rating >= 77)
			{
				CHECK(Paced.PeakMps >= Row.PeakLo);
				CHECK(Paced.TimeS <= Row.TimeHi * 1.2);
			}
		}
		// Flat out, a rating-77 player gets there no slower than paced, and the falloff
		// keeps a 4 m move below top speed (it reached it after 1.7 m before M1).
		const FMoveResult Flat = SingleMove(Rated(77), T, Row.DistanceM, 99.0);
		CHECK(Flat.TimeS <= SingleMove(Rated(77), T, Row.DistanceM, Row.PeakHi).TimeS + Dt);
		if (Row.DistanceM <= 4.0)
		{
			const FPlayerRatings R77 = Rated(77);
			CHECK(Flat.PeakMps < 0.98 * FPlayerMotion(&R77, &T, 1.0, Vec(9.5, 4.6, 0.0)).TopSpeed(false));
		}
	}
}
