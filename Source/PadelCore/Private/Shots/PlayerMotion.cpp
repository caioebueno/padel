#include "Shots/PlayerMotion.h"

#include "CourtSpec.h"
#include "Shots/MotionCurve.h"

namespace Padel::Shots
{
	FPlayerMotion::FPlayerMotion(const FPlayerRatings* InRatings, const FMovementTuning* InTuning, double InTeamSign, const FVec3& InStart)
		: Ratings(InRatings), Tuning(InTuning), TeamSign(InTeamSign), Position(InStart)
	{
		Position.Z = 0.0;
		ClampToOwnHalf();
	}

	double FPlayerMotion::TopSpeed(bool bWithSprint) const
	{
		const double Base = Lerp(Tuning->TopSpeedAt1Mps, Tuning->TopSpeedAt99Mps, InverseLerp(1.0 / 99.0, 1.0, Ratings->Unit(ERating::Speed)));
		// GDD 6.3: sprint adds 15% top speed.
		return bWithSprint ? Base * (1.0 + Tuning->SprintSpeedBonus) : Base;
	}

	double FPlayerMotion::TimeToTopSpeed() const
	{
		return Lerp(Tuning->TimeToTopAt1S, Tuning->TimeToTopAt99S, InverseLerp(1.0 / 99.0, 1.0, Ratings->Unit(ERating::Acceleration)));
	}

	void FPlayerMotion::Step(const FVec2& MoveDir, bool bSprint, double Dt, double SpeedMult)
	{
		const FVec2 Dir = Limit2(MoveDir, 1.0);
		const double DirLength = Length2(Dir);
		const bool bHasInput = DirLength > 1e-3;

		bSprinting = bSprint && bHasInput && Stamina > Tuning->SprintMinStamina;
		const double StaminaUnit = Ratings->Unit(ERating::Stamina);
		if (bSprinting)
		{
			Stamina -= Lerp(Tuning->SprintDrainAt1, Tuning->SprintDrainAt99, StaminaUnit) * Dt;
		}
		else
		{
			Stamina += Lerp(Tuning->RegenAt1, Tuning->RegenAt99, StaminaUnit) * Dt;
		}
		Stamina = Clamp(Stamina, 0.0, 1.0);

		const double Top = TopSpeed(bSprinting);
		FVec3 Desired;
		if (bHasInput)
		{
			Desired = Scale(TeamToWorld(Dir, TeamSign), Top * SpeedMult);
		}
		else if (bHasHome)
		{
			// GDD 6.3: with no input, drift home at a speed set by Positioning.
			const FVec3 ToHome = Vec(Home.X - Position.X, Home.Y - Position.Y, 0.0);
			const double Distance = LengthXY(ToHome);
			if (Distance > Tuning->HomeArriveM)
			{
				const double Drift = Lerp(Tuning->HomeDriftAt1, Tuning->HomeDriftAt99, Ratings->Unit(ERating::Positioning)) * Top * SpeedMult;
				Desired = Scale(ToHome, Drift / Distance);
			}
		}

		double Accel = MotionCurve::InitialAccel(Top, TimeToTopSpeed());
		if (SplitBoostLeftS > 0.0)
		{
			// GDD 6.3: a split-step gives full Agility to the first step.
			Accel *= Lerp(Tuning->SplitFirstStepAt1, Tuning->SplitFirstStepAt99, Ratings->Unit(ERating::Agility));
		}
		const double DesiredSpeed = LengthXY(Desired);
		if (DesiredSpeed < LengthXY(Velocity))
		{
			// Braking keeps its full strength at any speed (BIOMECHANICS_MOVEMENT 2.6, 3.1).
			Accel *= Tuning->BrakeMult;
		}
		else if (DesiredSpeed > 0.0)
		{
			// GDD 6.3: the push is strongest from standing and weakens as the player gets up
			// to speed in the direction they push (BIOMECHANICS_MOVEMENT 5.1). Only the speed
			// along the new direction counts, so a player running one way can still turn.
			const double Along = (Velocity.X * Desired.X + Velocity.Y * Desired.Y) / DesiredSpeed;
			Accel *= 1.0 - Tuning->AccelFalloff * Clamp(Along / Top, 0.0, 1.0);
		}

		const FVec3 Delta = Sub(Desired, Velocity);
		const double DeltaLength = LengthXY(Delta);
		const double MaxChange = Accel * Dt;
		Velocity = DeltaLength <= MaxChange ? Desired : Add(Velocity, Scale(Delta, MaxChange / DeltaLength));
		Velocity.Z = 0.0;
		Position = Add(Position, Scale(Velocity, Dt));
		ClampToOwnHalf();

		TimeSinceSlowS = LengthXY(Velocity) <= Tuning->SplitMaxSpeedMps ? 0.0 : TimeSinceSlowS + Dt;
		SplitBoostLeftS = SplitBoostLeftS > Dt ? SplitBoostLeftS - Dt : 0.0;
		SplitHopLeftS = SplitHopLeftS > Dt ? SplitHopLeftS - Dt : 0.0;
	}

	bool FPlayerMotion::NotifyOpponentContact()
	{
		// GDD 6.3: stopped or slower than 1 m/s inside 150 ms before the contact.
		if (TimeSinceSlowS > Tuning->SplitWindowS)
		{
			return false;
		}
		bSplitPending = true;
		SplitBoostLeftS = Tuning->SplitBoostS;
		SplitHopLeftS = Tuning->SplitHopS;
		++SplitStepCount;
		return true;
	}

	bool FPlayerMotion::ConsumeSplitStep()
	{
		const bool bWas = bSplitPending;
		bSplitPending = false;
		return bWas;
	}

	void FPlayerMotion::Teleport(const FVec3& InPosition)
	{
		Position = Vec(InPosition.X, InPosition.Y, 0.0);
		Velocity = FVec3{};
		ClampToOwnHalf();
	}

	void FPlayerMotion::ClampToOwnHalf()
	{
		// Players stay on their own half; crossing the net is a referee matter (GDD 3.3).
		const double Near = Tuning->NetMarginM;
		const double Far = Court::HalfLength - Tuning->WallMarginM;
		const double Side = Court::HalfWidth - Tuning->WallMarginM;
		const double Lo = TeamSign > 0.0 ? Near : -Far;
		const double Hi = TeamSign > 0.0 ? Far : -Near;
		const FVec3 Before = Velocity;
		if (Position.X < Lo || Position.X > Hi)
		{
			Position.X = Clamp(Position.X, Lo, Hi);
			Velocity.X = 0.0;
		}
		if (Position.Y < -Side || Position.Y > Side)
		{
			Position.Y = Clamp(Position.Y, -Side, Side);
			Velocity.Y = 0.0;
		}
		EdgeStopCount += Velocity.X != Before.X || Velocity.Y != Before.Y ? 1 : 0;
	}
}
