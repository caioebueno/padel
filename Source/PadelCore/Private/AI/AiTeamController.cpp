#include "AI/AiTeamController.h"
#include "PadelMath.h"

#include "AI/AiTeamFrame.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Padel::AI
{
FAiTeamController::FAiTeamController(const FAiTeamSetup& Setup, const FAiTactics& InTactics, const FAiDifficulty& InDifficulty,
	const IAiBallModel& InModel)
	: HalfSign(Setup.HalfSign)
	, HumanSlot(Setup.HumanSlot)
	, AccelFalloff(Setup.AccelFalloff)
	, BrakeMult(Setup.BrakeMult)
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
	// GDD 8: a side switch lasts until the point ends; each point starts on their own sides.
	Pair.SetSwapped(false);
	ReadAt = -1.0;
	FirstStepAt = { -1.0, -1.0 };
	bSplitLanding = { false, false };
	bCaptureHoldBall = false;
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
		HomeError[Slot] = { Padel::Math::Cos(Angle) * R, Padel::Math::Sin(Angle) * R };
	}
}

void FAiTeamController::OnPointEnded()
{
	Pair.ClearCommands();
	Pair.SetSwapped(false);
	ReadAt = -1.0;
	FirstStepAt = { -1.0, -1.0 };
	bSplitLanding = { false, false };
	bCaptureHoldBall = false;
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
		FirstStepAt = { -1.0, -1.0 };
	bSplitLanding = { false, false };
		bCaptureHoldBall = false;
		return;
	}
	LastOpponentShot = Shot;
	bIncoming = true;
	// GDD 8: the AI reacts after a read delay that the difficulty and Anticipation set.
	ReadAt = Time + TeamReadDelay();
	// Nobody moves on the new ball before the read; a split-step adds its landing (OnSplitStep).
	FirstStepAt = { ReadAt, ReadAt };
	bSplitLanding = { false, false };
	bCaptureHoldBall = true;
}

void FAiTeamController::OnSplitStep(int Slot)
{
	if (Slot >= 0 && Slot < 2 && FirstStepAt[Slot] >= 0.0)
	{
		FirstStepAt[Slot] += Tactics.FirstStepExtraS;
		bSplitLanding[Slot] = true;
	}
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

FAiGlassRead FAiTeamController::GetGlassRead(int Slot) const
{
	FAiGlassRead Out;
	if (Slot < 0 || Slot > 1 || !IsAi(Slot) || Taker != Slot || !Plans[Slot] || !Plans[Slot]->bReachable || !Plans[Slot]->Sample.bOffGlass)
	{
		return Out;
	}
	Out.bPlayingOffGlass = true;
	Out.Wall = Plans[Slot]->Sample.GlassWall;
	return Out;
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
	std::array<std::vector<FTrajectorySample>, 2> SlotSamples;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		// The human's read is modelled without noise: the AI only uses it to decide
		// which of the two should go.
		const double Noise = IsAi(Slot) ? Profiles[Slot].WallNoise(Tactics, Difficulty) : 0.0;
		std::vector<FTrajectorySample> Samples = Predictor.Predict(View.Ball, Noise, Rng);
		FInterceptQuery Intercept;
		Intercept.PlayerPosition = View.OwnPositions[Slot];
		Intercept.PlayerVelocity = View.OwnVelocities[Slot];
		Intercept.HalfSign = HalfSign;
		Intercept.bMustBounce = View.bMustBounce;
		Intercept.AccelFalloff = AccelFalloff;
		// The split-step landing still to come before this player can push off.
		Intercept.StartDelayS = FirstStepAt[Slot] >= 0.0 ? std::max(0.0, FirstStepAt[Slot] - View.Time) : 0.0;
		Plans[Slot] = InterceptPlanner::Plan(Samples, Intercept, Profiles[Slot], Tactics);
		Query.Plans[Slot] = Plans[Slot];
		Query.PlayerLocal[Slot] = TeamFrame::ToLocal(HalfSign, View.OwnPositions[Slot]);
		Query.Profiles[Slot] = Profiles[Slot];
		SlotSamples[Slot] = std::move(Samples);
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
	const int Crosser = SideSwitchTaker(Query, SlotSamples);
	if (Crosser >= 0)
	{
		Pair.SetSwapped(!Pair.IsSwapped());
		Taker = Crosser;
	}
	else
	{
		Taker = Pair.ChooseTaker(Query, Rng);
	}
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

int FAiTeamController::SideSwitchTaker(const FTakerQuery& Query, const std::array<std::vector<FTrajectorySample>, 2>& SlotSamples) const
{
	// GDD 8 side switch. A human picks their own side and gives the commands, so only an
	// all-AI pair crosses on its own; once crossed they stay so until the point ends.
	if (HumanSlot >= 0 || Pair.IsSwapped() || Tactics.SideSwitchMinLobDepthM <= 0.0 || Pair.GetFormation() != EFormation::BothNet)
	{
		return -1;
	}
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		if (Pair.GetCommand(Slot) != EPartnerCommand::None)
		{
			return -1;  // GDD 7: a command overrides the pair's own tactics
		}
	}
	const std::vector<FTrajectorySample>& Samples = SlotSamples[RightSlot].empty() ? SlotSamples[LeftSlot] : SlotSamples[RightSlot];
	const double PairDepth = (Query.PlayerLocal[RightSlot].Y + Query.PlayerLocal[LeftSlot].Y) * 0.5;
	std::optional<FAiVec2> Over;
	std::optional<FAiVec2> Landing;
	for (const FTrajectorySample& S : Samples)
	{
		if (S.Half != HalfSign)
		{
			continue;
		}
		const FAiVec2 Local = TeamFrame::ToLocal(HalfSign, S.Position);
		if (!Over && Local.Y >= PairDepth)
		{
			Over = Local;
		}
		if (S.GroundBounces >= 1)
		{
			Landing = Local;
			break;
		}
	}
	if (!Over || !Landing)
	{
		return -1;
	}
	const int Lobbed = Pair.LobbedSlot(LastOpponentShot, *Over, *Landing, Query.PlayerLocal);
	if (Lobbed < 0)
	{
		return -1;
	}
	// The partner crosses behind only when they get there with more time than the lobbed
	// player turning round would have; otherwise the lobbed player chases it as usual and the
	// pair keeps its sides (a needless cross leaves a hole while the two are crossing).
	const int Partner = 1 - Lobbed;
	if (!Query.Plans[Partner] || !Query.Plans[Partner]->bReachable)
	{
		return -1;
	}
	const bool bLobbedReaches = Query.Plans[Lobbed] && Query.Plans[Lobbed]->bReachable;
	return !bLobbedReaches || Query.Plans[Partner]->Slack > Query.Plans[Lobbed]->Slack ? Partner : -1;
}

FAiIntent FAiTeamController::MoveTo(int Slot, const FAiVec& From, const FAiVec& Target, bool bChasing, double Slack) const
{
	FAiIntent Intent;
	FAiVec Delta = Target - From;
	Delta.Z = 0.0;
	const double Distance = Delta.Length2D();
	if (Distance <= Tactics.ArriveRadiusM)
	{
		return Intent;
	}
	// Slow into the spot so the player arrives still (GDD 6.3 split-step needs it, and
	// GDD 5.2: a player running at contact plays worse and can't be set for a smash).
	// Only a ball with no time to spare is chased flat out to the last step.
	const bool bFlatOut = bChasing && Slack < Tactics.SetSlackS;
	double Speed = bFlatOut ? 1.0 : ClampD(Distance / Tactics.SlowRadiusM, 0.0, 1.0);
	Intent.bSprint = bChasing ? (Slack < Tactics.SprintSlackS || Distance > Tactics.SprintDistanceM) : Distance > Tactics.SprintDistanceM;
	// Never faster than the player can still stop in (GDD 6.3, M1: braking is about as
	// hard as starting), or they overrun the spot and meet the ball out of reach.
	const FAiPlayerProfile& Profile = Profiles[Slot];
	const double AccelTime = Profile.AccelTime(Tactics);
	const double Top = Profile.TopSpeed(Tactics);
	if (BrakeMult > 0.0 && AccelTime > 0.0 && Top > 0.0)
	{
		const double StopFrac = std::sqrt(2.0 * Top / AccelTime * BrakeMult * Distance) / Top;
		if (StopFrac < Speed)
		{
			Speed = StopFrac;
			Intent.bSprint = false;
		}
	}
	Intent.Move = TeamFrame::MoveVector(HalfSign, Delta * (Speed / Distance));
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
	if (bCaptureHoldBall)
	{
		HoldBallLocal = BallLocal;
		bCaptureHoldBall = false;
	}
	const std::array<FAiVec2, 2> Homes = Pair.HomePositions(BallLocal);
	const std::array<FAiVec2, 2> HoldHomes = Pair.HomePositions(HoldBallLocal);
	std::array<std::optional<FAiIntent>, 2> Out;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		if (!IsAi(Slot))
		{
			continue;
		}
		const bool bHolding = FirstStepAt[Slot] >= 0.0 && View.Time < FirstStepAt[Slot];
		if (FirstStepAt[Slot] >= 0.0 && !bHolding)
		{
			FirstStepAt[Slot] = -1.0;
			bSplitLanding[Slot] = false;
		}
		const bool bTaking = Taker == Slot && Plans[Slot].has_value();
		FAiIntent Intent;
		const FAiVec Hold = bSplitLanding[Slot] ? View.OwnPositions[Slot]
			: TeamFrame::ToCourt(HalfSign, HoldHomes[Slot] + HomeError[Slot]);
		if (bTaking)
		{
			const FInterceptPlan& Plan = *Plans[Slot];
			Intent = bHolding ? MoveTo(Slot, View.OwnPositions[Slot], Hold, false, 0.0)
				: MoveTo(Slot, View.OwnPositions[Slot], Plan.StandPoint, true, Plan.Slack);
			// The shot is chosen with the read, not the first step: the swing needs its lead.
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
		else if (bHolding)
		{
			Intent = MoveTo(Slot, View.OwnPositions[Slot], Hold, false, 0.0);
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
