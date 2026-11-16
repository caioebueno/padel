// Player movement on the ground (GDD 6.1, 6.3): acceleration from Acceleration, falling as
// speed rises (Shots/MotionCurve.h), top speed from Speed, sprint and stamina, the
// automatic split-step and the home drift.
// Fixed-step and engine-free, so the AI drives it with the same move vector a stick gives.

#pragma once

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
		// SpeedMult < 1 while swinging (planted feet).
		void Step(const FVec2& MoveDir, bool bSprint, double Dt, double SpeedMult = 1.0);

		// GDD 6.3: called at the opponent's contact. Returns true if the player split-stepped.
		bool NotifyOpponentContact();
		// The split-step removes the running penalty for the next shot only (GDD 5.2).
		bool ConsumeSplitStep();

		void SetHome(const FVec3& InHome) { Home = InHome; bHasHome = true; }
		void ClearHome() { bHasHome = false; }
		void Teleport(const FVec3& InPosition);

		double TopSpeed(bool bSprinting) const;
		// Top speed over the initial acceleration (MotionCurve); the real time to top speed
		// is longer when Movement.json's AccelFalloff is above 0.
		double TimeToTopSpeed() const;

		const FVec3& GetPosition() const { return Position; }
		const FVec3& GetVelocity() const { return Velocity; }
		double GetStamina() const { return Stamina; }
		void SetStamina(double InStamina) { Stamina = Clamp(InStamina, 0.0, 1.0); }
		double GetTeamSign() const { return TeamSign; }
		bool IsSprinting() const { return bSprinting; }
		// Split-step hop in progress, for animation.
		bool IsSplitHopping() const { return SplitHopLeftS > 0.0; }
		int GetSplitStepCount() const { return SplitStepCount; }
		// Times the wall or net margin stopped the player dead (a velocity component
		// zeroed in one tick). Read by the soak's realism metrics; changes nothing.
		int GetEdgeStopCount() const { return EdgeStopCount; }

	private:
		void ClampToOwnHalf();

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
	};
}
