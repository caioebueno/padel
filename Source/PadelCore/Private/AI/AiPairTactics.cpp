#include "AI/AiPairTactics.h"

#include <cmath>

namespace Padel::AI
{
FAiPairTactics::FAiPairTactics(const FAiTactics& InTactics)
	: Tactics(InTactics)
{
}

void FAiPairTactics::OnOwnShot(EAiShot Shot, double ContactDepth, double TargetDepth)
{
	switch (Shot)
	{
	case EAiShot::Lob:
		// A deep lob buys the time to advance; a short one doesn't.
		Formation = TargetDepth >= Tactics.LobAdvanceMinDepthM ? EFormation::BothNet : EFormation::BothBack;
		break;
	case EAiShot::Chiquita:  // GDD 4: the way to win the net back
	case EAiShot::Volley:
	case EAiShot::Bandeja:
	case EAiShot::Vibora:
	case EAiShot::Smash:
	case EAiShot::ServeFlat:  // the serving pair follows its serve in
	case EAiShot::ServeKick:
		Formation = EFormation::BothNet;
		break;
	case EAiShot::Drive:
		Formation = ContactDepth <= Tactics.DriveToNetMaxDepthM ? EFormation::BothNet : EFormation::BothBack;
		break;
	}
}

void FAiPairTactics::OnOpponentShot(EAiShot Shot)
{
	if (Shot == EAiShot::Lob)
	{
		Formation = EFormation::BothBack;
	}
}

void FAiPairTactics::FollowHuman(double HumanDepth)
{
	const double Half = Tactics.HumanDepthHysteresisM * 0.5;
	if (HumanDepth < Tactics.HumanNetDepthM - Half)
	{
		Formation = EFormation::BothNet;
	}
	else if (HumanDepth > Tactics.HumanNetDepthM + Half)
	{
		Formation = EFormation::BothBack;
	}
}

void FAiPairTactics::SetCommand(int Slot, EPartnerCommand Command)
{
	Commands[Slot] = Command;
}

void FAiPairTactics::ClearCommands()
{
	Commands = { EPartnerCommand::None, EPartnerCommand::None };
}

void FAiPairTactics::ConsumeYours()
{
	for (EPartnerCommand& Command : Commands)
	{
		if (Command == EPartnerCommand::Yours)
		{
			Command = EPartnerCommand::None;
		}
	}
}

double FAiPairTactics::SpacingFor(double BallLocalX) const
{
	// Movement doc 2.12, 5.4: narrow with the ball in the middle, wide with it at the
	// side glass, so the line and the middle both stay covered.
	const double Width = ClampD(std::abs(BallLocalX) / Court::HalfWidth, 0.0, 1.0);
	return Tactics.PartnerSpacingMinM + (Tactics.PartnerSpacingMaxM - Tactics.PartnerSpacingMinM) * Width;
}

std::array<FAiVec2, 2> FAiPairTactics::HomePositions(const FAiVec2& BallLocal) const
{
	const double BaseDepth = Formation == EFormation::BothNet ? Tactics.NetDepthM : Tactics.BackDepthM;
	const double Centre = ClampD(BallLocal.X * Tactics.BallShiftFactor, -Tactics.MaxLateralShiftM, Tactics.MaxLateralShiftM);
	const double HalfSpacing = SpacingFor(BallLocal.X) * 0.5;
	const double MaxX = Court::HalfWidth - Tactics.CourtMarginM;
	// GDD 8 stagger: at the net the partner away from the ball drops back to cover the
	// lob and the middle. It builds up across the middle band so the pair doesn't swap
	// depths in one step when the ball crosses the centre line.
	const double StaggerWeight = Tactics.MiddleBandM > 0.0
		? ClampD(std::abs(BallLocal.X) / Tactics.MiddleBandM, 0.0, 1.0)
		: (BallLocal.X != 0.0 ? 1.0 : 0.0);
	const int FarSlot = BallLocal.X >= 0.0 ? LeftSlot : RightSlot;

	std::array<FAiVec2, 2> Homes;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		const double SideSign = Slot == RightSlot ? 1.0 : -1.0;
		double X = Centre + SideSign * HalfSpacing;
		double Depth = BaseDepth;
		if (Formation == EFormation::BothNet)
		{
			// Movement doc 5.4: the ball-side player steps in to volley and the far one
			// drops back, so the pair stays centred on NetDepthM.
			Depth += (Slot == FarSlot ? 0.5 : -0.5) * Tactics.NetStaggerM * StaggerWeight;
		}
		// A command overrides the formation's depth, stagger included (GDD 7).
		switch (Commands[Slot])
		{
		case EPartnerCommand::GoNet:
			Depth = Tactics.NetDepthM;
			break;
		case EPartnerCommand::StayBack:
			Depth = Tactics.BackDepthM;
			break;
		case EPartnerCommand::TakeMiddle:
			X -= SideSign * Tactics.TakeMiddleShiftM;
			break;
		default:
			break;
		}
		Homes[Slot] = { ClampD(X, -MaxX, MaxX), ClampD(Depth, Tactics.MinPlayerDepthM, Court::HalfLength - Tactics.CourtMarginM) };
	}
	return Homes;
}

int FAiPairTactics::ChooseTaker(const FTakerQuery& Query, FPadelRng& Rng) const
{
	// A command from the human overrides the default split.
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		if (Commands[Slot] == EPartnerCommand::Yours && Query.Plans[Slot])
		{
			return Slot;
		}
	}
	const bool bRight = Query.Plans[RightSlot].has_value();
	const bool bLeft = Query.Plans[LeftSlot].has_value();
	if (bRight != bLeft)
	{
		return bRight ? RightSlot : LeftSlot;
	}
	const auto Slack = [&Query](int Slot) {
		return Query.Plans[Slot] ? Query.Plans[Slot]->Slack : -1e9;
	};
	const bool bRightReach = bRight && Query.Plans[RightSlot]->bReachable;
	const bool bLeftReach = bLeft && Query.Plans[LeftSlot]->bReachable;
	if (bRightReach != bLeftReach)
	{
		return bRightReach ? RightSlot : LeftSlot;
	}

	const double PairCentre = (Query.PlayerLocal[RightSlot].X + Query.PlayerLocal[LeftSlot].X) * 0.5;
	if (Query.BallLocalX > PairCentre + Tactics.MiddleBandM)
	{
		return RightSlot;
	}
	if (Query.BallLocalX < PairCentre - Tactics.MiddleBandM)
	{
		return LeftSlot;
	}

	// Down the middle.
	int Default = LeftSlot;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		if (Commands[Slot] == EPartnerCommand::TakeMiddle)
		{
			return Slot;
		}
	}
	// The forehand that faces the middle: a right-hander on the left, a left-hander on
	// the right. With two right-handers that is the left-side player (GDD 8 default).
	const bool bLeftForehandInside = Query.Profiles[LeftSlot].ForehandSign() > 0.0;
	const bool bRightForehandInside = Query.Profiles[RightSlot].ForehandSign() < 0.0;
	if (bRightForehandInside && !bLeftForehandInside)
	{
		Default = RightSlot;
	}
	const int Other = 1 - Default;
	int Taker = Slack(Other) - Slack(Default) > Tactics.BetterPlacedMarginS ? Other : Default;

	const double Confusion = Tactics.ConfusionMax * (1.0 - ByRating(Query.Teamwork, 0.0, 1.0));
	if (Random::Chance(Rng, Confusion))
	{
		Taker = 1 - Taker;
	}
	return Taker;
}
}
