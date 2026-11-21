#include "AI/AiMotionModel.h"

#include "AI/AiTeamFrame.h"
#include "Shots/MotionCurve.h"
#include "Shots/MovementTuning.h"
#include "Shots/PlayerMotion.h"

#include <cmath>

namespace Padel::AI::AiMotionModel
{
namespace
{
	// The same 1/120 s step the match runs, so the stepped run is the one played.
	constexpr double StepS = 1.0 / 120.0;

	enum class EHeld { Forward, Right, Left, Back };
	enum class EGait { Square, Turning, Turned };

	// FPlayerMotion::Classify: within 45 degrees of straight forward or back, else sideways.
	EHeld Classify(const FAiVec2& Stick)
	{
		constexpr double Cos45 = 0.70710678118654752440;
		if (Stick.Y >= Cos45)
		{
			return EHeld::Forward;
		}
		if (Stick.Y <= -Cos45)
		{
			return EHeld::Back;
		}
		return Stick.X >= 0.0 ? EHeld::Right : EHeld::Left;
	}

	// FPlayerMotion::SquareSpeedFrac.
	double SquareFrac(const FAiMotionParams& P, const FAiVec2& Stick)
	{
		const double Forward = Stick.Y > 0.0 ? Stick.Y : 0.0;
		const double Back = Stick.Y < 0.0 ? -Stick.Y : 0.0;
		return Forward * Forward + Stick.X * Stick.X * P.ShuffleSpeedFrac + Back * Back * P.BackpedalSpeedFrac;
	}

	// One step of FPlayerMotion's velocity update toward Desired (court axes, ground only).
	void Push(FAiVec& Velocity, Shots::AccelRamp::FState& Ramp, const FAiVec& Desired, double Accel, const FAiMotionParams& P, double JerkMax)
	{
		Shots::AccelRamp::Step(Velocity.X, Velocity.Y, Ramp, Desired.X, Desired.Y, Accel, P.AccelRiseS, JerkMax, StepS);
		Velocity.Z = 0.0;
	}
}

FAiMotionParams ParamsOf(const Shots::FPlayerMotion& Motion, const Shots::FMovementTuning& Tuning)
{
	FAiMotionParams P;
	P.TopSpeedMps = Motion.TopSpeed(false);
	P.SprintSpeedBonus = Tuning.SprintSpeedBonus;
	P.TimeToTopS = Motion.TimeToTopSpeed();
	P.AccelFalloff = Tuning.AccelFalloff;
	P.BrakeMult = Tuning.BrakeMult;
	P.ShuffleSpeedFrac = Tuning.ShuffleSpeedFrac;
	P.BackpedalSpeedFrac = Tuning.BackpedalSpeedFrac;
	P.TurnTimeS = Motion.TurnTimeS();
	P.CrossoverMinDistM = Tuning.CrossoverMinDistM;
	P.TurnBackMinDistM = Tuning.TurnBackMinDistM;
	P.AccelRiseS = Tuning.AccelRiseS;
	P.Pivot = Motion.PivotParams();
	P.AppliedAccelX = Motion.GetAppliedAccel().X;
	P.AppliedAccelY = Motion.GetAppliedAccel().Y;
	return P;
}

double StopSpeedFrac(const FAiMotionParams& P, double DistanceM)
{
	// Braking at BrakeMult times the standing start covers v^2 / (2 a) (GDD 6.3, M1).
	if (P.BrakeMult <= 0.0 || !P.IsValid())
	{
		return 1.0;
	}
	// With the brake building over AccelRiseS the stop is longer (BIOMECHANICS_MOVEMENT 5.7).
	const double Decel = Shots::MotionCurve::InitialAccel(P.TopSpeedMps, P.TimeToTopS) * P.BrakeMult;
	return Shots::AccelRamp::StopSpeed(DistanceM, Decel, P.AccelRiseS) / P.TopSpeedMps;
}

void LimitToStop(const FAiMotionParams& P, double DistanceM, double& Stick, bool& bSprint)
{
	const double StopFrac = StopSpeedFrac(P, DistanceM);
	if (bSprint && StopFrac > 1.0)
	{
		// Between running and sprinting top speed: a lighter stick on the sprint.
		const double SprintFrac = StopFrac / (1.0 + P.SprintSpeedBonus);
		Stick = SprintFrac < Stick ? SprintFrac : Stick;
		return;
	}
	if (StopFrac < Stick)
	{
		Stick = StopFrac;
		bSprint = false;
	}
}

double ArrivalTime(const FAiMotionParams& P, const FAiRunQuery& Q)
{
	if (!P.IsValid())
	{
		return 0.0;
	}
	FAiVec Position = { Q.From.X, Q.From.Y, 0.0 };
	FAiVec Velocity = { Q.Velocity.X, Q.Velocity.Y, 0.0 };
	const double BaseAccel = Shots::MotionCurve::InitialAccel(P.TopSpeedMps, P.TimeToTopS);
	// FPlayerMotion's jerk limit: the strongest push over the rise time.
	const double JerkMax = P.AccelRiseS > 0.0 ? BaseAccel * (P.BrakeMult > 1.0 ? P.BrakeMult : 1.0) / P.AccelRiseS : 0.0;
	Shots::AccelRamp::FState Ramp{ P.AppliedAccelX, P.AppliedAccelY };
	Shots::AccelRamp::FPivot Pivot;
	EGait Gait = EGait::Square;
	EHeld TurnDir = EHeld::Forward;
	double TurnLeft = 0.0;
	double Time = 0.0;

	// The hold: the player stands (brakes) until the first step (A5).
	while (Time + 1e-9 < Q.StartDelayS)
	{
		Push(Velocity, Ramp, FAiVec{}, BaseAccel * P.BrakeMult, P, JerkMax);
		Position += Velocity * StepS;
		Time += StepS;
	}

	while (true)
	{
		FAiVec Delta = Q.To - Position;
		Delta.Z = 0.0;
		const double Distance = Delta.Length2D();
		if (Distance <= Q.ArriveRadiusM)
		{
			return Time;
		}
		if (Time >= Q.MaxTimeS)
		{
			// Out of the window that matters: the rest at top speed, which only ranks the miss.
			return Time + Distance / (P.TopSpeedMps * (1.0 + (Q.bSprint ? P.SprintSpeedBonus : 0.0)));
		}

		// MoveTo: flat out, but never faster than the player can still stop in.
		double Stick = 1.0;
		bool bSprint = Q.bSprint;
		LimitToStop(P, Distance, Stick, bSprint);
		// The stick MoveTo sends and the unit direction FPlayerMotion reads from it, worked
		// the same way so a move along a gait boundary (45 degrees) is classed the same.
		const FAiVec2 StickVec = TeamFrame::MoveVector(Q.HalfSign, Delta * (Stick / Distance));
		const double StickLength = std::sqrt(StickVec.X * StickVec.X + StickVec.Y * StickVec.Y);
		if (StickLength <= 1e-3)
		{
			return Time + Distance / P.TopSpeedMps;
		}
		const FAiVec2 Dir = { StickVec.X / StickLength, StickVec.Y / StickLength };
		const EHeld Held = Classify(Dir);

		// FPlayerMotion::UpdateGait with the planned distance as the hint.
		if (Gait != EGait::Square && Held != TurnDir)
		{
			Gait = EGait::Square;
		}
		if (Gait == EGait::Turning)
		{
			TurnLeft -= StepS;
			if (TurnLeft <= 0.0)
			{
				Gait = EGait::Turned;
			}
		}
		else if (Gait == EGait::Square && Held != EHeld::Forward)
		{
			const double MinDist = Held == EHeld::Back ? P.TurnBackMinDistM : P.CrossoverMinDistM;
			if (Distance > MinDist)
			{
				TurnDir = Held;
				TurnLeft = P.TurnTimeS;
				Gait = TurnLeft > 0.0 ? EGait::Turning : EGait::Turned;
			}
		}

		const double Top = P.TopSpeedMps * (bSprint ? 1.0 + P.SprintSpeedBonus : 1.0);
		const double Frac = Gait == EGait::Turned ? 1.0 : SquareFrac(P, Dir);
		FAiVec Desired = Delta * (Top * Stick * Frac / Distance);
		if (Pivot.Update(Velocity.X, Velocity.Y, Desired.X, Desired.Y, P.Pivot, StepS))
		{
			Desired = FAiVec{};
		}

		const double DesiredSpeed = Desired.Length2D();
		const double Speed = Velocity.Length2D();
		double Accel = BaseAccel;
		if (DesiredSpeed < Speed)
		{
			Accel *= P.BrakeMult;
		}
		else if (DesiredSpeed > 0.0)
		{
			const double GaitTop = Top * Frac;
			const double Along = (Velocity.X * Desired.X + Velocity.Y * Desired.Y) / DesiredSpeed;
			Accel *= 1.0 - P.AccelFalloff * ClampD(Along / GaitTop, 0.0, 1.0);
			if (Gait == EGait::Turning)
			{
				Accel *= SquareFrac(P, Dir);
			}
		}
		Push(Velocity, Ramp, Desired, Accel, P, JerkMax);
		Position += Velocity * StepS;
		Time += StepS;
	}
}
}
