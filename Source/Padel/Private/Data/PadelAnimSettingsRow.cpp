#include "Data/PadelAnimSettingsRow.h"

Padel::Animation::FAnimSettings FPadelAnimSettingsRow::ToCore() const
{
	Padel::Animation::FAnimSettings Out;
	Out.WalkMinMps = WalkMinMps;
	Out.SprintMinMps = SprintMinMps;
	Out.RunClipSpeedMps = RunClipSpeedMps;
	Out.SprintClipSpeedMps = SprintClipSpeedMps;
	Out.StrideRateMin = StrideRateMin;
	Out.StrideRateMax = StrideRateMax;
	Out.ReadyBallWindowS = ReadyBallWindowS;
	Out.PlantedMaxSpeedMps = PlantedMaxSpeedMps;
	Out.FullBodyBlendPerS = FullBodyBlendPerS;
	Out.SwingBlendInS = SwingBlendInS;
	Out.ClipCrossfadeS = ClipCrossfadeS;
	Out.CelebrationS = CelebrationS;
	Out.CelebrationBlendS = CelebrationBlendS;
	Out.FacingTurnRateDegPerS = FacingTurnRateDegPerS;
	Out.GlassBackTurnDeg = GlassBackTurnDeg;
	Out.GlassSideTurnDeg = GlassSideTurnDeg;
	Out.BackRunTurnDeg = BackRunTurnDeg;
	for (const FPadelStanceRow& Row : Stances)
	{
		Padel::Animation::EStance Stance;
		if (Padel::Animation::ParseStance(TCHAR_TO_UTF8(*Row.Name), Stance))
		{
			Padel::Animation::FStancePose& P = Out.Stances[static_cast<std::size_t>(Stance)];
			P.CrouchAlpha = Row.CrouchAlpha;
			P.Racket = { Row.RacketRightM, Row.RacketForwardM, Row.RacketUpM };
			P.ArmAlpha = Row.ArmAlpha;
			P.WidenM = Row.WidenM;
			P.DropM = Row.DropM;
			P.TrunkDeg = Row.TrunkDeg;
		}
	}
	Out.StanceNetUntilM = StanceNetUntilM;
	Out.StanceBackFromM = StanceBackFromM;
	Out.StanceHalfLifeS = StanceHalfLifeS;
	Out.HeadLookHalfLifeS = HeadLookHalfLifeS;
	Out.HeadLookMaxYawDeg = HeadLookMaxYawDeg;
	Out.HeadLookMinPitchDeg = HeadLookMinPitchDeg;
	Out.HeadLookMaxPitchDeg = HeadLookMaxPitchDeg;
	Out.HeadLookEyeHeightM = HeadLookEyeHeightM;
	Out.FacingHalfLifeS = FacingHalfLifeS;
	Out.FacingChestHalfLifeS = FacingChestHalfLifeS;
	Out.FacingHeadHalfLifeS = FacingHeadHalfLifeS;
	Out.FacingMaxChestTwistDeg = FacingMaxChestTwistDeg;
	Out.FacingMaxHeadTwistDeg = FacingMaxHeadTwistDeg;
	Out.LeanDegPerMps2 = LeanDegPerMps2;
	Out.LeanMaxDeg = LeanMaxDeg;
	Out.LeanHalfLifeS = LeanHalfLifeS;
	Out.PlantDropPerMps2 = PlantDropPerMps2;
	Out.PlantDropMaxM = PlantDropMaxM;
	Out.PelvisHalfLifeS = PelvisHalfLifeS;
	Out.SplitHopHeightM = SplitHopHeightM;
	Out.SplitLandDropM = SplitLandDropM;
	Out.SplitLandS = SplitLandS;
	Out.ReadyBounceM = ReadyBounceM;
	Out.ReadyBounceHz = ReadyBounceHz;
	Out.ReadyBounceHalfLifeS = ReadyBounceHalfLifeS;
	for (const double T : TrajectoryTimesS)
	{
		Out.TrajectoryTimesS.push_back(T);
	}
	for (const FString& Asset : CelebrateAssets)
	{
		Out.CelebrateAssets.emplace_back(TCHAR_TO_UTF8(*Asset));
	}
	for (const FString& Asset : DejectedAssets)
	{
		Out.DejectedAssets.emplace_back(TCHAR_TO_UTF8(*Asset));
	}
	return Out;
}
