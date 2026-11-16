#include "Shots/PlayerMotion.h"

#include "CourtSpec.h"
#include "Shots/MotionCurve.h"

#include <algorithm>
#include <cmath>

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

	double FPlayerMotion::TurnTimeS() const
	{
		// GDD 6.1: Agility sets how quickly the player turns to run.
		return Lerp(Tuning->TurnToRunAt1S, Tuning->TurnToRunAt99S, Ratings->Unit(ERating::Agility));
	}

	double FPlayerMotion::SquareSpeedFrac(const FVec2& UnitDir) const
	{
		// Blend by the squared components (they sum to 1), so the fraction is smooth around
		// the circle and exact on the axes: forward 1, sideways shuffle, backwards backpedal.
		const double Forward = UnitDir.Y > 0.0 ? UnitDir.Y : 0.0;
		const double Back = UnitDir.Y < 0.0 ? -UnitDir.Y : 0.0;
		return Forward * Forward + UnitDir.X * UnitDir.X * Tuning->ShuffleSpeedFrac + Back * Back * Tuning->BackpedalSpeedFrac;
	}

	FPlayerMotion::EHeld FPlayerMotion::Classify(const FVec2& UnitDir)
	{
		// Within 45 degrees of straight forward or straight back; sideways otherwise.
		constexpr double Cos45 = 0.70710678118654752440;
		if (UnitDir.Y >= Cos45)
		{
			return EHeld::Forward;
		}
		if (UnitDir.Y <= -Cos45)
		{
			return EHeld::Back;
		}
		return UnitDir.X >= 0.0 ? EHeld::Right : EHeld::Left;
	}

	void FPlayerMotion::UpdateGait(EHeld Held, const FVec2& UnitDir, double Top, double Dt, double PlannedDistanceM)
	{
		if (Held != HeldDir)
		{
			HeldDir = Held;
			HeldS = 0.0;
		}
		else
		{
			HeldS += Dt;
		}
		// A turned player stays turned while the stick keeps to that direction.
		if (Gait != EGait::Square && Held != TurnDir)
		{
			Gait = EGait::Square;
			TurnDir = EHeld::None;
		}
		if (Gait == EGait::Turning)
		{
			TurnLeftS -= Dt;
			if (TurnLeftS <= 0.0)
			{
				Gait = EGait::Turned;
			}
			return;
		}
		if (Gait != EGait::Square || Held == EHeld::Forward || Held == EHeld::None)
		{
			return;
		}
		// GDD 6.3 (BIOMECHANICS_MOVEMENT 2.3, 2.4): short moves shuffle or backpedal, long
		// ones turn and run. The AI says how far it is going. For the stick, distance is
		// unknown, so the rule is time: once the stick has been held in one sideways (or
		// backward) direction for as long as shuffling (or backpedalling) CrossoverMinDistM
		// (TurnBackMinDistM) from standing takes, the move is a long one and the player turns.
		const bool bBack = Held == EHeld::Back;
		const double MinDist = bBack ? Tuning->TurnBackMinDistM : Tuning->CrossoverMinDistM;
		bool bLong = false;
		if (PlannedDistanceM >= 0.0)
		{
			bLong = PlannedDistanceM > MinDist;
		}
		else
		{
			const double Frac = SquareSpeedFrac(UnitDir);
			const double SquareTop = Top * Frac;
			bLong = HeldS >= MotionCurve::TimeToCover(MinDist, SquareTop, TimeToTopSpeed() * Frac, Tuning->AccelFalloff);
		}
		if (bLong)
		{
			TurnDir = Held;
			TurnLeftS = TurnTimeS();
			Gait = TurnLeftS > 0.0 ? EGait::Turning : EGait::Turned;
		}
	}

	void FPlayerMotion::Step(const FVec2& MoveDir, bool bSprint, double Dt, double SpeedMult, double PlannedDistanceM)
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

		// GDD 6.3: sprint adds 15% top speed in whatever gait; it doesn't start any harder.
		const double Top = TopSpeed(bSprinting);
		double Frac = 1.0;
		FVec3 Desired;
		FVec2 TeamDir;
		if (bHasInput)
		{
			TeamDir = { Dir.X / DirLength, Dir.Y / DirLength };
			UpdateGait(Classify(TeamDir), TeamDir, Top, Dt, PlannedDistanceM);
			Frac = Gait == EGait::Turned ? 1.0 : SquareSpeedFrac(TeamDir);
			Desired = Scale(TeamToWorld(Dir, TeamSign), Top * SpeedMult * Frac);
		}
		else
		{
			UpdateGait(EHeld::None, TeamDir, Top, Dt, PlannedDistanceM);
			if (bHasHome)
			{
				// GDD 6.3: with no input, drift home at a speed set by Positioning, still
				// square to the net.
				const FVec3 ToHome = Vec(Home.X - Position.X, Home.Y - Position.Y, 0.0);
				const double Distance = LengthXY(ToHome);
				if (Distance > Tuning->HomeArriveM)
				{
					TeamDir = { DotProduct(ToHome, RightAxis(TeamSign)) / Distance, DotProduct(ToHome, ForwardAxis(TeamSign)) / Distance };
					Frac = SquareSpeedFrac(TeamDir);
					const double Drift = Lerp(Tuning->HomeDriftAt1, Tuning->HomeDriftAt99, Ratings->Unit(ERating::Positioning)) * Top * SpeedMult * Frac;
					Desired = Scale(ToHome, Drift / Distance);
				}
			}
		}

		const double BaseAccel = MotionCurve::InitialAccel(TopSpeed(false), TimeToTopSpeed());
		LimitAtMargins(Desired, BaseAccel * Tuning->BrakeMult, Dt);

		double Accel = BaseAccel;
		if (SplitBoostLeftS > 0.0)
		{
			// GDD 6.3: a split-step gives full Agility to the first step.
			Accel *= Lerp(Tuning->SplitFirstStepAt1, Tuning->SplitFirstStepAt99, Ratings->Unit(ERating::Agility));
		}
		const double DesiredSpeed = LengthXY(Desired);
		const double Speed = LengthXY(Velocity);
		const bool bBraking = DesiredSpeed < Speed;
		if (bBraking)
		{
			// Braking keeps its full strength at any speed (BIOMECHANICS_MOVEMENT 2.6, 3.1).
			Accel *= Tuning->BrakeMult;
		}
		else if (DesiredSpeed > 0.0)
		{
			// GDD 6.3: the push is strongest from standing and weakens as the player gets up
			// to speed in the direction they push (BIOMECHANICS_MOVEMENT 5.1). Only the speed
			// along the new direction counts, so a player running one way can still turn.
			// The curve runs up to this gait's top speed.
			const double GaitTop = Top * Frac;
			const double Along = (Velocity.X * Desired.X + Velocity.Y * Desired.Y) / DesiredSpeed;
			Accel *= 1.0 - Tuning->AccelFalloff * Clamp(Along / GaitTop, 0.0, 1.0);
			if (Gait == EGait::Turning)
			{
				// The hip turn costs push: square-gait strength until the turn is done.
				Accel *= SquareSpeedFrac(TeamDir);
			}
		}

		const FVec3 Delta = Sub(Desired, Velocity);
		const double DeltaLength = LengthXY(Delta);
		const double MaxChange = Accel * Dt;
		Velocity = DeltaLength <= MaxChange ? Desired : Add(Velocity, Scale(Delta, MaxChange / DeltaLength));
		Velocity.Z = 0.0;
		Position = Add(Position, Scale(Velocity, Dt));
		ClampToOwnHalf();

		const double NewSpeed = LengthXY(Velocity);
		if (bBraking && NewSpeed > Tuning->SplitMaxSpeedMps)
		{
			Mode = EMovementMode::Decel;
		}
		else if (Gait == EGait::Turning)
		{
			Mode = TurnDir == EHeld::Back ? EMovementMode::TurnAndRun : EMovementMode::Crossover;
		}
		else if (NewSpeed <= Tuning->SplitMaxSpeedMps)
		{
			Mode = EMovementMode::Adjust;
		}
		else if (Gait == EGait::Turned || Classify(TeamDir) == EHeld::Forward)
		{
			Mode = bSprinting ? EMovementMode::Sprint : EMovementMode::Run;
		}
		else
		{
			Mode = Classify(TeamDir) == EHeld::Back ? EMovementMode::Backpedal : EMovementMode::Shuffle;
		}

		TimeSinceSlowS = NewSpeed <= Tuning->SplitMaxSpeedMps ? 0.0 : TimeSinceSlowS + Dt;
		SplitBoostLeftS = SplitBoostLeftS > Dt ? SplitBoostLeftS - Dt : 0.0;
		SplitHopLeftS = SplitHopLeftS > Dt ? SplitHopLeftS - Dt : 0.0;
	}

	void FPlayerMotion::LimitAtMargins(FVec3& Desired, double Brake, double Dt) const
	{
		// GDD 6.3 (BIOMECHANICS_MOVEMENT 2.6): a player arriving at the glass or the net
		// slows over the last steps instead of stopping dead. Toward a margin, ask for no
		// more speed than the player can still brake away inside the room left. The limit is
		// the stepped one, not sqrt(2 a d): speed falls by Brake * Dt a tick and each tick
		// moves the player its new speed * Dt, so k ticks of braking from k * Brake * Dt
		// cover Brake * Dt^2 * k (k + 1) / 2.
		if (Brake <= 0.0 || Dt <= 0.0)
		{
			return;
		}
		double Lo = 0.0;
		double Hi = 0.0;
		double Side = 0.0;
		OwnHalfLimits(Lo, Hi, Side);
		const double Step = Brake * Dt;
		const auto MaxSpeed = [Step, Dt](double Room)
		{
			// Whole ticks, so each one sheds exactly Brake * Dt; inside the last tick, just the
			// room that is left.
			const double Ticks = std::floor(0.5 * (std::sqrt(1.0 + 8.0 * std::max(0.0, Room) / (Step * Dt)) - 1.0));
			return Ticks >= 1.0 ? Ticks * Step : std::max(0.0, Room) / Dt;
		};
		const auto Limit = [&MaxSpeed](double& Component, double Pos, double Min, double Max)
		{
			if (Component > 0.0)
			{
				Component = std::min(Component, MaxSpeed(Max - Pos));
			}
			else if (Component < 0.0)
			{
				Component = std::max(Component, -MaxSpeed(Pos - Min));
			}
		};
		Limit(Desired.X, Position.X, Lo, Hi);
		Limit(Desired.Y, Position.Y, -Side, Side);
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

	void FPlayerMotion::OwnHalfLimits(double& Lo, double& Hi, double& Side) const
	{
		const double Near = Tuning->NetMarginM;
		const double Far = Court::HalfLength - Tuning->WallMarginM;
		Side = Court::HalfWidth - Tuning->WallMarginM;
		Lo = TeamSign > 0.0 ? Near : -Far;
		Hi = TeamSign > 0.0 ? Far : -Near;
	}

	void FPlayerMotion::ClampToOwnHalf()
	{
		// Players stay on their own half; crossing the net is a referee matter (GDD 3.3).
		// LimitAtMargins brakes the player into these limits; this is the hard stop behind it.
		double Lo = 0.0;
		double Hi = 0.0;
		double Side = 0.0;
		OwnHalfLimits(Lo, Hi, Side);
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
