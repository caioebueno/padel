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
