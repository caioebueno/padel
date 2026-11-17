// F1 body dynamics (docs/animation/MOVEMENT_FLUENCY.md): the facing spring and its head and
// chest lead, lean into the acceleration, the pelvis sinking on a brake, the split-step hop
// and landing, and the ready-stance bounce. The pose applies these over the clips; here the
// model's numbers are checked against the real Data/Animation.json.

#include "doctest.h"
#include "AnimTestData.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "ShotTestData.h"

#include <algorithm>
#include <cmath>

using namespace ShotTestData;
using namespace AnimTestData;
using doctest::Approx;
using Padel::EMovementMode;
using Padel::Animation::FAnimSettings;
using Padel::Animation::FPlayerAnimFrame;
using Padel::Animation::FPlayerAnimInput;
using Padel::Animation::FPlayerAnimModel;
using Padel::Shots::FVec3;
using Padel::Shots::Vec;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	// Team A (+X half) faces the net along -X: forward is -X, the body's right is -Y.
	FPlayerAnimInput BackCourt()
	{
		FPlayerAnimInput In;
		In.TeamSign = 1.0;
		In.bRallyLive = true;
		In.Position = Vec(7.0, 0.0, 0.0);
		return In;
	}

	double AngleDiff(double A, double B)
	{
		return std::remainder(A - B, 360.0);
	}
}

TEST_CASE("Anim.Body: the new Animation.json fields load and validate (F1)")
{
	const FAnimSettings S = LoadSettings();
	CHECK(S.Validate().empty());
	CHECK(S.FacingHeadHalfLifeS <= S.FacingChestHalfLifeS);
	CHECK(S.FacingChestHalfLifeS <= S.FacingHalfLifeS);

	FAnimSettings Bad = S;
	Bad.FacingHeadHalfLifeS = S.FacingHalfLifeS * 2.0;
	CHECK_FALSE(Bad.Validate().empty());
	Bad = S;
	Bad.LeanMaxDeg = -1.0;
	CHECK_FALSE(Bad.Validate().empty());
}

TEST_CASE("Anim.Body: a turn starts at the head, then the chest, and the hips follow smoothly (F1)")
{
	const FAnimSettings S = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&S, &Clips, &Lib);
	FPlayerAnimInput In = BackCourt();
	Model.Step(In, Dt);
	CHECK(Model.GetFrame().FacingYawDeg == Approx(180.0));

	// Turn and run to the body's right (-Y): the target facing is -90.
	In.Mode = EMovementMode::Run;
	In.Velocity = Vec(0.0, -S.RunClipSpeedMps, 0.0);
	double Prev = 180.0;
	double PrevMoved = 0.0;
	bool bHeadLed = false;
	for (int I = 0; I < static_cast<int>(1.5 / Dt); ++I)
	{
		const FPlayerAnimFrame& F = Model.Step(In, Dt);
		CAPTURE(I);
		CHECK(std::fabs(F.ChestTwistDeg) <= S.FacingMaxChestTwistDeg + 1e-9);
		CHECK(std::fabs(F.HeadTwistDeg) <= S.FacingMaxHeadTwistDeg + 1e-9);
		// The pelvis turns the short way (180 -> 270 = -90): positive yaw. The chest and
		// head are ahead of it the same way, never behind.
		CHECK(F.ChestTwistDeg >= -1e-9);
		CHECK(F.ChestTwistDeg + F.HeadTwistDeg >= -1e-9);
		if (I == 3)
		{
			bHeadLed = F.HeadTwistDeg > 0.0 && F.ChestTwistDeg > 0.0;
		}
		// No jump in turning speed from one tick to the next.
		const double Moved = AngleDiff(F.FacingYawDeg, Prev);
		CHECK(std::fabs(Moved - PrevMoved) <= 1.5);
		PrevMoved = Moved;
		Prev = F.FacingYawDeg;
	}
	CHECK(bHeadLed);
	const FPlayerAnimFrame& End = Model.GetFrame();
	CHECK(AngleDiff(End.FacingYawDeg, -90.0) == Approx(0.0));
	CHECK(End.ChestTwistDeg == Approx(0.0));
	CHECK(End.HeadTwistDeg == Approx(0.0));
}

TEST_CASE("Anim.Body: the trunk leans into a start, back on a brake and into a sideways push (F1)")
{
	const FAnimSettings S = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&S, &Clips, &Lib);
	FPlayerAnimInput In = BackCourt();
	In.Mode = EMovementMode::Adjust;
	Model.Step(In, Dt);

	// Forward to the net (-X) at 6 m/s^2 for 0.3 s.
	double MaxLean = 0.0;
	for (int I = 1; I <= 36; ++I)
	{
		In.Velocity = Vec(-6.0 * I * Dt, 0.0, 0.0);
		const FPlayerAnimFrame& F = Model.Step(In, Dt);
		MaxLean = std::max(MaxLean, std::hypot(F.LeanForwardDeg, F.LeanRightDeg));
	}
	CHECK(Model.GetFrame().LeanForwardDeg > 3.0);
	CHECK(std::fabs(Model.GetFrame().LeanRightDeg) < 0.1);

	// Brake hard to a stop: the trunk leans back and the pelvis sinks, feet planted.
	double MinDrop = 0.0;
	const double V0 = -6.0 * 36 * Dt;
	for (int I = 1; I <= 24; ++I)
	{
		In.Velocity = Vec(V0 * (1.0 - I / 24.0), 0.0, 0.0);
		const FPlayerAnimFrame& F = Model.Step(In, Dt);
		MaxLean = std::max(MaxLean, std::hypot(F.LeanForwardDeg, F.LeanRightDeg));
		MinDrop = std::min(MinDrop, F.PelvisDropM);
	}
	CHECK(Model.GetFrame().LeanForwardDeg < -3.0);
	CHECK(MinDrop < -0.02);
	CHECK(MinDrop >= -(S.PlantDropMaxM + S.ReadyBounceM) - 1e-9);
	CHECK(MaxLean <= S.LeanMaxDeg + 1e-9);

	// Standing again, everything settles back.
	In.Velocity = Vec(0.0, 0.0, 0.0);
	In.bRallyLive = false;
	for (int I = 0; I < 120; ++I)
	{
		Model.Step(In, Dt);
	}
	CHECK(Model.GetFrame().LeanForwardDeg == Approx(0.0).epsilon(0.01));
	CHECK(Model.GetFrame().PelvisDropM == Approx(0.0).epsilon(0.002));

	// A shuffle to the body's right (-Y for Team A) leans right.
	In.Mode = EMovementMode::Shuffle;
	for (int I = 1; I <= 24; ++I)
	{
		In.Velocity = Vec(0.0, -5.0 * I * Dt, 0.0);
		Model.Step(In, Dt);
	}
	CHECK(Model.GetFrame().LeanRightDeg > 3.0);
}

TEST_CASE("Anim.Body: the split step is a visible hop and a loaded landing (F1, GDD 6.3)")
{
	const FAnimSettings S = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&S, &Clips, &Lib);
	FPlayerAnimInput In = BackCourt();
	Model.Step(In, Dt);

	const double HopS = 0.18;
	double Peak = 0.0;
	In.bSplitHopping = true;
	for (double Left = HopS - Dt; Left > 0.0; Left -= Dt)
	{
		In.SplitHopLeftS = Left;
		const FPlayerAnimFrame& F = Model.Step(In, Dt);
		CHECK(F.HopLiftM >= 0.0);
		CHECK(F.HopLiftM <= S.SplitHopHeightM + 1e-9);
		Peak = std::max(Peak, F.HopLiftM);
	}
	CHECK(Peak == Approx(S.SplitHopHeightM).epsilon(0.05));

	// Landed: no lift, and the legs take the landing.
	In.bSplitHopping = false;
	In.SplitHopLeftS = 0.0;
	double MinDrop = 0.0;
	for (int I = 0; I < static_cast<int>(S.SplitLandS / Dt); ++I)
	{
		const FPlayerAnimFrame& F = Model.Step(In, Dt);
		CHECK(F.HopLiftM == 0.0);
		MinDrop = std::min(MinDrop, F.PelvisDropM);
	}
	CHECK(MinDrop < -0.5 * S.SplitLandDropM);
}

TEST_CASE("Anim.Body: standing ready in a rally bounces lightly; moving or between points doesn't (F1)")
{
	const FAnimSettings S = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&S, &Clips, &Lib);
	FPlayerAnimInput In = BackCourt();
	for (int I = 0; I < 120; ++I)
	{
		Model.Step(In, Dt);
	}
	double Lo = 0.0;
	double Hi = -1.0;
	for (int I = 0; I < 120; ++I)
	{
		const FPlayerAnimFrame& F = Model.Step(In, Dt);
		Lo = std::min(Lo, F.PelvisDropM);
		Hi = std::max(Hi, F.PelvisDropM);
	}
	CHECK(Lo < -0.8 * S.ReadyBounceM);
	CHECK(Lo >= -S.ReadyBounceM - 1e-9);
	CHECK(Hi == Approx(0.0).epsilon(0.002));

	// Between points (no rally): the relaxed idle, no bounce.
	In.bRallyLive = false;
	for (int I = 0; I < 120; ++I)
	{
		Model.Step(In, Dt);
	}
	Lo = 0.0;
	for (int I = 0; I < 60; ++I)
	{
		Lo = std::min(Lo, Model.Step(In, Dt).PelvisDropM);
	}
	CHECK(Lo > -0.1 * S.ReadyBounceM);
}
