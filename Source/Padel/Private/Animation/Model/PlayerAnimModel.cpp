#include "Animation/Model/PlayerAnimModel.h"

#include "CourtSpec.h"

#include <algorithm>
#include <cmath>

namespace Padel::Animation
{
	using namespace Padel::Shots;

	namespace PlayerAnimModelDetail
	{
		constexpr double RadToDeg = 57.29577951308232;
		// Swing progress landmarks (see the header).
		constexpr double WindupEndP = 1.0;
		constexpr double ContactP = 2.0;
		constexpr double FollowEndP = 3.0;

		FPlayerFrameVec Mirror(const FPlayerFrameVec& V, double RightSign)
		{
			return { V.RightM * RightSign, V.ForwardM, V.UpM };
		}

		FPlayerFrameVec LerpVec(const FPlayerFrameVec& A, const FPlayerFrameVec& B, double T)
		{
			return { Lerp(A.RightM, B.RightM, T), Lerp(A.ForwardM, B.ForwardM, T), Lerp(A.UpM, B.UpM, T) };
		}

		ESwingVariant VariantFor(const FShotIntent& Intent)
		{
			switch (Intent.TimingGrade)
			{
			case ETimingGrade::Early: return ESwingVariant::Early;
			case ETimingGrade::Late: return ESwingVariant::Late;
			case ETimingGrade::Miss: return Intent.TimingOffsetS < 0.0 ? ESwingVariant::Early : ESwingVariant::Late;
			default: return ESwingVariant::Perfect;
			}
		}

		double MoveToward(double Value, double Target, double MaxDelta)
		{
			return Value < Target ? std::fmin(Target, Value + MaxDelta) : std::fmax(Target, Value - MaxDelta);
		}

		// (-180, 180].
		double WrapDeg(double A)
		{
			A = std::fmod(A, 360.0);
			if (A > 180.0)
			{
				A -= 360.0;
			}
			else if (A <= -180.0)
			{
				A += 360.0;
			}
			return A;
		}

		// Turns Angle toward Target the short way, at most MaxDelta degrees.
		double TurnToward(double Angle, double Target, double MaxDelta)
		{
			const double D = WrapDeg(Target - Angle);
			return WrapDeg(Angle + (D > 0.0 ? std::fmin(D, MaxDelta) : std::fmax(D, -MaxDelta)));
		}

		// Movement doc 3.3, 5.3: the gaits that turn the body toward where it's going.
		bool FacesTravel(EMovementMode Mode)
		{
			return Mode == EMovementMode::Crossover || Mode == EMovementMode::TurnAndRun || Mode == EMovementMode::Run;
		}
	}

	const char* LocomotionStateName(ELocomotionState State)
	{
		switch (State)
		{
		case ELocomotionState::Ready: return "Ready";
		case ELocomotionState::Run: return "Run";
		case ELocomotionState::Sprint: return "Sprint";
		case ELocomotionState::SplitStep: return "SplitStep";
		default: return "Idle";
		}
	}

	FPlayerAnimInput FPlayerAnimInput::FromController(const FPlayerShotController& Controller, const FSwingEvents& Events, bool bInRallyLive)
	{
		FPlayerAnimInput In;
		In.Phase = Controller.GetPhase();
		In.PhaseTimeS = Controller.GetPhaseTimeS();
		In.Charge = Controller.GetCharge();
		In.Button = Controller.GetButton();
		In.bAttack = Controller.IsAttackHeld();
		In.bTouch = Controller.IsTouchHeld();
		In.bServing = Controller.IsServing();
		In.bHasIdeal = Controller.HasIdealMoment();
		In.TimeToIdealS = Controller.TimeToIdealS();
		In.TimeToContactS = Controller.TimeToContactS();
		In.bIdealBounced = Controller.IsIdealBounced();
		In.IdealBallPosition = Controller.GetIdealBallPosition();
		In.Outcome = Events.Outcome;
		In.Intent = Events.Intent;
		const FPlayerMotion& Motion = Controller.GetMotion();
		In.Position = Motion.GetPosition();
		In.Velocity = Motion.GetVelocity();
		In.bSprinting = Motion.IsSprinting();
		In.bSplitHopping = Motion.IsSplitHopping();
		In.TeamSign = Controller.GetTeamSign();
		In.ForehandSign = Controller.GetRatings().ForehandSign();
		In.Mode = Motion.GetMode();
		In.bRallyLive = bInRallyLive;
		return In;
	}

	FPlayerAnimModel::FPlayerAnimModel(const FAnimSettings* InSettings, const FAnimClipLibrary* InClips, const FShotLibrary* InShots)
		: Settings(InSettings)
		, Clips(InClips)
		, ShotLib(InShots)
	{
	}

	const FPlayerAnimFrame& FPlayerAnimModel::Step(const FPlayerAnimInput& In, double Dt)
	{
		Frame.bContactFrame = false;
		StepSwing(In, Dt);
		StepLocomotion(In, Dt);
		StepCelebration(In, Dt);
		StepRacket(In);
		StepTrajectory(In, Dt);
		LastPhase = In.Phase;
		bFirstStep = false;
		return Frame;
	}

	void FPlayerAnimModel::StepLocomotion(const FPlayerAnimInput& In, double Dt)
	{
		const FAnimSettings& S = *Settings;
		const FVec3 Forward = ForwardAxis(In.TeamSign);
		const FVec3 Right = RightAxis(In.TeamSign);
		const double Speed = LengthXY(In.Velocity);
		Frame.SpeedMps = Speed;
		Frame.Mode = In.Mode;
		using PlayerAnimModelDetail::RadToDeg;
		Frame.NetYawDeg = std::atan2(Forward.Y, Forward.X) * RadToDeg;

		// Facing (N1): square to the net, or toward travel in the turning gaits (up to 180
		// degrees running back to the glass). A swing squares up again; forehand or backhand
		// still comes from the ball's side (SideOf, on the court's axes).
		double Want = Frame.NetYawDeg;
		const bool bRunning = PlayerAnimModelDetail::FacesTravel(In.Mode) && Speed >= S.WalkMinMps;
		if (bRunning && In.Phase == ESwingPhase::Idle)
		{
			Want = std::atan2(In.Velocity.Y, In.Velocity.X) * RadToDeg;
		}
		else if (In.GlassWall != EGlassWall::None && In.Phase == ESwingPhase::Idle)
		{
			// N1c (movement doc 2.10): waiting on a ball off the glass, stand side-on to it.
			// Running there still faces travel; the swing squares up as above.
			Want = GlassFacingYaw(In, Forward, Right);
		}
		Frame.FacingYawDeg = bFirstStep ? Want : PlayerAnimModelDetail::TurnToward(Frame.FacingYawDeg, Want, S.FacingTurnRateDegPerS * Dt);

		// Direction of travel relative to where the body faces, for the cycle blend.
		const double FacingRad = Frame.FacingYawDeg / RadToDeg;
		const FVec3 BodyForward = Vec(std::cos(FacingRad), std::sin(FacingRad), 0.0);
		// The body's right is its forward turned the same way the court's right is from the net.
		const double Handed = DotProduct(Vec(-Forward.Y, Forward.X, 0.0), Right) >= 0.0 ? 1.0 : -1.0;
		const FVec3 BodyRight = Vec(-BodyForward.Y * Handed, BodyForward.X * Handed, 0.0);
		Frame.DirectionDeg = Speed > 1e-6 ? std::atan2(DotProduct(In.Velocity, BodyRight), DotProduct(In.Velocity, BodyForward)) * RadToDeg : 0.0;

		// Ready-stance depth (N1, movement doc 2.1): taller at the net, lower at the back glass.
		const double FromNet = Clamp(std::fabs(In.Position.X) / Court::HalfLength, 0.0, 1.0);
		Frame.CrouchAlpha = Lerp(S.ReadyCrouchAlphaNet, S.ReadyCrouchAlphaBack, FromNet);

		const bool bSwinging = In.Phase != ESwingPhase::Idle;
		if (In.bSplitHopping)
		{
			Frame.Locomotion = ELocomotionState::SplitStep;
		}
		else if (Speed >= S.SprintMinMps && In.bSprinting)
		{
			Frame.Locomotion = ELocomotionState::Sprint;
		}
		else if (Speed >= S.WalkMinMps)
		{
			Frame.Locomotion = ELocomotionState::Run;
		}
		else
		{
			const bool bBallComing = In.bHasIdeal && In.TimeToIdealS < S.ReadyBallWindowS;
			Frame.Locomotion = (In.bRallyLive || bBallComing || bSwinging || In.bServing) ? ELocomotionState::Ready : ELocomotionState::Idle;
		}
		const double ClipSpeed = Frame.Locomotion == ELocomotionState::Sprint ? S.SprintClipSpeedMps : S.RunClipSpeedMps;
		Frame.StridePlayRate = Frame.Locomotion == ELocomotionState::Run || Frame.Locomotion == ELocomotionState::Sprint
			? Clamp(Speed / ClipSpeed, S.StrideRateMin, S.StrideRateMax) : 1.0;

		// The swing takes the legs only while the feet can stay planted.
		const double Target = bSwinging && Speed <= S.PlantedMaxSpeedMps ? 1.0 : 0.0;
		Frame.FullBodyAlpha = bFirstStep ? Target : PlayerAnimModelDetail::MoveToward(Frame.FullBodyAlpha, Target, S.FullBodyBlendPerS * Dt);
		if (!bSwinging)
		{
			Frame.FullBodyAlpha = 0.0;
		}
		Frame.FootSlideMps = Frame.FullBodyAlpha * Frame.SwingWeight * Speed;
	}

	EShotType FPlayerAnimModel::FamilyShot(const FPlayerAnimInput& In) const
	{
		// The windup plays before the shot is chosen (at contact, GDD 4), so it uses the
		// clip of the shot the button most likely plays, like the shot controller does.
		if (In.bServing)
		{
			return In.bAttack ? EShotType::ServeKick : EShotType::ServeFlat;
		}
		switch (In.Button)
		{
		case EShotButton::Lob: return EShotType::Lob;
		case EShotButton::Overhead: return In.bAttack ? EShotType::Vibora : EShotType::Bandeja;
		default:
			if (In.bTouch)
			{
				return EShotType::Chiquita;
			}
			return (!In.bHasIdeal || In.bIdealBounced) ? EShotType::Drive : EShotType::Volley;
		}
	}

	double FPlayerAnimModel::GlassFacingYaw(const FPlayerAnimInput& In, const FVec3& Forward, const FVec3& Right) const
	{
		using PlayerAnimModelDetail::RadToDeg;
		double TurnDeg = Settings->GlassSideTurnDeg;
		// Which way across the court the chest turns.
		double Across = 0.0;
		if (In.GlassWall == EGlassWall::Back)
		{
			// Chest toward the ball's side: the racket shoulder ends up toward the glass,
			// the other toward the net (a closed stance for either wing).
			TurnDeg = Settings->GlassBackTurnDeg;
			const ESwingSide Side = In.bHasIdeal ? SideOf(In, In.IdealBallPosition) : ESwingSide::Fore;
			Across = Sign(In.ForehandSign) * (Side == ESwingSide::Fore ? 1.0 : -1.0);
		}
		else
		{
			// Side glass: open toward that wall, in world Y, whichever half the player is on.
			const double WallY = In.GlassWall == EGlassWall::SidePlusY ? 1.0 : -1.0;
			Across = DotProduct(Vec(0.0, WallY, 0.0), Right) >= 0.0 ? 1.0 : -1.0;
		}
		const double T = TurnDeg / RadToDeg;
		const FVec3 Dir = Add(Scale(Forward, std::cos(T)), Scale(Right, std::sin(T) * Across));
		return std::atan2(Dir.Y, Dir.X) * RadToDeg;
	}

	ESwingSide FPlayerAnimModel::SideOf(const FPlayerAnimInput& In, const FVec3& BallPosition) const
	{
		const double Across = DotProduct(Sub(BallPosition, In.Position), RightAxis(In.TeamSign));
		if (Across == 0.0)
		{
			return ESwingSide::Fore;
		}
		return Sign(Across) == Sign(In.ForehandSign) ? ESwingSide::Fore : ESwingSide::Back;
	}

	double FPlayerAnimModel::FollowThroughS(EShotType Shot) const
	{
		return ShotLib->Get(Shot).FollowThroughS;
	}

	double FPlayerAnimModel::ClipTime(const FAnimClip& Clip, double Progress)
	{
		using namespace PlayerAnimModelDetail;
		if (Progress <= WindupEndP)
		{
			return Clip.WindupS * Clamp(Progress, 0.0, 1.0);
		}
		if (Progress <= ContactP)
		{
			return Lerp(Clip.WindupS, Clip.ContactS, Progress - WindupEndP);
		}
		return Lerp(Clip.ContactS, Clip.EndS, Clamp(Progress - ContactP, 0.0, 1.0));
	}

	void FPlayerAnimModel::SetClip(const FAnimClip* InClip)
	{
		if (InClip == Frame.Clip)
		{
			return;
		}
		if (Frame.Clip && Frame.SwingWeight > 0.0)
		{
			Frame.PrevClip = Frame.Clip;
			CrossfadeLeftS = Settings->ClipCrossfadeS;
			Frame.ClipAlpha = 0.0;
		}
		else
		{
			Frame.PrevClip = nullptr;
			Frame.ClipAlpha = 1.0;
		}
		Frame.Clip = InClip;
	}

	void FPlayerAnimModel::StepSwing(const FPlayerAnimInput& In, double Dt)
	{
		using namespace PlayerAnimModelDetail;
		const FAnimSettings& S = *Settings;
		const bool bPhaseChanged = In.Phase != LastPhase;
		const bool bContact = In.Outcome != ESwingOutcome::None;
		double& P = Frame.SwingProgress;

		if (In.Phase == ESwingPhase::Windup)
		{
			if (bPhaseChanged)
			{
				Frame.Clip = nullptr;
				Frame.PrevClip = nullptr;
				Frame.SwingWeight = 0.0;
				bHasContactKey = false;
			}
			Frame.Variant = ESwingVariant::Perfect;
			Frame.Side = In.bHasIdeal ? SideOf(In, In.IdealBallPosition) : ESwingSide::Fore;
			SetClip(Clips->Find(FamilyShot(In), Frame.Side, Frame.Variant));
			P = Clamp(In.Charge, 0.0, 1.0);
			Frame.SwingWeight = std::fmin(1.0, Frame.SwingWeight + Dt / S.SwingBlendInS);
		}
		else if (In.Phase == ESwingPhase::Forward)
		{
			if (bPhaseChanged)
			{
				// The forward swing: contact is TimeToContactS away (rounded up to the tick
				// the controller makes contact on).
				ForwardFromP = P;
				ForwardLengthS = std::fmax(Dt, std::ceil(In.TimeToContactS / Dt - 1e-9) * Dt);
			}
			// Hold just short of the contact frame until the contact tick shows it.
			P = std::fmin(ContactP - 1e-6, Lerp(ForwardFromP, ContactP, In.PhaseTimeS / ForwardLengthS));
			Frame.SwingWeight = 1.0;
		}
		else if (In.Phase == ESwingPhase::FollowThrough)
		{
			if (bContact)
			{
				// The contact tick: P is exactly the contact frame, whatever came before
				// (a late release goes straight from the backswing to contact).
				P = ContactP;
				Frame.bContactFrame = true;
				Frame.SwingWeight = 1.0;
				EShotType Family = FamilyShot(In);
				if (In.Outcome == ESwingOutcome::Hit)
				{
					Frame.Side = SideOf(In, In.Intent.Origin);
					Frame.Variant = VariantFor(In.Intent);
					PlayedShot = In.Intent.ShotType == EShotType::Mishit ? Family : In.Intent.ShotType;
					const FVec3 ToBall = Sub(In.Intent.Origin, In.Position);
					ContactKey = { DotProduct(ToBall, RightAxis(In.TeamSign)), DotProduct(ToBall, ForwardAxis(In.TeamSign)), In.Intent.Origin.Z };
					bHasContactKey = true;
				}
				else
				{
					PlayedShot = Family;
				}
				SetClip(Clips->Find(PlayedShot, Frame.Side, Frame.Variant));
				FollowLengthS = FollowThroughS(PlayedShot);
			}
			else if (FollowLengthS > 0.0)
			{
				P = ContactP + Clamp(In.PhaseTimeS / FollowLengthS, 0.0, 1.0);
			}
		}
		else if (In.Phase == ESwingPhase::Recovery)
		{
			P = FollowEndP;
			const double RecoveryS = ShotLib->Get(PlayedShot).RecoveryS;
			Frame.SwingWeight = RecoveryS > 0.0 ? Clamp(1.0 - In.PhaseTimeS / RecoveryS, 0.0, 1.0) : 0.0;
		}
		else
		{
			Frame.SwingWeight = 0.0;
			Frame.Clip = nullptr;
			Frame.PrevClip = nullptr;
			P = 0.0;
		}

		if (CrossfadeLeftS > 0.0 && !Frame.bContactFrame)
		{
			CrossfadeLeftS = std::fmax(0.0, CrossfadeLeftS - Dt);
		}
		Frame.ClipAlpha = Frame.PrevClip && S.ClipCrossfadeS > 0.0 ? 1.0 - CrossfadeLeftS / S.ClipCrossfadeS : 1.0;
		if (Frame.ClipAlpha >= 1.0)
		{
			Frame.PrevClip = nullptr;
		}
		Frame.ClipTimeS = Frame.Clip ? ClipTime(*Frame.Clip, P) : 0.0;
		Frame.PrevClipTimeS = Frame.PrevClip ? ClipTime(*Frame.PrevClip, P) : 0.0;
	}

	void FPlayerAnimModel::NotifyPointEnded(bool bWon, unsigned PointIndex)
	{
		const std::vector<std::string>& List = bWon ? Settings->CelebrateAssets : Settings->DejectedAssets;
		if (List.empty())
		{
			return;
		}
		PendingCelebration = List[PointIndex % List.size()];
		bCelebrationPending = true;
	}

	void FPlayerAnimModel::StepCelebration(const FPlayerAnimInput& In, double Dt)
	{
		const FAnimSettings& S = *Settings;
		if (In.Phase != ESwingPhase::Idle)
		{
			// A new swing (the next serve) cuts it; one still finishing waits for it.
			if (In.Phase == ESwingPhase::Windup)
			{
				bCelebrating = false;
				bCelebrationPending = false;
			}
		}
		else if (bCelebrationPending)
		{
			bCelebrationPending = false;
			bCelebrating = true;
			CelebrationLeftS = S.CelebrationS;
			Frame.CelebrationAsset = PendingCelebration;
		}
		if (bCelebrating)
		{
			CelebrationLeftS -= Dt;
			if (CelebrationLeftS <= 0.0)
			{
				bCelebrating = false;
			}
		}
		if (!bCelebrating)
		{
			Frame.CelebrationAsset.clear();
			Frame.CelebrationTimeS = 0.0;
			Frame.CelebrationWeight = 0.0;
			return;
		}
		Frame.CelebrationTimeS = S.CelebrationS - CelebrationLeftS;
		const double Edge = std::fmin(Frame.CelebrationTimeS, CelebrationLeftS);
		Frame.CelebrationWeight = Clamp(Edge / S.CelebrationBlendS, 0.0, 1.0);
	}

	void FPlayerAnimModel::StepRacket(const FPlayerAnimInput& In)
	{
		using namespace PlayerAnimModelDetail;
		const FAnimSettings& S = *Settings;
		// The right-hander's keys mirror to the side being played.
		const double RightSign = (Frame.Side == ESwingSide::Fore ? 1.0 : -1.0) * In.ForehandSign;
		const FPlayerFrameVec Ready = Mirror(S.ReadyRacket, In.ForehandSign);
		FPlayerFrameVec Local = Ready;
		if (Frame.Clip && Frame.SwingWeight > 0.0)
		{
			if (!bHasContactKey || In.Phase == ESwingPhase::Windup || In.Phase == ESwingPhase::Forward)
			{
				// Before contact the racket heads for where the ball will be at the ideal
				// moment, so on screen it arrives with the ball.
				if (In.bHasIdeal)
				{
					const FVec3 ToBall = Sub(In.IdealBallPosition, In.Position);
					ContactKey = { DotProduct(ToBall, RightAxis(In.TeamSign)), DotProduct(ToBall, ForwardAxis(In.TeamSign)), In.IdealBallPosition.Z };
				}
				else if (!bHasContactKey)
				{
					// The played shot's own contact point on this side (Shots.json, S1).
					const FShotParams& Shot = ShotLib->Get(PlayedShot);
					const bool bForehand = Frame.Side == ESwingSide::Fore;
					ContactKey = { RightSign * Shot.ContactSideFor(bForehand), Shot.ContactForwardFor(bForehand), Shot.PreferredContactHeightM };
				}
			}
			const FPlayerFrameVec Back = Mirror(Frame.Clip->Backswing, RightSign);
			const FPlayerFrameVec Follow = Mirror(Frame.Clip->Follow, RightSign);
			const double P = Frame.SwingProgress;
			FPlayerFrameVec Arc;
			if (P <= WindupEndP)
			{
				Arc = LerpVec(Ready, Back, P);
			}
			else if (P <= ContactP)
			{
				Arc = LerpVec(Back, ContactKey, P - WindupEndP);
			}
			else
			{
				Arc = LerpVec(ContactKey, Follow, Clamp(P - ContactP, 0.0, 1.0));
			}
			Local = LerpVec(Ready, Arc, Frame.SwingWeight);
		}
		FVec3 World = Add(In.Position, Add(Scale(RightAxis(In.TeamSign), Local.RightM), Scale(ForwardAxis(In.TeamSign), Local.ForwardM)));
		World.Z = Local.UpM;
		Frame.RacketPosition = World;
	}

	void FPlayerAnimModel::StepTrajectory(const FPlayerAnimInput& In, double Dt)
	{
		const std::vector<double>& Times = Settings->TrajectoryTimesS;
		const double Oldest = Times.empty() ? 0.0 : std::fmin(0.0, Times.front());
		const std::size_t Keep = static_cast<std::size_t>(std::ceil(-Oldest / Dt)) + 1;
		History.push_back(In.Position);
		if (History.size() > Keep)
		{
			History.erase(History.begin(), History.begin() + static_cast<std::ptrdiff_t>(History.size() - Keep));
		}
		Frame.TrajectoryCount = static_cast<int>(std::min<std::size_t>(Times.size(), MaxTrajectorySamples));
		for (int I = 0; I < Frame.TrajectoryCount; ++I)
		{
			const double T = Times[static_cast<std::size_t>(I)];
			FVec3 Sample;
			if (T < 0.0)
			{
				// Past: what the player actually did (oldest kept sample before the history fills).
				const std::size_t Back = std::min(History.size() - 1, static_cast<std::size_t>(std::lround(-T / Dt)));
				Sample = History[History.size() - 1 - Back];
			}
			else
			{
				// Future: the current velocity carried on, on the ground.
				Sample = Add(In.Position, Scale(Vec(In.Velocity.X, In.Velocity.Y, 0.0), T));
			}
			Frame.Trajectory[static_cast<std::size_t>(I)] = Sample;
		}
	}
}
