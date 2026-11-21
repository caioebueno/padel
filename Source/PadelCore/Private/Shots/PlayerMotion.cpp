#include "Shots/PlayerMotion.h"

#include "PadelMath.h"
#include "Shots/AccelRamp.h"
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
		// GDD 6.3: sprint adds 15% top speed; fatigue takes a little away.
		return (bWithSprint ? Base * (1.0 + Tuning->SprintSpeedBonus) : Base) * FatigueFactor();
	}

	double FPlayerMotion::FatigueFactor() const
	{
		// GDD 6.3: at low stamina some top speed and acceleration go. The initial push is
		// top speed over the time to top speed, so scaling the top speed scales both.
		return 1.0 - Tuning->FatigueTopSpeedLossAtZero * (1.0 - Stamina);
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

	double FPlayerMotion::PivotPlantS() const
	{
		return Lerp(Tuning->PivotPlantAt1S, Tuning->PivotPlantAt99S, Ratings->Unit(ERating::Agility));
	}

	AccelRamp::FPivotParams FPlayerMotion::PivotParams() const
	{
		AccelRamp::FPivotParams P;
		if (Tuning->PivotMinDeg > 0.0)
		{
			P.CosMin = Math::Cos(Tuning->PivotMinDeg * Math::Pi / 180.0);
		}
		P.StartMps = Tuning->SplitMaxSpeedMps;
		P.PlantMps = Tuning->PivotSpeedMps;
		P.PlantS = PivotPlantS();
		return P;
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

	FPlayerMotion::EHeld FPlayerMotion::ClassifyHeld(const FVec2& UnitDir) const
	{
		// GDD 6.3: a stick wobbling around 45 degrees keeps its gait instead of flickering
		// between a run and a shuffle (or backpedal). The current class holds until the
		// direction is GaitHysteresisDeg past the line.
		const EHeld Plain = Classify(UnitDir);
		if (HeldDir == EHeld::None || Plain == HeldDir || Tuning->GaitHysteresisDeg <= 0.0)
		{
			return Plain;
		}
		const double Band = Tuning->GaitHysteresisDeg * Math::Pi / 180.0;
		// Forward or back holds out to 45 + band from the net axis; sideways holds in to
		// 45 - band.
		const double Wide = Math::Cos(0.25 * Math::Pi + Band);
		const double Narrow = Math::Cos(0.25 * Math::Pi - Band);
		switch (HeldDir)
		{
		case EHeld::Forward:
			return UnitDir.Y >= Wide ? EHeld::Forward : Plain;
		case EHeld::Back:
			return UnitDir.Y <= -Wide ? EHeld::Back : Plain;
		case EHeld::Right:
		case EHeld::Left:
			// Only against forward or back; right and left meet where the stick is on the
			// net axis, far from 45 degrees.
			if ((Plain == EHeld::Forward || Plain == EHeld::Back) && UnitDir.Y < Narrow && UnitDir.Y > -Narrow)
			{
				return UnitDir.X >= 0.0 ? EHeld::Right : EHeld::Left;
			}
			return Plain;
		default:
			return Plain;
		}
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
			// The push builds over AccelRiseS, which costs about half of it (AccelRamp).
			bLong = HeldS >= MotionCurve::TimeToCover(MinDist, SquareTop, TimeToTopSpeed() * Frac, Tuning->AccelFalloff) + 0.5 * Tuning->AccelRiseS;
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
			UpdateGait(ClassifyHeld(TeamDir), TeamDir, Top, Dt, PlannedDistanceM);
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

		// BIOMECHANICS_MOVEMENT 2.6, 5.7: a reversal brakes, plants, then pushes off.
		if (Pivot.Update(Velocity.X, Velocity.Y, Desired.X, Desired.Y, PivotParams(), Dt))
		{
			Desired = FVec3{};
		}

		const double BaseAccel = MotionCurve::InitialAccel(TopSpeed(false), TimeToTopSpeed());
		// Plan the braking with the push the player has at the end of their stamina: a hard
		// effort or a long rally can take a little away mid-stop, and the plan must still
		// hold (GDD 6.3 fatigue).
		const double MarginBrake = BaseAccel * Tuning->BrakeMult * (1.0 - Tuning->FatigueTopSpeedLossAtZero) / FatigueFactor();
		// FPlayerMotion's jerk limit (BIOMECHANICS_MOVEMENT 5.7): the strongest push (the
		// brake) over the rise time.
		const double JerkMax = Tuning->AccelRiseS > 0.0 ? BaseAccel * std::max(1.0, Tuning->BrakeMult) / Tuning->AccelRiseS : 0.0;
		if (JerkMax > 0.0)
		{
			RampMarginBraking(Desired, MarginBrake, JerkMax, Dt);
		}
		else
		{
			LimitAtMargins(Desired, MarginBrake, Dt);
		}

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

		FVec3 Delta = Sub(Desired, Velocity);
		const double MaxChange = Accel * Dt;
		bool bAllocated = false;
		{
			// GDD 6.3 (BIOMECHANICS_MOVEMENT 2.6): braking into a margin comes first. A player
			// redirecting near the glass or net still plants against the direction they are
			// travelling; without this the sideways turn shares the tick's change with the
			// braking, the braking falls behind the plan, and the player hits the margin.
			bool bBrakeX = false;
			bool bBrakeY = false;
			MarginBraking(MarginBrake, JerkMax, Dt, bBrakeX, bBrakeY);
			bBrakeX = bBrakeX && Delta.X * Velocity.X < 0.0;
			bBrakeY = bBrakeY && Delta.Y * Velocity.Y < 0.0;
			if (bBrakeX || bBrakeY)
			{
				// The braking gets at least the strength the margin plan assumed, even while the
				// hip turn weakens the push. The plan is per axis, so the budget is too: heading
				// into a corner, each wall's braking gets the strength its own plan counted on
				// (a moment's extra plant, not a stop at the glass).
				const double BrakeBudget = std::max(MaxChange, MarginBrake * Dt);
				const auto Brake = [BrakeBudget](bool bOn, double D)
				{
					return bOn ? Clamp(D, -BrakeBudget, BrakeBudget) : 0.0;
				};
				const FVec3 First = Vec(Brake(bBrakeX, Delta.X), Brake(bBrakeY, Delta.Y), 0.0);
				FVec3 Rest = Vec(bBrakeX ? 0.0 : Delta.X, bBrakeY ? 0.0 : Delta.Y, 0.0);
				const double Left = std::sqrt(std::max(0.0, MaxChange * MaxChange - (First.X * First.X + First.Y * First.Y)));
				const double RestLength = LengthXY(Rest);
				if (RestLength > Left)
				{
					Rest = RestLength > 0.0 ? Scale(Rest, Left / RestLength) : Rest;
				}
				// The change fits the tick's budget now, so the update below takes it whole.
				Delta = Add(First, Rest);
				Desired = Add(Velocity, Delta);
				bAllocated = true;
			}
		}
		// Braking that this tick can't finish is a deceleration; a small trim of speed (the
		// stick wobbling between gaits' top speeds) isn't one. With AccelRiseS a trim takes a
		// few ticks to settle, so a trim is anything the brake would shed while it builds.
		const bool bDecelerating = bBraking && Speed - DesiredSpeed > std::max(MaxChange, Accel * Tuning->AccelRiseS);
		if (bAllocated)
		{
			// The margin plan's braking is a safety net and takes effect at once; the ramp
			// carries on from the acceleration it applied.
			Ramp.AX = (Desired.X - Velocity.X) / Dt;
			Ramp.AY = (Desired.Y - Velocity.Y) / Dt;
			Velocity = Desired;
		}
		else
		{
			// BIOMECHANICS_MOVEMENT 5.7: the push and the brake build over AccelRiseS.
			AccelRamp::Step(Velocity.X, Velocity.Y, Ramp, Desired.X, Desired.Y, Accel, Tuning->AccelRiseS, JerkMax, Dt);
		}
		Velocity.Z = 0.0;
		Position = Add(Position, Scale(Velocity, Dt));
		const int EdgeStopsBefore = EdgeStopCount;
		ClampToOwnHalf();
		UpdateFatigue(EdgeStopCount != EdgeStopsBefore, Dt);

		const double NewSpeed = LengthXY(Velocity);
		// With no stick the home drift's direction stands in for the held one.
		const EHeld ModeDir = HeldDir != EHeld::None ? HeldDir : Classify(TeamDir);
		if (bDecelerating && NewSpeed > Tuning->SplitMaxSpeedMps)
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
		else if (Gait == EGait::Turned || ModeDir == EHeld::Forward)
		{
			Mode = bSprinting ? EMovementMode::Sprint : EMovementMode::Run;
		}
		else
		{
			Mode = ModeDir == EHeld::Back ? EMovementMode::Backpedal : EMovementMode::Shuffle;
		}

		TimeSinceSlowS = NewSpeed <= Tuning->SplitMaxSpeedMps ? 0.0 : TimeSinceSlowS + Dt;
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
		Ramp = AccelRamp::FState{};
		Pivot = AccelRamp::FPivot{};
		bMarginBrakeX = false;
		bMarginBrakeY = false;
		ClampToOwnHalf();
		HardEfforts.Reset();
	}

	void FPlayerMotion::SetRallyLive(bool bLive)
	{
		if (bLive)
		{
			if (Rally != ERally::Live)
			{
				RallyS = 0.0;
				HardEfforts.Reset();
			}
			Rally = ERally::Live;
			return;
		}
		if (Rally != ERally::Dead)
		{
			// GDD 6.3: fatigue recovers between points (BIOMECHANICS_MOVEMENT 5.5: the 15 to
			// 20 s between real points). The match's gap is shorter, so the rest it stands
			// for is credited when the point ends, at the player's usual regen rate.
			const double Regen = Lerp(Tuning->RegenAt1, Tuning->RegenAt99, Ratings->Unit(ERating::Stamina));
			Stamina = Clamp(Stamina + Regen * Tuning->FatigueBetweenPointRestS, 0.0, 1.0);
		}
		Rally = ERally::Dead;
	}

	void FPlayerMotion::UpdateFatigue(bool bEdgeStop, double Dt)
	{
		// GDD 6.3 fatigue (BIOMECHANICS_MOVEMENT 2.13, 5.5): hard efforts and long rallies
		// drain stamina on top of sprinting, more slowly at a higher Stamina rating. Between
		// points the players are placed for the serve, which isn't effort.
		if (Rally == ERally::Dead)
		{
			return;
		}
		const double StaminaUnit = Ratings->Unit(ERating::Stamina);
		if (HardEfforts.Tick(Velocity, bEdgeStop, Tuning->FatigueHardAccelMps2))
		{
			++HardEffortCount;
			Stamina -= Lerp(Tuning->FatigueDrainPerHardAccelAt1, Tuning->FatigueDrainPerHardAccelAt99, StaminaUnit);
		}
		if (Rally == ERally::Live)
		{
			const double Before = RallyS;
			RallyS += Dt;
			const double Past = RallyS - std::max(Before, Tuning->FatigueRallyStartS);
			if (Past > 0.0)
			{
				Stamina -= Lerp(Tuning->FatigueDrainPerRallySAt1, Tuning->FatigueDrainPerRallySAt99, StaminaUnit) * Past;
			}
		}
		Stamina = Clamp(Stamina, 0.0, 1.0);
	}
}
