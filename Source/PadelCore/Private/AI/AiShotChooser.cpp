#include "AI/AiShotChooser.h"

#include "AI/AiTeamFrame.h"

#include <cmath>

namespace Padel::AI::ShotChooser
{
namespace
{
	// Opponents' positions in their own frame.
	std::vector<FAiVec2> OpponentsLocal(const FShotContext& C)
	{
		std::vector<FAiVec2> Out;
		for (const FAiVec& P : C.Opponents)
		{
			Out.push_back(TeamFrame::ToLocal(-C.HalfSign, P));
		}
		return Out;
	}

	double Charge(EAiShot Shot, const FAiTactics& T)
	{
		switch (Shot)
		{
		case EAiShot::Drive: return T.ChargeDrive;
		case EAiShot::Volley: return T.ChargeVolley;
		case EAiShot::Lob: return T.ChargeLob;
		case EAiShot::Bandeja: return T.ChargeBandeja;
		case EAiShot::Vibora: return T.ChargeVibora;
		case EAiShot::Smash: return T.ChargeSmash;
		case EAiShot::Chiquita: return T.ChargeChiquita;
		case EAiShot::ServeFlat:
		case EAiShot::ServeKick: return T.ChargeServe;
		}
		return T.ChargeDrive;
	}

	// Intended target, then the GDD 8 extra aim scatter. The scatter is not clamped
	// back inside the court: on Easy some of it lands out, which is the point.
	FAiShotDecision Finish(EAiShot Shot, FAiVec Target, const FAiTactics& T, const FAiDifficulty& D, FPadelRng& Rng)
	{
		const double Angle = Rng.Range(0.0, 6.283185307179586);
		const double Radius = T.AimScatterBaseM * D.AimScatterMult * std::sqrt(Rng.NextDouble());
		Target.X += std::cos(Angle) * Radius;
		Target.Y += std::sin(Angle) * Radius;
		Target.Z = 0.0;

		FAiShotDecision Decision;
		Decision.Shot = Shot;
		Decision.Target = Target;
		Decision.Charge = Charge(Shot, T);
		SampleTiming(T, D, Rng, Decision.Grade, Decision.TimingOffsetS);
		return Decision;
	}

	// A point in the opponents' frame, clamped to the safe area of their half.
	FAiVec OpponentPoint(const FShotContext& C, double LocalX, double Depth, const FAiTactics& T)
	{
		const FAiVec2 Local = { ClampD(LocalX, -T.TargetSideLimitM, T.TargetSideLimitM),
			ClampD(Depth, T.TargetDepthMinM, T.TargetDepthMaxM) };
		return TeamFrame::ToCourt(-C.HalfSign, Local);
	}
}

EOpponentShape ReadOpponents(const FShotContext& C, const FAiTactics& T)
{
	int AtNet = 0;
	for (const FAiVec2& P : OpponentsLocal(C))
	{
		if (P.Y < T.AtNetDepthM)
		{
			++AtNet;
		}
	}
	if (AtNet >= 2)
	{
		return EOpponentShape::BothNet;
	}
	return AtNet == 1 && C.Opponents.size() >= 2 ? EOpponentShape::Split : EOpponentShape::BothBack;
}

std::vector<FShotOption> Options(const FShotContext& C, const FAiTactics& T, const FAiDifficulty& D)
{
	const double Tactical = D.TacticalWeight;
	const double Safe = D.SafeShotBias;
	const double Depth = TeamFrame::Depth(C.HalfSign, C.Contact);
	const double Height = C.Contact.Z;
	const EOpponentShape Shape = ReadOpponents(C, T);
	std::vector<FShotOption> Out;

	if (C.Kind == EContactKind::Overhead)
	{
		// GDD 8: bandeja by default, víbora for left-side players, smash when the ball is short and high.
		Out.push_back({ EAiShot::Bandeja, T.WeightBandeja * (1.0 + Safe) });
		const double Vibora = C.Profile.PreferredSide == ESide::Left ? T.WeightViboraLeft : T.WeightViboraRight;
		Out.push_back({ EAiShot::Vibora, Vibora * (1.0 - 0.5 * Safe) });
		const bool bShortAndHigh = Depth < T.SmashMaxDepthM && Height > T.SmashMinHeightM && C.Slack > T.SetSlackS;
		if (bShortAndHigh)
		{
			Out.push_back({ EAiShot::Smash, T.WeightSmash * (0.5 + Tactical) });
		}
		if (Depth > T.PinnedDepthM)
		{
			Out.push_back({ EAiShot::Lob, T.WeightOverheadLobFromBack * (1.0 + Safe) });
		}
		return Out;
	}

	if (C.Kind == EContactKind::Volley)
	{
		Out.push_back({ EAiShot::Volley, T.WeightVolley });
		if (Shape == EOpponentShape::BothNet)
		{
			Out.push_back({ EAiShot::Lob, T.WeightLobVolley * (1.0 + Tactical) });
		}
		return Out;
	}

	// Groundstrokes, including off the glass.
	const bool bPinned = C.bOffGlass || Depth > T.PinnedDepthM;
	const bool bLow = Height < T.LowBallHeightM;
	double Drive = T.WeightDriveBase;
	double Lob = T.WeightLobBase + Safe * T.SafeLobBonus;
	double Chiquita = T.WeightChiquitaBase;
	switch (Shape)
	{
	case EOpponentShape::BothNet:
		// GDD 8 first rule: lob deep, or chiquita if the ball is in front.
		Lob = T.WeightLobVsNet * (1.0 + Tactical) + Safe * T.SafeLobBonus;
		Chiquita = Depth < T.ChiquitaMaxDepthM ? T.WeightChiquitaVsNet * (bLow ? 0.5 + Tactical : 0.3) : T.WeightChiquitaBase * 0.5;
		Drive = T.WeightDriveVsNet;
		break;
	case EOpponentShape::Split:
		// GDD 8: punish the split through the middle.
		Drive = T.WeightSplitDrive * (1.0 + Tactical);
		break;
	case EOpponentShape::BothBack:
		break;
	}
	if (bPinned)
	{
		// GDD 8: pinned at the back glass, a lob or a safe drive.
		Lob += T.WeightLobPinned;
		Drive *= T.PinnedDriveScale;
		Chiquita *= T.PinnedDriveScale;
	}
	Out.push_back({ EAiShot::Drive, Drive });
	Out.push_back({ EAiShot::Lob, Lob });
	Out.push_back({ EAiShot::Chiquita, Chiquita });
	return Out;
}

FAiVec IntendedTarget(EAiShot Shot, const FShotContext& C, const FAiTactics& T, const FAiDifficulty& D, FPadelRng& Rng)
{
	const std::vector<FAiVec2> Opp = OpponentsLocal(C);
	double MeanX = 0.0;
	for (const FAiVec2& P : Opp)
	{
		MeanX += P.X;
	}
	MeanX = Opp.empty() ? 0.0 : MeanX / static_cast<double>(Opp.size());
	// The opponent nearest the net, and the gap side for a tactical choice.
	FAiVec2 Front = { 0.0, T.AtNetDepthM };
	for (const FAiVec2& P : Opp)
	{
		if (P.Y < Front.Y)
		{
			Front = P;
		}
	}
	const bool bTactical = Random::Chance(Rng, D.TacticalWeight);
	const double AwaySide = MeanX >= 0.0 ? -1.0 : 1.0;
	const EOpponentShape Shape = ReadOpponents(C, T);

	switch (Shot)
	{
	case EAiShot::Lob:
		return OpponentPoint(C, bTactical ? AwaySide * T.LobSideM : 0.0, T.LobDepthM, T);
	case EAiShot::Chiquita:
		return OpponentPoint(C, (Front.X + MeanX) * 0.5, T.ChiquitaDepthM, T);
	case EAiShot::Volley:
		if (Shape != EOpponentShape::BothBack)
		{
			return OpponentPoint(C, Front.X, Front.Y + T.FeetDepthOffsetM, T);
		}
		return OpponentPoint(C, MeanX, T.VolleyDeepDepthM, T);
	case EAiShot::Bandeja:
		return OpponentPoint(C, bTactical ? AwaySide * T.BandejaSideM : MeanX, T.BandejaDepthM, T);
	case EAiShot::Vibora:
	{
		// Cross-court into the side glass: the opposite side of the court from the contact.
		const double ContactSide = SignOf(TeamFrame::ToLocal(-C.HalfSign, C.Contact).X);
		return OpponentPoint(C, -ContactSide * T.ViboraSideM, T.ViboraDepthM, T);
	}
	case EAiShot::Smash:
		return OpponentPoint(C, bTactical ? AwaySide * T.BandejaSideM : 0.0, T.SmashDepthM, T);
	case EAiShot::Drive:
	default:
		if (Shape == EOpponentShape::Split)
		{
			// Through the middle of the split pair, at the net player's feet.
			return OpponentPoint(C, MeanX, Front.Y + T.FeetDepthOffsetM, T);
		}
		if (Shape == EOpponentShape::BothNet)
		{
			return OpponentPoint(C, MeanX, Front.Y + T.FeetDepthOffsetM, T);
		}
		return OpponentPoint(C, bTactical ? AwaySide * T.DriveSideM : MeanX * 0.5, T.DriveDeepDepthM, T);
	}
}

FAiShotDecision ChooseRallyShot(const FShotContext& C, const FAiTactics& T, const FAiDifficulty& D, FPadelRng& Rng)
{
	const std::vector<FShotOption> Opts = Options(C, T, D);
	std::vector<double> Weights;
	for (const FShotOption& O : Opts)
	{
		Weights.push_back(O.Weight);
	}
	const EAiShot Shot = Opts.empty() ? EAiShot::Drive : Opts[static_cast<size_t>(Random::WeightedIndex(Rng, Weights))].Shot;
	return Finish(Shot, IntendedTarget(Shot, C, T, D, Rng), T, D, Rng);
}

FAiShotDecision ChooseServe(int HalfSign, const FAiVec& Server, const FAiTactics& T, const FAiDifficulty& D, FPadelRng& Rng)
{
	// Diagonal: the target is on the other side of the centre line from the server.
	const double ServerSide = SignOf(Server.Y);
	FAiVec Target = { -HalfSign * T.ServeDepthM, -ServerSide * T.ServeSideM, 0.0 };
	const EAiShot Shot = Random::Chance(Rng, T.KickServeChance * D.TacticalWeight) ? EAiShot::ServeKick : EAiShot::ServeFlat;
	return Finish(Shot, Target, T, D, Rng);
}

void SampleTiming(const FAiTactics& T, const FAiDifficulty& D, FPadelRng& Rng, EAiTimingGrade& OutGrade, double& OutOffsetS)
{
	const int Grade = Random::WeightedIndex(Rng, { D.TimingWeightPerfect, D.TimingWeightGood, D.TimingWeightEarlyLate, D.TimingWeightMiss });
	const double Sign = Random::Chance(Rng, 0.5) ? -1.0 : 1.0;
	double Lo = 0.0;
	double Hi = T.WindowPerfectS;
	switch (Grade)
	{
	case 0:
		OutGrade = EAiTimingGrade::Perfect;
		break;
	case 1:
		OutGrade = EAiTimingGrade::Good;
		Lo = T.WindowPerfectS;
		Hi = T.WindowGoodS;
		break;
	case 2:
		OutGrade = Sign < 0.0 ? EAiTimingGrade::Early : EAiTimingGrade::Late;
		Lo = T.WindowGoodS;
		Hi = T.WindowEarlyLateS;
		break;
	default:
		OutGrade = EAiTimingGrade::Miss;
		Lo = T.WindowEarlyLateS;
		Hi = T.WindowEarlyLateS + T.WindowMissExtraS;
		break;
	}
	OutOffsetS = Sign * Rng.Range(Lo, Hi);
}
}
