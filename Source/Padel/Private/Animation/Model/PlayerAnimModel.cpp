#include "Animation/Model/PlayerAnimModel.h"

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
		Frame.FacingYawDeg = std::atan2(Forward.Y, Forward.X) * PlayerAnimModelDetail::RadToDeg;
		Frame.DirectionDeg = Speed > 1e-6 ? std::atan2(DotProduct(In.Velocity, Right), DotProduct(In.Velocity, Forward)) * PlayerAnimModelDetail::RadToDeg : 0.0;

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
				// Early release: the racket meets the ball at the ideal moment, which is
				// TimeToIdealS away (rounded up to the tick the controller makes contact on).
				ForwardFromP = P;
				ForwardLengthS = std::fmax(Dt, std::ceil(In.TimeToIdealS / Dt - 1e-9) * Dt);
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
					ContactKey = { RightSign * ShotLib->Model.IdealContactSideM, ShotLib->Model.IdealContactForwardM, ShotLib->Get(PlayedShot).PreferredContactHeightM };
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
