// GDD 5.1: releasing the button starts the forward swing, and the racket meets the ball
// the shot's ForwardS later. Release + ForwardS is what gets graded, so the same
// effective timing gives the same grade whatever the swing length (BIOMECHANICS_SHOTS
// 4.2 point 1, 5.3).
#include "doctest.h"
#include "ShotTestData.h"
#include "Shots/PlayerShotController.h"
#include "Shots/TimingGrader.h"
#include "Support/LineBall.h"

#include <cmath>
#include <optional>

using namespace ShotTestData;
using ShotTestSupport::FLineBall;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	FShotLibrary WithDriveForward(double ForwardS)
	{
		FShotLibrary Lib = LoadLibrary();
		FShotParams Drive = Lib.Get(EShotType::Drive);
		Drive.ForwardS = ForwardS;
		Lib.SetShot(Drive);
		return Lib;
	}

	struct FSwing
	{
		ESwingOutcome Outcome = ESwingOutcome::None;
		FShotIntent Intent;
		// Seconds from the release to the outcome tick, and from the ideal moment to it.
		double ReleaseToOutcomeS = -1.0;
		double IdealToOutcomeS = 0.0;
		bool bSawForward = false;
	};

	// A human drive: the ball comes straight at the forehand contact point after a bounce,
	// the button goes down at once and comes up so that release + ForwardS lands
	// ContactOffsetS after the ideal moment (never, when empty: held through).
	FSwing PlayDrive(const FShotLibrary& Lib, std::optional<double> ContactOffsetS, double SideM = -1.0)
	{
		const FPlayerRatings Player = LoadPlayer();
		const FMovementTuning Move = LoadMovement();
		const FShotParams& Drive = Lib.Get(EShotType::Drive);
		const FVec3 PlayerAt = Vec(6.0, 0.0, 0.0);
		FPlayerShotController P(0, 1.0, &Player, &Lib, &Move, PlayerAt, 9);
		const double Side = SideM >= 0.0 ? SideM : Drive.ContactSideFor(true);
		FVec3 Start = Add(PlayerAt, Add(Scale(RightAxis(1.0), Player.ForehandSign() * Side), Scale(ForwardAxis(1.0), 1.0)));
		Start.Z = Drive.PreferredContactHeightM;
		FLineBall Ball(Start, Scale(ForwardAxis(1.0), -0.5), 1);
		FSwing Out;
		double ReleasedAt = -1.0;
		double Now = 0.0;
		for (int Step = 0; Step < 1200 && Out.Outcome == ESwingOutcome::None; ++Step)
		{
			FPlayerInputFrame In;
			In.bDrive = ReleasedAt < 0.0;
			if (ContactOffsetS && P.GetPhase() == ESwingPhase::Windup && P.HasIdealMoment()
				&& P.ForwardSwingS() - P.TimeToIdealS() >= *ContactOffsetS - 1e-9)
			{
				In.bDrive = false;
			}
			const double IdealAt = Now + P.TimeToIdealS();
			const ESwingPhase Before = P.GetPhase();
			const FSwingEvents E = P.Tick(In, Dt, Ball);
			Now += Dt;
			if (Before == ESwingPhase::Windup && P.GetPhase() != ESwingPhase::Windup && ReleasedAt < 0.0)
			{
				ReleasedAt = Now;
			}
			Out.bSawForward = Out.bSawForward || P.GetPhase() == ESwingPhase::Forward;
			if (E.Outcome != ESwingOutcome::None)
			{
				Out.Outcome = E.Outcome;
				Out.Intent = E.Intent;
				Out.ReleaseToOutcomeS = ReleasedAt >= 0.0 ? Now - ReleasedAt : -1.0;
				Out.IdealToOutcomeS = Now - IdealAt;
			}
			Ball.Advance(Dt);
		}
		return Out;
	}
}

TEST_CASE("Shots.Forward: ForwardS sits inside the real forward-swing ranges (BIOMECHANICS_SHOTS 3)")
{
	const FShotLibrary Lib = LoadLibrary();
	struct FRange
	{
		EShotType Type;
		double Min, Max;
	};
	const FRange Ranges[] = {
		{ EShotType::Drive, 0.18, 0.30 },
		{ EShotType::Volley, 0.08, 0.15 },
		{ EShotType::Lob, 0.20, 0.30 },
		{ EShotType::Bandeja, 0.20, 0.30 },
		{ EShotType::Vibora, 0.15, 0.25 },
		{ EShotType::Smash, 0.20, 0.30 },
		{ EShotType::Chiquita, 0.15, 0.25 },
		{ EShotType::ServeFlat, 0.20, 0.35 },
		{ EShotType::ServeKick, 0.20, 0.35 },
	};
	for (const FRange& R : Ranges)
	{
		CAPTURE(ShotTypeName(R.Type));
		CHECK(Lib.Get(R.Type).ForwardS >= R.Min);
		CHECK(Lib.Get(R.Type).ForwardS <= R.Max);
	}
}

TEST_CASE("Shots.Forward: the same effective timing gives the same grade with ForwardS 0 and 0.2 (GDD 5.1)")
{
	const FShotLibrary Instant = WithDriveForward(0.0);
	const FShotLibrary Swing = WithDriveForward(0.2);
	for (const double Offset : { -0.15, -0.07, -0.02, 0.0, 0.03, 0.07, 0.12 })
	{
		CAPTURE(Offset);
		const FSwing A = PlayDrive(Instant, Offset);
		const FSwing B = PlayDrive(Swing, Offset);
		REQUIRE(A.Outcome == ESwingOutcome::Hit);
		REQUIRE(B.Outcome == ESwingOutcome::Hit);
		CHECK(A.Intent.TimingGrade == B.Intent.TimingGrade);
		CHECK(std::abs(A.Intent.TimingOffsetS - B.Intent.TimingOffsetS) <= Dt + 1e-9);
		// Within two ticks: the release lands on a tick, and the ideal moment is refined.
		CHECK(std::abs(B.Intent.TimingOffsetS - Offset) <= 2.0 * Dt + 1e-9);
		// The swing really takes ForwardS: contact comes 0.2 s after a release that isn't early.
		CHECK(B.bSawForward);
		if (Offset >= 0.0)
		{
			CHECK(std::abs(B.ReleaseToOutcomeS - 0.2) <= Dt + 1e-9);
		}
	}
	CHECK(PlayDrive(Swing, 0.0).Intent.TimingGrade == ETimingGrade::Perfect);
	CHECK(PlayDrive(Swing, 0.12).Intent.TimingGrade == ETimingGrade::Late);
	CHECK(PlayDrive(Swing, -0.15).Intent.TimingGrade == ETimingGrade::Early);
}

TEST_CASE("Shots.Forward: holding through the window still makes contact on the late edge (GDD 5.1)")
{
	const FPlayerRatings Player = LoadPlayer();
	for (const double ForwardS : { 0.0, 0.2 })
	{
		CAPTURE(ForwardS);
		const FShotLibrary Lib = WithDriveForward(ForwardS);
		const FShotParams& Drive = Lib.Get(EShotType::Drive);
		const double Rating = std::fmin(Player.Unit(Drive.ForehandRating), Player.Unit(Drive.BackhandRating));
		const double LateEdge = FTimingGrader::LateEdgeS(Drive, Rating, Lib.Model);
		const FSwing S = PlayDrive(Lib, std::nullopt);
		REQUIRE(S.Outcome == ESwingOutcome::Hit);
		CHECK(S.Intent.TimingGrade == ETimingGrade::Late);
		// The last tick that still lands inside the window.
		CHECK(S.Intent.TimingOffsetS <= LateEdge + 1e-9);
		CHECK(S.Intent.TimingOffsetS >= LateEdge - Dt);
	}
}

TEST_CASE("Shots.Forward: a far-early release is still a shadow swing, a ball out of reach still a whiff")
{
	const FShotLibrary Lib = WithDriveForward(0.2);
	const double ShadowLimitS = Lib.Model.ShadowSwingWindows * Lib.Get(EShotType::Drive).EarlyLateWindowMs / 1000.0;
	// Release + ForwardS lands past the shadow limit before the ideal moment.
	const FSwing Shadow = PlayDrive(Lib, -(ShadowLimitS + 0.05));
	CHECK(Shadow.Outcome == ESwingOutcome::ShadowSwing);
	// Just inside it, the racket waits for the ball: an early hit.
	const FSwing Early = PlayDrive(Lib, -(ShadowLimitS - 0.05));
	CHECK(Early.Outcome == ESwingOutcome::Hit);
	CHECK(Early.Intent.TimingGrade == ETimingGrade::Miss);

	// Far to the side, out of reach: the forward swing runs, then misses at contact.
	const FSwing Wide = PlayDrive(Lib, 0.0, 3.0);
	CHECK(Wide.Outcome == ESwingOutcome::Whiff);
	CHECK(std::abs(Wide.ReleaseToOutcomeS - 0.2) <= Dt + 1e-9);
}
