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
	Out.AccelFalloff = AccelFalloff;
	Out.AccelRiseS = AccelRiseS;
	Out.PivotMinDeg = PivotMinDeg;
	Out.PivotSpeedMps = PivotSpeedMps;
	Out.PivotPlantAt1S = PivotPlantAt1S;
	Out.PivotPlantAt99S = PivotPlantAt99S;
	Out.ShuffleSpeedFrac = ShuffleSpeedFrac;
	Out.BackpedalSpeedFrac = BackpedalSpeedFrac;
	Out.TurnToRunAt1S = TurnToRunAt1S;
	Out.TurnToRunAt99S = TurnToRunAt99S;
	Out.CrossoverMinDistM = CrossoverMinDistM;
	Out.TurnBackMinDistM = TurnBackMinDistM;
	Out.GaitHysteresisDeg = GaitHysteresisDeg;
	Out.EdgeStopToleranceMps = EdgeStopToleranceMps;
	Out.FatigueHardAccelMps2 = FatigueHardAccelMps2;
	Out.FatigueDrainPerHardAccelAt1 = FatigueDrainPerHardAccelAt1;
	Out.FatigueDrainPerHardAccelAt99 = FatigueDrainPerHardAccelAt99;
	Out.FatigueRallyStartS = FatigueRallyStartS;
	Out.FatigueDrainPerRallySAt1 = FatigueDrainPerRallySAt1;
	Out.FatigueDrainPerRallySAt99 = FatigueDrainPerRallySAt99;
	Out.FatigueBetweenPointRestS = FatigueBetweenPointRestS;
	Out.FatigueTopSpeedLossAtZero = FatigueTopSpeedLossAtZero;
	return Out;
}
