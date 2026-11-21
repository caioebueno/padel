// A3 (movement doc 3.2, 5.2): the intercept planner times a run with the motion the player
// will make. The planned arrival must match FPlayerMotion stepped the way the AI drives it,
// in every direction and over short and long moves.
#include "doctest.h"
#include "ShotTestData.h"
#include "AI/AiMotionModel.h"
#include "AI/AiTeamFrame.h"
#include "Shots/PlayerMotion.h"

#include <algorithm>
#include <cmath>

using namespace ShotTestData;
using namespace Padel::AI;
using Padel::Shots::FPlayerMotion;
using Padel::Shots::FPlayerRatings;
using Padel::Shots::FMovementTuning;

namespace
{
	constexpr double Dt = 1.0 / 120.0;
	constexpr double ArriveM = 0.15;

	FAiVec ToAi(const Padel::Shots::FVec3& V) { return { V.X, V.Y, V.Z }; }

	// The AI's chase (FAiTeamController::MoveTo, flat out) on the real motion, until the
	// player is within the arrive radius. Returns the seconds it took.
	double RunActual(FPlayerMotion Motion, const FMovementTuning& Tuning, int HalfSign, const FAiVec& Target)
	{
		const FAiMotionParams P = AiMotionModel::ParamsOf(Motion, Tuning);
		for (int Step = 0; Step < 1200; ++Step)
		{
			FAiVec Delta = Target - ToAi(Motion.GetPosition());
			Delta.Z = 0.0;
			const double Distance = Delta.Length2D();
			if (Distance <= ArriveM)
			{
				return Step * Dt;
			}
			double Speed = 1.0;
			bool bSprint = true;
			AiMotionModel::LimitToStop(P, Distance, Speed, bSprint);
			const FAiVec2 Stick = TeamFrame::MoveVector(HalfSign, Delta * (Speed / Distance));
			Motion.Step({ Stick.X, Stick.Y }, bSprint, Dt, 1.0, Distance);
		}
		return 1e9;
	}

	double Planned(const FPlayerMotion& Motion, const FMovementTuning& Tuning, int HalfSign, const FAiVec& Target)
	{
		FAiRunQuery Q;
		Q.From = ToAi(Motion.GetPosition());
		Q.Velocity = ToAi(Motion.GetVelocity());
		Q.To = Target;
		Q.HalfSign = HalfSign;
		Q.ArriveRadiusM = ArriveM;
		Q.bSprint = true;
		Q.MaxTimeS = 10.0;
		return AiMotionModel::ArrivalTime(AiMotionModel::ParamsOf(Motion, Tuning), Q);
	}
}

TEST_CASE("Ai.MotionModel: planned arrival matches the run within 0.05 s, every direction, 0.5 to 8 m")
{
	const FMovementTuning Tuning = LoadMovement();
	FPlayerRatings Ratings;
	// Stamina high enough that sprint never runs out over one run.
	Ratings.Set(Padel::Shots::ERating::Stamina, 90);
	const double Distances[] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0 };
	// Team-frame stick directions: forward, back, right, left, and diagonals either side of
	// the 45-degree line where FPlayerMotion switches between the forward/back and the
	// sideways gait (37 and 53 degrees off the axis).
	const FAiVec2 Dirs[] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 },
		{ 0.6, 0.8 }, { -0.8, 0.6 }, { 0.8, -0.6 }, { -0.6, -0.8 },
		{ 0.8, 0.6 }, { -0.6, 0.8 }, { 0.6, -0.8 }, { -0.8, -0.6 } };
	double Worst = 0.0;
	int Runs = 0;
	for (int HalfSign : { 1, -1 })
	{
		for (const FAiVec2& Dir : Dirs)
		{
			for (double Distance : Distances)
			{
				const FAiVec2 StickMove = { Dir.X * Distance, Dir.Y * Distance };
				// Start where the whole move stays clear of the wall and net margins.
				const double StartDepth = Dir.Y > 0.1 ? 9.0 : (Dir.Y < -0.1 ? 1.0 : 5.0);
				const double StartAcross = Dir.X > 0.1 ? -4.4 : (Dir.X < -0.1 ? 4.4 : 0.0);
				const double Lateral = std::clamp(StartAcross, -4.4 + 0.0, 4.4);
				const FAiVec Start = TeamFrame::ToCourt(HalfSign, FAiVec2{ Lateral, StartDepth });
				const FAiVec Target = Start + TeamFrame::CourtDirection(HalfSign, StickMove);
				FPlayerMotion Motion(&Ratings, &Tuning, HalfSign, { Start.X, Start.Y, 0.0 });
				const double Plan = Planned(Motion, Tuning, HalfSign, Target);
				const double Actual = RunActual(Motion, Tuning, HalfSign, Target);
				CAPTURE(HalfSign);
				CAPTURE(Dir.X);
				CAPTURE(Dir.Y);
				CAPTURE(Distance);
				CHECK(std::abs(Plan - Actual) <= 0.05);
				Worst = std::max(Worst, std::abs(Plan - Actual));
				++Runs;
			}
		}
	}
	MESSAGE("worst planned-vs-actual arrival over ", Runs, " runs: ", Worst, " s");
}

TEST_CASE("Ai.MotionModel: a running start the wrong way is planned too")
{
	const FMovementTuning Tuning = LoadMovement();
	const FPlayerRatings Ratings;
	FPlayerMotion Motion(&Ratings, &Tuning, 1.0, { 6.0, 0.0, 0.0 });
	// Get going toward the left for half a second, then chase a spot 3 m to the right.
	for (int Step = 0; Step < 60; ++Step)
	{
		Motion.Step({ -1.0, 0.0 }, false, Dt, 1.0, 4.0);
	}
	const FAiVec Target = ToAi(Motion.GetPosition()) + TeamFrame::CourtDirection(1, FAiVec2{ 3.0, 0.0 });
	const double Plan = Planned(Motion, Tuning, 1, Target);
	const double Actual = RunActual(Motion, Tuning, 1, Target);
	CHECK(std::abs(Plan - Actual) <= 0.05);
	// Stopping first costs time over a run from standing.
	FPlayerMotion Standing(&Ratings, &Tuning, 1.0, Motion.GetPosition());
	CHECK(Plan > Planned(Standing, Tuning, 1, Target) + 0.1);
}

TEST_CASE("Ai.MotionModel: sideways and backwards cost more than forward, and long moves turn")
{
	const FMovementTuning Tuning = LoadMovement();
	const FPlayerRatings Ratings;
	const FPlayerMotion Motion(&Ratings, &Tuning, 1.0, { 5.0, 0.0, 0.0 });
	const auto At = [&](double Right, double Forward) {
		return Planned(Motion, Tuning, 1, ToAi(Motion.GetPosition()) + TeamFrame::CourtDirection(1, FAiVec2{ Right, Forward }));
	};
	CHECK(At(1.5, 0.0) > At(0.0, 1.5));
	CHECK(At(0.0, -1.2) > At(1.2, 0.0));
	// A long sideways move turns and runs, so per metre it is cheaper than a short shuffle.
	CHECK(At(6.0, 0.0) / 6.0 < At(2.0, 0.0) / 2.0);
}
