// GDD 5.2: each shot has its own ideal contact point (BIOMECHANICS_SHOTS 5.1), and the
// overheads meet the ball higher for a taller player (5.2). A slow straight-line ball
// lets the tests put the ball exactly where they want it at the contact moment.
#include "doctest.h"
#include "ShotTestData.h"
#include "Support/LineBall.h"
#include "AI/AiEngineBridge.h"
#include "AI/AiInterceptPlanner.h"
#include "AI/AiTeamFrame.h"
#include "Shots/PlayerShotController.h"
#include "Support/AiTestData.h"

#include <cmath>

using namespace ShotTestData;
using ShotTestSupport::FLineBall;
using doctest::Approx;

namespace
{
	constexpr double Dt = 1.0 / 120.0;
	// Slow enough that 0.05 m along the path is 100 ms, outside the Perfect window.
	constexpr double BallSpeedMps = 0.5;
	// Where every shot met the ball before S1 (ShotModel.json's generic point).
	constexpr double OldSideM = 0.8;
	constexpr double OldForwardM = 0.3;

	struct FCase
	{
		EShotType Type;
		bool bBounced;
		double Charge;
	};

	const FCase Cases[] = {
		{ EShotType::Drive, true, 0.5 },
		{ EShotType::Volley, false, 0.5 },
		{ EShotType::Lob, true, 0.5 },
		{ EShotType::Chiquita, true, 0.3 },
		{ EShotType::Bandeja, false, 0.5 },
		{ EShotType::Vibora, false, 0.5 },
		{ EShotType::Smash, false, 1.0 },
	};

	struct FContact
	{
		bool bHit = false;
		FShotIntent Intent;
		// The ball relative to the player at contact: beside (toward Side) and in front.
		double SideM = 0.0;
		double ForwardM = 0.0;
	};

	// Team A player at (6, 0) facing -X. The ball comes straight at them, Side * SideM
	// beside them at the shot's preferred height, and the AI swing releases OffsetS
	// after the controller's ideal moment.
	FContact PlayAlong(const FShotLibrary& Lib, const FPlayerRatings& Player, const FCase& C, double Side, double SideM, double OffsetS)
	{
		const FMovementTuning Move = LoadMovement();
		const FShotParams& Shot = Lib.Get(C.Type);
		const FVec3 PlayerAt = Vec(6.0, 0.0, 0.0);
		FPlayerShotController P(0, 1.0, &Player, &Lib, &Move, PlayerAt, 3);
		const double H = Shot.PreferredHeightFor(Player.HeightM);
		FVec3 Start = Add(PlayerAt, Add(Scale(RightAxis(1.0), Side * SideM), Scale(ForwardAxis(1.0), 1.2)));
		Start.Z = H;
		FLineBall Ball(Start, Scale(ForwardAxis(1.0), -BallSpeedMps), C.bBounced ? 1 : 0);
		P.RequestAiShot(C.Type, Vec(-7.0, 0.0, 0.0), C.Charge, OffsetS);
		FContact Out;
		for (int Step = 0; Step < 1200 && !Ball.bHit; ++Step)
		{
			const FVec3 Before = Ball.Current().Position;
			P.Tick(FPlayerInputFrame{}, Dt, Ball);
			if (Ball.bHit)
			{
				const FVec3 Rel = Sub(Before, P.GetMotion().GetPosition());
				Out.SideM = DotProduct(Rel, RightAxis(1.0)) * Side;
				Out.ForwardM = DotProduct(Rel, ForwardAxis(1.0));
			}
			Ball.Advance(Dt);
		}
		Out.bHit = Ball.bHit;
		Out.Intent = Ball.Intent;
		return Out;
	}
}

TEST_CASE("Shots.ContactPoint: Shots.json carries the biomechanics notes' points (BIOMECHANICS_SHOTS 5.1)")
{
	const FShotLibrary Lib = LoadLibrary();
	REQUIRE(Lib.Validate().empty());
	struct FWant
	{
		EShotType Type;
		double Side, Forward, SideBack, ForwardBack;
	};
	const FWant Wants[] = {
		{ EShotType::Drive, 0.7, 0.3, 0.6, 0.4 },
		{ EShotType::Volley, 0.5, 0.45, 0.4, 0.45 },
		{ EShotType::Lob, 0.6, 0.3, 0.6, 0.3 },
		{ EShotType::Bandeja, 0.4, 0.4, 0.4, 0.4 },
		{ EShotType::Vibora, 0.5, 0.3, 0.5, 0.3 },
		{ EShotType::Smash, 0.3, 0.35, 0.3, 0.35 },
		{ EShotType::Chiquita, 0.6, 0.3, 0.6, 0.3 },
		{ EShotType::ServeFlat, 0.7, 0.4, 0.7, 0.4 },
		{ EShotType::ServeKick, 0.7, 0.4, 0.7, 0.4 },
	};
	for (const FWant& W : Wants)
	{
		CAPTURE(ShotTypeName(W.Type));
		const FShotParams& P = Lib.Get(W.Type);
		CHECK(P.ContactSideFor(true) == Approx(W.Side));
		CHECK(P.ContactForwardFor(true) == Approx(W.Forward));
		CHECK(P.ContactSideFor(false) == Approx(W.SideBack));
		CHECK(P.ContactForwardFor(false) == Approx(W.ForwardBack));
	}
}

TEST_CASE("Shots.ContactPoint: each shot grades Perfect at its own point, not at the old one (GDD 5.2)")
{
	const FShotLibrary Lib = LoadLibrary();
	const FPlayerRatings Player = LoadPlayer();
	for (const FCase& C : Cases)
	{
		// The drive and the volley have their own backhand points; test both sides.
		for (const double Side : { 1.0, -1.0 })
		{
			if (Side < 0.0 && C.Type != EShotType::Drive && C.Type != EShotType::Volley)
			{
				continue;
			}
			const bool bForehand = Side == Player.ForehandSign();
			const FShotParams& Shot = Lib.Get(C.Type);
			const double OwnSide = Shot.ContactSideFor(bForehand);
			const double OwnForward = Shot.ContactForwardFor(bForehand);
			CAPTURE(ShotTypeName(C.Type));
			CAPTURE(Side);

			// Released on the ideal moment: the ball is at the shot's own point.
			const FContact Own = PlayAlong(Lib, Player, C, Side, OwnSide, 0.0);
			REQUIRE(Own.bHit);
			CHECK(Own.Intent.ShotType == C.Type);
			CHECK(Own.Intent.TimingGrade == ETimingGrade::Perfect);
			CHECK(Own.SideM == Approx(OwnSide).epsilon(0.01));
			CHECK(Own.ForwardM == Approx(OwnForward).epsilon(BallSpeedMps * Dt * 1.5 / OwnForward));

			// The ball on the old line, released when it reaches the old point.
			const FContact Old = PlayAlong(Lib, Player, C, Side, OldSideM, (OwnForward - OldForwardM) / BallSpeedMps);
			REQUIRE(Old.bHit);
			if (std::abs(OwnForward - OldForwardM) > 0.04)
			{
				CHECK(Old.Intent.TimingGrade != ETimingGrade::Perfect);
			}
			// Out of position: more scatter (GDD 5.2's 1 + 0.6 x distance).
			CHECK(Old.Intent.ScatterRadiusM > Own.Intent.ScatterRadiusM);
		}
	}
}

TEST_CASE("Shots.ContactPoint: a taller player meets overheads higher (GDD 5.2, BIOMECHANICS_SHOTS 5.2)")
{
	const FShotLibrary Lib = LoadLibrary();
	FPlayerRatings Short = LoadPlayer();
	FPlayerRatings Tall = Short;
	Short.HeightM = 1.78;
	Tall.HeightM = 1.86;
	for (const EShotType Type : { EShotType::Bandeja, EShotType::Vibora, EShotType::Smash })
	{
		CAPTURE(ShotTypeName(Type));
		const FShotParams& Shot = Lib.Get(Type);
		CHECK(Shot.ContactHeightPerBodyHeight == Approx(1.0));
		// About 0.06 m either side of the 1.82 m reference.
		CHECK(Shot.PreferredHeightFor(Short.HeightM) - Shot.PreferredContactHeightM == Approx(-0.06).epsilon(0.15));
		CHECK(Shot.PreferredHeightFor(Tall.HeightM) - Shot.PreferredContactHeightM == Approx(0.06).epsilon(0.15));
		CHECK(Shot.MaxHeightFor(Tall.HeightM) > Shot.MaxHeightFor(Short.HeightM));

		// The controller picks its ideal moment at that height: a ball dropping
		// straight down at the contact point is met higher by the taller player.
		const FMovementTuning Move = LoadMovement();
		double IdealZ[2] = {};
		for (int Index = 0; Index < 2; ++Index)
		{
			const FPlayerRatings& R = Index == 0 ? Short : Tall;
			const FVec3 PlayerAt = Vec(4.0, 0.0, 0.0);
			FPlayerShotController P(0, 1.0, &R, &Lib, &Move, PlayerAt, 5);
			P.RequestAiShot(Type, Vec(-7.0, 0.0, 0.0), 1.0, 0.0);
			FVec3 Start = P.IdealContactPoint(Shot, R.ForehandSign(), 3.4);
			FLineBall Ball(Start, Vec(0.0, 0.0, -BallSpeedMps), 0);
			P.Tick(FPlayerInputFrame{}, Dt, Ball);
			REQUIRE(P.HasIdealMoment());
			IdealZ[Index] = P.GetIdealBallPosition().Z;
			CHECK(IdealZ[Index] == Approx(Shot.PreferredHeightFor(R.HeightM)).epsilon(0.01));
		}
		CHECK(IdealZ[1] - IdealZ[0] == Approx(0.12).epsilon(0.15));
	}
	// Everything below the head is the same for every body.
	for (const EShotType Type : { EShotType::Drive, EShotType::Volley, EShotType::Lob, EShotType::Chiquita, EShotType::ServeFlat })
	{
		CAPTURE(ShotTypeName(Type));
		const FShotParams& Shot = Lib.Get(Type);
		CHECK(Shot.PreferredHeightFor(Short.HeightM) == Approx(Shot.PreferredHeightFor(Tall.HeightM)));
		CHECK(Shot.MaxHeightFor(Short.HeightM) == Approx(Shot.MaxHeightFor(Tall.HeightM)));
	}
	CHECK(Lib.Get(EShotType::Vibora).PreferredContactHeightM == Approx(2.2));
}

TEST_CASE("Shots.ContactPoint: the serve toss drops at the serve's own point")
{
	const FShotLibrary Lib = LoadLibrary();
	const FPlayerRatings Player = LoadPlayer();
	const FMovementTuning Move = LoadMovement();
	struct FTossPort final : IBallPort
	{
		FBallView Current() const override { return {}; }
		int LastHitterTeam() const override { return 0; }
		std::vector<FBallView> Predict(double, double) const override { return {}; }
		void ApplyShot(const FShotIntent&) override {}
		void ServeToss(const FVec3& InAt, int) override { At = InAt; }
		FVec3 At;
	} Port;
	FPlayerShotController P(0, 1.0, &Player, &Lib, &Move, Vec(9.0, 1.0, 0.0), 1);
	P.TossForServe(Port);
	const FShotParams& Serve = Lib.Get(EShotType::ServeFlat);
	const FVec3 Rel = Sub(Port.At, P.GetMotion().GetPosition());
	CHECK(DotProduct(Rel, RightAxis(1.0)) * Player.ForehandSign() == Approx(Serve.ContactSideFor(true)));
	CHECK(DotProduct(Rel, ForwardAxis(1.0)) == Approx(Serve.ContactForwardFor(true)));
}

TEST_CASE("Ai.Intercept: the AI stands at the shot system's per-shot point (GDD 5.2)")
{
	using namespace Padel::AI;
	const FShotLibrary Lib = LoadLibrary();
	const FAiTactics T = Bridge::WithShotContactPoints(TestData::Tactics(), Lib);
	REQUIRE(T.bHasShotContactPoints);
	struct FKind
	{
		Padel::AI::EContactKind Kind;
		EShotType Shot;
		double Height;
		int Bounces;
	};
	const FKind Kinds[] = {
		{ Padel::AI::EContactKind::Groundstroke, EShotType::Drive, 0.9, 1 },
		{ Padel::AI::EContactKind::Volley, EShotType::Volley, 1.1, 0 },
		{ Padel::AI::EContactKind::Overhead, EShotType::Bandeja, 2.3, 0 },
	};
	const FAiPlayerProfile Profile = TestData::Player(ESide::Right);
	for (const FKind& K : Kinds)
	{
		for (const double Across : { 1.0, -1.0 })
		{
			CAPTURE(ShotTypeName(K.Shot));
			CAPTURE(Across);
			// A ball hanging still, well to one side, with time to spare.
			FTrajectorySample S;
			S.T = 2.0;
			S.Half = 1;
			S.GroundBounces = K.Bounces;
			S.Position = TeamFrame::ToCourt(1, { Across * 1.5, 4.0 });
			S.Position.Z = K.Height;
			FInterceptQuery Query;
			Query.PlayerPosition = TeamFrame::ToCourt(1, { 0.0, 4.0 });
			Query.HalfSign = 1;
			const auto Plan = InterceptPlanner::Plan({ S }, Query, Profile, T);
			REQUIRE(Plan.has_value());
			CHECK(Plan->Kind == K.Kind);
			const bool bForehand = Across * Profile.ForehandSign() > 0.0;
			const FShotParams& Shot = Lib.Get(K.Shot);
			const FAiVec2 Ball = TeamFrame::ToLocal(1, S.Position);
			const FAiVec2 Stand = TeamFrame::ToLocal(1, Plan->StandPoint);
			CHECK((Ball.X - Stand.X) * Across == Approx(Shot.ContactSideFor(bForehand)));
			CHECK(Stand.Y - Ball.Y == Approx(Shot.ContactForwardFor(bForehand)));
		}
	}
}
