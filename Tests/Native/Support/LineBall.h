// A ball that moves in a straight line at constant velocity, from the other team, so a
// shot test can put the ball exactly where it wants it at the contact moment.
#pragma once

#include "Shots/BallPort.h"
#include "Shots/ShotMath.h"

#include <vector>

namespace ShotTestSupport
{
using namespace Padel;
using namespace Padel::Shots;

class FLineBall final : public IBallPort
{
public:
	FLineBall(const FVec3& InStart, const FVec3& InVelocity, int InBounces)
		: Start(InStart), Velocity(InVelocity), Bounces(InBounces)
	{
	}

	void Advance(double S) { Time += S; }

	FBallView Current() const override { return At(0.0); }
	int LastHitterTeam() const override { return -1; }
	std::vector<FBallView> Predict(double HorizonS, double StepS) const override
	{
		std::vector<FBallView> Out;
		for (double T = StepS; T <= HorizonS + 1e-9; T += StepS)
		{
			Out.push_back(At(T));
		}
		return Out;
	}
	void ApplyShot(const FShotIntent& InIntent) override
	{
		Intent = InIntent;
		bHit = true;
	}
	void ServeToss(const FVec3&, int) override {}

	bool bHit = false;
	FShotIntent Intent;

private:
	FBallView At(double Ahead) const
	{
		FBallView V;
		V.T = Ahead;
		V.Position = Add(Start, Scale(Velocity, Time + Ahead));
		V.Velocity = Velocity;
		V.GroundBounces = Bounces;
		V.bLive = !bHit;
		return V;
	}

	FVec3 Start;
	FVec3 Velocity;
	int Bounces = 0;
	double Time = 0.0;
};
}
