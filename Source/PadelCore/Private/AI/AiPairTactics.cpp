#include "AI/AiPairTactics.h"

#include <cmath>
#include <utility>

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
	const int FarSide = BallLocal.X >= 0.0 ? LeftSlot : RightSlot;

	std::array<FAiVec2, 2> Homes;
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		// After a side switch each player stands on the partner's side (GDD 8).
		const int Side = SideOf(Slot);
		const double SideSign = Side == RightSlot ? 1.0 : -1.0;
		double X = Centre + SideSign * HalfSpacing;
		double Depth = BaseDepth;
		if (Formation == EFormation::BothNet)
		{
			// Movement doc 5.4: the ball-side player steps in to volley and the far one
			// drops back, so the pair stays centred on NetDepthM.
			Depth += (Side == FarSide ? 0.5 : -0.5) * Tactics.NetStaggerM * StaggerWeight;
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

int FAiPairTactics::ChooseTaker(const FTakerQuery& PlayerQuery, FPadelRng& Rng) const
{
	// The rules below are by side; after a side switch the players' sides are swapped.
	FTakerQuery Query = PlayerQuery;
	std::array<EPartnerCommand, 2> SideCommands = Commands;
	if (bSwapped)
	{
		std::swap(Query.Plans[0], Query.Plans[1]);
		std::swap(Query.PlayerLocal[0], Query.PlayerLocal[1]);
		std::swap(Query.Profiles[0], Query.Profiles[1]);
		std::swap(SideCommands[0], SideCommands[1]);
	}
	return SideOf(ChooseTakerBySide(Query, SideCommands, Rng));
}

int FAiPairTactics::LobbedSlot(EAiShot Shot, const FAiVec2& OverLocal, const FAiVec2& LandingLocal, const std::array<FAiVec2, 2>& PlayerLocal) const
{
	// GDD 8, movement doc 2.12: only a deep lob over a pair at the net makes them cross.
	if (Tactics.SideSwitchMinLobDepthM <= 0.0 || Shot != EAiShot::Lob || Formation != EFormation::BothNet
		|| LandingLocal.Y < Tactics.SideSwitchMinLobDepthM)
	{
		return -1;
	}
	// The player the ball passes over is the one nearer its line at the pair's depth.
	const int Lobbed = std::abs(OverLocal.X - PlayerLocal[RightSlot].X) <= std::abs(OverLocal.X - PlayerLocal[LeftSlot].X) ? RightSlot : LeftSlot;
	// It has to land on that player's side or in the middle; a lob that drifts over to
	// the partner's own side is simply the partner's ball.
	const double Centre = (PlayerLocal[RightSlot].X + PlayerLocal[LeftSlot].X) * 0.5;
	const double LobbedSign = PlayerLocal[Lobbed].X >= PlayerLocal[1 - Lobbed].X ? 1.0 : -1.0;
	const double Across = (LandingLocal.X - Centre) * LobbedSign;
	return Across >= -Tactics.MiddleBandM ? Lobbed : -1;
}

int FAiPairTactics::ChooseTakerBySide(const FTakerQuery& Query, const std::array<EPartnerCommand, 2>& SideCommands, FPadelRng& Rng) const
{
	// A command from the human overrides the default split.
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		if (SideCommands[Slot] == EPartnerCommand::Yours && Query.Plans[Slot])
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
		if (SideCommands[Slot] == EPartnerCommand::TakeMiddle)
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
