// GDD 6.1 (Speed, Acceleration) and 6.3 (sprint, split-step, home drift).
#include "doctest.h"
#include "ShotTestData.h"
#include "Shots/PlayerMotion.h"

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

TEST_CASE("Shots.Motion: Acceleration sets the time to top speed")
{
	const FMovementTuning T = LoadMovement();
	FPlayerRatings P;
	P.Acceleration = 99;
	FPlayerMotion Motion(&P, &T, 1.0, Vec(9.0, 4.5, 0.0));
	CHECK(Motion.TimeToTopSpeed() == Approx(0.45));
	// Run across the court (toward -Y is the player's right for Team A).
	const double Top = Motion.TopSpeed(false);
	CHECK(RunFor(Motion, { 1.0, 0.0 }, false, 0.3) < Top);
	CHECK(RunFor(Motion, { 1.0, 0.0 }, false, 0.2) == Approx(Top));
	CHECK(Motion.GetVelocity().Y < 0.0);
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
	// Stopped, then moving for 300 ms before the contact: outside the window.
	FPlayerMotion N(&P, &T, 1.0, Vec(8.0, 4.5, 0.0));
	RunFor(N, { 0.0, 0.0 }, false, 0.2);
	RunFor(N, { 1.0, 0.0 }, false, 0.3);
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
