// A throwaway stand-in ball for the ball machine practice, behind IBallPort, until the
// match engine's ball sim (Agent 2) lands and gets its own IBallPort adapter. Gravity
// and simple bounces off the ground, glass, mesh and net; no spin in flight. It is
// deliberately small: the real physics belongs to the engine.

#pragma once

#include "Shots/ShotMath.h"

#include "Shots/BallPort.h"
#include "Shots/PracticeBallParams.h"

namespace Padel::Shots
{
	class PADELCORE_API FPracticeBall : public IBallPort
	{
	public:
		explicit FPracticeBall(const FPracticeBallParams* InParams) : Params(InParams) {}

		// IBallPort
		FBallView Current() const override;
		int LastHitterTeam() const override { return LastHitter; }
		std::vector<FBallView> Predict(double HorizonS, double StepS) const override;
		void ApplyShot(const FShotIntent& Intent) override;
		void ServeToss(const FVec3& At, int TeamSign) override;

		// Ball machine feed: launched as if hit by FromTeam.
		void Feed(const FVec3& From, const FVec3& Target, double SpeedMps, double ApexM, int FromTeam);

		// Advances by Dt in fixed sub-steps.
		void Advance(double Dt);

		bool IsLive() const { return bLive; }
		// The ball has finished and lingered (or was never put in play): ready for the next one.
		bool IsDone() const { return !bLive && (!bEverLaunched || DeadForS >= Params->DeadLingerS); }
		const FVec3& GetPosition() const { return Position; }
		int GetHitCount() const { return HitCount; }

		// Drag-free launch velocity from Origin to Target: by apex when ApexM > 0, else
		// the flatter arc at SpeedMps (45 degrees when the target is out of range).
		static FVec3 SolveLaunch(const FVec3& Origin, const FVec3& Target, double SpeedMps, double ApexM, double Gravity);

	private:
		void SubStep(double Dt);
		void Kill() { bLive = false; }

		const FPracticeBallParams* Params = nullptr;
		FVec3 Position;
		FVec3 Velocity;
		bool bLive = false;
		bool bEverLaunched = false;
		int LastHitter = 0;
		int GroundBounces = 0;
		bool bOffGlass = false;
		double DeadForS = 0.0;
		double Accumulator = 0.0;
		int HitCount = 0;
	};
}
