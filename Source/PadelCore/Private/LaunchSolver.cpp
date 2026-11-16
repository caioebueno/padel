#include "LaunchSolver.h"
#include "PadelMath.h"

#include <algorithm>
#include <cmath>

namespace Padel
{
	using namespace Court;

	namespace
	{
		// Close enough for a shot: well under the GDD 5.3 scatter radii.
		constexpr double Tolerance = 0.05;
		constexpr double MaxFlightSeconds = 8.0;

		struct FFlight
		{
			FVec3 Landing;
			double Time = 0.0;
			double Apex = 0.0;
			bool bClearsNet = true;
			bool bLanded = false;
		};

		FFlight Fly(const FBallSim& Sim, const FVec3& Origin, const FVec3& Velocity, const FVec3& Spin)
		{
			FFlight Result;
			FBallState S;
			S.Position = Origin;
			S.Velocity = Velocity;
			S.Spin = Spin;
			Result.Apex = Origin.Z;
			const double H = Sim.SubStepSeconds();
			double T = 0.0;
			while (T < MaxFlightSeconds)
			{
				const FVec3 Before = S.Position;
				Sim.FlightSubStep(S, H);
				T += H;
				Result.Apex = std::max(Result.Apex, S.Position.Z);
				if ((Before.X >= 0.0) != (S.Position.X >= 0.0))
				{
					const double F = Before.X / (Before.X - S.Position.X);
					const FVec3 Cross0 = Before + (S.Position - Before) * F;
					if (Cross0.Z - BallRadius < NetHeightAt(Cross0.Y))
					{
						Result.bClearsNet = false;
					}
				}
				if (S.Velocity.Z < 0.0 && S.Position.Z <= BallRadius)
				{
					const double F = (Before.Z - BallRadius) / (Before.Z - S.Position.Z);
					Result.Landing = Before + (S.Position - Before) * std::clamp(F, 0.0, 1.0);
					Result.Landing.Z = 0.0;
					Result.Time = T - H * (1.0 - std::clamp(F, 0.0, 1.0));
					Result.bLanded = true;
					return Result;
				}
			}
			Result.Landing = { S.Position.X, S.Position.Y, 0.0 };
			Result.Time = T;
			return Result;
		}

		FVec3 Horizontal(double Yaw)
		{
			return { Padel::Math::Cos(Yaw), Padel::Math::Sin(Yaw), 0.0 };
		}
	}

	FVec3 SpinLocalToWorld(const FVec3& SpinLocal, const FVec3& Forward)
	{
		const FVec3 F = SafeNormal({ Forward.X, Forward.Y, 0.0 });
		// Axis whose positive rotation makes the bottom of the ball move backwards: topspin.
		const FVec3 TopAxis = Cross(UpVector, F);
		return TopAxis * SpinLocal.X + UpVector * SpinLocal.Y;
	}

	FLaunchResult SolveLaunch(const FBallSim& Sim, const FLaunchRequest& Request)
	{
		const FBallPhysicsParams& P = Sim.GetParams();
		const FVec3 Origin = Request.Origin;
		const FVec3 Target{ Request.Target.X, Request.Target.Y, 0.0 };
		const FVec3 ToTarget{ Target.X - Origin.X, Target.Y - Origin.Y, 0.0 };
		const double Distance = std::max(SizeXY(ToTarget), 0.01);
		const FVec3 Dir0 = SafeNormal(ToTarget);
		const FVec3 Perp0 = Cross(UpVector, Dir0);
		const double BaseYaw = Padel::Math::Atan2(Dir0.Y, Dir0.X);
		const double G = P.Gravity;
		const double Rise = BallRadius - Origin.Z;

		FLaunchResult Best;
		Best.Miss = 1e9;
		double Yaw = BaseYaw;

		auto Evaluate = [&](const FVec3& Velocity, double& AlongError, double& LateralError)
		{
			const FVec3 Spin = SpinLocalToWorld(Request.SpinLocal, Velocity);
			const FFlight Flight = Fly(Sim, Origin, Velocity, Spin);
			const FVec3 Delta = Flight.Landing - Target;
			AlongError = Dot(Delta, Dir0);
			LateralError = Dot(Delta, Perp0);
			const double Miss = SizeXY(Delta);
			if (Miss < Best.Miss)
			{
				Best.Velocity = Velocity;
				Best.Spin = Spin;
				Best.Landing = Flight.Landing;
				Best.FlightTime = Flight.Time;
				Best.Apex = Flight.Apex;
				Best.bClearsNet = Flight.bClearsNet;
				Best.Miss = Miss;
				Best.bOk = Flight.bLanded && Miss <= Tolerance;
			}
			return Flight.Apex;
		};

		if (Request.ApexHeight > 0.0)
		{
			// Apex solve: the vertical speed sets the apex, the horizontal speed the length.
			const double ApexAbove = std::max(Request.ApexHeight - Origin.Z, 0.05);
			double Vz = std::sqrt(2.0 * G * ApexAbove);
			const double TimeUp = Vz / G;
			const double TimeDown = std::sqrt(2.0 * std::max(Request.ApexHeight - BallRadius, 0.05) / G);
			double Vh = Distance / (TimeUp + TimeDown);
			for (int Iteration = 0; Iteration < 24 && Best.Miss > Tolerance * 0.5; ++Iteration)
			{
				const FVec3 Velocity = Horizontal(Yaw) * Vh + UpVector * Vz;
				double Along = 0.0;
				double Lateral = 0.0;
				const double Apex = Evaluate(Velocity, Along, Lateral);
				const double Reached = std::max(Apex - Origin.Z, 0.01);
				Vz *= std::clamp(std::sqrt(ApexAbove / Reached), 0.7, 1.4);
				Vh *= std::clamp(Distance / std::max(Distance + Along, 0.01), 0.5, 2.0);
				Yaw -= Lateral / Distance;
			}
			return Best;
		}

		// Flat solve at a fixed speed. Along the low arc the length grows with the launch
		// angle, so a bracketed false-position search on the angle is safe.
		const double Speed = std::max(Request.Speed, 0.1);
		auto VelocityAt = [&](double Elevation)
		{
			return Horizontal(Yaw) * (Speed * Padel::Math::Cos(Elevation)) + UpVector * (Speed * Padel::Math::Sin(Elevation));
		};
		double Low = -1.3;
		double High = 0.75;
		double AlongLow = 0.0;
		double AlongHigh = 0.0;
		double Lateral = 0.0;
		Evaluate(VelocityAt(High), AlongHigh, Lateral);
		Yaw -= Lateral / Distance;
		if (AlongHigh < 0.0)
		{
			// Too far for this speed: report the longest flat flight we found.
			return Best;
		}
		Evaluate(VelocityAt(Low), AlongLow, Lateral);

		// Start from the drag-free angle, which is usually within a few degrees.
		const double V2 = Speed * Speed;
		const double Disc = V2 * V2 - G * (G * Distance * Distance + 2.0 * Rise * V2);
		double Elevation = Disc >= 0.0 ? Padel::Math::Atan((V2 - std::sqrt(Disc)) / (G * Distance)) : 0.5 * (Low + High);
		Elevation = std::clamp(Elevation, Low, High);
		int Side = 0;
		for (int Iteration = 0; Iteration < 30 && Best.Miss > Tolerance * 0.5; ++Iteration)
		{
			double Along = 0.0;
			Evaluate(VelocityAt(Elevation), Along, Lateral);
			Yaw -= Lateral / Distance;
			if (Along < 0.0)
			{
				Low = Elevation;
				AlongLow = Along;
				if (Side == -1)
				{
					AlongHigh *= 0.5;
				}
				Side = -1;
			}
			else
			{
				High = Elevation;
				AlongHigh = Along;
				if (Side == 1)
				{
					AlongLow *= 0.5;
				}
				Side = 1;
			}
			// Illinois false position: converges fast without leaving the bracket.
			Elevation = (Low * AlongHigh - High * AlongLow) / (AlongHigh - AlongLow);
			if (!(Elevation > Low && Elevation < High))
			{
				Elevation = 0.5 * (Low + High);
			}
		}
		return Best;
	}
}
