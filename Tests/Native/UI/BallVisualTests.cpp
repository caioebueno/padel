// How the ball is shown in motion (GDD 10: readable, a subtle trail at speed, a seam
// that shows spin). The model behind APadelBall; see docs/decisions/0007-ball.md.
#include "doctest.h"

#include "MiniJson.h"
#include "Presentation/Model/PadelBallVisual.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

using namespace Padel::Presentation;
using Padel::FVec3;
using doctest::Approx;

namespace
{
	constexpr double Frame = 1.0 / 60.0;

	std::string ReadRepoFile(const char* Relative)
	{
		std::ifstream In(std::string(PADEL_REPO_ROOT) + "/" + Relative);
		std::stringstream Buffer;
		Buffer << In.rdbuf();
		return Buffer.str();
	}

	// Angle between where the mesh's +X axis points now and where it pointed before.
	double TurnedBy(const FBallQuat& Before, const FBallQuat& After)
	{
		const FVec3 A = Rotate(Before, { 1.0, 0.0, 0.0 });
		const FVec3 B = Rotate(After, { 1.0, 0.0, 0.0 });
		return std::acos(std::clamp(Padel::Court::Dot(A, B), -1.0, 1.0));
	}
}

TEST_CASE("Ball.Visual: Data/Ball.json loads and matches the defaults")
{
	FBallVisualSettings Settings;
	std::string Error;
	REQUIRE_MESSAGE(ParseBallVisualSettings(ReadRepoFile("Data/Ball.json"), Settings, Error), Error);
	const FBallVisualSettings Defaults;
	CHECK(Settings.MaxShownRadPerFrame == Approx(Defaults.MaxShownRadPerFrame));
	CHECK(Settings.TrailSeconds == Approx(Defaults.TrailSeconds));
	CHECK(Settings.TrailMinSpeedMps < Settings.TrailFullSpeedMps);
	CHECK(Settings.TrailMaxOpacity > 0.0);
	CHECK(Settings.TrailMaxOpacity < 1.0);

	FBallVisualSettings Kept;
	Kept.SpinScale = 2.0;
	CHECK_FALSE(ParseBallVisualSettings("{\"Other\": {}}", Kept, Error));
	CHECK(Kept.SpinScale == 2.0);
	CHECK_FALSE(ParseBallVisualSettings("{ nope", Kept, Error));
}

TEST_CASE("Ball.Visual: slow spin is shown exactly, fast spin is capped per frame")
{
	const FBallVisualSettings S;
	// A sliced lob, about 2 turns a second: 0.2 rad a frame, well under the cap.
	const double Slow = 2.0 * 2.0 * 3.14159265358979;
	CHECK(ShownSpinAngle({ 0.0, Slow, 0.0 }, Frame, S) == Approx(Slow * Frame));
	// A smash, 60 turns a second: 6 rad a frame would alias. Capped, but still turning.
	const double Fast = 60.0 * 2.0 * 3.14159265358979;
	CHECK(ShownSpinAngle({ 0.0, Fast, 0.0 }, Frame, S) == Approx(S.MaxShownRadPerFrame));
	CHECK(ShownSpinAngle({ 0.0, Fast, 0.0 }, 0.0, S) == 0.0);
	CHECK(ShownSpinAngle({ 0.0, 0.0, 0.0 }, Frame, S) == 0.0);
}

TEST_CASE("Ball.Visual: the mesh turns about the spin axis, in the spin's direction")
{
	FBallVisual Ball;
	Ball.Reset({ 0.0, 0.0, 1.0 });
	const FVec3 Spin{ 0.0, 30.0, 0.0 };
	Ball.Update({ -0.1, 0.0, 1.0 }, { -20.0, 0.0, 0.0 }, Spin, Frame);
	const FVec3 Front = Rotate(Ball.GetOrientation(), { 1.0, 0.0, 0.0 });
	// Rotating +X about +Y by a small positive angle moves it toward -Z (right-handed, like BallSim).
	CHECK(Front.Z < 0.0);
	CHECK(Front.Y == Approx(0.0).epsilon(1e-9));
	CHECK(TurnedBy(FBallQuat(), Ball.GetOrientation()) == Approx(30.0 * Frame));

	// No spin, no turn; the orientation stays a unit quaternion over many frames.
	const FBallQuat Before = Ball.GetOrientation();
	Ball.Update({ -0.2, 0.0, 1.0 }, { -20.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, Frame);
	CHECK(TurnedBy(Before, Ball.GetOrientation()) == Approx(0.0).epsilon(1e-9));
	for (int I = 0; I < 2000; ++I)
	{
		Ball.Update({ -0.2, 0.0, 1.0 }, { 0.0, 0.0, 0.0 }, { 300.0, -200.0, 100.0 }, Frame);
	}
	const FBallQuat& Q = Ball.GetOrientation();
	CHECK(Q.W * Q.W + Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z == Approx(1.0));
}

TEST_CASE("Ball.Visual: no trail when slow, a trail back along the path when fast")
{
	FBallVisual Ball;
	const FBallVisualSettings& S = Ball.GetSettings();
	CHECK(TrailOpacityForSpeed(S.TrailMinSpeedMps - 1.0, S) == 0.0);
	CHECK(TrailOpacityForSpeed(S.TrailFullSpeedMps + 1.0, S) == Approx(S.TrailMaxOpacity));
	const double Mid = TrailOpacityForSpeed(0.5 * (S.TrailMinSpeedMps + S.TrailFullSpeedMps), S);
	CHECK(Mid > 0.0);
	CHECK(Mid < S.TrailMaxOpacity);

	// A lob at 8 m/s: no trail.
	Ball.Reset({ 0.0, 0.0, 1.0 });
	for (int I = 1; I <= 10; ++I)
	{
		Ball.Update({ 8.0 * Frame * I, 0.0, 1.0 }, { 8.0, 0.0, 0.0 }, {}, Frame);
	}
	CHECK(Ball.GetTrailOpacity() == 0.0);

	// A smash at 35 m/s along +X: full trail, TrailSeconds of path long, pointing back (-X).
	Ball.Reset({ 0.0, 0.0, 1.0 });
	for (int I = 1; I <= 10; ++I)
	{
		Ball.Update({ 35.0 * Frame * I, 0.0, 1.0 }, { 35.0, 0.0, 0.0 }, {}, Frame);
	}
	CHECK(Ball.GetTrailOpacity() == Approx(S.TrailMaxOpacity));
	CHECK(Ball.GetTrailLength() == Approx(std::min(35.0 * S.TrailSeconds, S.TrailMaxLengthM)));
	CHECK(Ball.GetTrailTail().X < Ball.GetPosition().X);
}

TEST_CASE("Ball.Visual: after a bounce the trail points back along the path, not through the glass")
{
	FBallVisual Ball;
	// Toward the back glass at X = 10 at 30 m/s, bouncing off it at frame 5.
	const double V = 30.0;
	FVec3 P{ 10.0 - 5.0 * V * Frame, 0.0, 1.0 };
	Ball.Reset(P);
	for (int I = 1; I <= 7; ++I)
	{
		const bool bBack = I > 5;
		P.X += (bBack ? -V : V) * Frame;
		Ball.Update(P, { bBack ? -V : V, 0.0, 0.0 }, {}, Frame);
	}
	// Moving away from the glass now: the tail is toward the glass, never past it.
	CHECK(Ball.GetTrailTail().X > Ball.GetPosition().X);
	CHECK(Ball.GetTrailTail().X <= 10.0 + 1e-9);
}

TEST_CASE("Ball.Visual: the trail is capped, and a jump is a new ball with no trail")
{
	FBallVisualSettings S;
	S.TrailSeconds = 0.5;
	S.TrailMaxLengthM = 0.6;
	FBallVisual Ball(S);
	Ball.Reset({ 0.0, 0.0, 1.0 });
	for (int I = 1; I <= 40; ++I)
	{
		Ball.Update({ 30.0 * Frame * I, 0.0, 1.0 }, { 30.0, 0.0, 0.0 }, {}, Frame);
	}
	CHECK(Ball.GetTrailLength() == Approx(0.6));

	// The server's toss appears 9 m away: reset, no streak across the court.
	Ball.Update({ -8.0, 2.0, 1.0 }, { 30.0, 0.0, 0.0 }, {}, Frame);
	CHECK(Ball.GetTrailOpacity() == 0.0);
	CHECK(Ball.GetTrailLength() == Approx(0.0));
}
