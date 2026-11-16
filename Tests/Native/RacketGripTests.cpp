// Where the racket sits in the player's hand (GDD 10; docs/decisions/0008-racket.md).
// The model behind UPadelPlayerAnimComponent's racket attachment.
#include "doctest.h"

#include "Animation/Model/RacketGrip.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

using namespace Padel::Animation;
using Padel::FVec3;
using Padel::Court::Cross;
using Padel::Court::Dot;
using Padel::Court::Size;
using doctest::Approx;

namespace
{
	std::string ReadRepoFile(const char* Relative)
	{
		std::ifstream In(std::string(PADEL_REPO_ROOT) + "/" + Relative);
		std::stringstream Buffer;
		Buffer << In.rdbuf();
		return Buffer.str();
	}

	// A right arm held out sideways in a T-pose, facing +Y, palm down (the placeholder's
	// and the MetaHumans' reference pose, in metres).
	FRacketGripInput RightArmTPose(bool bWithKnuckle)
	{
		FRacketGripInput In;
		In.Forearm = { 0.50, 0.0, 1.42 };
		In.Hand = { 0.75, 0.0, 1.42 };
		In.Knuckle = { 0.84, 0.0, 1.42 };
		In.bHasKnuckle = bWithKnuckle;
		return In;
	}

	void CheckOrthonormal(const FRacketGripFrame& Frame)
	{
		CHECK(Size(Frame.AxisX) == Approx(1.0));
		CHECK(Size(Frame.AxisY) == Approx(1.0));
		CHECK(Size(Frame.AxisZ) == Approx(1.0));
		CHECK(Dot(Frame.AxisX, Frame.AxisY) == Approx(0.0).epsilon(1e-9));
		CHECK(Dot(Frame.AxisY, Frame.AxisZ) == Approx(0.0).epsilon(1e-9));
		CHECK(Dot(Frame.AxisZ, Frame.AxisX) == Approx(0.0).epsilon(1e-9));
		// Right-handed, so it maps onto an Unreal rotation without a mirror.
		CHECK(Dot(Cross(Frame.AxisX, Frame.AxisY), Frame.AxisZ) == Approx(1.0));
	}
}

TEST_CASE("Racket.Grip: Data/Racket.json loads and matches the defaults")
{
	FRacketSettings Settings;
	Settings.PalmShare = -1.0;
	std::string Error;
	REQUIRE_MESSAGE(ParseRacketSettings(ReadRepoFile("Data/Racket.json"), Settings, Error), Error);
	const FRacketSettings Defaults;
	CHECK(Settings.Mesh == Defaults.Mesh);
	CHECK(Settings.FaceCentreM == Approx(Defaults.FaceCentreM));
	CHECK(Settings.ForearmBone == Defaults.ForearmBone);
	CHECK(Settings.KnuckleBone == Defaults.KnuckleBone);
	CHECK(Settings.PalmShare == Approx(Defaults.PalmShare));
	CHECK(Settings.PalmLengthM == Approx(Defaults.PalmLengthM));
	CHECK(Settings.GripOutM == Approx(Defaults.GripOutM));
	CHECK(Settings.GripAlongM == Approx(Defaults.GripAlongM));
	CHECK(Settings.HeadTiltDeg == Approx(Defaults.HeadTiltDeg));
	CHECK(Settings.FaceRollDeg == Approx(Defaults.FaceRollDeg));
}

TEST_CASE("Racket.Grip: a file without a Racket object leaves the settings alone")
{
	FRacketSettings Settings;
	Settings.GripOutM = 0.5;
	std::string Error;
	CHECK_FALSE(ParseRacketSettings("{\"Ball\":{}}", Settings, Error));
	CHECK_FALSE(Error.empty());
	CHECK(Settings.GripOutM == Approx(0.5));
	CHECK_FALSE(ParseRacketSettings("{not json", Settings, Error));
	CHECK(Settings.GripOutM == Approx(0.5));
}

TEST_CASE("Racket.Grip: the racket's frame is orthonormal with the handle in front of the palm")
{
	const FRacketSettings Settings;
	const FRacketGripInput In = RightArmTPose(true);
	const FRacketGripFrame Frame = SolveRacketGrip(In, Settings);
	REQUIRE(Frame.bValid);
	CheckOrthonormal(Frame);

	// Palm centre part way to the knuckles; the palm faces down in the reference pose.
	CHECK(Frame.PalmCentre.X == Approx(0.75 + 0.09 * Settings.PalmShare));
	CHECK(Frame.PalmNormal.Z == Approx(-1.0));

	// The handle lies GripOutM in front of the palm; the grip centre is along it.
	const FVec3 FromPalm = Frame.Origin - Frame.PalmCentre;
	CHECK(Dot(FromPalm, Frame.PalmNormal) == Approx(Settings.GripOutM));
	CHECK(Dot(FromPalm, Frame.AxisZ) == Approx(Settings.GripAlongM));
}

TEST_CASE("Racket.Grip: the head points to the thumb side, tilted towards the fingers")
{
	const FRacketSettings Settings;
	const FRacketGripFrame Frame = SolveRacketGrip(RightArmTPose(true), Settings);
	REQUIRE(Frame.bValid);
	// Thumb side is forward (+Y); fingers run out along +X for the right arm.
	CHECK(Frame.AxisZ.Y > 0.0);
	CHECK(Frame.AxisZ.X > 0.0);
	CHECK(Frame.AxisZ.Z == Approx(0.0).epsilon(1e-9));
	const double Tilt = std::atan2(Frame.AxisZ.X, Frame.AxisZ.Y) * 180.0 / 3.14159265358979323846;
	CHECK(Tilt == Approx(Settings.HeadTiltDeg));

	// The face is rolled from the palm's plane by FaceRollDeg about the handle.
	const double Roll = std::acos(Dot(Frame.AxisY, Frame.PalmNormal)) * 180.0 / 3.14159265358979323846;
	CHECK(Roll == Approx(Settings.FaceRollDeg));

	// The left hand mirrors it: the head still points forward.
	FRacketGripInput Left = RightArmTPose(true);
	Left.Forearm.X = -Left.Forearm.X;
	Left.Hand.X = -Left.Hand.X;
	Left.Knuckle.X = -Left.Knuckle.X;
	const FRacketGripFrame LeftFrame = SolveRacketGrip(Left, Settings);
	REQUIRE(LeftFrame.bValid);
	CheckOrthonormal(LeftFrame);
	CHECK(LeftFrame.AxisZ.Y > 0.0);
	CHECK(LeftFrame.AxisZ.X < 0.0);
}

TEST_CASE("Racket.Grip: a skeleton without finger bones uses the forearm")
{
	const FRacketSettings Settings;
	const FRacketGripFrame WithFingers = SolveRacketGrip(RightArmTPose(true), Settings);
	const FRacketGripFrame Without = SolveRacketGrip(RightArmTPose(false), Settings);
	REQUIRE(Without.bValid);
	CheckOrthonormal(Without);
	CHECK(Without.PalmCentre.X == Approx(0.75 + Settings.PalmLengthM));
	// Same arm direction, so the same orientation.
	CHECK(Dot(Without.AxisZ, WithFingers.AxisZ) == Approx(1.0));
	CHECK(Dot(Without.AxisY, WithFingers.AxisY) == Approx(1.0));
}

TEST_CASE("Racket.Grip: an A-pose arm keeps the face square to the handle")
{
	// Arm 45 degrees down: the palm normal is still square to the hand.
	FRacketGripInput In;
	In.Forearm = { 0.30, 0.0, 1.40 };
	In.Hand = { 0.50, 0.0, 1.20 };
	In.Knuckle = { 0.56, 0.0, 1.14 };
	In.bHasKnuckle = true;
	const FRacketGripFrame Frame = SolveRacketGrip(In, FRacketSettings{});
	REQUIRE(Frame.bValid);
	CheckOrthonormal(Frame);
	const FVec3 Along = Padel::Court::SafeNormal(In.Knuckle - In.Hand);
	CHECK(Dot(Frame.PalmNormal, Along) == Approx(0.0).epsilon(1e-9));
	CHECK(Frame.PalmNormal.Z < 0.0);
	CHECK(Frame.AxisZ.Y > 0.0);
}

TEST_CASE("Racket.Grip: degenerate bones give no frame")
{
	FRacketGripInput In;
	In.Forearm = { 0.5, 0.0, 1.4 };
	In.Hand = In.Forearm;
	In.Knuckle = In.Forearm;
	In.bHasKnuckle = true;
	CHECK_FALSE(SolveRacketGrip(In, FRacketSettings{}).bValid);
}
