// GDD 4 (shot selection), 5.1 (timing), 5.3 (formulas) and 7 (aim).
#include "doctest.h"
#include "ShotTestData.h"
#include "Shots/AimMapper.h"
#include "Shots/ShotResolver.h"
#include "Shots/ShotSelector.h"
#include "Shots/TimingGrader.h"

using namespace ShotTestData;
using doctest::Approx;

namespace
{
	struct FFixture
	{
		FShotLibrary Lib = LoadLibrary();
		FPlayerRatings Player = LoadPlayer();

		FShotContext Context(EShotButton Button, double Height, bool bBounced = true)
		{
			FShotContext C;
			C.Player = &Player;
			C.TeamSign = 1.0;
			C.Button = Button;
			C.ContactPosition = Vec(8.0, 0.0, Height);
			C.ContactSide = Player.ForehandSign();
			C.bBallBounced = bBounced;
			C.Charge = 0.5;
			C.TopSpeedMps = 6.0;
			return C;
		}
	};
}

TEST_CASE("Shots.Timing: grades follow the GDD 5.1 windows")
{
	FFixture F;
	const FShotParams& Drive = F.Lib.Get(EShotType::Drive);
	// A rating that gives a window scale of exactly 1 is not on the 1..99 grid, so check with the scale.
	const double R = 0.5;
	const double S = FTimingGrader::WindowScale(R, F.Lib.Model);
	CHECK(FTimingGrader::Grade(0.0, Drive, R, F.Lib.Model) == ETimingGrade::Perfect);
	CHECK(FTimingGrader::Grade(0.039 * S, Drive, R, F.Lib.Model) == ETimingGrade::Perfect);
	CHECK(FTimingGrader::Grade(-0.080 * S, Drive, R, F.Lib.Model) == ETimingGrade::Good);
	CHECK(FTimingGrader::Grade(-0.150 * S, Drive, R, F.Lib.Model) == ETimingGrade::Early);
	CHECK(FTimingGrader::Grade(0.150 * S, Drive, R, F.Lib.Model) == ETimingGrade::Late);
	CHECK(FTimingGrader::Grade(0.200 * S, Drive, R, F.Lib.Model) == ETimingGrade::Miss);
}

TEST_CASE("Shots.Timing: rating 99 widens the windows by 35%, rating 1 narrows by 20%")
{
	FFixture F;
	CHECK(FTimingGrader::WindowScale(1.0, F.Lib.Model) == Approx(1.35));
	CHECK(FTimingGrader::WindowScale(1.0 / 99.0, F.Lib.Model) == Approx(0.80));
	const FShotParams& Drive = F.Lib.Get(EShotType::Drive);
	// 50 ms is Good for a weak player and Perfect for a great one.
	CHECK(FTimingGrader::Grade(0.050, Drive, 1.0 / 99.0, F.Lib.Model) == ETimingGrade::Good);
	CHECK(FTimingGrader::Grade(0.050, Drive, 1.0, F.Lib.Model) == ETimingGrade::Perfect);
	CHECK(FTimingGrader::LateEdgeS(Drive, 1.0, F.Lib.Model) == Approx(0.160 * 1.35));
}

TEST_CASE("Shots.Select: drive button plays drive after the bounce and volley before it")
{
	FFixture F;
	CHECK(FShotSelector::Select(F.Context(EShotButton::Drive, 0.9, true), F.Lib).ShotType == EShotType::Drive);
	CHECK(FShotSelector::Select(F.Context(EShotButton::Drive, 1.1, false), F.Lib).ShotType == EShotType::Volley);
}

TEST_CASE("Shots.Select: touch modifier on a low ball is a chiquita")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Drive, 0.6);
	C.bModTouch = true;
	CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::Chiquita);
	C.ContactPosition.Z = 1.4;
	CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::Drive);
}

TEST_CASE("Shots.Select: lob below the shoulder, bandeja above it")
{
	FFixture F;
	CHECK(FShotSelector::Select(F.Context(EShotButton::Lob, 0.8), F.Lib).ShotType == EShotType::Lob);
	CHECK(FShotSelector::Select(F.Context(EShotButton::Lob, 2.2), F.Lib).ShotType == EShotType::Bandeja);
}

TEST_CASE("Shots.Select: GDD 4 overhead context rules")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Overhead, 2.5);

	SUBCASE("tap or half charge is a bandeja")
	{
		C.Charge = 0.5;
		C.bSet = true;
		CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::Bandeja);
	}
	SUBCASE("full charge and set is a smash")
	{
		C.Charge = 1.0;
		C.bSet = true;
		CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::Smash);
	}
	SUBCASE("full charge but not set is a bandeja with extra error")
	{
		C.Charge = 1.0;
		C.bSet = false;
		const FShotChoice Choice = FShotSelector::Select(C, F.Lib);
		CHECK(Choice.ShotType == EShotType::Bandeja);
		CHECK(Choice.ExtraErrorMult == Approx(F.Lib.Model.UnsetFullChargeErrorMult));
	}
	SUBCASE("RB on the forehand is a víbora")
	{
		C.bModAttack = true;
		CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::Vibora);
	}
	SUBCASE("RB on the backhand is a less accurate bandeja")
	{
		C.bModAttack = true;
		C.ContactSide = -F.Player.ForehandSign();
		const FShotChoice Choice = FShotSelector::Select(C, F.Lib);
		CHECK(Choice.ShotType == EShotType::Bandeja);
		CHECK(Choice.ExtraErrorMult == Approx(F.Lib.Model.BackhandOverheadErrorMult));
	}
	SUBCASE("a low ball falls back to the groundstroke")
	{
		C.ContactPosition.Z = 0.9;
		CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::Drive);
	}
}

TEST_CASE("Shots.Select: serve is flat by default and kick with RB")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Drive, 0.8);
	C.bServe = true;
	CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::ServeFlat);
	C.bModAttack = true;
	CHECK(FShotSelector::Select(C, F.Lib).ShotType == EShotType::ServeKick);
}

TEST_CASE("Shots.Aim: up is deep, down is short, no input is the safe middle, for both teams")
{
	FFixture F;
	const FShotParams& Drive = F.Lib.Get(EShotType::Drive);
	const FVec3 Neutral = FAimMapper::Target({ 0.0, 0.0 }, Drive, 1.0);
	CHECK(Neutral.X == Approx(-Drive.DepthNeutralM));
	CHECK(Neutral.Y == Approx(0.0));
	CHECK(FAimMapper::Target({ 0.0, 1.0 }, Drive, 1.0).X == Approx(-Drive.DepthDeepM));
	CHECK(FAimMapper::Target({ 0.0, -1.0 }, Drive, 1.0).X == Approx(-Drive.DepthShortM));
	// Team A faces -X, so its right is -Y; Team B's right is +Y.
	CHECK(FAimMapper::Target({ 1.0, 0.0 }, Drive, 1.0).Y == Approx(-Drive.LateralMaxM));
	CHECK(FAimMapper::Target({ 1.0, 0.0 }, Drive, -1.0).Y == Approx(Drive.LateralMaxM));
	CHECK(FAimMapper::Target({ 0.0, 1.0 }, Drive, -1.0).X == Approx(Drive.DepthDeepM));
}

TEST_CASE("Shots.Aim: serves go diagonally into the box")
{
	FFixture F;
	const FShotParams& Serve = F.Lib.Get(EShotType::ServeFlat);
	for (const double ServerY : { -2.0, 2.0 })
	{
		const FVec3 T = FAimMapper::ServeTarget({ 0.0, 0.0 }, Serve, 1.0, ServerY);
		CHECK(T.X < 0.0);
		CHECK(T.X > -Padel::Court::ServiceLineX);
		CHECK(Sign(T.Y) == -Sign(ServerY));
		CHECK(std::abs(T.Y) < Padel::Court::HalfWidth);
	}
}

TEST_CASE("Shots.Resolve: perfect timing scatters less and hits faster than late")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Drive, 0.9);
	FPadelRng Rng(1);
	C.TimingOffsetS = 0.0;
	const FShotIntent Perfect = FShotResolver::Resolve(C, F.Lib, Rng);
	C.TimingOffsetS = 0.15;
	const FShotIntent Late = FShotResolver::Resolve(C, F.Lib, Rng);
	CHECK(Perfect.TimingGrade == ETimingGrade::Perfect);
	CHECK(Late.TimingGrade == ETimingGrade::Late);
	CHECK(Late.ScatterRadiusM == Approx(Perfect.ScatterRadiusM * F.Lib.Model.EarlyLateErrorMult / F.Lib.Model.PerfectErrorMult));
	CHECK(Perfect.SpeedMps == Approx(Late.SpeedMps * (1.0 + F.Lib.Model.PerfectSpeedBonus)));
}

TEST_CASE("Shots.Resolve: early pulls across the body, late pushes toward the contact side")
{
	FFixture F;
	F.Lib.Shots[static_cast<int>(EShotType::Drive)].BaseErrorM = 1e-9; // isolate the pull from scatter
	FShotContext C = F.Context(EShotButton::Drive, 0.9);
	C.ContactSide = 1.0; // ball on the player's right
	FPadelRng Rng(2);
	C.TimingOffsetS = -0.15;
	const FShotIntent Early = FShotResolver::Resolve(C, F.Lib, Rng);
	C.TimingOffsetS = 0.15;
	const FShotIntent Late = FShotResolver::Resolve(C, F.Lib, Rng);
	const FVec3 Right = RightAxis(1.0);
	CHECK(DotProduct(Sub(Early.Target, Early.AimedTarget), Right) == Approx(-F.Lib.Model.EarlyLatePullM).epsilon(0.01));
	CHECK(DotProduct(Sub(Late.Target, Late.AimedTarget), Right) == Approx(F.Lib.Model.EarlyLatePullM).epsilon(0.01));
}

TEST_CASE("Shots.Resolve: a miss is a weak mishit")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Drive, 0.9);
	C.TimingOffsetS = -0.3;
	FPadelRng Rng(3);
	const FShotIntent I = FShotResolver::Resolve(C, F.Lib, Rng);
	CHECK(I.ShotType == EShotType::Mishit);
	CHECK(I.TimingGrade == ETimingGrade::Miss);
	CHECK(I.SpeedKmh() <= F.Lib.Model.MishitSpeedMaxKmh + 1e-9);
}

TEST_CASE("Shots.Resolve: GDD 5.3 multipliers")
{
	FFixture F;
	const FShotModel& M = F.Lib.Model;

	SUBCASE("power mult is 1 up to 80% charge and 1.6 at full")
	{
		CHECK(FShotResolver::PowerMult(0.5, M) == Approx(1.0));
		CHECK(FShotResolver::PowerMult(0.8, M) == Approx(1.0));
		CHECK(FShotResolver::PowerMult(0.9, M) == Approx(1.3));
		CHECK(FShotResolver::PowerMult(1.0, M) == Approx(1.6));
	}
	SUBCASE("stamina factor is 1 above half, 0.85 at zero")
	{
		CHECK(FShotResolver::StaminaFactor(0.7, M) == Approx(1.0));
		CHECK(FShotResolver::StaminaFactor(0.25, M) == Approx(0.925));
		CHECK(FShotResolver::StaminaFactor(0.0, M) == Approx(0.85));
	}
	SUBCASE("position limits power to 1 - 0.3 d, at least 0.5")
	{
		CHECK(FShotResolver::PositionPowerLimit(0.0, M) == Approx(1.0));
		CHECK(FShotResolver::PositionPowerLimit(1.0, M) == Approx(0.7));
		CHECK(FShotResolver::PositionPowerLimit(3.0, M) == Approx(0.5));
	}
	SUBCASE("pressure point costs 0.4 x (1 - composure)")
	{
		FShotContext C = F.Context(EShotButton::Drive, 0.9);
		C.bPressurePoint = true;
		CHECK(FShotResolver::PressureMult(C, M) == Approx(1.0 + 0.4 * (1.0 - F.Player.Composure / 99.0)));
	}
	SUBCASE("running at full speed costs 1.3 unless split-stepped")
	{
		FShotContext C = F.Context(EShotButton::Drive, 0.9);
		C.PlayerVelocity = Vec(0.0, 6.0, 0.0);
		CHECK(FShotResolver::RunningMult(C, M) == Approx(1.3));
		C.bSplitStepped = true;
		CHECK(FShotResolver::RunningMult(C, M) == Approx(1.0));
	}
}

TEST_CASE("Shots.Resolve: off-the-glass shots scatter more for a poor wall player")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Drive, 0.9);
	FPadelRng Rng(4);
	const double Clean = FShotResolver::Resolve(C, F.Lib, Rng).ScatterRadiusM;
	C.bOffGlass = true;
	F.Player.WallPlay = 1;
	const FShotIntent Glass = FShotResolver::Resolve(C, F.Lib, Rng);
	CHECK(Glass.bWallShot);
	CHECK(Glass.ScatterRadiusM == Approx(Clean * F.Lib.Model.WallScatterAt1).epsilon(0.02));
}

TEST_CASE("Shots.Resolve: Power raises speed, the main rating lowers scatter")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Drive, 0.9);
	FPadelRng Rng(5);
	F.Player.Power = 1;
	F.Player.Drive = 1;
	const FShotIntent Weak = FShotResolver::Resolve(C, F.Lib, Rng);
	F.Player.Power = 99;
	F.Player.Drive = 99;
	const FShotIntent Strong = FShotResolver::Resolve(C, F.Lib, Rng);
	CHECK(Strong.SpeedMps > Weak.SpeedMps);
	CHECK(Strong.ScatterRadiusM < Weak.ScatterRadiusM);
	CHECK(Strong.Spin.X > Weak.Spin.X);
}

TEST_CASE("Shots.Resolve: same seed, same shot")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Drive, 0.9);
	FPadelRng A(42);
	FPadelRng B(42);
	const FShotIntent First = FShotResolver::Resolve(C, F.Lib, A);
	const FShotIntent Second = FShotResolver::Resolve(C, F.Lib, B);
	CHECK(First.Target.X == Second.Target.X);
	CHECK(First.Target.Y == Second.Target.Y);
}

TEST_CASE("Shots.Resolve: lob uses the apex range by charge")
{
	FFixture F;
	FShotContext C = F.Context(EShotButton::Lob, 0.8);
	FPadelRng Rng(6);
	C.Charge = 0.0;
	CHECK(FShotResolver::Resolve(C, F.Lib, Rng).ApexHeightM == Approx(6.0));
	C.Charge = 1.0;
	CHECK(FShotResolver::Resolve(C, F.Lib, Rng).ApexHeightM == Approx(8.0));
}
