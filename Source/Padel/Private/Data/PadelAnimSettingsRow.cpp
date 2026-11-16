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
	Out.ReadyRacket = { ReadyRacketRightM, ReadyRacketForwardM, ReadyRacketUpM };
	Out.FacingTurnRateDegPerS = FacingTurnRateDegPerS;
	Out.ReadyCrouchAlphaNet = ReadyCrouchAlphaNet;
	Out.ReadyCrouchAlphaBack = ReadyCrouchAlphaBack;
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
