#include "AI/AiTeamController.h"

#include "AI/AiTeamFrame.h"

#include <algorithm>
#include <cmath>

namespace Padel::AI
{
FAiTeamController::FAiTeamController(const FAiTeamSetup& Setup, const FAiTactics& InTactics, const FAiDifficulty& InDifficulty,
	const IAiBallModel& InModel)
	: HalfSign(Setup.HalfSign)
	, HumanSlot(Setup.HumanSlot)
	, Tactics(InTactics)
	, Difficulty(InDifficulty)
	, Predictor(InModel, InTactics)
	, Pair(InTactics)
	, Rng(Setup.Seed)
{
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		// GDD 8: the difficulty's attribute scale applies to the AI's players only.
		Profiles[Slot] = IsAi(Slot) ? Setup.Profiles[Slot].WithOffset(Difficulty.AttributeOffset) : Setup.Profiles[Slot];
	}
}

void FAiTeamController::OnPointStarted(bool bServing)
{
	(void)bServing;
	// Both pairs start a point from the back: the server serves from behind the
	// service line and follows the serve in (AiPairTactics::OnOwnShot).
	Pair.SetFormation(EFormation::BothBack);
	Pair.ClearCommands();
	ReadAt = -1.0;
	bIncoming = false;
	bShotSent = false;
	Taker = -1;
	Plans = {};
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		// GDD 6.1 Positioning: how close to the ideal spot the player stands.
		const double Radius = Tactics.HomeErrorMaxM * (1.0 - ByRating(Profiles[Slot].Positioning, 0.0, 1.0));
		const double Angle = Rng.Range(0.0, 6.283185307179586);
		const double R = Radius * std::sqrt(Rng.NextDouble());
		HomeError[Slot] = { std::cos(Angle) * R, std::sin(Angle) * R };
	}
}

void FAiTeamController::OnPointEnded()
{
	Pair.ClearCommands();
	ReadAt = -1.0;
	bIncoming = false;
	Taker = -1;
	Plans = {};
}

double FAiTeamController::TeamReadDelay() const
{
	double Sum = 0.0;
	int Count = 0;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		if (IsAi(Slot))
		{
			Sum += Profiles[Slot].ReadDelay(Tactics, Difficulty);
			++Count;
		}
	}
	return Count > 0 ? Sum / Count : 0.0;
}

void FAiTeamController::OnBallHit(double Time, int HitterHalfSign, EAiShot Shot, const FAiVec& Contact, const FAiVec& Target)
{
	Plans = {};
	Taker = -1;
	bShotSent = false;
	if (HitterHalfSign == HalfSign)
	{
		Pair.OnOwnShot(Shot, TeamFrame::Depth(HalfSign, Contact), TeamFrame::Depth(-HalfSign, Target));
		Pair.ConsumeYours();
		bIncoming = false;
		ReadAt = -1.0;
		return;
	}
	LastOpponentShot = Shot;
	bIncoming = true;
	// GDD 8: the AI reacts after a read delay that the difficulty and Anticipation set.
	ReadAt = Time + TeamReadDelay();
}

void FAiTeamController::OnBallContact(double Time, Court::ESurface Surface)
{
	if (!bIncoming)
	{
		return;
	}
	// The mesh is random, so read it again at once; the glass is predictable but the
	// first read carried Wall Play noise, so a second look sharpens it.
	if (Surface == Court::ESurface::Mesh)
	{
		ReadAt = Time;
	}
	else if (Surface == Court::ESurface::Glass && !bShotSent)
	{
		const double Again = Time + TeamReadDelay() * Tactics.GlassRereadDelayScale;
		ReadAt = ReadAt < 0.0 ? Again : std::min(ReadAt, Again);
	}
}

bool FAiTeamController::GiveCommand(EPartnerCommand Command)
{
	if (HumanSlot < 0)
	{
		return false;
	}
	const int Partner = 1 - HumanSlot;
	Pair.SetCommand(Partner, Command);
	return true;
}

int FAiTeamController::ChooseGoldenPointReceiver() const
{
	return Profiles[LeftSlot].ReturnSkill > Profiles[RightSlot].ReturnSkill ? LeftSlot : RightSlot;
}

FAiShotDecision FAiTeamController::ChooseServe(const FAiVec& ServerPosition)
{
	return ShotChooser::ChooseServe(HalfSign, ServerPosition, Tactics, Difficulty, Rng);
}

void FAiTeamController::Read(const FAiWorldView& View)
{
	ReadAt = -1.0;
	PlanTime = View.Time;
	Plans = {};
	FTakerQuery Query;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		// The human's read is modelled without noise: the AI only uses it to decide
		// which of the two should go.
		const double Noise = IsAi(Slot) ? Profiles[Slot].WallNoise(Tactics, Difficulty) : 0.0;
		const std::vector<FTrajectorySample> Samples = Predictor.Predict(View.Ball, Noise, Rng);
		FInterceptQuery Intercept;
		Intercept.PlayerPosition = View.OwnPositions[Slot];
		Intercept.PlayerVelocity = View.OwnVelocities[Slot];
		Intercept.HalfSign = HalfSign;
		Intercept.bMustBounce = View.bMustBounce;
		Plans[Slot] = InterceptPlanner::Plan(Samples, Intercept, Profiles[Slot], Tactics);
		Query.Plans[Slot] = Plans[Slot];
		Query.PlayerLocal[Slot] = TeamFrame::ToLocal(HalfSign, View.OwnPositions[Slot]);
		Query.Profiles[Slot] = Profiles[Slot];
	}
	if (!Plans[RightSlot] && !Plans[LeftSlot])
	{
		Taker = -1;
		return;
	}
	const FInterceptPlan& Any = Plans[RightSlot] ? *Plans[RightSlot] : *Plans[LeftSlot];
	Query.BallLocalX = TeamFrame::ToLocal(HalfSign, Any.Sample.Position).X;
	Query.Teamwork = std::min(Profiles[RightSlot].Teamwork, Profiles[LeftSlot].Teamwork);
	const int PreviousTaker = Taker;
	Taker = Pair.ChooseTaker(Query, Rng);
	if (Taker != PreviousTaker)
	{
		// A new read can hand the ball to the partner (the mesh, a second look at the
		// glass): the new taker still needs its shot.
		bShotSent = false;
	}
	// GDD 8: a lob over them sends the pair back, but a lob they can meet in the air
	// is a bandeja that keeps the net (GDD 4).
	const bool bOverhead = Taker >= 0 && Plans[Taker] && Plans[Taker]->bReachable && Plans[Taker]->Kind == EContactKind::Overhead;
	if (!bOverhead)
	{
		Pair.OnOpponentShot(LastOpponentShot);
	}
}

FAiIntent FAiTeamController::MoveTo(int Slot, const FAiVec& From, const FAiVec& Target, bool bChasing, double Slack) const
{
	(void)Slot;
	FAiIntent Intent;
	FAiVec Delta = Target - From;
	Delta.Z = 0.0;
	const double Distance = Delta.Length2D();
	if (Distance <= Tactics.ArriveRadiusM)
	{
		return Intent;
	}
	// Slow into the spot so the player arrives still (GDD 6.3 split-step needs it).
	const double Speed = bChasing ? 1.0 : ClampD(Distance / Tactics.SlowRadiusM, 0.0, 1.0);
	Intent.Move = TeamFrame::MoveVector(HalfSign, Delta * (Speed / Distance));
	Intent.bSprint = bChasing ? (Slack < Tactics.SprintSlackS || Distance > Tactics.SprintDistanceM) : Distance > Tactics.SprintDistanceM;
	return Intent;
}

std::array<std::optional<FAiIntent>, 2> FAiTeamController::Tick(const FAiWorldView& View)
{
	if (HumanSlot >= 0)
	{
		Pair.FollowHuman(TeamFrame::Depth(HalfSign, View.OwnPositions[HumanSlot]));
	}
	if (ReadAt >= 0.0 && View.Time >= ReadAt)
	{
		Read(View);
	}

	const FAiVec2 BallLocal = TeamFrame::ToLocal(HalfSign, View.Ball.Position);
	const std::array<FAiVec2, 2> Homes = Pair.HomePositions(BallLocal);
	std::array<std::optional<FAiIntent>, 2> Out;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		if (!IsAi(Slot))
		{
			continue;
		}
		const bool bTaking = Taker == Slot && Plans[Slot].has_value();
		FAiIntent Intent;
		if (bTaking)
		{
			const FInterceptPlan& Plan = *Plans[Slot];
			Intent = MoveTo(Slot, View.OwnPositions[Slot], Plan.StandPoint, true, Plan.Slack);
			const double ContactTime = PlanTime + Plan.Sample.T;
			if (!bShotSent && View.Time >= ContactTime - Tactics.SwingLeadS)
			{
				FShotContext Context;
				Context.HalfSign = HalfSign;
				Context.Contact = Plan.Sample.Position;
				Context.Kind = Plan.Kind;
				Context.bOffGlass = Plan.Sample.bOffGlass;
				Context.Slack = Plan.Slack;
				Context.Profile = Profiles[Slot];
				Context.Opponents = { View.OpponentPositions[0], View.OpponentPositions[1] };
				Intent.Shot = ShotChooser::ChooseRallyShot(Context, Tactics, Difficulty, Rng);
				Intent.ShotContactTime = ContactTime;
				bShotSent = true;
			}
		}
		else
		{
			const FAiVec Home = TeamFrame::ToCourt(HalfSign, Homes[Slot] + HomeError[Slot]);
			Intent = MoveTo(Slot, View.OwnPositions[Slot], Home, false, 0.0);
		}
		Out[Slot] = Intent;
	}
	return Out;
}
}
