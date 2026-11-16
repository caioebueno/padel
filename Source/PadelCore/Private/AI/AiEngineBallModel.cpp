#include "AI/AiEngineBallModel.h"

#include "AI/AiEngineBridge.h"

namespace Padel::AI
{
std::vector<FAiContact> FAiEngineBallModel::Step(FAiBall& State, double Dt) const
{
	FBallState Engine = Bridge::ToEngine(State);
	std::vector<FBallContact> Contacts;
	Sim.Step(Engine, Dt, nullptr, Contacts);
	State = Bridge::ToAi(Engine);

	std::vector<FAiContact> Out;
	for (const FBallContact& C : Contacts)
	{
		// The roll report counts as a bounce: GDD 3.3, a rolling ball has bounced twice.
		if (C.Kind == Padel::EContactKind::Surface)
		{
			Out.push_back({ C.Surface, Bridge::ToAi(C.Position) });
		}
	}
	return Out;
}

bool FAiEngineBallModel::IsDead(const FAiBall& State) const
{
	return !State.bInCourt || State.bRolling;
}
}
