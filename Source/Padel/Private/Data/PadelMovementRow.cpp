#include "Data/PadelMovementRow.h"

Padel::Shots::FMovementTuning FPadelMovementRow::ToCore() const
{
	Padel::Shots::FMovementTuning Out;
	Out.TopSpeedAt1Mps = TopSpeedAt1Mps;
	Out.TopSpeedAt99Mps = TopSpeedAt99Mps;
	Out.TimeToTopAt1S = TimeToTopAt1S;
	Out.TimeToTopAt99S = TimeToTopAt99S;
	Out.BrakeMult = BrakeMult;
	Out.SprintSpeedBonus = SprintSpeedBonus;
	Out.SprintDrainAt1 = SprintDrainAt1;
	Out.SprintDrainAt99 = SprintDrainAt99;
	Out.RegenAt1 = RegenAt1;
	Out.RegenAt99 = RegenAt99;
	Out.SprintMinStamina = SprintMinStamina;
	Out.SplitMaxSpeedMps = SplitMaxSpeedMps;
	Out.SplitWindowS = SplitWindowS;
	Out.SplitHopS = SplitHopS;
	Out.SplitFirstStepAt1 = SplitFirstStepAt1;
	Out.SplitFirstStepAt99 = SplitFirstStepAt99;
	Out.SplitBoostS = SplitBoostS;
	Out.HomeDriftAt1 = HomeDriftAt1;
	Out.HomeDriftAt99 = HomeDriftAt99;
	Out.HomeArriveM = HomeArriveM;
	Out.WallMarginM = WallMarginM;
	Out.NetMarginM = NetMarginM;
	return Out;
}
