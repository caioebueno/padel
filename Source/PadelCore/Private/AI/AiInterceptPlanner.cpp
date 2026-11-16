#include "AI/AiInterceptPlanner.h"

#include "AI/AiTeamFrame.h"
#include "Shots/MotionCurve.h"

#include <cmath>
#include <limits>

namespace Padel::AI::InterceptPlanner
{
namespace
{
	// The player stands so the ball meets them at the shot model's ideal contact point
	// (GDD 5.2): beside them, on the side it is already on (the forehand when it comes
	// straight at them), and a little in front. Never inside a wall or past the net.
	FAiVec StandPointFor(const FAiVec& Ball, const FInterceptQuery& Query, const FAiPlayerProfile& Profile, const FAiTactics& T)
	{
		const FAiVec2 BallLocal = TeamFrame::ToLocal(Query.HalfSign, Ball);
		const FAiVec2 PlayerLocal = TeamFrame::ToLocal(Query.HalfSign, Query.PlayerPosition);
		const double Across = BallLocal.X - PlayerLocal.X;
		const double Side = std::abs(Across) > T.ContactSideM * 0.25 ? SignOf(Across) : Profile.ForehandSign();
		// Local Y is depth: the player stands deeper than the ball by the forward offset.
		const FAiVec2 StandLocal = { BallLocal.X - Side * T.ContactSideM, BallLocal.Y + T.ContactForwardM };
		FAiVec Stand = TeamFrame::ToCourt(Query.HalfSign, StandLocal);
		const double MaxY = Court::HalfWidth - T.CourtMarginM;
		Stand.Y = ClampD(Stand.Y, -MaxY, MaxY);
		const double Depth = ClampD(TeamFrame::Depth(Query.HalfSign, Stand), T.MinPlayerDepthM, Court::HalfLength - T.CourtMarginM);
		Stand.X = Depth * Query.HalfSign;
		return Stand;
	}
}

double TimeToCover(double Distance, double TopSpeed, double AccelTime)
{
	// The players' shared curve with no falloff: constant acceleration, then cruise. A3
	// passes the movement tuning's AccelFalloff once the planner models it.
	return Shots::MotionCurve::TimeToCover(Distance, TopSpeed, AccelTime, 0.0);
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
		Candidate.StandPoint = StandPointFor(S.Position, Query, Profile, T);
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

		// Near the net an overhead is the attack (GDD 4, 8: bandeja, víbora, smash when
		// short and high), so it has its own ideal height, up where those are played.
		// From the back a high ball is better let down or off the glass, so a deep
		// overhead is scored against waist height like any other ball, with half of
		// that height penalty given back.
		const bool bOverhead = Candidate.Kind == EContactKind::Overhead;
		const bool bAttackingOverhead = bOverhead && Depth < T.SmashMaxDepthM;
		const double IdealHeight = bAttackingOverhead ? T.OverheadIdealHeightM : T.IdealContactHeightM;
		double Score = -std::abs(Height - IdealHeight) * T.ScoreHeightWeight
			+ ClampD(Candidate.Slack, 0.0, T.SlackCapS) * T.ScoreSlackWeight
			- S.T * T.ScoreTimeWeight;
		if (Candidate.Kind == EContactKind::Volley && bAtNet)
		{
			Score += T.VolleyAtNetBonus;
		}
		if (bAttackingOverhead)
		{
			Score += T.OverheadBonus;
		}
		else if (bOverhead)
		{
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
