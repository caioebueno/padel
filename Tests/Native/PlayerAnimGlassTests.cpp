// N1c (movement doc 2.10): an AI player waiting on a ball off the glass stands side-on to it.
// The wall comes from FAiMatchRunner::GlassRead (#52); the model only turns the body.

#include "doctest.h"
#include "AnimTestData.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "ShotTestData.h"

#include <cmath>

using namespace ShotTestData;
using namespace AnimTestData;
using doctest::Approx;
using Padel::EGlassWall;
using Padel::EMovementMode;
using Padel::Animation::FAnimSettings;
using Padel::Animation::FPlayerAnimInput;
using Padel::Animation::FPlayerAnimModel;
using Padel::Shots::ESwingPhase;
using Padel::Shots::Vec;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	// Steps long enough for any turn on the facing spring (F1), and returns the facing.
	double SettleFacing(FPlayerAnimModel& Model, FPlayerAnimInput& In, const FAnimSettings& Settings)
	{
		const int Ticks = static_cast<int>(std::ceil((360.0 / Settings.FacingTurnRateDegPerS + 20.0 * Settings.FacingHalfLifeS) / Dt));
		for (int I = 0; I < Ticks; ++I)
		{
			Model.Step(In, Dt);
		}
		return Model.GetFrame().FacingYawDeg;
	}

	double AngleDiff(double A, double B)
	{
		return std::remainder(A - B, 360.0);
	}

	// A Team A player (TeamSign +1: net is yaw 180, the court's right is -Y) near the back glass.
	FPlayerAnimInput BackCourtInput(double ForehandSign)
	{
		FPlayerAnimInput In;
		In.TeamSign = 1.0;
		In.ForehandSign = ForehandSign;
		In.bRallyLive = true;
		In.Position = Vec(8.5, -1.0, 0.0);
		return In;
	}
}

TEST_CASE("Anim.Glass: the turn angles are valid data (N1c)")
{
	const FAnimSettings Settings = LoadSettings();
	CHECK(Settings.Validate().empty());
	CHECK(Settings.GlassBackTurnDeg > 0.0);
	CHECK(Settings.GlassSideTurnDeg > 0.0);

	FAnimSettings Bad = Settings;
	Bad.GlassBackTurnDeg = 200.0;
	CHECK_FALSE(Bad.Validate().empty());
	Bad = Settings;
	Bad.GlassSideTurnDeg = -5.0;
	CHECK_FALSE(Bad.Validate().empty());
}

TEST_CASE("Anim.Glass: off the back glass the chest turns toward the ball's side, either hand (N1c)")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	const double Turn = Settings.GlassBackTurnDeg;

	struct FCase
	{
		double ForehandSign;
		double BallY;
		double WantYaw;
	};
	// Net is 180. The court's right is -Y, so a ball at Y -2.5 is on the right: the chest turns
	// right (toward -Y, yaw 180 + Turn), racket shoulder to the glass for a right-hander's
	// forehand and a left-hander's backhand alike.
	const FCase Cases[] = {
		{ 1.0, -2.5, 180.0 + Turn },
		{ 1.0, 0.5, 180.0 - Turn },
		{ -1.0, -2.5, 180.0 + Turn },
		{ -1.0, 0.5, 180.0 - Turn },
	};
	for (const FCase& C : Cases)
	{
		CAPTURE(C.ForehandSign);
		CAPTURE(C.BallY);
		FPlayerAnimModel Model(&Settings, &Clips, &Lib);
		FPlayerAnimInput In = BackCourtInput(C.ForehandSign);
		Model.Step(In, Dt);
		CHECK(Model.GetFrame().FacingYawDeg == Approx(180.0));

		In.GlassWall = EGlassWall::Back;
		In.bHasIdeal = true;
		In.IdealBallPosition = Vec(9.2, C.BallY, 0.9);
		// It eases into the turn (F1's spring), not in one tick.
		Model.Step(In, Dt);
		const double Moved = std::fabs(AngleDiff(Model.GetFrame().FacingYawDeg, 180.0));
		CHECK(Moved > 0.0);
		CHECK(Moved < Settings.FacingTurnRateDegPerS * Dt);
		CHECK(AngleDiff(SettleFacing(Model, In, Settings), C.WantYaw) == Approx(0.0).epsilon(1e-6));
	}
}

TEST_CASE("Anim.Glass: off a side glass the body opens toward that wall, on either half (N1c)")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	const double Turn = Settings.GlassSideTurnDeg;

	struct FCase
	{
		double TeamSign;
		EGlassWall Wall;
		double WantYaw;
	};
	// Team A faces yaw 180, Team B yaw 0. +Y is yaw 90, -Y is yaw -90.
	const FCase Cases[] = {
		{ 1.0, EGlassWall::SidePlusY, 180.0 - Turn },
		{ 1.0, EGlassWall::SideMinusY, 180.0 + Turn },
		{ -1.0, EGlassWall::SidePlusY, Turn },
		{ -1.0, EGlassWall::SideMinusY, -Turn },
	};
	for (const FCase& C : Cases)
	{
		CAPTURE(C.TeamSign);
		CAPTURE(static_cast<int>(C.Wall));
		FPlayerAnimModel Model(&Settings, &Clips, &Lib);
		FPlayerAnimInput In;
		In.TeamSign = C.TeamSign;
		In.bRallyLive = true;
		In.Position = Vec(7.5 * C.TeamSign, C.Wall == EGlassWall::SidePlusY ? 3.5 : -3.5, 0.0);
		In.GlassWall = C.Wall;
		CHECK(AngleDiff(SettleFacing(Model, In, Settings), C.WantYaw) == Approx(0.0).epsilon(1e-6));
	}
}

TEST_CASE("Anim.Glass: running still faces travel, a swing squares up, and no read means square (N1c)")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();

	// No glass read: square to the net, as before N1c.
	{
		FPlayerAnimModel Model(&Settings, &Clips, &Lib);
		FPlayerAnimInput In = BackCourtInput(1.0);
		CHECK(AngleDiff(SettleFacing(Model, In, Settings), 180.0) == Approx(0.0).epsilon(1e-6));
	}

	// Running back to the glass faces travel even with the read set.
	{
		FPlayerAnimModel Model(&Settings, &Clips, &Lib);
		FPlayerAnimInput In = BackCourtInput(1.0);
		In.GlassWall = EGlassWall::Back;
		In.Mode = EMovementMode::TurnAndRun;
		In.Velocity = Vec(4.0, 0.0, 0.0);
		CHECK(AngleDiff(SettleFacing(Model, In, Settings), 0.0) == Approx(0.0).epsilon(1e-6));
	}

	// Side-on while waiting, then square for the swing.
	{
		FPlayerAnimModel Model(&Settings, &Clips, &Lib);
		FPlayerAnimInput In = BackCourtInput(1.0);
		In.GlassWall = EGlassWall::Back;
		In.bHasIdeal = true;
		In.IdealBallPosition = Vec(9.2, -2.5, 0.9);
		CHECK(AngleDiff(SettleFacing(Model, In, Settings), 180.0 + Settings.GlassBackTurnDeg) == Approx(0.0).epsilon(1e-6));
		In.Phase = ESwingPhase::Windup;
		CHECK(AngleDiff(SettleFacing(Model, In, Settings), 180.0) == Approx(0.0).epsilon(1e-6));
	}
}
