// A drive struck low can't stay on a flat line over the tape: the shot model opens the
// face, slowing the ball onto a higher arc to the same target (Shots.json
// LowContactHeightM / LowContactNetClearanceM). Since contact heights became real (#45),
// the deep drive from (8.5, 1.5) was struck at about 0.4 m and netted on every seed.
#include "doctest.h"
#include "ShotTestData.h"
#include "TestSupport.h"

#include "BallSim.h"
#include "CourtSpec.h"
#include "LaunchSolver.h"
#include "Shots/PracticeBall.h"
#include "Shots/ShotResolver.h"

#include <cmath>
#include <vector>

using namespace ShotTestData;

namespace
{
	constexpr double Dt = 1.0 / 120.0;
	// The clearance asked for, under the 0.2 m in Shots.json to leave the sim's step.
	constexpr double MinClearanceM = 0.15;

	FShotContext LowDrive(const FPlayerRatings& Player, FVec3 At, double Charge, FVec2 Aim)
	{
		FShotContext C;
		C.Player = &Player;
		C.TeamSign = 1.0;
		C.Button = EShotButton::Drive;
		C.ContactPosition = At;
		C.ContactSide = Player.ForehandSign();
		C.bBallBounced = true;
		C.Charge = Charge;
		C.TopSpeedMps = 6.0;
		C.Aim = Aim;
		return C;
	}

	// Ball centre over the tape where it crosses X = 0, or -1 if it never did.
	double PracticeClearance(const FShotIntent& Intent, const FPracticeBallParams& Params)
	{
		FPracticeBall Ball(&Params);
		Ball.ApplyShot(Intent);
		for (int Step = 0; Step < 600 && Ball.IsLive(); ++Step)
		{
			const FVec3 Before = Ball.Current().Position;
			Ball.Advance(Dt);
			const FVec3 After = Ball.Current().Position;
			if (Before.X > 0.0 && After.X <= 0.0)
			{
				const double T = Before.X / (Before.X - After.X);
				const double Y = Before.Y + T * (After.Y - Before.Y);
				return Before.Z + T * (After.Z - Before.Z) - Padel::Court::NetHeightAt(Y);
			}
		}
		return -1.0;
	}

	// The same over the match ball: drag and the drive's topspin, launched as the match
	// does (with the solver's best velocity even when a slow ball falls short).
	double MatchClearance(const FShotIntent& Intent)
	{
		const Padel::FBallSim Sim(Padel::Test::Physics());
		Padel::FLaunchRequest R;
		R.Origin = Intent.Origin;
		R.Target = Intent.Target;
		R.Speed = Intent.SpeedMps;
		R.SpinLocal = Intent.Spin;
		const Padel::FLaunchResult L = Padel::SolveLaunch(Sim, R);
		Padel::FBallState Ball;
		Ball.Position = Intent.Origin;
		Ball.Velocity = L.Velocity;
		Ball.Spin = L.Spin;
		std::vector<Padel::FBallContact> Contacts;
		for (int Tick = 0; Tick < 600; ++Tick)
		{
			const FVec3 Before = Ball.Position;
			Contacts.clear();
			Sim.Tick(Ball, nullptr, Contacts);
			if (Before.X > 0.0 && Ball.Position.X <= 0.0)
			{
				const double T = Before.X / (Before.X - Ball.Position.X);
				const double Y = Before.Y + T * (Ball.Position.Y - Before.Y);
				return Before.Z + T * (Ball.Position.Z - Before.Z) - Padel::Court::NetHeightAt(Y);
			}
			if (!Contacts.empty())
			{
				return -1.0;
			}
		}
		return -1.0;
	}
}

TEST_CASE("Shots.LowContact: a drive struck from 0.3 to 0.5 m clears the tape by 15 cm")
{
	const FShotLibrary Lib = LoadLibrary();
	const FPlayerRatings Player = LoadPlayer();
	const FPracticeBallParams BallParams = LoadPracticeBall();
	const FShotParams& Drive = Lib.Get(EShotType::Drive);
	REQUIRE(Drive.LowContactHeightM > 0.5);
	const FVec3 Spots[] = { Vec(8.5, 1.5, 0.0), Vec(8.5, -2.0, 0.0), Vec(9.3, 0.0, 0.0), Vec(6.0, 2.5, 0.0), Vec(4.0, -1.0, 0.0) };
	const FVec2 Aims[] = { FVec2{}, FVec2{ 0.0, 1.0 }, FVec2{ 0.8, 0.6 }, FVec2{ -0.8, 0.6 }, FVec2{ 0.0, -1.0 } };
	double Worst = 1e9;
	for (const FVec3& Spot : Spots)
	{
		for (const double Height : { 0.3, 0.4, 0.5 })
		{
			for (const double Charge : { 0.0, 0.5, 1.0 })
			{
				for (const FVec2& Aim : Aims)
				{
					for (uint64_t Seed = 1; Seed <= 8; ++Seed)
					{
						CAPTURE(Spot.X);
						CAPTURE(Spot.Y);
						CAPTURE(Height);
						CAPTURE(Charge);
						CAPTURE(Aim.X);
						CAPTURE(Aim.Y);
						CAPTURE(Seed);
						FPadelRng Rng(Seed);
						const FShotContext C = LowDrive(Player, Vec(Spot.X, Spot.Y, Height), Charge, Aim);
						const FShotIntent I = FShotResolver::Resolve(C, Lib, Rng);
						REQUIRE(I.ShotType == EShotType::Drive);
						const double Practice = PracticeClearance(I, BallParams);
						const double Match = MatchClearance(I);
						CHECK(Practice >= MinClearanceM);
						CHECK(Match >= MinClearanceM);
						Worst = std::fmin(Worst, std::fmin(Practice, Match));
					}
				}
			}
		}
	}
	MESSAGE("worst clearance " << Worst);
}

TEST_CASE("Shots.LowContact: only low drives slow down, and never below what reaches the target")
{
	const FShotLibrary Lib = LoadLibrary();
	const FPlayerRatings Player = LoadPlayer();
	const FShotParams& Drive = Lib.Get(EShotType::Drive);
	// From a good height the drive keeps its pace.
	{
		FPadelRng A(3);
		FPadelRng B(3);
		const FShotIntent High = FShotResolver::Resolve(LowDrive(Player, Vec(8.5, 0.0, Drive.LowContactHeightM + 0.05), 1.0, {}), Lib, A);
		const FShotIntent Low = FShotResolver::Resolve(LowDrive(Player, Vec(8.5, 0.0, 0.35), 1.0, {}), Lib, B);
		CHECK(Low.SpeedMps < High.SpeedMps);
		CHECK(High.SpeedMps > Drive.SpeedMinMps());
	}
	// The cap is the fastest arc that clears: a touch faster passes lower than asked.
	const FVec3 From = Vec(8.5, 1.5, 0.4);
	const FVec3 To = Vec(-8.0, 1.0, 0.0);
	const double G = Lib.Model.GravityMps2;
	const double Cap = FShotResolver::NetClearingSpeed(From, To, 30.0, Drive.LowContactNetClearanceM, G);
	const double T = From.X / (From.X - To.X);
	const double Need = Padel::Court::NetHeightAt(From.Y + T * (To.Y - From.Y)) + Drive.LowContactNetClearanceM;
	CHECK(Cap < 30.0);
	CHECK(FShotResolver::NetCrossingHeight(From, To, Cap, G) >= Need - 1e-6);
	CHECK(FShotResolver::NetCrossingHeight(From, To, Cap * 1.01, G) < Need);
	// A slow ball that already clears is left alone.
	CHECK(FShotResolver::NetClearingSpeed(From, To, Cap * 0.9, Drive.LowContactNetClearanceM, G) == Cap * 0.9);
}

TEST_CASE("Shots.LowContact: the shot model's gravity is the ball's")
{
	const FShotLibrary Lib = LoadLibrary();
	CHECK(Lib.Model.GravityMps2 == Padel::Test::Physics().Gravity);
	CHECK(Lib.Model.GravityMps2 == LoadPracticeBall().GravityMps2);
}
