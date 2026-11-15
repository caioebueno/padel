#include "AI/AiPlayerProfile.h"

namespace Padel::AI
{
namespace
{
	int ClampRating(int Value)
	{
		return Value < 1 ? 1 : (Value > 99 ? 99 : Value);
	}
}

FAiPlayerProfile FAiPlayerProfile::WithOffset(int Offset) const
{
	FAiPlayerProfile Out = *this;
	for (int* Rating : { &Out.Speed, &Out.Acceleration, &Out.Reach, &Out.Jump, &Out.WallPlay,
			 &Out.Anticipation, &Out.Positioning, &Out.Teamwork, &Out.ReturnSkill })
	{
		*Rating = ClampRating(*Rating + Offset);
	}
	return Out;
}

double FAiPlayerProfile::TopSpeed(const FAiTactics& T) const
{
	return ByRating(Speed, T.SpeedAt1Mps, T.SpeedAt99Mps);
}

double FAiPlayerProfile::AccelTime(const FAiTactics& T) const
{
	return ByRating(Acceleration, T.AccelTimeAt1S, T.AccelTimeAt99S);
}

double FAiPlayerProfile::SideReach(const FAiTactics& T) const
{
	return ByRating(Reach, T.ReachSideAt1M, T.ReachSideAt99M);
}

double FAiPlayerProfile::MaxContactHeight(const FAiTactics& T) const
{
	return ByRating(Reach, T.OverheadReachAt1M, T.OverheadReachAt99M) + ByRating(Jump, T.JumpExtraAt1M, T.JumpExtraAt99M);
}

double FAiPlayerProfile::WallNoise(const FAiTactics& T, const FAiDifficulty& D) const
{
	return ByRating(WallPlay, T.WallNoiseAt1M, T.WallNoiseAt99M) * D.PredictionNoiseMult;
}

double FAiPlayerProfile::ReadDelay(const FAiTactics& T, const FAiDifficulty& D) const
{
	return D.ReadDelayS * ByRating(Anticipation, T.AnticipationDelayScaleAt1, T.AnticipationDelayScaleAt99);
}
}
