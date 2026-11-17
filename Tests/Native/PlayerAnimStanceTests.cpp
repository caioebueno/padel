// P1 padel stances and the head on the ball (docs/animation/MOVEMENT_FLUENCY.md, the motion
// catalogue's section 1): before the serve each of the four stands their own way; in a rally,
// taller with the racket up at the net and lower and wider at the back; relaxed between points;
// the head follows the ball. Checked against the real Data/Animation.json.

#include "doctest.h"
#include "AnimTestData.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "JsonLite.h"
#include "ShotTestData.h"

#include <cmath>
#include <set>
#include <string>

using namespace ShotTestData;
using namespace AnimTestData;
using doctest::Approx;
using Padel::Animation::EServeRole;
using Padel::Animation::EStance;
using Padel::Animation::ELocomotionState;
using Padel::Animation::FAnimSettings;
using Padel::Animation::FPlayerAnimFrame;
using Padel::Animation::FPlayerAnimInput;
using Padel::Animation::FPlayerAnimModel;
using Padel::Animation::FStancePose;
using Padel::Shots::Vec;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	const FStancePose& PoseOf(const FAnimSettings& S, EStance Stance)
	{
		return S.Stances[static_cast<std::size_t>(Stance)];
	}

	// Team A (+X half) faces the net along -X; its right is -Y.
	FPlayerAnimInput TeamA(double X)
	{
		FPlayerAnimInput In;
		In.TeamSign = 1.0;
		In.Position = Vec(X, 2.0, 0.0);
		return In;
	}

	const FPlayerAnimFrame& Settle(FPlayerAnimModel& Model, const FPlayerAnimInput& In, double Seconds = 1.5)
	{
		for (int I = 0; I < static_cast<int>(Seconds / Dt); ++I)
		{
			Model.Step(In, Dt);
		}
		return Model.GetFrame();
	}
}

TEST_CASE("Anim.Stance: every stance is listed once and they differ the way real players stand (P1)")
{
	const JsonLite::FValue Settings = ShotTestData::Row(JsonLite::ParseFile(DataPath("Animation.json")), "default");
	std::set<std::string> Names;
	for (const JsonLite::FValue& Stance : Settings["Stances"].Array)
	{
		EStance Parsed;
		CHECK(Padel::Animation::ParseStance(Stance["Name"].String, Parsed));
		CHECK(Names.insert(Stance["Name"].String).second);
	}
	CHECK(Names.size() == static_cast<std::size_t>(Padel::Animation::NumStances));

	const FAnimSettings S = LoadSettings();
	CHECK(S.Validate().empty());
	const FStancePose& Relaxed = PoseOf(S, EStance::Relaxed);
	const FStancePose& Receive = PoseOf(S, EStance::Receive);
	const FStancePose& Net = PoseOf(S, EStance::Net);
	const FStancePose& Back = PoseOf(S, EStance::Back);
	// Between points the racket hangs and the body is upright.
	CHECK(Relaxed.ArmAlpha == 0.0);
	CHECK(Relaxed.CrouchAlpha == 0.0);
	// The receiver is the lowest and widest stance in the game.
	for (int I = 0; I < Padel::Animation::NumStances; ++I)
	{
		const FStancePose& P = S.Stances[static_cast<std::size_t>(I)];
		CHECK(Receive.DropM >= P.DropM);
		CHECK(Receive.WidenM >= P.WidenM);
		CHECK(Receive.CrouchAlpha >= P.CrouchAlpha);
	}
	// At the net the racket head is up at the chest; at the back it is lower and the body lower and wider.
	CHECK(Net.Racket.UpM > Back.Racket.UpM + 0.15);
	CHECK(Back.CrouchAlpha > Net.CrouchAlpha);
	CHECK(Back.WidenM > Net.WidenM);
	CHECK(Back.TrunkDeg > Net.TrunkDeg);

	FAnimSettings Bad = S;
	Bad.StanceBackFromM = Bad.StanceNetUntilM;
	CHECK_FALSE(Bad.Validate().empty());
	Bad = S;
	Bad.Stances[static_cast<std::size_t>(EStance::Net)].ArmAlpha = 1.5;
	CHECK_FALSE(Bad.Validate().empty());
	Bad = S;
	Bad.HeadLookMaxPitchDeg = -5.0;
	CHECK_FALSE(Bad.Validate().empty());
}

TEST_CASE("Anim.Stance: in a rally, net stance near the net, back stance at the back, a smooth blend between (P1)")
{
	const FAnimSettings S = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&S, &Clips, &Lib);

	FPlayerAnimInput In = TeamA(2.5);
	In.bRallyLive = true;
	const FPlayerAnimFrame& AtNet = Settle(Model, In);
	CHECK(AtNet.Stance == EStance::Net);
	CHECK(AtNet.StancePose.Racket.UpM == Approx(PoseOf(S, EStance::Net).Racket.UpM).epsilon(0.001));
	CHECK(AtNet.Locomotion == ELocomotionState::Ready);
	// The racket is carried where the stance holds it, on the forehand side.
	CHECK(AtNet.RacketPosition.Z == Approx(PoseOf(S, EStance::Net).Racket.UpM).epsilon(0.001));
	CHECK(AtNet.RacketPosition.Y < In.Position.Y);

	// Walk back to the glass over 2 s: no tick jumps, and it ends in the back stance.
	double LastCrouch = AtNet.StancePose.CrouchAlpha;
	double LastUp = AtNet.StancePose.Racket.UpM;
	const int Ticks = static_cast<int>(2.0 / Dt);
	for (int I = 1; I <= Ticks; ++I)
	{
		In.Position = Vec(2.5 + 6.5 * I / Ticks, 2.0, 0.0);
		const FPlayerAnimFrame& F = Model.Step(In, Dt);
		CHECK(std::fabs(F.StancePose.CrouchAlpha - LastCrouch) < 0.01);
		CHECK(std::fabs(F.StancePose.Racket.UpM - LastUp) < 0.01);
		LastCrouch = F.StancePose.CrouchAlpha;
		LastUp = F.StancePose.Racket.UpM;
	}
	const FPlayerAnimFrame& AtBack = Settle(Model, In);
	CHECK(AtBack.Stance == EStance::Back);
	CHECK(AtBack.StancePose.CrouchAlpha == Approx(PoseOf(S, EStance::Back).CrouchAlpha).epsilon(0.001));
	CHECK(AtBack.StancePose.DropM == Approx(PoseOf(S, EStance::Back).DropM).epsilon(0.001));

	// Halfway between the zones, halfway between the poses.
	In.Position = Vec(0.5 * (S.StanceNetUntilM + S.StanceBackFromM), 2.0, 0.0);
	const FPlayerAnimFrame& Mid = Settle(Model, In);
	CHECK(Mid.StancePose.WidenM == Approx(0.5 * (PoseOf(S, EStance::Net).WidenM + PoseOf(S, EStance::Back).WidenM)).epsilon(0.01));

	// The point ends: relaxed, racket down.
	In.bRallyLive = false;
	const FPlayerAnimFrame& After = Settle(Model, In);
	CHECK(After.Stance == EStance::Relaxed);
	CHECK(After.Locomotion == ELocomotionState::Idle);
	CHECK(After.StancePose.ArmAlpha == Approx(0.0).epsilon(0.001));
}

TEST_CASE("Anim.Stance: before the serve the server, the receiver and their partners each stand their own way (P1)")
{
	const FAnimSettings S = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	struct FCase { EServeRole Role; double X; EStance Expect; };
	const FCase Cases[] = {
		{ EServeRole::Server, 8.5, EStance::Serve },
		{ EServeRole::ServerPartner, 2.5, EStance::Net },
		{ EServeRole::Receiver, 9.0, EStance::Receive },
		// The receiver's partner stands back near the service line, even closer to the net.
		{ EServeRole::ReceiverPartner, 6.5, EStance::Back },
	};
	for (const FCase& Case : Cases)
	{
		CAPTURE(static_cast<int>(Case.Role));
		FPlayerAnimModel Model(&S, &Clips, &Lib);
		FPlayerAnimInput In = TeamA(Case.X);
		In.ServeRole = Case.Role;
		const FPlayerAnimFrame& F = Settle(Model, In);
		CHECK(F.Stance == Case.Expect);
		CHECK(F.Locomotion == ELocomotionState::Ready);
		CHECK(F.StancePose.DropM == Approx(PoseOf(S, Case.Expect).DropM).epsilon(0.001));
	}
}

TEST_CASE("Anim.Stance: the head follows the ball, within what the neck can turn (P1)")
{
	const FAnimSettings S = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&S, &Clips, &Lib);
	FPlayerAnimInput In = TeamA(3.0);
	In.bRallyLive = true;
	Settle(Model, In);
	CHECK(Model.GetFrame().HeadTwistDeg == Approx(0.0));
	CHECK(Model.GetFrame().HeadPitchDeg == Approx(0.0));

	// A lob up to the front right (-Y for Team A): the head turns right and looks up; the body stays square.
	In.bBallVisible = true;
	In.BallPosition = Vec(In.Position.X - 3.0, In.Position.Y - 3.0, 5.0);
	const FPlayerAnimFrame& Up = Settle(Model, In, 0.6);
	CHECK(Up.FacingYawDeg == Approx(180.0));
	CHECK(Up.ChestTwistDeg == Approx(0.0));
	CHECK(Up.HeadTwistDeg == Approx(45.0).epsilon(0.02));
	CHECK(Up.HeadPitchDeg > 20.0);
	CHECK(Up.HeadPitchDeg <= S.HeadLookMaxPitchDeg + 1e-9);

	// A ball low at the feet in front: the head looks down.
	In.BallPosition = Vec(In.Position.X - 0.8, In.Position.Y, 0.1);
	const FPlayerAnimFrame& Down = Settle(Model, In, 0.6);
	CHECK(Down.HeadTwistDeg == Approx(0.0).epsilon(0.01));
	CHECK(Down.HeadPitchDeg < -20.0);
	CHECK(Down.HeadPitchDeg >= S.HeadLookMinPitchDeg - 1e-9);

	// A ball right behind: the neck turns only as far as it can.
	In.BallPosition = Vec(In.Position.X + 4.0, In.Position.Y - 0.5, 1.5);
	const FPlayerAnimFrame& Behind = Settle(Model, In, 0.6);
	CHECK(std::fabs(Behind.HeadTwistDeg) <= std::fmin(S.HeadLookMaxYawDeg, S.FacingMaxHeadTwistDeg) + 1e-6);
	CHECK(std::fabs(Behind.HeadTwistDeg) > 45.0);

	// Before the serve the server's partner faces the receivers, not the ball behind.
	FPlayerAnimModel Partner(&S, &Clips, &Lib);
	FPlayerAnimInput P = TeamA(2.5);
	P.ServeRole = EServeRole::ServerPartner;
	P.bBallVisible = true;
	P.BallPosition = Vec(8.5, -2.0, 0.9);
	const FPlayerAnimFrame& Waits = Settle(Partner, P);
	CHECK(Waits.HeadTwistDeg == Approx(0.0));
	CHECK(Waits.HeadPitchDeg == Approx(0.0));
}
