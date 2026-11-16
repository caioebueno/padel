#include "Shots/PlayerShotController.h"

#include "Shots/ShotResolver.h"
#include "Shots/ShotSelector.h"
#include "Shots/TimingGrader.h"

#include <cmath>

namespace Padel::Shots
{
	// Named, not anonymous: Unreal unity builds merge .cpp files.
	namespace PlayerShotControllerDetail
	{
		struct FAiButton
		{
			EShotButton Button;
			bool bAttack;
			bool bTouch;
		};

		// The buttons a human would press for a shot (GDD 7), so the AI goes through
		// the same selector and can end up with a different shot when the ball says so.
		FAiButton ButtonFor(EShotType Type)
		{
			switch (Type)
			{
			case EShotType::Lob: return { EShotButton::Lob, false, false };
			case EShotType::Bandeja:
			case EShotType::Smash: return { EShotButton::Overhead, false, false };
			case EShotType::Vibora: return { EShotButton::Overhead, true, false };
			case EShotType::Chiquita: return { EShotButton::Drive, false, true };
			case EShotType::ServeKick: return { EShotButton::Drive, true, false };
			default: return { EShotButton::Drive, false, false };
			}
		}
	}
	using namespace PlayerShotControllerDetail;
	FPlayerShotController::FPlayerShotController(int InPlayerId, double InTeamSign, const FPlayerRatings* InRatings,
		const FShotLibrary* InLibrary, const FMovementTuning* InMovement, const FVec3& InStart, std::uint64_t InSeed)
		: PlayerId(InPlayerId), TeamSign(InTeamSign), Ratings(InRatings), Library(InLibrary),
		  Motion(InRatings, InMovement, InTeamSign, InStart), Rng(InSeed)
	{
	}

	FVec3 FPlayerShotController::IdealContactPoint(const FShotParams& Shot, double Side, double Height) const
	{
		// GDD 5.2: each shot has its own point, mirrored to the backhand side.
		const bool bForehand = Side * Ratings->ForehandSign() > 0.0;
		FVec3 Point = Add(Motion.GetPosition(), Add(Scale(RightAxis(TeamSign), Side * Shot.ContactSideFor(bForehand)),
			Scale(ForwardAxis(TeamSign), Shot.ContactForwardFor(bForehand))));
		Point.Z = Height;
		return Point;
	}

	double FPlayerShotController::MaxContactHeight() const
	{
		const FShotModel& M = Library->Model;
		return Ratings->HeightM + M.OverheadReachAboveHeightM + M.JumpExtraHeightM * Ratings->Unit(ERating::Jump);
	}

	double FPlayerShotController::ReachM() const
	{
		const FShotModel& M = Library->Model;
		return Lerp(M.ReachMinM, M.ReachMaxM, Ratings->Unit(ERating::Reach));
	}

	const FShotParams& FPlayerShotController::FamilyShot(bool bBallBounced) const
	{
		if (bServing)
		{
			return Library->Get(EShotType::ServeFlat);
		}
		switch (Button)
		{
		case EShotButton::Lob: return Library->Get(EShotType::Lob);
		case EShotButton::Overhead: return Library->Get(EShotType::Bandeja);
		default: return Library->Get(bBallBounced ? EShotType::Drive : EShotType::Volley);
		}
	}

	const FShotParams& FPlayerShotController::ContactShot(bool bBallBounced) const
	{
		// The AI knows its shot before the swing, so the ideal moment uses that shot's point.
		if ((bAiPending || bAiSwinging))
		{
			return Library->Get(AiShotType);
		}
		return FamilyShot(bBallBounced);
	}

	bool FPlayerShotController::IsPlayable(const FBallView& View, int LastHitter) const
	{
		// One touch per team (GDD 3.3), on our half, before the second bounce.
		if (!View.bLive || LastHitter == static_cast<int>(TeamSign) || View.Position.X * TeamSign <= 0.0 || View.GroundBounces > 1)
		{
			return false;
		}
		// GDD 3.2: the server lets the toss bounce first.
		return !bServing || View.GroundBounces >= 1;
	}

	void FPlayerShotController::UpdateIdeal(const IBallPort& Ball)
	{
		const FShotModel& M = Library->Model;
		const int LastHitter = Ball.LastHitterTeam();
		std::vector<FBallView> Samples = Ball.Predict(M.PredictHorizonS, M.PredictStepS);
		Samples.insert(Samples.begin(), Ball.Current());

		const FBallView* Best = nullptr;
		double BestCost = 0.0;
		for (const FBallView& S : Samples)
		{
			if (!IsPlayable(S, LastHitter))
			{
				continue;
			}
			const FShotParams& Shot = ContactShot(S.GroundBounces >= 1);
			const double H = S.Position.Z;
			const double Planar = std::fmin(DistanceXY(S.Position, IdealContactPoint(Shot, 1.0, 0.0)), DistanceXY(S.Position, IdealContactPoint(Shot, -1.0, 0.0)));
			const double OutsideBand = std::fmax(0.0, std::fmax(Shot.ContactHeightMinM - H, H - Shot.MaxHeightFor(Ratings->HeightM)));
			// Earlier wins a tie, so a ball that passes the same height twice (rising and
			// falling after a bounce) picks the first chance.
			const double Cost = Planar + M.HeightCostWeight * std::abs(H - Shot.PreferredHeightFor(Ratings->HeightM))
				+ M.OutOfBandCostWeight * OutsideBand + M.TimeCostPerS * S.T;
			if (!Best || Cost < BestCost)
			{
				Best = &S;
				BestCost = Cost;
			}
		}
		if (!Best)
		{
			bHasIdeal = false;
			return;
		}
		const double Candidate = NowS + Best->T;
		bool bTake = !bHasIdeal;
		if (bHasIdeal && NowS < IdealTimeS)
		{
			if (Phase == ESwingPhase::Windup)
			{
				// Mid-swing the player is committed to this ball: refine the moment, but
				// never jump to a different chance at it.
				bTake = std::abs(Candidate - IdealTimeS) <= FamilyShot(bIdealBounced).EarlyLateWindowMs / 1000.0;
			}
			else
			{
				bTake = true;
			}
		}
		else if (bHasIdeal && Phase != ESwingPhase::Windup)
		{
			// The last moment has passed with no swing: look for the next chance.
			bTake = Best->T > M.PredictStepS * 0.5;
		}
		if (bTake)
		{
			IdealTimeS = Candidate;
			IdealBallPosition = Best->Position;
			bIdealBounced = Best->GroundBounces >= 1;
			bIdealOffGlass = Best->bOffGlass;
		}
		bHasIdeal = true;
	}

	void FPlayerShotController::RequestAiShot(EShotType ShotType, const FVec3& TargetM, double InCharge, double TimingOffsetS)
	{
		bAiPending = true;
		AiShotType = ShotType;
		AiTarget = TargetM;
		AiCharge = Clamp(InCharge, 0.0, 1.0);
		AiOffsetS = TimingOffsetS;
	}

	double FPlayerShotController::AiSwingLeadS(double InCharge, double TimingOffsetS) const
	{
		const FShotModel& M = Library->Model;
		// A smash needs the full charge (GDD 4), and the predicted moment can drift
		// earlier during the windup, so a full-charge swing starts with some lead.
		const double LeadS = InCharge >= M.SmashMinCharge ? M.AiChargeLeadS : 0.0;
		return Clamp(InCharge, 0.0, 1.0) * M.ChargeTimeS + LeadS - TimingOffsetS;
	}

	bool FPlayerShotController::NotifyOpponentContact()
	{
		return Motion.NotifyOpponentContact();
	}

	void FPlayerShotController::TossForServe(IBallPort& Ball)
	{
		// Dropped at the forehand contact point, so it comes back up where the swing meets it.
		const FVec3 At = IdealContactPoint(Library->Get(EShotType::ServeFlat), Ratings->ForehandSign(), Library->Model.ServeTossHeightM);
		Ball.ServeToss(At, static_cast<int>(TeamSign));
		bAwaitingServe = false;
		bServing = true;
		bHasIdeal = false;
	}

	void FPlayerShotController::StartSwing(EShotButton InButton, bool bInAttack, bool bInTouch)
	{
		Button = InButton;
		bAttack = bInAttack;
		bTouch = bInTouch;
		PressTimeS = NowS;
		Charge = 0.0;
		SetPhase(ESwingPhase::Windup);
	}

	void FPlayerShotController::EndSwing(double FollowThroughS, double RecoveryS)
	{
		PhaseLengthS = FollowThroughS;
		RecoveryLengthS = RecoveryS;
		bAiSwinging = false;
		SetPhase(ESwingPhase::FollowThrough);
	}

	FSwingEvents FPlayerShotController::Tick(const FPlayerInputFrame& Input, double Dt, IBallPort& Ball)
	{
		FSwingEvents Events;
		NowS += Dt;
		PhaseTimeS += Dt;
		const FShotModel& M = Library->Model;

		const bool bDrivePressed = Input.bDrive && !Previous.bDrive;
		const bool bLobPressed = Input.bLob && !Previous.bLob;
		const bool bOverheadPressed = Input.bOverhead && !Previous.bOverhead;
		bool bPressUsed = false;

		if (bAwaitingServe && Phase == ESwingPhase::Idle && bDrivePressed)
		{
			TossForServe(Ball);
			Events.bServeTossed = true;
			bPressUsed = true;
		}

		if (Phase == ESwingPhase::Idle || Phase == ESwingPhase::Windup)
		{
			UpdateIdeal(Ball);
		}

		if (Phase == ESwingPhase::Idle && bAiPending && bHasIdeal)
		{
			// Start early enough that the backswing reaches the requested charge at release.
			const double StartAt = IdealTimeS - AiSwingLeadS(AiCharge, AiOffsetS);
			if (NowS >= StartAt)
			{
				const FAiButton B = ButtonFor(AiShotType);
				StartSwing(B.Button, B.bAttack, B.bTouch);
				bAiPending = false;
				bAiSwinging = true;
				Events.bSwingStarted = true;
			}
		}
		else if (Phase == ESwingPhase::Idle && !bPressUsed && !bAwaitingServe)
		{
			// While serving, only the drive button swings (GDD 4: serve is A / J).
			if (bDrivePressed)
			{
				StartSwing(EShotButton::Drive, Input.bModAttack, Input.bModTouch);
				Events.bSwingStarted = true;
			}
			else if (!bServing && bLobPressed)
			{
				StartSwing(EShotButton::Lob, Input.bModAttack, Input.bModTouch);
				Events.bSwingStarted = true;
			}
			else if (!bServing && bOverheadPressed)
			{
				StartSwing(EShotButton::Overhead, Input.bModAttack, Input.bModTouch);
				Events.bSwingStarted = true;
			}
		}

		switch (Phase)
		{
		case ESwingPhase::Windup:
		{
			Charge = Clamp((NowS - PressTimeS) / M.ChargeTimeS, 0.0, bAiSwinging ? AiCharge : 1.0);
			bool bReleased = false;
			if (bAiSwinging)
			{
				bReleased = bHasIdeal && NowS >= IdealTimeS + AiOffsetS;
			}
			else
			{
				// GDD 4: the modifiers count if held at any point in the swing.
				bAttack = bAttack || Input.bModAttack;
				bTouch = bTouch || Input.bModTouch;
				const bool bHeld = Button == EShotButton::Drive ? Input.bDrive : (Button == EShotButton::Lob ? Input.bLob : Input.bOverhead);
				bReleased = !bHeld;
			}
			// GDD 5.1: held through the whole window fires at the late edge. Use the
			// narrowest window this contact could be graded with, and fire on the last
			// tick inside it.
			const FShotParams& Family = FamilyShot(bIdealBounced);
			double Rating = std::fmin(Ratings->Unit(Family.ForehandRating), Ratings->Unit(Family.BackhandRating));
			if (bIdealOffGlass)
			{
				Rating = std::fmin(Rating, Ratings->Unit(ERating::WallPlay));
			}
			const double LateEdge = FTimingGrader::LateEdgeS(Family, Rating * (bWrongSide ? M.WrongSideMult : 1.0), M);
			const bool bAutoFire = bHasIdeal && NowS + Dt > IdealTimeS + LateEdge;
			if (bReleased || bAutoFire)
			{
				ReleaseAim = Input.Move;
				Release(Events, Ball);
			}
			break;
		}
		case ESwingPhase::Forward:
			if (NowS >= ContactTimeS)
			{
				Contact(Events, Ball);
			}
			break;
		case ESwingPhase::FollowThrough:
			if (PhaseTimeS >= PhaseLengthS)
			{
				SetPhase(ESwingPhase::Recovery);
			}
			break;
		case ESwingPhase::Recovery:
			if (PhaseTimeS >= RecoveryLengthS)
			{
				SetPhase(ESwingPhase::Idle);
			}
			break;
		default:
			break;
		}

		const double MoveMult = Phase == ESwingPhase::Idle ? 1.0 : FamilyShot(bIdealBounced).SwingMoveMult;
		Motion.Step(Input.Move, Input.bSprint, Dt, MoveMult);
		Previous = Input;
		return Events;
	}

	void FPlayerShotController::Release(FSwingEvents& Events, IBallPort& Ball)
	{
		const FShotParams& Family = FamilyShot(bIdealBounced);
		if (!bHasIdeal)
		{
			Events.Outcome = ESwingOutcome::ShadowSwing;
			EndSwing(Family.FollowThroughS, Family.RecoveryS);
			return;
		}
		ReleaseOffsetS = NowS - IdealTimeS;
		const double ShadowLimitS = Library->Model.ShadowSwingWindows * Family.EarlyLateWindowMs / 1000.0;
		if (ReleaseOffsetS < -ShadowLimitS)
		{
			Events.Outcome = ESwingOutcome::ShadowSwing;
			EndSwing(Family.FollowThroughS, Family.RecoveryS);
			return;
		}
		if (ReleaseOffsetS < 0.0)
		{
			// Early: the racket still meets the ball at the contact point, graded early.
			ContactTimeS = IdealTimeS;
			SetPhase(ESwingPhase::Forward);
			return;
		}
		Contact(Events, Ball);
	}

	void FPlayerShotController::Contact(FSwingEvents& Events, IBallPort& Ball)
	{
		const FShotModel& M = Library->Model;
		const FBallView View = Ball.Current();
		const FShotParams& Family = FamilyShot(View.GroundBounces >= 1);
		// A late swing is judged where the player set up for the ball: at the ideal
		// moment. Otherwise anything past the Good window would be out of reach, and
		// GDD 5.1 makes a late release a late hit, not a whiff.
		const bool bLate = ReleaseOffsetS > 0.0;
		const FVec3 Judged = bLate ? IdealBallPosition : View.Position;
		const FVec3 ToBall = Sub(Judged, Motion.GetPosition());
		const double Planar = LengthXY(ToBall);

		if (!IsPlayable(View, Ball.LastHitterTeam()) || Planar > ReachM() + M.ReachSlackM || Judged.Z > MaxContactHeight())
		{
			Events.Outcome = ESwingOutcome::Whiff;
			EndSwing(Family.FollowThroughS, Family.RecoveryS);
			return;
		}

		FShotContext C;
		C.Player = Ratings;
		C.PlayerId = PlayerId;
		C.TeamSign = TeamSign;
		C.Button = Button;
		C.bModAttack = bAttack;
		C.bModTouch = bTouch;
		C.Charge = Charge;
		C.Aim = ReleaseAim;
		if (bAiSwinging)
		{
			C.bHasTargetOverride = true;
			C.TargetOverride = AiTarget;
		}
		C.TimingOffsetS = ReleaseOffsetS;
		// The height decides the shot (GDD 4); the launch starts where the ball is now.
		C.ContactPosition = View.Position;
		C.ContactPosition.Z = Judged.Z;
		const double SideDot = DotProduct(ToBall, RightAxis(TeamSign));
		C.ContactSide = SideDot != 0.0 ? Sign(SideDot) : Ratings->ForehandSign();
		C.bBallBounced = View.GroundBounces >= 1;
		C.bOffGlass = View.bOffGlass;
		C.PlayerVelocity = Motion.GetVelocity();
		C.TopSpeedMps = Motion.TopSpeed(false);
		C.Stamina = Motion.GetStamina();
		C.bSplitStepped = Motion.ConsumeSplitStep();
		C.bSet = LengthXY(Motion.GetVelocity()) <= M.SetMaxSpeedMps && DotProduct(ToBall, ForwardAxis(TeamSign)) >= M.SetMinAheadM;
		C.bServe = bServing;
		C.bReturn = bNextIsReturn;
		C.bPressurePoint = bPressurePoint;
		C.bWrongSide = bWrongSide;

		// GDD 5.2: distance from the chosen shot's own ideal point;
		// heights inside the shot's band don't count against the player.
		const FShotParams& Shot = Library->Get(FShotSelector::Select(C, *Library).ShotType);
		const double H = Judged.Z;
		const double OutsideBand = std::fmax(0.0, std::fmax(Shot.ContactHeightMinM - H, H - Shot.MaxHeightFor(Ratings->HeightM)));
		C.PositionDistanceM = DistanceXY(Judged, IdealContactPoint(Shot, C.ContactSide, H)) + OutsideBand;

		Events.Intent = FShotResolver::Resolve(C, *Library, Rng);
		Events.Intent.Origin = View.Position;
		Events.Outcome = ESwingOutcome::Hit;
		Ball.ApplyShot(Events.Intent);

		bNextIsReturn = false;
		bServing = false;
		bHasIdeal = false;
		const FShotParams& Played = Events.Intent.ShotType == EShotType::Mishit ? Family : Library->Get(Events.Intent.ShotType);
		EndSwing(Played.FollowThroughS, Played.RecoveryS);
	}
}
