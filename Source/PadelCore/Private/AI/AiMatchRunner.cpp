#include "AI/AiMatchRunner.h"

#include "AI/AiEngineBridge.h"
#include "AI/AiTeamFrame.h"
#include "LaunchSolver.h"
#include "Shots/TimingGrader.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Padel::AI
{
FAiMatchRunner::FAiMatchRunner(FMatchSimulation& InMatch, const Shots::FShotLibrary& InLibrary,
	const Shots::FMovementTuning& InMovement, const FAiTactics& InTactics, const FAiMatchSetup& InSetup)
	: Match(InMatch)
	, Library(InLibrary)
	, Movement(InMovement)
	, Tactics(InTactics)
	, Setup(InSetup)
	, Model(InMatch.BallSim())
	, Port(InMatch)
{
	for (int TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		RightPlayer[TeamIdx] = Match.Score().RightSidePlayer(static_cast<ETeam>(TeamIdx));
	}
	std::array<FVec3, 4> Starts;
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		const int TeamIdx = TeamIndex(TeamOfPlayer(Id));
		// GDD 8: the difficulty scales the AI's attributes, for the shot model too.
		Ratings[Id] = IsAi(Id) ? Bridge::WithOffset(Setup.Players[Id], Setup.Difficulties[TeamIdx]->AttributeOffset) : Setup.Players[Id];
		const int Half = Match.HalfSignOf(static_cast<ETeam>(TeamIdx));
		Starts[Id] = Bridge::ToEngine(TeamFrame::ToCourt(Half, { 0.0, Tactics.BackDepthM }));
	}
	BuildControllers(Starts);

	for (int TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		FAiTeamSetup TeamSetup;
		TeamSetup.HalfSign = Match.HalfSignOf(static_cast<ETeam>(TeamIdx));
		for (int Slot = 0; Slot < 2; ++Slot)
		{
			const int Id = PlayerIdOf(TeamIdx, Slot);
			// Unscaled: the team controller applies the difficulty offset to its AI players.
			TeamSetup.Profiles[Slot] = Bridge::ProfileFromRatings(Setup.Players[Id]);
			if (!IsAi(Id))
			{
				TeamSetup.HumanSlot = Slot;
			}
		}
		TeamSetup.Seed = Setup.Seed * 8 + static_cast<uint64_t>(TeamIdx) + 1;
		Teams[TeamIdx].emplace(TeamSetup, Tactics, *Setup.Difficulties[TeamIdx], Model);
	}
	Match.AddListener(this);
}

FAiMatchRunner::~FAiMatchRunner()
{
	Match.RemoveListener(this);
}

int FAiMatchRunner::SlotOf(int PlayerId) const
{
	return RightPlayer[TeamIndex(TeamOfPlayer(PlayerId))] == PlayerId ? RightSlot : LeftSlot;
}

void FAiMatchRunner::BuildControllers(const std::array<FVec3, 4>& Positions)
{
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		const double Stamina = Controllers[Id] ? Controllers[Id]->GetMotion().GetStamina() : 1.0;
		const double Sign = Match.HalfSignOf(TeamOfPlayer(Id));
		const uint64_t Seed = Setup.Seed * 64 + static_cast<uint64_t>(EndsChanges) * 4 + static_cast<uint64_t>(Id) + 101;
		Controllers[Id].emplace(Id, Sign, &Ratings[Id], &Library, &Movement, Positions[Id], Seed);
		Controllers[Id]->GetMotion().SetStamina(Stamina);
		// GDD 6.2: playing the side they don't prefer costs a little on technical shots.
		const Shots::ECourtSide Side = SlotOf(Id) == RightSlot ? Shots::ECourtSide::Right : Shots::ECourtSide::Left;
		Controllers[Id]->SetWrongSide(Side != Ratings[Id].PreferredSide);
	}
}

void FAiMatchRunner::Start()
{
	if (Match.Phase() == EMatchPhase::NotStarted)
	{
		Match.StartMatch();
		Match.BeginPoint();
	}
}

bool FAiMatchRunner::GiveCommand(EPartnerCommand Command)
{
	if (Setup.HumanPlayerId < 0)
	{
		return false;
	}
	return Teams[TeamIndex(TeamOfPlayer(Setup.HumanPlayerId))]->GiveCommand(Command);
}

bool FAiMatchRunner::IsWaitingForGoldenReceiver() const
{
	return GoldenReceivingTeam >= 0 && Setup.HumanPlayerId >= 0 && TeamIndex(TeamOfPlayer(Setup.HumanPlayerId)) == GoldenReceivingTeam;
}

bool FAiMatchRunner::ChooseGoldenPointReceiver(int PlayerId)
{
	if (GoldenReceivingTeam < 0 || TeamIndex(TeamOfPlayer(PlayerId)) != GoldenReceivingTeam || !Match.ChooseGoldenPointReceiver(PlayerId))
	{
		return false;
	}
	GoldenReceivingTeam = -1;
	return true;
}

void FAiMatchRunner::OnServeStarted(int /*ServerId*/)
{
	bServePending = true;
}

void FAiMatchRunner::OnBallBounced(Court::ESurface Surface, Court::EHalf /*Half*/, const FVec3& /*Position*/)
{
	for (std::optional<FAiTeamController>& Team : Teams)
	{
		Team->OnBallContact(Match.Time(), Surface);
	}
}

void FAiMatchRunner::OnPointEnded(ETeam /*WinningTeam*/, EPointReason /*Reason*/, int /*LastHitterId*/)
{
	PointEndedAt = Match.Time();
	TossAt = -1.0;
	for (std::optional<FAiTeamController>& Team : Teams)
	{
		Team->OnPointEnded();
	}
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		ClearShots(Id);
		Controllers[Id]->SetAwaitingServe(false);
	}
}

void FAiMatchRunner::OnChangeOfEnds()
{
	bEndsPending = true;
}

void FAiMatchRunner::OnGoldenPointStarted(ETeam ReceivingTeam)
{
	GoldenReceivingTeam = TeamIndex(ReceivingTeam);
}

void FAiMatchRunner::SetUpServe()
{
	bServePending = false;
	Port.ResetToss();
	const int Server = Match.ServerId();
	const int Receiver = Match.Score().ReceiverId();
	const int ServingTeam = TeamIndex(TeamOfPlayer(Server));
	// GDD 3.2: the server and the receiver stand on the serve side, diagonally.
	const double Side = Match.Score().ServeSide() == EServeSide::Right ? 1.0 : -1.0;
	const double Wide = Tactics.PartnerSpacingM * 0.5;

	const FScoreSnapshot Score = Match.Score().Snapshot();
	const bool bPressure = Score.bBreakPoint || Score.bSetPoint || Score.bMatchPoint || Score.bGoldenPoint;
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		const int TeamIdx = TeamIndex(TeamOfPlayer(Id));
		const int Half = Match.HalfSignOf(static_cast<ETeam>(TeamIdx));
		FAiVec2 Local;
		if (Id == Server)
		{
			Local = TeamFrame::ToLocal(Half, Bridge::ToAi(Match.DefaultServeContact().ServerFeet));
		}
		else if (Id == Receiver)
		{
			Local = { Side * Wide, Tactics.BackDepthM };
		}
		else
		{
			// The partners take the other side: the server's at the net, the receiver's back.
			Local = { -Side * Wide, TeamIdx == ServingTeam ? Tactics.NetDepthM : Tactics.BackDepthM };
		}
		Shots::FPlayerShotController& C = *Controllers[Id];
		C.GetMotion().Teleport(Bridge::ToEngine(TeamFrame::ToCourt(Half, Local)));
		ClearShots(Id);
		C.SetAwaitingServe(Id == Server);
		C.SetNextIsReturn(Id == Receiver);
		C.SetPressurePoint(bPressure);
	}
	for (int TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		Teams[TeamIdx]->OnPointStarted(TeamIdx == ServingTeam);
	}
	TossAt = IsAi(Server) ? Match.Time() + Tactics.ServeDelayS : -1.0;
}

FAiWorldView FAiMatchRunner::ViewFor(int TeamIdx) const
{
	FAiWorldView View;
	View.Time = Match.Time();
	View.Ball = Bridge::ToAi(Match.Ball());
	// GDD 3.2: the serve must bounce before the receiver plays it.
	View.bMustBounce = Match.IsServe() && TeamIdx != TeamIndex(TeamOfPlayer(Match.ServerId()));
	for (int Slot = 0; Slot < 2; ++Slot)
	{
		const Shots::FPlayerMotion& Own = Controllers[PlayerIdOf(TeamIdx, Slot)]->GetMotion();
		View.OwnPositions[Slot] = Bridge::ToAi(Own.GetPosition());
		View.OwnVelocities[Slot] = Bridge::ToAi(Own.GetVelocity());
		View.OpponentPositions[Slot] = Bridge::ToAi(Controllers[PlayerIdOf(1 - TeamIdx, Slot)]->GetMotion().GetPosition());
	}
	return View;
}

void FAiMatchRunner::ThinkAi(std::array<Shots::FPlayerInputFrame, 4>& Inputs)
{
	for (int TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		const std::array<std::optional<FAiIntent>, 2> Intents = Teams[TeamIdx]->Tick(ViewFor(TeamIdx));
		for (int Slot = 0; Slot < 2; ++Slot)
		{
			if (!Intents[Slot])
			{
				continue;
			}
			const int Id = PlayerIdOf(TeamIdx, Slot);
			Inputs[Id] = Bridge::InputFrom(*Intents[Slot]);
			if (const std::optional<FAiShotDecision>& Chosen = Intents[Slot]->Shot)
			{
				const std::optional<FInterceptPlan>& Plan = Teams[TeamIdx]->GetPlan(Slot);
				const FAiShotDecision Shot = Plan ? AimOverNet(Plan->Sample.Position, Teams[TeamIdx]->GetHalfSign(), *Chosen) : *Chosen;
				Pending[Id] = FPendingShot{ Shot, Intents[Slot]->ShotContactTime };
			}
			const int Taker = Teams[TeamIdx]->GetTaker();
			if (Taker >= 0 && Taker != Slot)
			{
				// One player of the pair plays the ball.
				ClearShots(Id);
			}
		}
	}
	ReleasePending();
}

double FAiMatchRunner::RealTimingOffset(int PlayerId, const FAiShotDecision& Shot) const
{
	const Shots::FShotModel& M = Library.Model;
	// Overheads are graded in the bandeja's windows (the overhead button's family shot).
	const Shots::FShotParams& Params = Library.Get(IsOverhead(Shot.Shot) ? EShotType::Bandeja : Bridge::ToEngine(Shot.Shot));
	const Shots::FPlayerRatings& R = Ratings[PlayerId];
	const double Unit = std::min(R.Unit(Params.ForehandRating), R.Unit(Params.BackhandRating));
	const double Scale = Shots::FTimingGrader::WindowScale(Unit, M);
	const std::array<double, 4> AiEdges = { 0.0, Tactics.WindowPerfectS, Tactics.WindowGoodS, Tactics.WindowEarlyLateS };
	const std::array<double, 4> RealEdges = { 0.0, Params.PerfectWindowMs * Scale / 1000.0, Params.GoodWindowMs * Scale / 1000.0,
		Params.EarlyLateWindowMs * Scale / 1000.0 };
	const double Abs = std::abs(Shot.TimingOffsetS);
	const double Sign = Shot.TimingOffsetS < 0.0 ? -1.0 : 1.0;
	for (size_t Band = 1; Band < AiEdges.size(); ++Band)
	{
		if (Abs <= AiEdges[Band])
		{
			const double T = (Abs - AiEdges[Band - 1]) / (AiEdges[Band] - AiEdges[Band - 1]);
			return Sign * (RealEdges[Band - 1] + T * (RealEdges[Band] - RealEdges[Band - 1]));
		}
	}
	// A miss: as far past the early/late edge as the AI asked.
	return Sign * (RealEdges[3] + (Abs - AiEdges[3]));
}

void FAiMatchRunner::ClearShots(int PlayerId)
{
	Pending[PlayerId].reset();
	Controllers[PlayerId]->CancelAiShot();
}

double FAiMatchRunner::BallDiesAt() const
{
	// When the ball stops being playable (its second bounce, or out): no swing can meet
	// it after that.
	const double Now = Match.Time();
	for (const Shots::FBallView& V : Port.Predict(Library.Model.PredictHorizonS, SimTickSeconds))
	{
		if (!V.bLive || V.GroundBounces > 1)
		{
			return Now + V.T;
		}
	}
	return std::numeric_limits<double>::infinity();
}

void FAiMatchRunner::ReleasePending()
{
	const double Now = Match.Time();
	double DiesAt = -1.0;
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		if (!Pending[Id])
		{
			continue;
		}
		if (DiesAt < 0.0)
		{
			DiesAt = BallDiesAt();
		}
		const FPendingShot& P = *Pending[Id];
		const Shots::FPlayerShotController& C = *Controllers[Id];
		const bool bSameMoment = C.HasIdealMoment() && std::abs(Now + C.TimeToIdealS() - P.ContactTime) <= Tactics.ContactMatchS;
		// The controller picks its moment from where the player stands now, so on the way
		// to the stand point it can still see another chance at the ball. Wait for it to
		// agree, taking backswing off, and only at the very end swing at whatever it sees.
		// A smash can't spare any backswing (GDD 4: it needs the full charge).
		const double PlannedOffset = RealTimingOffset(Id, P.Shot);
		const double FullLead = C.AiSwingLeadS(P.Shot.Charge, PlannedOffset);
		const bool bNeedsFullCharge = P.Shot.Shot == EAiShot::Smash;
		const bool bLastChance = Now >= P.ContactTime - (bNeedsFullCharge ? FullLead : std::min(FullLead, Tactics.LateRequestS));
		// The plan was read from a noisy prediction (GDD 8 Wall Play) and its moment never
		// comes: the ball dies first. Play the controller's own moment instead.
		const bool bStale = P.ContactTime >= DiesAt - Tactics.ContactMatchS;
		if (!bSameMoment && !bLastChance && !bStale)
		{
			continue;
		}
		// A late swing still has to meet the ball before it dies.
		const double IdealAt = C.HasIdealMoment() ? Now + C.TimeToIdealS() : P.ContactTime;
		const double Offset = std::min(PlannedOffset, DiesAt - IdealAt - Tactics.DeadBallMarginS);
		// A late request gets the backswing there is time for; a smash keeps its full
		// charge, which the shot system's own lead (AiSwingLeadS) has room for.
		const double TimeLeft = std::max(0.0, IdealAt + Offset - Now);
		const double Charge = bNeedsFullCharge ? P.Shot.Charge : std::min(P.Shot.Charge, TimeLeft / Library.Model.ChargeTimeS);
		Controllers[Id]->RequestAiShot(Bridge::ToEngine(P.Shot.Shot), Bridge::ToEngine(P.Shot.Target), Charge, Offset);
		Pending[Id].reset();
	}
}

bool FAiMatchRunner::ClearsNet(const FVec3& Origin, const FVec3& Target, double Speed, const FVec3& SpinLocal) const
{
	FLaunchRequest Request;
	Request.Origin = Origin;
	Request.Target = Target;
	Request.Speed = Speed;
	Request.SpinLocal = SpinLocal;
	const FBallSim& Sim = Match.BallSim();
	const FLaunchResult Launch = SolveLaunch(Sim, Request);
	if (!Launch.bOk)
	{
		return false;
	}
	FBallState Ball;
	Ball.Position = Origin;
	Ball.Velocity = Launch.Velocity;
	Ball.Spin = Launch.Spin;
	std::vector<FBallContact> Contacts;
	const int MaxTicks = static_cast<int>(std::ceil((Launch.FlightTime + 1.0) / SimTickSeconds));
	for (int Tick = 0; Tick < MaxTicks; ++Tick)
	{
		const double PreviousX = Ball.Position.X;
		Contacts.clear();
		Sim.Tick(Ball, nullptr, Contacts);
		for (const FBallContact& C : Contacts)
		{
			if (C.Kind == Padel::EContactKind::Surface && (C.Surface == Court::ESurface::Net || C.Surface == Court::ESurface::Ground))
			{
				return false;  // into the tape, or down on our own side
			}
		}
		if ((PreviousX >= 0.0) != (Ball.Position.X >= 0.0))
		{
			return Ball.Position.Z >= Court::NetHeightAt(Ball.Position.Y) + Tactics.NetClearanceM;
		}
	}
	return false;
}

FAiShotDecision FAiMatchRunner::AimOverNet(const FAiVec& Contact, int HitterHalf, const FAiShotDecision& Decision) const
{
	// Lobs are solved for their apex, serves for the box; overheads come down from above
	// the tape, except a smash driven down at the feet.
	if (Decision.Shot == EAiShot::Lob || IsServe(Decision.Shot) || (IsOverhead(Decision.Shot) && Decision.Shot != EAiShot::Smash))
	{
		return Decision;
	}
	const Shots::FShotParams& Params = Library.Get(Bridge::ToEngine(Decision.Shot));
	// The speed the shot model will give it at this charge, before the player's own
	// power and position (Shots/ShotResolver.cpp); the clearance margin covers the rest.
	const FVec3 Spin = { Params.BaseSpinTop, Params.BaseSpinSide, 0.0 };
	const FVec3 Origin = Bridge::ToEngine(Contact);
	// The chosen pace first, then more (a soft ball that falls short), then less (a
	// hard one that can't dip over the tape in time).
	std::vector<double> Charges = { Decision.Charge };
	if (Tactics.NetRetargetChargeStep > 0.0)
	{
		for (double C = Decision.Charge + Tactics.NetRetargetChargeStep; C <= 1.0; C += Tactics.NetRetargetChargeStep)
		{
			Charges.push_back(C);
		}
		for (double C = Decision.Charge - Tactics.NetRetargetChargeStep; C >= 0.0; C -= Tactics.NetRetargetChargeStep)
		{
			Charges.push_back(C);
		}
	}
	for (const double Charge : Charges)
	{
		FAiShotDecision Try = Decision;
		Try.Charge = Charge;
		const double Speed = Params.SpeedMinMps() + (Params.SpeedMaxMps() - Params.SpeedMinMps()) * Charge;
		for (int Step = 0; Step <= Tactics.NetRetargetSteps; ++Step)
		{
			if (ClearsNet(Origin, Bridge::ToEngine(Try.Target), Speed, Spin))
			{
				return Try;
			}
			// Deeper into the other half: its depth grows along -HitterHalf.
			const double Depth = std::min(-HitterHalf * Try.Target.X + Tactics.NetRetargetDepthStepM, Tactics.TargetDepthMaxM);
			Try.Target.X = -HitterHalf * Depth;
		}
	}
	// Nothing flat gets there from this ball: lob it (GDD 8, the safe shot), deep and
	// on the same side.
	FAiShotDecision Lob = Decision;
	Lob.Shot = EAiShot::Lob;
	Lob.Charge = Tactics.ChargeLob;
	Lob.Target.X = -HitterHalf * Tactics.LobDepthM;
	Lob.Target.Y = ClampD(Decision.Target.Y, -Tactics.LobSideM, Tactics.LobSideM);
	return Lob;
}

void FAiMatchRunner::ServeAi(std::array<Shots::FPlayerInputFrame, 4>& Inputs)
{
	(void)Inputs;  // everyone stands still for the serve
	const int Server = Match.ServerId();
	Shots::FPlayerShotController& C = *Controllers[Server];
	Port.SetServerStance(C.GetMotion().GetPosition(), Ratings[Server].HeightM * Tactics.WaistHeightFraction);
	if (TossAt < 0.0 || Match.Time() < TossAt || !C.IsAwaitingServe())
	{
		return;
	}
	TossAt = -1.0;
	const int TeamIdx = TeamIndex(TeamOfPlayer(Server));
	const FAiShotDecision Serve = Teams[TeamIdx]->ChooseServe(Bridge::ToAi(C.GetMotion().GetPosition()));
	C.TossForServe(Port);
	C.RequestAiShot(Bridge::ToEngine(Serve.Shot), Bridge::ToEngine(Serve.Target), Serve.Charge, RealTimingOffset(Server, Serve));
}

void FAiMatchRunner::AfterHit(int HitterId)
{
	const Shots::FShotIntent& Intent = Events[HitterId].Intent;
	const int HitterHalf = Match.HalfSignOf(TeamOfPlayer(HitterId));
	for (std::optional<FAiTeamController>& Team : Teams)
	{
		Team->OnBallHit(Match.Time(), HitterHalf, Bridge::ToAi(Intent.ShotType), Bridge::ToAi(Intent.Origin), Bridge::ToAi(Intent.Target));
	}
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		ClearShots(Id);
		if (TeamOfPlayer(Id) != TeamOfPlayer(HitterId))
		{
			// GDD 6.3: the split-step at the opponent's contact.
			Controllers[Id]->NotifyOpponentContact();
		}
	}
}

void FAiMatchRunner::Tick()
{
	if (Match.Phase() == EMatchPhase::NotStarted)
	{
		return;
	}
	if (bEndsPending)
	{
		// GDD 3.1 change of ends: everyone mirrors to the other half.
		bEndsPending = false;
		++EndsChanges;
		std::array<FVec3, 4> Mirrored;
		for (int Id = 0; Id < NumPlayers; ++Id)
		{
			const FVec3& P = Controllers[Id]->GetMotion().GetPosition();
			Mirrored[Id] = { -P.X, -P.Y, 0.0 };
		}
		BuildControllers(Mirrored);
		for (int TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
		{
			Teams[TeamIdx]->SetHalfSign(Match.HalfSignOf(static_cast<ETeam>(TeamIdx)));
		}
	}
	if (GoldenReceivingTeam >= 0 && !IsWaitingForGoldenReceiver() && Match.Phase() == EMatchPhase::PointOver)
	{
		// GDD 3.1: an AI pair picks its receiver.
		Match.ChooseGoldenPointReceiver(PlayerIdOf(GoldenReceivingTeam, Teams[GoldenReceivingTeam]->ChooseGoldenPointReceiver()));
		GoldenReceivingTeam = -1;
	}
	if (Match.Phase() == EMatchPhase::PointOver && Setup.bAutoBeginPoints && !IsWaitingForGoldenReceiver()
		&& Match.Time() - PointEndedAt >= Tactics.PointGapS)
	{
		Match.BeginPoint();
	}
	if (bServePending && Match.Phase() == EMatchPhase::AwaitingServe)
	{
		SetUpServe();
	}

	std::array<Shots::FPlayerInputFrame, 4> Inputs{};
	if (Setup.HumanPlayerId >= 0)
	{
		Inputs[Setup.HumanPlayerId] = HumanInput;
	}
	if (Match.Phase() == EMatchPhase::Live)
	{
		ThinkAi(Inputs);
	}
	else if (Match.Phase() == EMatchPhase::AwaitingServe)
	{
		ServeAi(Inputs);
	}
	for (int Id = 0; Id < NumPlayers; ++Id)
	{
		Events[Id] = Controllers[Id]->Tick(Inputs[Id], SimTickSeconds, Port);
		bAccepted[Id] = Events[Id].Outcome == Shots::ESwingOutcome::Hit && Port.LastShotAccepted();
		if (bAccepted[Id])
		{
			AfterHit(Id);
		}
	}
	Port.Tick();
	Match.Tick();
}
}
