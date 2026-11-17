// A3: the AI chases a ball from off its spot and gets there on the plan's time. The
// ball-machine tests start the bot on its stand point (#45, M2), so this covers the chase:
// a deep drive and a deep lob, with the taker starting 1.5 to 2 m off the spot it plans
// to stand on, moved by the real FPlayerMotion the way the match runner moves it.
#include "doctest.h"
#include "ShotTestData.h"
#include "AI/AiEngineBridge.h"
#include "AI/AiMotionModel.h"
#include "AI/AiTeamController.h"
#include "AI/AiTeamFrame.h"
#include "Shots/PlayerMotion.h"
#include "Support/AiTestData.h"

#include <cmath>
#include <optional>

using namespace Padel::AI;
using Padel::Shots::FPlayerMotion;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	struct FIncoming
	{
		const char* Name;
		EAiShot Shot;
		FAiBall Ball;
	};

	// From the opponents' baseline (-X) into team A's half (+X), both landing deep.
	const FIncoming Balls[] = {
		{ "deep drive", EAiShot::Drive, { { -8.5, -1.0, 1.0 }, { 17.0, 0.7, 4.5 }, {} } },
		{ "deep lob", EAiShot::Lob, { { -8.5, 1.0, 1.0 }, { 11.0, -0.6, 10.0 }, {} } },
	};

	FAiTeamSetup Pair()
	{
		FAiTeamSetup S;
		S.HalfSign = 1;
		const FAiPlayerProfile Profile = Bridge::ProfileFromRatings(ShotTestData::LoadPlayer());
		S.Profiles = { Profile, Profile };
		S.Profiles[LeftSlot].PreferredSide = ESide::Left;
		S.Seed = 9;
		const Padel::Shots::FMovementTuning Move = ShotTestData::LoadMovement();
		S.AccelFalloff = Move.AccelFalloff;
		S.BrakeMult = Move.BrakeMult;
		return S;
	}

	struct FChase
	{
		int Taker = -1;
		bool bPlanned = false;
		bool bReachable = false;
		FAiVec StandPoint;
		FAiVec ContactBall;
		// How close the ball has come to the plan's contact point (the contact moment).
		double BallToContactM = 1e9;
		// How far the taker is from the plan's stand point at its contact time.
		double MissM = 1e9;
	};

	// Plays the incoming ball against the pair, both players on the real motion.
	FChase Play(const FIncoming& In, const std::array<FAiVec, 2>& Start)
	{
		static const Padel::Shots::FMovementTuning Move = ShotTestData::LoadMovement();
		static const Padel::Shots::FPlayerRatings Ratings = ShotTestData::LoadPlayer();
		const IAiBallModel& Model = TestData::EngineModel();
		FAiTeamController Team(Pair(), TestData::Tactics(), TestData::Difficulty("Medium"), Model);
		Team.OnPointStarted(false);
		std::array<FPlayerMotion, 2> Motion = { FPlayerMotion(&Ratings, &Move, 1.0, { Start[0].X, Start[0].Y, 0.0 }),
			FPlayerMotion(&Ratings, &Move, 1.0, { Start[1].X, Start[1].Y, 0.0 }) };
		Team.OnBallHit(0.0, -1, In.Shot, In.Ball.Position, FAiVec{ 8.0, 0.0, 0.0 });

		FChase Out;
		FAiBall Ball = In.Ball;
		for (int Step = 1; Step < 4 * 120; ++Step)
		{
			const double Time = Step * Dt;
			Model.Step(Ball, Dt);
			FAiWorldView View;
			View.Time = Time;
			View.Ball = Ball;
			View.OpponentPositions = { FAiVec{ -8.5, 2.25, 0.0 }, FAiVec{ -8.5, -2.25, 0.0 } };
			for (int Slot = 0; Slot < 2; ++Slot)
			{
				const auto& P = Motion[Slot].GetPosition();
				const auto& V = Motion[Slot].GetVelocity();
				View.OwnPositions[Slot] = { P.X, P.Y, 0.0 };
				View.OwnVelocities[Slot] = { V.X, V.Y, 0.0 };
				View.OwnMotion[Slot] = AiMotionModel::ParamsOf(Motion[Slot], Move);
			}
			const auto Intents = Team.Tick(View);
			const int Taker = Team.GetTaker();
			if (Taker >= 0 && Team.GetPlan(Taker))
			{
				const FInterceptPlan& Plan = *Team.GetPlan(Taker);
				Out.Taker = Taker;
				Out.bPlanned = true;
				Out.bReachable = Plan.bReachable;
				FAiVec Moved = Plan.Sample.Position - Out.ContactBall;
				if (Moved.Length2D() > 0.01 || std::abs(Moved.Z) > 0.01)
				{
					// A new read: time the new contact from here.
					Out.BallToContactM = 1e9;
				}
				Out.StandPoint = Plan.StandPoint;
				Out.ContactBall = Plan.Sample.Position;
			}
			for (int Slot = 0; Slot < 2; ++Slot)
			{
				if (Intents[Slot])
				{
					const FAiIntent& I = *Intents[Slot];
					Motion[Slot].Step({ I.Move.X, I.Move.Y }, I.bSprint, Dt, 1.0, I.PlannedDistanceM);
				}
			}
			if (Out.bPlanned && Out.Taker >= 0)
			{
				const FAiVec ToContact = Ball.Position - Out.ContactBall;
				const double BallDistance = std::sqrt(ToContact.X * ToContact.X + ToContact.Y * ToContact.Y + ToContact.Z * ToContact.Z);
				if (BallDistance < Out.BallToContactM)
				{
					Out.BallToContactM = BallDistance;
					const auto& P = Motion[Out.Taker].GetPosition();
					FAiVec Off = Out.StandPoint - FAiVec{ P.X, P.Y, 0.0 };
					Off.Z = 0.0;
					Out.MissM = Off.Length2D();
				}
			}
		}
		return Out;
	}

	// The taker's stand point when both start on their usual back spots.
	FChase FromHome(const FIncoming& In)
	{
		const std::array<FAiVec, 2> Home = { TeamFrame::ToCourt(1, FAiVec2{ 2.0, 8.0 }), TeamFrame::ToCourt(1, FAiVec2{ -2.0, 8.0 }) };
		return Play(In, Home);
	}
}

TEST_CASE("Ai.OffSpotChase: from 1.5 to 2 m off its spot the taker still gets there")
{
	const double Arrive = TestData::Tactics().ArriveRadiusM;
	for (const FIncoming& In : Balls)
	{
		CAPTURE(In.Name);
		const FChase Base = FromHome(In);
		REQUIRE(Base.bPlanned);
		REQUIRE(Base.bReachable);
		const FAiVec2 Spot = TeamFrame::ToLocal(1, Base.StandPoint);
		// Off to either side, in front, behind and on the diagonal (team frame: X right, Y depth).
		const FAiVec2 Offsets[] = { { 1.5, 0.0 }, { -1.5, 0.0 }, { 0.0, -1.5 }, { 0.0, 1.2 }, { 1.4, -1.4 }, { -1.4, -1.4 }, { 2.0, 0.0 }, { -2.0, 0.0 } };
		for (const FAiVec2& Off : Offsets)
		{
			CAPTURE(Off.X);
			CAPTURE(Off.Y);
			std::array<FAiVec2, 2> Local = { FAiVec2{ 2.0, 8.0 }, FAiVec2{ -2.0, 8.0 } };
			Local[Base.Taker] = { Spot.X + Off.X, std::fmin(Spot.Y + Off.Y, 9.4) };
			const FChase Run = Play(In, { TeamFrame::ToCourt(1, Local[0]), TeamFrame::ToCourt(1, Local[1]) });
			REQUIRE(Run.bPlanned);
			CHECK(Run.bReachable);
			// At the contact the plan timed, the taker is on its stand point.
			CHECK(Run.BallToContactM < 0.2);
			CHECK(Run.MissM <= Arrive + 0.1);
		}
	}
}
