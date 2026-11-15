#include "AI/AiInterceptPlanner.h"

#include "AI/AiTeamFrame.h"

#include <cmath>
#include <limits>

namespace Padel::AI::InterceptPlanner
{
namespace
{
	// The player stands beside the ball, a contact offset away from it (GDD 5.2),
	// on the side they are coming from, and never inside a wall or past the net.
	FAiVec StandPointFor(const FAiVec& Ball, const FInterceptQuery& Query, const FAiTactics& T)
	{
		FAiVec Away = Query.PlayerPosition - Ball;
		Away.Z = 0.0;
		double Len = Away.Length2D();
		if (Len < 1e-6)
		{
			// Standing on the ball's spot: step back toward the own glass.
			Away = { static_cast<double>(Query.HalfSign), 0.0, 0.0 };
			Len = 1.0;
		}
		FAiVec Stand = Ball + Away * (T.ContactOffsetM / Len);
		Stand.Z = 0.0;
		const double MaxY = Court::HalfWidth - T.CourtMarginM;
		Stand.Y = ClampD(Stand.Y, -MaxY, MaxY);
		const double Depth = ClampD(TeamFrame::Depth(Query.HalfSign, Stand), T.MinPlayerDepthM, Court::HalfLength - T.CourtMarginM);
		Stand.X = Depth * Query.HalfSign;
		return Stand;
	}
}

double TimeToCover(double Distance, double TopSpeed, double AccelTime)
{
	if (Distance <= 0.0)
	{
		return 0.0;
	}
	if (TopSpeed <= 0.0)
	{
		return std::numeric_limits<double>::infinity();
	}
	if (AccelTime <= 0.0)
	{
		return Distance / TopSpeed;
	}
	const double Accel = TopSpeed / AccelTime;
	const double AccelDistance = 0.5 * TopSpeed * AccelTime;
	if (Distance <= AccelDistance)
	{
		return std::sqrt(2.0 * Distance / Accel);
	}
	return AccelTime + (Distance - AccelDistance) / TopSpeed;
}

std::optional<FInterceptPlan> Plan(const std::vector<FTrajectorySample>& Samples, const FInterceptQuery& Query,
	const FAiPlayerProfile& Profile, const FAiTactics& T)
{
	const double TopSpeed = Profile.TopSpeed(T) * T.PlanningSpeedFactor;
	const double AccelTime = Profile.AccelTime(T);
	const double MaxHeight = Profile.MaxContactHeight(T);
	const double PlayerDepth = TeamFrame::Depth(Query.HalfSign, Query.PlayerPosition);
	const bool bAtNet = PlayerDepth < T.AtNetDepthM;

	std::optional<FInterceptPlan> Best;
	double BestScore = -std::numeric_limits<double>::infinity();
	std::optional<FInterceptPlan> Closest;

	for (const FTrajectorySample& S : Samples)
	{
		if (S.Half != Query.HalfSign || S.GroundBounces >= 2)
		{
			continue;
		}
		if (Query.bMustBounce && S.GroundBounces == 0)
		{
			continue;
		}
		const double Height = S.Position.Z;
		if (Height < T.MinContactHeightM || Height > MaxHeight)
		{
			continue;
		}
		// The player can't stand in the glass, and the ball must still be over the court.
		const double Depth = TeamFrame::Depth(Query.HalfSign, S.Position);
		if (std::abs(S.Position.Y) > Court::HalfWidth - T.InterceptWallMarginM
			|| Depth > Court::HalfLength - T.InterceptWallMarginM || Depth < 0.0)
		{
			continue;
		}

		FInterceptPlan Candidate;
		Candidate.Sample = S;
		Candidate.Kind = Height >= T.OverheadMinHeightM ? EContactKind::Overhead
			: (S.GroundBounces == 0 ? EContactKind::Volley : EContactKind::Groundstroke);
		Candidate.StandPoint = StandPointFor(S.Position, Query, T);
		FAiVec ToStand = Candidate.StandPoint - Query.PlayerPosition;
		ToStand.Z = 0.0;
		double Distance = ToStand.Length2D();
		double Turn = 0.0;
		if (Distance > 1e-6 && AccelTime > 0.0)
		{
			// Velocity away from the stand point has to be cancelled first.
			const double Accel = TopSpeed / AccelTime;
			const double Away = -(Query.PlayerVelocity.X * ToStand.X + Query.PlayerVelocity.Y * ToStand.Y) / Distance;
			if (Away > 0.0)
			{
				Turn = Away / Accel;
				Distance += Away * Away / (2.0 * Accel);
			}
		}
		Candidate.TimeNeeded = Query.StartDelayS + Turn + TimeToCover(Distance, TopSpeed, AccelTime);
		Candidate.Slack = S.T - Candidate.TimeNeeded;
		Candidate.bReachable = Candidate.Slack >= 0.0;

		if (!Candidate.bReachable)
		{
			if (!Closest || Candidate.Slack > Closest->Slack)
			{
				Closest = Candidate;
			}
			continue;
		}

		double Score = -std::abs(Height - T.IdealContactHeightM) * T.ScoreHeightWeight
			+ ClampD(Candidate.Slack, 0.0, T.SlackCapS) * T.ScoreSlackWeight
			- S.T * T.ScoreTimeWeight;
		if (Candidate.Kind == EContactKind::Volley && bAtNet)
		{
			Score += T.VolleyAtNetBonus;
		}
		if (Candidate.Kind == EContactKind::Overhead)
		{
			// Overheads are scored against the ideal height too; give them their own credit.
			Score += T.OverheadBonus + std::abs(Height - T.IdealContactHeightM) * T.ScoreHeightWeight * 0.5;
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best ? Best : Closest;
}
}
