// GDD 6.3 movement (realism plan M2b): the gait holds on a stick wobbling around 45
// degrees, and arriving at the glass or net margin slowly isn't a dead stop.
#include "doctest.h"
#include "BallState.h"
#include "PadelMath.h"
#include "ShotTestData.h"
#include "Shots/PlayerMotion.h"

#include <vector>

using namespace ShotTestData;

namespace
{
	constexpr double Dt = Padel::SimTickSeconds;

	FVec2 AtDegreesFromForward(double Degrees)
	{
		// Team-relative: Y toward the net, X to the player's right.
		const double R = Degrees * Padel::Math::Pi / 180.0;
		return { Padel::Math::Sin(R), Padel::Math::Cos(R) };
	}

	// Runs the stick for Seconds (Wobble degrees either side of Degrees, switching every
	// three ticks) and returns how many times the mode changed once the player was moving.
	int ModeChanges(const FMovementTuning& T, double Degrees, double Wobble, double Seconds, EMovementMode* Last = nullptr)
	{
		const FPlayerRatings R;
		FPlayerMotion M(&R, &T, 1.0, Vec(6.0, -2.0, 0.0));
		int Changes = 0;
		bool bStarted = false;
		EMovementMode Previous = EMovementMode::Adjust;
		for (int I = 0; I < static_cast<int>(Seconds / Dt); ++I)
		{
			const double Side = (I / 3) % 2 == 0 ? Wobble : -Wobble;
			M.Step(AtDegreesFromForward(Degrees + Side), false, Dt);
			if (M.GetMode() == EMovementMode::Adjust)
			{
				continue;
			}
			Changes += bStarted && M.GetMode() != Previous ? 1 : 0;
			bStarted = true;
			Previous = M.GetMode();
		}
		if (Last)
		{
			*Last = Previous;
		}
		return Changes;
	}

	FMovementTuning OpenField()
	{
		FMovementTuning T = LoadMovement();
		T.WallMarginM = -100.0;
		T.NetMarginM = -100.0;
		// No turn to run, so the gait is the stick's alone.
		T.CrossoverMinDistM = 100.0;
		T.TurnBackMinDistM = 100.0;
		return T;
	}
}

TEST_CASE("Shots.Gait: a steady 45-degree stick keeps one gait")
{
	const FMovementTuning T = OpenField();
	REQUIRE(T.GaitHysteresisDeg > 0.0);
	EMovementMode Last = EMovementMode::Adjust;
	CHECK(ModeChanges(T, 45.0, 0.0, 1.5, &Last) == 0);
	CHECK(Last == EMovementMode::Run);
	CHECK(ModeChanges(T, 135.0, 0.0, 1.5) == 0);
}

TEST_CASE("Shots.Gait: a stick wobbling around 45 degrees doesn't flicker between run and shuffle")
{
	FMovementTuning T = OpenField();
	// The AI's direction wanders a few degrees tick to tick.
	for (const double Line : { 45.0, 135.0 })
	{
		CAPTURE(Line);
		CHECK(ModeChanges(T, Line, 3.0, 1.5) == 0);
	}
	// Without the band the same stick flickers, which is what the band is for.
	T.GaitHysteresisDeg = 0.0;
	CHECK(ModeChanges(T, 45.0, 3.0, 1.5) > 10);
}

TEST_CASE("Shots.Gait: a clear change of direction still changes the gait")
{
	const FMovementTuning T = OpenField();
	const FPlayerRatings R;
	FPlayerMotion M(&R, &T, 1.0, Vec(6.0, -2.0, 0.0));
	for (int I = 0; I < 60; ++I)
	{
		M.Step(AtDegreesFromForward(40.0), false, Dt);
	}
	CHECK(M.GetMode() == EMovementMode::Run);
	// Past the band: sideways.
	for (int I = 0; I < 60; ++I)
	{
		M.Step(AtDegreesFromForward(45.0 + T.GaitHysteresisDeg + 5.0), false, Dt);
	}
	CHECK(M.GetMode() == EMovementMode::Shuffle);
	// Inside the band from the sideways side: still a shuffle.
	for (int I = 0; I < 60; ++I)
	{
		M.Step(AtDegreesFromForward(45.0 - T.GaitHysteresisDeg + 3.0), false, Dt);
	}
	CHECK(M.GetMode() == EMovementMode::Shuffle);
}

TEST_CASE("Shots.Gait: creeping onto the back margin isn't a dead stop")
{
	// A3's soak finding: slow Adjust and Decel creeps onto the back wall's margin (9.7 m)
	// were counted as dead stops because the clamp trims a rounding hair.
	FMovementTuning T = LoadMovement();
	const FPlayerRatings R;
	const double Margin = 10.0 - T.WallMarginM;
	for (const double Stick : { 0.01, 0.03 })
	{
		CAPTURE(Stick);
		FPlayerMotion M(&R, &T, 1.0, Vec(Margin - 0.05, 0.0, 0.0));
		for (int I = 0; I < 240; ++I)
		{
			M.Step({ 0.0, -Stick }, false, Dt);
		}
		CHECK(M.GetPosition().X == doctest::Approx(Margin));
		CHECK(M.GetEdgeStopCount() == 0);
	}
}

TEST_CASE("Shots.Gait: turning sideways while running at the back glass still brakes in time")
{
	// The soak's real dead stops: a player running toward a margin turns the stick
	// sideways near it. The braking toward the glass has to come first, or the sideways
	// push takes part of the tick and the player hits the margin at speed.
	FMovementTuning T = LoadMovement();
	T.EdgeStopToleranceMps = 0.0; // the safety net off: this is the braking's job
	for (const int Rating : { 1, 50, 99 })
	{
		for (const double TurnAt : { 8.6, 8.9, 9.2, 9.4 })
		{
			for (const double Sideways : { 0.5, 1.0 })
			{
				CAPTURE(Rating);
				CAPTURE(TurnAt);
				CAPTURE(Sideways);
				FPlayerRatings R;
				R.Speed = R.Acceleration = R.Agility = Rating;
				FPlayerMotion M(&R, &T, 1.0, Vec(5.0, 0.0, 0.0));
				// Back toward the glass, planned as a long move so it turns and runs.
				int I = 0;
				for (; I < 600 && M.GetPosition().X < TurnAt; ++I)
				{
					M.Step({ 0.0, -1.0 }, false, Dt, 1.0, 10.0);
				}
				REQUIRE(LengthXY(M.GetVelocity()) > 0.5);
				for (int J = 0; J < 120; ++J)
				{
					M.Step({ Sideways, -0.2 }, false, Dt);
				}
				CHECK(M.GetEdgeStopCount() == 0);
				CHECK(M.GetPosition().X <= 10.0 - T.WallMarginM);
			}
		}
	}
}

TEST_CASE("Shots.Gait: the AI lane's back-wall stops brake in time")
{
	// Two shapes from A3's soak repro, with the safety net off. B: the AI's arrival ends
	// still moving on a diagonal toward the glass and the stick goes to zero; with the
	// stick turned sideways the diagonal heads into the corner, so both walls brake at once. A: the
	// player swings (SpeedMult 0.3) with the stick toward the net while still drifting back.
	FMovementTuning T = LoadMovement();
	T.EdgeStopToleranceMps = 0.0;
	const double Margin = 10.0 - T.WallMarginM;
	for (const int Rating : { 1, 50, 99 })
	{
		FPlayerRatings R;
		R.Speed = R.Acceleration = R.Agility = Rating;
		for (const double LetGoAt : { 9.2, 9.4, 9.55, 9.65 })
		{
			for (const double Side : { 0.0, 0.7, 1.0 })
			{
				CAPTURE(Rating);
				CAPTURE(LetGoAt);
				CAPTURE(Side);
				// Shape B: team +1, back is +X; the stick's X is across the court.
				FPlayerMotion B(&R, &T, 1.0, Vec(7.5, -3.0, 0.0));
				const FVec2 Diagonal = { Side, -1.0 };
				for (int I = 0; I < 600 && B.GetPosition().X < LetGoAt; ++I)
				{
					B.Step(Diagonal, false, Dt, 1.0, 3.0);
				}
				for (int I = 0; I < 180; ++I)
				{
					B.Step({ 0.0, 0.0 }, false, Dt);
				}
				CHECK(B.GetEdgeStopCount() == 0);
				CHECK(B.GetPosition().X <= Margin);

				// Shape A: team -1, drifting back toward -X, then swinging with the stick
				// toward the net and a little sideways.
				FPlayerMotion A(&R, &T, -1.0, Vec(-LetGoAt + 0.6, -1.7, 0.0));
				for (int I = 0; I < 600 && A.GetPosition().X > -LetGoAt; ++I)
				{
					A.Step({ Side * 0.5, -0.6 }, false, Dt);
				}
				for (int I = 0; I < 120; ++I)
				{
					A.Step({ 0.05, 0.76 }, false, Dt, 0.3, 1.35);
				}
				CHECK(A.GetEdgeStopCount() == 0);
				CHECK(A.GetPosition().X >= -Margin);
			}
		}
	}
}
