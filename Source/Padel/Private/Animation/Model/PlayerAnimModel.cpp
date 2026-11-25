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

		// A critically damped spring by half-life (the time to close half the gap), stepped
		// exactly for Dt: no overshoot, continuous velocity, the same at any frame rate.
		void Spring(double& X, double& V, double Goal, double HalfLifeS, double Dt)
		{
			const double Y = 2.0 * 0.6931471805599453 / std::fmax(HalfLifeS, 1e-4);
			const double J0 = X - Goal;
			const double J1 = V + J0 * Y;
			const double E = std::exp(-Y * Dt);
			X = E * (J0 + J1 * Dt) + Goal;
			V = E * (V - J1 * Y * Dt);
		}

		// The spring on an angle in degrees, the short way round, its speed capped at MaxRate
		// (0 = no cap). Snaps when within a quarter of a degree and nearly still (too small to
		// see), so a turn ends exactly on its target.
		void SpringAngle(double& Deg, double& Rate, double Goal, double HalfLifeS, double MaxRate, double Dt)
		{
			const double From = Deg;
			double X = Deg;
			Spring(X, Rate, Deg + WrapDeg(Goal - Deg), HalfLifeS, Dt);
			if (MaxRate > 0.0 && std::fabs(X - From) > MaxRate * Dt)
			{
				X = From + (X > From ? MaxRate : -MaxRate) * Dt;
				Rate = X > From ? MaxRate : -MaxRate;
			}
			Deg = WrapDeg(X);
			if (std::fabs(WrapDeg(Goal - Deg)) < 0.25 && std::fabs(Rate) < 15.0)
			{
				Deg = WrapDeg(Goal);
				Rate = 0.0;
			}
		}

		// P1: every field of a stance on its own spring.
		void SpringStance(FStancePose& X, FStancePose& V, const FStancePose& Goal, double HalfLifeS, double Dt)
		{
			Spring(X.CrouchAlpha, V.CrouchAlpha, Goal.CrouchAlpha, HalfLifeS, Dt);
			Spring(X.Racket.RightM, V.Racket.RightM, Goal.Racket.RightM, HalfLifeS, Dt);
			Spring(X.Racket.ForwardM, V.Racket.ForwardM, Goal.Racket.ForwardM, HalfLifeS, Dt);
			Spring(X.Racket.UpM, V.Racket.UpM, Goal.Racket.UpM, HalfLifeS, Dt);
			Spring(X.ArmAlpha, V.ArmAlpha, Goal.ArmAlpha, HalfLifeS, Dt);
			Spring(X.WidenM, V.WidenM, Goal.WidenM, HalfLifeS, Dt);
			Spring(X.DropM, V.DropM, Goal.DropM, HalfLifeS, Dt);
			Spring(X.TrunkDeg, V.TrunkDeg, Goal.TrunkDeg, HalfLifeS, Dt);
		}

		FStancePose LerpStance(const FStancePose& A, const FStancePose& B, double T)
		{
			FStancePose Out;
			Out.CrouchAlpha = A.CrouchAlpha + (B.CrouchAlpha - A.CrouchAlpha) * T;
			Out.Racket.RightM = A.Racket.RightM + (B.Racket.RightM - A.Racket.RightM) * T;
			Out.Racket.ForwardM = A.Racket.ForwardM + (B.Racket.ForwardM - A.Racket.ForwardM) * T;
			Out.Racket.UpM = A.Racket.UpM + (B.Racket.UpM - A.Racket.UpM) * T;
			Out.ArmAlpha = A.ArmAlpha + (B.ArmAlpha - A.ArmAlpha) * T;
			Out.WidenM = A.WidenM + (B.WidenM - A.WidenM) * T;
			Out.DropM = A.DropM + (B.DropM - A.DropM) * T;
			Out.TrunkDeg = A.TrunkDeg + (B.TrunkDeg - A.TrunkDeg) * T;
			return Out;
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

	FPlayerAnimInput FPlayerAnimInput::FromController(const FPlayerShotController& Controller, const FSwingEvents& Events, const FPlayerAnimContext& Context)
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
		In.DesiredVelocity = Motion.GetDesiredVelocity();
		In.AccelLimitMps2 = Motion.GetAccelLimit();
		In.bHasDesired = true;
		In.bSprinting = Motion.IsSprinting();
		In.bSplitHopping = Motion.IsSplitHopping();
		In.SplitHopLeftS = Motion.GetSplitHopLeftS();
		In.TeamSign = Controller.GetTeamSign();
		In.ForehandSign = Controller.GetRatings().ForehandSign();
		In.Mode = Motion.GetMode();
		In.bRallyLive = Context.bRallyLive;
		In.GlassWall = Context.GlassWall;
		In.ServeRole = Context.ServeRole;
		In.bBallVisible = Context.bBallVisible;
		In.BallPosition = Context.BallPosition;
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
		Frame.VelocityMps = In.Velocity;
		Frame.DesiredVelocityMps = In.DesiredVelocity;
		Frame.AccelLimitMps2 = In.AccelLimitMps2;
		Frame.bHasDesired = In.bHasDesired;
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
			// P1b (the motion catalogue, "lob over the net player"): going back with a ball to
			// watch, real players don't turn their back on it. They turn side-on, racket shoulder
			// back, and cross over, the head on the ball; the body turns at most BackRunTurnDeg
			// from square to the net.
			const double Back = -DotProduct(In.Velocity, Forward);
			if (WatchesBall(In) && Back > 0.0 && Back >= 0.7 * Speed)
			{
				const double T = S.BackRunTurnDeg / RadToDeg;
				const FVec3 Dir = Add(Scale(Forward, std::cos(T)), Scale(Right, std::sin(T) * Sign(In.ForehandSign)));
				Want = std::atan2(Dir.Y, Dir.X) * RadToDeg;
			}
		}
		else if (In.GlassWall != EGlassWall::None && In.Phase == ESwingPhase::Idle)
		{
			// N1c (movement doc 2.10): waiting on a ball off the glass, stand side-on to it.
			// Running there still faces travel; the swing squares up as above.
			Want = GlassFacingYaw(In, Forward, Right);
		}
		// F1: a spring, not a constant rate, so a turn eases in and out; the chest and head
		// lead it (StepBody).
		if (bFirstStep)
		{
			Frame.FacingYawDeg = Want;
			FacingRateDegPerS = 0.0;
		}
		else
		{
			PlayerAnimModelDetail::SpringAngle(Frame.FacingYawDeg, FacingRateDegPerS, Want, S.FacingHalfLifeS, S.FacingTurnRateDegPerS, Dt);
		}

		// Direction of travel relative to where the body faces, for the cycle blend.
		const double FacingRad = Frame.FacingYawDeg / RadToDeg;
		const FVec3 BodyForward = Vec(std::cos(FacingRad), std::sin(FacingRad), 0.0);
		// The body's right is its forward turned the same way the court's right is from the net.
		const double Handed = DotProduct(Vec(-Forward.Y, Forward.X, 0.0), Right) >= 0.0 ? 1.0 : -1.0;
		const FVec3 BodyRight = Vec(-BodyForward.Y * Handed, BodyForward.X * Handed, 0.0);
		Frame.DirectionDeg = Speed > 1e-6 ? std::atan2(DotProduct(In.Velocity, BodyRight), DotProduct(In.Velocity, BodyForward)) * RadToDeg : 0.0;

		const bool bSwinging = In.Phase != ESwingPhase::Idle;
		StepStance(In, Dt);
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
			Frame.Locomotion = (In.bRallyLive || bBallComing || bSwinging || In.bServing || In.ServeRole != EServeRole::None)
				? ELocomotionState::Ready : ELocomotionState::Idle;
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
		StepBody(In, Want, Dt);
	}

	bool FPlayerAnimModel::WatchesBall(const FPlayerAnimInput& In)
	{
		// The server's partner faces the receivers, not the ball behind them.
		return In.bBallVisible && (In.bRallyLive || (In.ServeRole != EServeRole::None && In.ServeRole != EServeRole::ServerPartner));
	}

	void FPlayerAnimModel::StepStance(const FPlayerAnimInput& In, double Dt)
	{
		using namespace PlayerAnimModelDetail;
		const FAnimSettings& S = *Settings;
		auto Pose = [&S](EStance Stance) -> const FStancePose& { return S.Stances[static_cast<std::size_t>(Stance)]; };

		// The motion catalogue, section 1: before the serve each of the four stands its own way;
		// in a rally, taller at the net and lower toward the back glass; between points, relaxed.
		FStancePose Goal;
		const bool bBallComing = In.bHasIdeal && In.TimeToIdealS < S.ReadyBallWindowS;
		if (In.bServing || In.ServeRole == EServeRole::Server)
		{
			Frame.Stance = EStance::Serve;
			Goal = Pose(EStance::Serve);
		}
		else if (In.ServeRole == EServeRole::Receiver)
		{
			Frame.Stance = EStance::Receive;
			Goal = Pose(EStance::Receive);
		}
		else if (In.ServeRole == EServeRole::ServerPartner)
		{
			Frame.Stance = EStance::Net;
			Goal = Pose(EStance::Net);
		}
		else if (In.ServeRole == EServeRole::ReceiverPartner)
		{
			Frame.Stance = EStance::Back;
			Goal = Pose(EStance::Back);
		}
		else if (In.bRallyLive || bBallComing || In.Phase != ESwingPhase::Idle)
		{
			const double FromNet = std::fabs(In.Position.X);
			const double T = Clamp((FromNet - S.StanceNetUntilM) / std::fmax(S.StanceBackFromM - S.StanceNetUntilM, 1e-3), 0.0, 1.0);
			Frame.Stance = T < 0.5 ? EStance::Net : EStance::Back;
			Goal = LerpStance(Pose(EStance::Net), Pose(EStance::Back), T);
		}
		else
		{
			Frame.Stance = EStance::Relaxed;
			Goal = Pose(EStance::Relaxed);
		}
		if (bFirstStep)
		{
			Frame.StancePose = Goal;
			StanceRate = FStancePose{};
		}
		else
		{
			SpringStance(Frame.StancePose, StanceRate, Goal, S.StanceHalfLifeS, Dt);
		}
	}

	void FPlayerAnimModel::StepBody(const FPlayerAnimInput& In, double WantYawDeg, double Dt)
	{
		using namespace PlayerAnimModelDetail;
		const FAnimSettings& S = *Settings;
		BodyClockS += Dt;

		// Facing lead: the head, then the chest, reach the new facing before the hips do.
		if (bFirstStep)
		{
			ChestYawDeg = HeadYawDeg = Frame.FacingYawDeg;
			ChestRateDegPerS = HeadRateDegPerS = 0.0;
			LastVelocity = In.Velocity;
		}
		else
		{
			SpringAngle(ChestYawDeg, ChestRateDegPerS, WantYawDeg, S.FacingChestHalfLifeS, 0.0, Dt);
			// P1: the head follows the ball when there is one to watch, within what the neck
			// can turn from the chest; otherwise it leads the body's turn.
			double HeadGoal = WantYawDeg;
			double PitchGoal = 0.0;
			double HeadHalfLife = S.FacingHeadHalfLifeS;
			if (WatchesBall(In))
			{
				const FVec3 ToBall = Sub(In.BallPosition, In.Position);
				const double Across = LengthXY(ToBall);
				if (Across > 0.05)
				{
					const double BallYaw = std::atan2(ToBall.Y, ToBall.X) * RadToDeg;
					HeadGoal = ChestYawDeg + Clamp(WrapDeg(BallYaw - ChestYawDeg), -S.HeadLookMaxYawDeg, S.HeadLookMaxYawDeg);
				}
				const double Eye = S.HeadLookEyeHeightM - Frame.StancePose.DropM;
				PitchGoal = Clamp(std::atan2(In.BallPosition.Z - Eye, std::fmax(Across, 0.3)) * RadToDeg, S.HeadLookMinPitchDeg, S.HeadLookMaxPitchDeg);
				HeadHalfLife = S.HeadLookHalfLifeS;
			}
			SpringAngle(HeadYawDeg, HeadRateDegPerS, HeadGoal, HeadHalfLife, 0.0, Dt);
			Spring(Frame.HeadPitchDeg, HeadPitchRateDegPerS, PitchGoal, S.HeadLookHalfLifeS, Dt);
		}
		Frame.ChestTwistDeg = Clamp(WrapDeg(ChestYawDeg - Frame.FacingYawDeg), -S.FacingMaxChestTwistDeg, S.FacingMaxChestTwistDeg);
		Frame.HeadTwistDeg = Clamp(WrapDeg(HeadYawDeg - Frame.FacingYawDeg) - Frame.ChestTwistDeg, -S.FacingMaxHeadTwistDeg, S.FacingMaxHeadTwistDeg);

		// Lean into the acceleration, in the body's own frame (Frame.FacingYawDeg is the pelvis).
		const FVec3 RawAccel = Dt > 0.0 ? Scale(Sub(In.Velocity, LastVelocity), 1.0 / Dt) : FVec3{};
		LastVelocity = In.Velocity;
		Spring(Accel.X, AccelRate.X, RawAccel.X, S.LeanHalfLifeS, Dt);
		Spring(Accel.Y, AccelRate.Y, RawAccel.Y, S.LeanHalfLifeS, Dt);
		const double FacingRad = Frame.FacingYawDeg / RadToDeg;
		const FVec3 BodyForward = Vec(std::cos(FacingRad), std::sin(FacingRad), 0.0);
		const FVec3 NetForward = ForwardAxis(In.TeamSign);
		const FVec3 CourtRight = RightAxis(In.TeamSign);
		const double Handed = DotProduct(Vec(-NetForward.Y, NetForward.X, 0.0), CourtRight) >= 0.0 ? 1.0 : -1.0;
		const FVec3 BodyRight = Vec(-BodyForward.Y * Handed, BodyForward.X * Handed, 0.0);
		double LeanF = DotProduct(Accel, BodyForward) * S.LeanDegPerMps2;
		double LeanR = DotProduct(Accel, BodyRight) * S.LeanDegPerMps2;
		const double Lean = std::sqrt(LeanF * LeanF + LeanR * LeanR);
		if (Lean > S.LeanMaxDeg && Lean > 0.0)
		{
			LeanF *= S.LeanMaxDeg / Lean;
			LeanR *= S.LeanMaxDeg / Lean;
		}
		Frame.LeanForwardDeg = LeanF;
		Frame.LeanRightDeg = LeanR;

		// Split step: a hop shaped over FPlayerMotion's hop, then the landing loads the legs.
		Frame.HopLiftM = 0.0;
		if (In.bSplitHopping)
		{
			HopElapsedS += Dt;
			const double Total = HopElapsedS + std::fmax(In.SplitHopLeftS, 0.0);
			const double T = Total > 0.0 ? Clamp(HopElapsedS / Total, 0.0, 1.0) : 1.0;
			Frame.HopLiftM = S.SplitHopHeightM * std::sin(3.141592653589793 * T);
		}
		else
		{
			if (HopElapsedS > 0.0)
			{
				LandLeftS = S.SplitLandS;
			}
			HopElapsedS = 0.0;
			LandLeftS = std::fmax(LandLeftS - Dt, 0.0);
		}

		// Ready bounce: only standing in the ready stance; it fades rather than stopping dead.
		const bool bStill = Frame.Locomotion == ELocomotionState::Ready && Frame.SpeedMps < S.WalkMinMps && In.Phase == ESwingPhase::Idle;
		Spring(ReadyBounceAmount, ReadyBounceRate, bStill ? 1.0 : 0.0, S.ReadyBounceHalfLifeS, Dt);
		const double Bounce = Clamp(ReadyBounceAmount, 0.0, 1.0) * S.ReadyBounceM * 0.5 * (1.0 - std::cos(2.0 * 3.141592653589793 * S.ReadyBounceHz * BodyClockS));

		// Pelvis: braking (deceleration along the velocity) and the landing sink it; springs back.
		const double Speed = LengthXY(In.Velocity);
		const double Braking = Speed > 1e-6 ? std::fmax(-DotProduct(Accel, In.Velocity) / Speed, 0.0) : 0.0;
		const double Plant = std::fmin(Braking * S.PlantDropPerMps2, S.PlantDropMaxM);
		const double Land = LandLeftS > 0.0 ? S.SplitLandDropM : 0.0;
		if (bFirstStep)
		{
			PelvisSinkM = 0.0;
			PelvisSinkRate = 0.0;
		}
		else
		{
			Spring(PelvisSinkM, PelvisSinkRate, std::fmax(Plant, Land), S.PelvisHalfLifeS, Dt);
		}
		Frame.PelvisDropM = -std::fmax(PelvisSinkM, 0.0) - Bounce;
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
		// The right-hander's keys mirror to the side being played.
		const double RightSign = (Frame.Side == ESwingSide::Fore ? 1.0 : -1.0) * In.ForehandSign;
		const FPlayerFrameVec Ready = Mirror(Frame.StancePose.Racket, In.ForehandSign);
		const FVec3 NetForward = ForwardAxis(In.TeamSign);
		const FVec3 CourtRight = RightAxis(In.TeamSign);
		auto Place = [&In](const FPlayerFrameVec& V, const FVec3& Forward, const FVec3& Right)
		{
			FVec3 Out = Add(In.Position, Add(Scale(Right, V.RightM), Scale(Forward, V.ForwardM)));
			Out.Z = V.UpM;
			return Out;
		};
		// The ready carry goes with the body: facing the net that is the court's frame, but a
		// body turned to run (back to the net on a run back) carried it on the court's frame
		// beside or behind itself, and the arm IK swung round the torso to reach it. The
		// swing's arc stays on the court's frame (its contact is where the ball is).
		const double FacingRad = Frame.FacingYawDeg / RadToDeg;
		const FVec3 BodyForward = Vec(std::cos(FacingRad), std::sin(FacingRad), 0.0);
		const double Handed = DotProduct(Vec(-NetForward.Y, NetForward.X, 0.0), CourtRight) >= 0.0 ? 1.0 : -1.0;
		const FVec3 BodyRight = Vec(-BodyForward.Y * Handed, BodyForward.X * Handed, 0.0);
		const FVec3 Carried = Place(Ready, BodyForward, BodyRight);
		FVec3 World = Carried;
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
			const FVec3 Swung = Place(Arc, NetForward, CourtRight);
			World = Add(Carried, Scale(Sub(Swung, Carried), Frame.SwingWeight));
		}
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
