// Player movement on the ground (GDD 6.1, 6.3): acceleration from Acceleration, falling as
// speed rises (Shots/MotionCurve.h), top speed from Speed, sprint and stamina, the
// automatic split-step and the home drift.
// Fixed-step and engine-free, so the AI drives it with the same move vector a stick gives.

#pragma once

#include "PadelTypes.h"
#include "Shots/AccelRamp.h"
#include "Shots/HardEffortCounter.h"
#include "Shots/MovementTuning.h"
#include "Shots/PlayerRatings.h"
#include "Shots/ShotMath.h"

namespace Padel::Shots
{
	class PADELCORE_API FPlayerMotion
	{
	public:
		FPlayerMotion() = default;
		FPlayerMotion(const FPlayerRatings* InRatings, const FMovementTuning* InTuning, double InTeamSign, const FVec3& InStart);

		// MoveDir is team-relative: X = the player's right, Y = toward the net, length 0..1.
		// SpeedMult < 1 while swinging (planted feet). PlannedDistanceM is how far the move
		// is going, when the caller knows it (the AI); below 0 it is unknown (the stick),
		// and how long the stick has been held in one direction stands in for it.
		void Step(const FVec2& MoveDir, bool bSprint, double Dt, double SpeedMult = 1.0, double PlannedDistanceM = -1.0);

		// GDD 6.3: called at the opponent's contact. Returns true if the player split-stepped.
		bool NotifyOpponentContact();
		// The split-step removes the running penalty for the next shot only (GDD 5.2).
		bool ConsumeSplitStep();
		// GDD 6.3 fatigue: whether a rally is on, once a tick before Step (the shot
		// controller passes the ball's live flag). A rally drains stamina once it runs past
		// FatigueRallyStartS; when it ends the player gets the between-point rest. Until
		// the first call nothing is known: no rally drain and no rest, as before M3.
		void SetRallyLive(bool bLive);

		void SetHome(const FVec3& InHome) { Home = InHome; bHasHome = true; }
		void ClearHome() { bHasHome = false; }
		void Teleport(const FVec3& InPosition);

		double TopSpeed(bool bSprinting) const;
		// GDD 6.1 Agility: the hip turn into a crossover or turn-and-run.
		double TurnTimeS() const;
		// GDD 6.1 Agility: how long the player plants in a pivot (Movement.json PivotMinDeg).
		double PivotPlantS() const;
		// Braking into or planted in a pivot, for animation and tests.
		bool IsPivoting() const { return Pivot.Stage != AccelRamp::FPivot::EStage::None; }
		bool IsPivotPlanted() const { return Pivot.Stage == AccelRamp::FPivot::EStage::Planted; }
		// The pivot rule's numbers for this player (the AI's motion model steps the same).
		AccelRamp::FPivotParams PivotParams() const;
		// The acceleration applied last tick (m/s^2, court axes), for animation and tests.
		FVec3 GetAppliedAccel() const { return Vec(Ramp.AX, Ramp.AY, 0.0); }
		// GDD 6.3: the top-speed fraction for a team-relative unit direction while square
		// to the net (1 forward, ShuffleSpeedFrac sideways, BackpedalSpeedFrac backwards).
		double SquareSpeedFrac(const FVec2& UnitDir) const;
		EMovementMode GetMode() const { return Mode; }
		// Top speed over the initial acceleration (MotionCurve); the real time to top speed
		// is longer when Movement.json's AccelFalloff is above 0.
		double TimeToTopSpeed() const;

		const FVec3& GetPosition() const { return Position; }
		const FVec3& GetVelocity() const { return Velocity; }
		double GetStamina() const { return Stamina; }
		// GDD 6.3: the share of top speed and acceleration left at the current stamina.
		double FatigueFactor() const;
		// Hard efforts counted so far (Shots/HardEffortCounter.h), for tests and the soak.
		int GetHardEffortCount() const { return HardEffortCount; }
		double GetRallyTimeS() const { return RallyS; }
		void SetStamina(double InStamina) { Stamina = Clamp(InStamina, 0.0, 1.0); }
		double GetTeamSign() const { return TeamSign; }
		bool IsSprinting() const { return bSprinting; }
		// Split-step hop in progress, for animation.
		bool IsSplitHopping() const { return SplitHopLeftS > 0.0; }
		// Time left in the hop, so animation can shape it (read-only, F1 body dynamics).
		double GetSplitHopLeftS() const { return SplitHopLeftS; }
		int GetSplitStepCount() const { return SplitStepCount; }
		// Times the wall or net margin stopped the player dead (a velocity component
		// zeroed in one tick). Read by the soak's realism metrics; changes nothing.
		int GetEdgeStopCount() const { return EdgeStopCount; }

	private:
		// Stick directions the gait logic tells apart (team-relative).
		enum class EHeld : std::uint8_t { None, Forward, Right, Left, Back };
		enum class EGait : std::uint8_t { Square, Turning, Turned };
		enum class ERally : std::uint8_t { Unknown, Live, Dead };

		static EHeld Classify(const FVec2& UnitDir);
		// Classify with Movement.json's GaitHysteresisDeg around the 45-degree lines,
		// starting from the direction held last tick.
		EHeld ClassifyHeld(const FVec2& UnitDir) const;
		void UpdateGait(EHeld Held, const FVec2& UnitDir, double Top, double Dt, double PlannedDistanceM);
		void LimitAtMargins(FVec3& Desired, double Brake, double Dt) const;
		// The most speed toward a margin Room away that whole ticks of Step can brake away.
		static double MarginSpeed(double Room, double Step, double Dt);
		void MarginBraking(double Brake, double JerkMax, double Dt, bool& bX, bool& bY) const;
		void OwnHalfLimits(double& Lo, double& Hi, double& Side) const;
		void ClampToOwnHalf();
		void UpdateFatigue(bool bEdgeStop, double Dt);
		// With AccelRiseS: brake on an axis once the ramped stop (AccelRamp::StopDistance)
		// would only just end at the glass or net margin; the other axis holds its speed.
		void RampMarginBraking(FVec3& Desired, double Brake, double JerkMax, double Dt);
		// The share of Brake an axis moving at AxisSpeed gets when the player stops.
		double AxisBrake(double Brake, double AxisSpeed) const;

		const FPlayerRatings* Ratings = nullptr;
		const FMovementTuning* Tuning = nullptr;
		double TeamSign = 1.0;
		FVec3 Position;
		FVec3 Velocity;
		FVec3 Home;
		bool bHasHome = false;
		double Stamina = 1.0;
		bool bSprinting = false;
		double TimeSinceSlowS = 0.0;
		double SplitBoostLeftS = 0.0;
		double SplitHopLeftS = 0.0;
		bool bSplitPending = false;
		int SplitStepCount = 0;
		int EdgeStopCount = 0;
		EMovementMode Mode = EMovementMode::Adjust;
		EGait Gait = EGait::Square;
		EHeld HeldDir = EHeld::None;
		EHeld TurnDir = EHeld::None;
		double HeldS = 0.0;
		double TurnLeftS = 0.0;
		FHardEffortCounter HardEfforts;
		int HardEffortCount = 0;
		ERally Rally = ERally::Unknown;
		double RallyS = 0.0;
		AccelRamp::FState Ramp;
		AccelRamp::FPivot Pivot;
		// Braking into a margin on that axis, until stopped or the stick turns away.
		bool bMarginBrakeX = false;
		bool bMarginBrakeY = false;
	};
}
