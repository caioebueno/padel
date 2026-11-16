// The animation model against the real shot controller and ball machine
// (docs/decisions/0005-player-animation.md): every swing's contact frame is on screen on
// the tick the ball is hit, the feet don't slide, and the data covers every slice shot
// in early, perfect and late versions.
#include "doctest.h"
#include "AnimTestData.h"
#include "Animation/Model/PlayerAnimModel.h"
#include "Shots/MotionCurve.h"
#include "Shots/PlayerShotController.h"
#include "Shots/PracticeBall.h"

#include <algorithm>
#include <cmath>
#include <functional>

using namespace ShotTestData;
using namespace AnimTestData;
using doctest::Approx;
using Padel::Animation::FPlayerAnimFrame;
using Padel::Animation::FPlayerAnimInput;
using Padel::Animation::FPlayerAnimModel;
using Padel::Animation::ELocomotionState;

namespace
{
	constexpr double Dt = 1.0 / 120.0;

	struct FAnimPlan
	{
		EShotButton Button = EShotButton::Drive;
		bool bAttack = false;
		bool bTouch = false;
		double LeadS = 0.3;
		double OffsetS = 0.0;
		double Side = 1.0;
	};

	struct FTick
	{
		FPlayerAnimFrame Frame;
		ESwingPhase Phase = ESwingPhase::Idle;
		double Charge = 0.0;
		ESwingOutcome Outcome = ESwingOutcome::None;
		FShotIntent Intent;
	};

	struct FAnimRig
	{
		FShotLibrary Lib = LoadLibrary();
		FMovementTuning Move = LoadMovement();
		FPracticeBallParams BallParams = LoadPracticeBall();
		FPlayerRatings Player = LoadPlayer();
		FAnimSettings Settings = LoadSettings();
		FAnimClipLibrary Clips = LoadClips();
		FPracticeBall Ball{ &BallParams };
		std::vector<FTick> Ticks;
		int ContactTick = -1;

		void Feed(const std::string& Name)
		{
			for (const FBallMachineFeed& F : LoadFeeds())
			{
				if (F.Name == Name)
				{
					Ball.Feed(Vec(F.OriginX, F.OriginY, F.OriginZ), Vec(F.TargetX, F.TargetY, 0.0), F.SpeedKmh / 3.6, F.ApexM, -1);
					return;
				}
			}
			FAIL("no feed " << Name);
		}

		// Chases the ball to the plan's side, swings, and runs until the swing is over,
		// stepping the animation model after every controller tick as the game does.
		void Play(FPlayerShotController& P, const FAnimPlan& Plan, bool bServe = false, double MaxS = 6.0)
		{
			FPlayerAnimModel Model(&Settings, &Clips, &Lib);
			bool bPressed = false;
			bool bReleased = false;
			for (int Step = 0; Step < static_cast<int>(MaxS / Dt); ++Step)
			{
				FPlayerInputFrame In;
				if (bServe && P.IsAwaitingServe() && Step == 1)
				{
					In.bDrive = true; // toss
				}
				if (ContactTick < 0 && P.HasIdealMoment() && !P.IsAwaitingServe())
				{
					if (!bServe && !bPressed)
					{
						const FVec3 Want = Sub(P.GetIdealBallPosition(), Sub(P.IdealContactPoint(P.ContactShot(), Plan.Side, 0.0), P.GetMotion().GetPosition()));
						const FVec3 To = Sub(Want, P.GetMotion().GetPosition());
						const double D = LengthXY(To);
						if (D > 0.05)
						{
							// Arrive rather than overrun: no faster than the player can still stop
							// in (braking has been no harder than starting since M1).
							const FPlayerMotion& M = P.GetMotion();
							const double Top = M.TopSpeed(false);
							const double Brake = MotionCurve::InitialAccel(Top, M.TimeToTopSpeed()) * Move.BrakeMult;
							const double Gain = std::min({ 1.0, D / 0.5, std::sqrt(2.0 * Brake * D) / Top });
							In.Move = FVec2{ DotProduct(To, RightAxis(P.GetTeamSign())) / D * Gain, DotProduct(To, ForwardAxis(P.GetTeamSign())) / D * Gain };
						}
					}
					if (!bPressed && P.TimeToIdealS() - P.ForwardSwingS() <= Plan.LeadS && P.GetPhase() == ESwingPhase::Idle)
					{
						bPressed = true;
					}
				}
				if (bPressed && !bReleased)
				{
					In.bModAttack = Plan.bAttack;
					In.bModTouch = Plan.bTouch;
					In.bDrive = Plan.Button == EShotButton::Drive;
					In.bLob = Plan.Button == EShotButton::Lob;
					In.bOverhead = Plan.Button == EShotButton::Overhead;
					if (P.GetPhase() == ESwingPhase::Windup && P.HasIdealMoment() && P.ForwardSwingS() - P.TimeToIdealS() >= Plan.OffsetS - 1e-9)
					{
						In.bDrive = In.bLob = In.bOverhead = false;
						bReleased = true;
					}
				}
				const FSwingEvents E = P.Tick(In, Dt, Ball);
				const FPlayerAnimFrame& F = Model.Step(FPlayerAnimInput::FromController(P, E, true), Dt);
				Ball.Advance(Dt);
				Ticks.push_back({ F, P.GetPhase(), P.GetCharge(), E.Outcome, E.Intent });
				if (E.Outcome != ESwingOutcome::None && ContactTick < 0)
				{
					ContactTick = static_cast<int>(Ticks.size()) - 1;
				}
				if (ContactTick >= 0 && P.GetPhase() == ESwingPhase::Idle)
				{
					return;
				}
			}
		}

		const FTick& Contact() const { return Ticks[static_cast<std::size_t>(ContactTick)]; }
	};

	struct FShotCase
	{
		const char* Name;
		const char* Feed;
		FVec3 Start;
		FAnimPlan Plan;
	};

	std::vector<FShotCase> SliceCases()
	{
		FAnimPlan Drive;
		FAnimPlan Lob;
		Lob.Button = EShotButton::Lob;
		FAnimPlan Chiquita;
		Chiquita.bTouch = true;
		Chiquita.LeadS = 0.55;
		FAnimPlan Bandeja;
		Bandeja.Button = EShotButton::Overhead;
		Bandeja.LeadS = 0.2;
		FAnimPlan Smash;
		Smash.Button = EShotButton::Overhead;
		Smash.LeadS = 0.95; // full charge, with room for the ideal moment to move while chasing
		FAnimPlan Vibora;
		Vibora.Button = EShotButton::Overhead;
		Vibora.bAttack = true;
		// The deep feed's stand point is (8.5, 2.0), as in Shots.Controller: since M2 a 2 m
		// shuffle from the middle is still moving the ideal moment at an early release.
		return {
			{ "drive", "deep_middle", Vec(8.5, 2.0, 0.0), Drive },
			{ "volley", "volley_body", Vec(3.0, 0.0, 0.0), Drive },
			{ "lob", "deep_middle", Vec(8.5, 2.0, 0.0), Lob },
			{ "chiquita", "short_low", Vec(6.0, 0.0, 0.0), Chiquita },
			{ "bandeja", "lob_deep", Vec(4.0, 0.0, 0.0), Bandeja },
			{ "smash", "lob_short", Vec(3.5, 0.0, 0.0), Smash },
			{ "vibora", "lob_deep", Vec(4.0, 0.0, 0.0), Vibora },
		};
	}

	// The checks every swing must pass on its contact tick (the Agent 6 brief: the contact
	// frame lines up with the moment the shot system hits the ball).
	void CheckContactSync(const FAnimRig& Rig, bool bRequireHit = true)
	{
		REQUIRE(Rig.ContactTick >= 0);
		const FTick& C = Rig.Contact();
		if (!bRequireHit && C.Outcome != ESwingOutcome::Hit)
		{
			// A whiff still swings through the contact frame on its tick.
			CHECK(C.Frame.bContactFrame);
			REQUIRE(C.Frame.Clip != nullptr);
			CHECK(C.Frame.ClipTimeS == Approx(C.Frame.Clip->ContactS));
			return;
		}
		REQUIRE(C.Outcome == ESwingOutcome::Hit);
		REQUIRE(C.Frame.Clip != nullptr);
		CHECK(C.Frame.bContactFrame);
		CHECK(C.Frame.SwingProgress == Approx(2.0));
		CHECK(C.Frame.ClipTimeS == Approx(C.Frame.Clip->ContactS));
		CHECK(C.Frame.SwingWeight == Approx(1.0));
		if (C.Frame.PrevClip)
		{
			// The family clip fading out shows its own contact frame too.
			CHECK(C.Frame.PrevClipTimeS == Approx(C.Frame.PrevClip->ContactS));
		}
		// The placeholder racket is on the ball.
		CHECK(Length(Sub(C.Frame.RacketPosition, C.Intent.Origin)) < 1e-6);
		// The played shot's clip, in the version for the grade.
		if (C.Intent.ShotType != EShotType::Mishit)
		{
			CHECK(C.Frame.Clip->Shot == C.Intent.ShotType);
		}
		const ESwingVariant Want = C.Intent.TimingGrade == ETimingGrade::Early ? ESwingVariant::Early
			: (C.Intent.TimingGrade == ETimingGrade::Late ? ESwingVariant::Late : ESwingVariant::Perfect);
		if (C.Intent.TimingGrade != ETimingGrade::Miss)
		{
			CHECK(C.Frame.Variant == Want);
			CHECK(C.Frame.Clip->Variant == Want);
		}
		// No tick before contact shows the contact frame.
		for (int I = 0; I < Rig.ContactTick; ++I)
		{
			CHECK_FALSE(Rig.Ticks[static_cast<std::size_t>(I)].Frame.bContactFrame);
			CHECK(Rig.Ticks[static_cast<std::size_t>(I)].Frame.SwingProgress < 2.0);
		}
		// Only the contact tick does.
		for (std::size_t I = static_cast<std::size_t>(Rig.ContactTick) + 1; I < Rig.Ticks.size(); ++I)
		{
			CHECK_FALSE(Rig.Ticks[I].Frame.bContactFrame);
		}
	}
}

TEST_CASE("Anim.Data: the real tuning files are valid and cover every slice shot")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	CHECK(Settings.Validate().empty());
	for (const std::string& Problem : Clips.Validate())
	{
		FAIL_CHECK(Problem);
	}
	CHECK(Settings.TrajectoryTimesS.size() <= static_cast<std::size_t>(Padel::Animation::MaxTrajectorySamples));
	// Every slice shot, both sides, every version resolves to a clip of that shot.
	for (int Index = 0; Index < static_cast<int>(EShotType::Mishit); ++Index)
	{
		const EShotType Shot = static_cast<EShotType>(Index);
		for (const ESwingSide Side : { ESwingSide::Fore, ESwingSide::Back })
		{
			for (const ESwingVariant Variant : { ESwingVariant::Early, ESwingVariant::Perfect, ESwingVariant::Late })
			{
				const FAnimClip* Clip = Clips.Find(Shot, Side, Variant);
				REQUIRE(Clip != nullptr);
				CHECK(Clip->Shot == Shot);
				CHECK(Clip->Variant == Variant);
			}
		}
	}
}

TEST_CASE("Anim.Data: a missing backhand falls back to the forehand, a missing version to perfect")
{
	FAnimClipLibrary Lib;
	FAnimClip Fore;
	Fore.Shot = EShotType::Smash;
	Fore.Name = FAnimClip::MakeName(Fore.Shot, ESwingSide::Fore, ESwingVariant::Perfect);
	Lib.Add(Fore);
	const FAnimClip* Found = Lib.Find(EShotType::Smash, ESwingSide::Back, ESwingVariant::Late);
	REQUIRE(Found != nullptr);
	CHECK(Found->Name == "smash_fore_perfect");
	CHECK(Lib.Find(EShotType::Drive, ESwingSide::Fore, ESwingVariant::Perfect) == nullptr);
	CHECK_FALSE(Lib.Validate().empty());
}

TEST_CASE("Anim.Contact: every slice shot shows its contact frame on the hit tick, early, perfect and late")
{
	int Versions[3] = { 0, 0, 0 };
	for (const FShotCase& Case : SliceCases())
	{
		for (const double Offset : { -0.16, 0.0, 0.13 })
		{
			CAPTURE(std::string(Case.Name));
			CAPTURE(Offset);
			FAnimRig Rig;
			FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Case.Start, 7);
			Rig.Feed(Case.Feed);
			FAnimPlan Plan = Case.Plan;
			Plan.OffsetS = Offset;
			Rig.Play(P, Plan);
			CheckContactSync(Rig);
			if (Rig.ContactTick >= 0)
			{
				++Versions[static_cast<int>(Rig.Contact().Frame.Variant)];
			}
		}
	}
	// The offsets reach all three versions.
	CHECK(Versions[static_cast<int>(ESwingVariant::Early)] >= 5);
	CHECK(Versions[static_cast<int>(ESwingVariant::Perfect)] >= 5);
	CHECK(Versions[static_cast<int>(ESwingVariant::Late)] >= 5);
}

TEST_CASE("Anim.Contact: both serves line up with the toss")
{
	for (const bool bKick : { false, true })
	{
		CAPTURE(bKick);
		FAnimRig Rig;
		FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Vec(8.5, -2.0, 0.0), 5);
		P.SetAwaitingServe(true);
		FAnimPlan Plan;
		Plan.bAttack = bKick;
		Plan.LeadS = 0.25;
		Rig.Play(P, Plan, true);
		CheckContactSync(Rig);
		CHECK(Rig.Contact().Frame.Clip->Shot == (bKick ? EShotType::ServeKick : EShotType::ServeFlat));
	}
}

TEST_CASE("Anim.Contact: the windup is scrubbed by the charge, the early forward swing runs to contact")
{
	FAnimRig Rig;
	// On the drive's stand point (M2): the early release comes while the player is planted,
	// not while a slow shuffle still moves the ideal moment.
	FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Vec(8.5, 2.0, 0.0), 3);
	Rig.Feed("deep_middle");
	FAnimPlan Plan;
	Plan.LeadS = 0.4;
	Plan.OffsetS = -0.15;
	Rig.Play(P, Plan);
	CheckContactSync(Rig);
	CHECK(Rig.Contact().Intent.TimingGrade == ETimingGrade::Early);

	int ForwardTicks = 0;
	double LastP = -1.0;
	for (int I = 0; I < Rig.ContactTick; ++I)
	{
		const FTick& T = Rig.Ticks[static_cast<std::size_t>(I)];
		if (T.Phase == ESwingPhase::Windup)
		{
			REQUIRE(T.Frame.Clip != nullptr);
			CHECK(T.Frame.ClipTimeS == Approx(T.Frame.Clip->WindupS * T.Charge));
		}
		if (T.Phase == ESwingPhase::Forward)
		{
			++ForwardTicks;
			CHECK(T.Frame.SwingProgress > LastP);
			LastP = T.Frame.SwingProgress;
		}
	}
	// 0.15 s early is about 18 ticks of forward swing, and the last one is nearly at contact
	// (within about two ticks of progress: M2's slower sideways chase shifts the ideal moment).
	CHECK(ForwardTicks >= 15);
	CHECK(LastP > 1.85);
}

TEST_CASE("Anim.Contact: a ball on the backhand side plays the backhand clip")
{
	FAnimRig Rig;
	FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Vec(8.5, 0.0, 0.0), 11);
	Rig.Feed("deep_middle");
	FAnimPlan Plan;
	Plan.Side = -Rig.Player.ForehandSign();
	Rig.Play(P, Plan);
	CheckContactSync(Rig);
	CHECK(Rig.Contact().Frame.Side == ESwingSide::Back);
	CHECK(Rig.Contact().Frame.Clip->Side == ESwingSide::Back);
}

TEST_CASE("Anim.Swing: the follow-through ends at the clip's end, then the layer fades out by Idle")
{
	FAnimRig Rig;
	FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Vec(8.5, 0.0, 0.0), 11);
	Rig.Feed("deep_middle");
	Rig.Play(P, FAnimPlan{});
	CheckContactSync(Rig);
	bool bSawRecovery = false;
	for (std::size_t I = static_cast<std::size_t>(Rig.ContactTick) + 1; I < Rig.Ticks.size(); ++I)
	{
		const FTick& T = Rig.Ticks[I];
		if (T.Phase == ESwingPhase::Recovery && !bSawRecovery)
		{
			bSawRecovery = true;
			CHECK(T.Frame.ClipTimeS == Approx(T.Frame.Clip->EndS));
		}
	}
	CHECK(bSawRecovery);
	CHECK(Rig.Ticks.back().Phase == ESwingPhase::Idle);
	CHECK(Rig.Ticks.back().Frame.SwingWeight == 0.0);
	CHECK(Rig.Ticks.back().Frame.Clip == nullptr);
}

TEST_CASE("Anim.Feet: a swing owns the legs only while the player is planted, so the feet never slide")
{
	for (const FShotCase& Case : SliceCases())
	{
		CAPTURE(std::string(Case.Name));
		FAnimRig Rig;
		FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Case.Start, 7);
		Rig.Feed(Case.Feed);
		Rig.Play(P, Case.Plan);
		REQUIRE(Rig.ContactTick >= 0);
		for (const FTick& T : Rig.Ticks)
		{
			CHECK(T.Frame.FootSlideMps <= Rig.Settings.PlantedMaxSpeedMps + 1e-9);
		}
	}

	// Swinging on the run: the swing goes to the upper body and the legs keep running.
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&Settings, &Clips, &Lib);
	FPlayerAnimInput In;
	In.Phase = ESwingPhase::Windup;
	In.Velocity = Vec(0.0, -4.0, 0.0);
	In.Position = Vec(6.0, 0.0, 0.0);
	for (int I = 0; I < 30; ++I)
	{
		In.PhaseTimeS = I * Dt;
		In.Charge = In.PhaseTimeS / 0.6;
		Model.Step(In, Dt);
	}
	CHECK(Model.GetFrame().FullBodyAlpha == 0.0);
	CHECK(Model.GetFrame().FootSlideMps == 0.0);
	CHECK(Model.GetFrame().Locomotion == ELocomotionState::Run);
	CHECK(Model.GetFrame().SwingWeight == Approx(1.0));
}

TEST_CASE("Anim.Locomotion: idle, ready, run, sprint and split-step, with stride matching")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&Settings, &Clips, &Lib);
	FPlayerAnimInput In;
	In.TeamSign = 1.0;

	CHECK(Model.Step(In, Dt).Locomotion == ELocomotionState::Idle);
	In.bRallyLive = true;
	CHECK(Model.Step(In, Dt).Locomotion == ELocomotionState::Ready);

	// Team A faces -X; moving toward -Y is moving to the player's right.
	In.Velocity = Vec(0.0, -Settings.RunClipSpeedMps, 0.0);
	const FPlayerAnimFrame& Run = Model.Step(In, Dt);
	CHECK(Run.Locomotion == ELocomotionState::Run);
	CHECK(Run.DirectionDeg == Approx(90.0));
	CHECK(Run.StridePlayRate == Approx(1.0));
	CHECK(Run.FacingYawDeg == Approx(180.0));

	In.Velocity = Vec(-0.5, 0.0, 0.0);
	const FPlayerAnimFrame& Slow = Model.Step(In, Dt);
	CHECK(Slow.DirectionDeg == Approx(0.0));
	CHECK(Slow.StridePlayRate == Approx(Settings.StrideRateMin));

	In.Velocity = Vec(-Settings.SprintClipSpeedMps, 0.0, 0.0);
	In.bSprinting = true;
	const FPlayerAnimFrame& Sprint = Model.Step(In, Dt);
	CHECK(Sprint.Locomotion == ELocomotionState::Sprint);
	CHECK(Sprint.StridePlayRate == Approx(1.0));

	In.bSplitHopping = true;
	CHECK(Model.Step(In, Dt).Locomotion == ELocomotionState::SplitStep);
}

TEST_CASE("Anim.Locomotion: the trajectory is the real past and the velocity carried forward")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&Settings, &Clips, &Lib);
	FPlayerAnimInput In;
	In.Velocity = Vec(-2.0, 0.0, 0.0);
	for (int I = 0; I <= 240; ++I)
	{
		In.Position = Vec(8.0 - 2.0 * I * Dt, 0.0, 0.0);
		Model.Step(In, Dt);
	}
	const FPlayerAnimFrame& F = Model.GetFrame();
	REQUIRE(F.TrajectoryCount == static_cast<int>(Settings.TrajectoryTimesS.size()));
	for (int I = 0; I < F.TrajectoryCount; ++I)
	{
		const double T = Settings.TrajectoryTimesS[static_cast<std::size_t>(I)];
		CAPTURE(T);
		CHECK(F.Trajectory[static_cast<std::size_t>(I)].X == Approx(In.Position.X - 2.0 * T).epsilon(1e-6));
	}
}

TEST_CASE("Anim.Celebration: waits for the swing, picks by point index, and the next swing cuts it")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&Settings, &Clips, &Lib);
	FPlayerAnimInput In;
	In.Phase = ESwingPhase::Recovery;
	Model.NotifyPointEnded(true, 4);
	CHECK(Model.Step(In, Dt).CelebrationAsset.empty());

	In.Phase = ESwingPhase::Idle;
	CHECK(Model.Step(In, Dt).CelebrationAsset == Settings.CelebrateAssets[4 % Settings.CelebrateAssets.size()]);
	CHECK(Model.GetFrame().CelebrationWeight < 1.0);
	for (int I = 0; I < static_cast<int>(Settings.CelebrationS / 2.0 / Dt); ++I)
	{
		Model.Step(In, Dt);
	}
	CHECK(Model.GetFrame().CelebrationWeight == Approx(1.0));

	In.Phase = ESwingPhase::Windup;
	CHECK(Model.Step(In, Dt).CelebrationAsset.empty());

	In.Phase = ESwingPhase::Idle;
	Model.NotifyPointEnded(false, 1);
	CHECK(Model.Step(In, Dt).CelebrationAsset == Settings.DejectedAssets[1 % Settings.DejectedAssets.size()]);
	for (int I = 0; I < static_cast<int>(Settings.CelebrationS / Dt) + 2; ++I)
	{
		Model.Step(In, Dt);
	}
	CHECK(Model.GetFrame().CelebrationAsset.empty());
}

TEST_CASE("Anim.Contact: an AI swing lines up the same way")
{
	for (const double Offset : { -0.1, 0.0, 0.08 })
	{
		CAPTURE(Offset);
		FAnimRig Rig;
		FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Vec(8.5, 0.0, 0.0), 13);
		Rig.Feed("deep_middle");
		FPlayerAnimModel Model(&Rig.Settings, &Rig.Clips, &Rig.Lib);
		P.RequestAiShot(EShotType::Drive, Vec(-7.0, 1.0, 0.0), 0.4, Offset);
		for (int Step = 0; Step < 720 && !(Rig.ContactTick >= 0 && P.GetPhase() == ESwingPhase::Idle); ++Step)
		{
			// Stand where the ball comes to the forehand, like the AI's intercept planner.
			FPlayerInputFrame In;
			if (P.HasIdealMoment() && Rig.ContactTick < 0)
			{
				const FVec3 Want = Sub(P.GetIdealBallPosition(), Sub(P.IdealContactPoint(P.ContactShot(), 1.0, 0.0), P.GetMotion().GetPosition()));
				const FVec3 To = Sub(Want, P.GetMotion().GetPosition());
				const double D = LengthXY(To);
				if (D > 0.05)
				{
					In.Move = FVec2{ DotProduct(To, RightAxis(1.0)) / D, DotProduct(To, ForwardAxis(1.0)) / D };
				}
			}
			const FSwingEvents E = P.Tick(In, Dt, Rig.Ball);
			const FPlayerAnimFrame& F = Model.Step(FPlayerAnimInput::FromController(P, E, true), Dt);
			Rig.Ball.Advance(Dt);
			Rig.Ticks.push_back({ F, P.GetPhase(), P.GetCharge(), E.Outcome, E.Intent });
			if (E.Outcome != ESwingOutcome::None && Rig.ContactTick < 0)
			{
				Rig.ContactTick = static_cast<int>(Rig.Ticks.size()) - 1;
			}
		}
		// The contact tick is whatever the shot model decides (a hit or a whiff): the
		// animation follows it either way.
		CheckContactSync(Rig, false);
	}
}

TEST_CASE("Anim.Facing: square to the net by default, toward travel in Crossover, TurnAndRun and Run (N1)")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	using Padel::EMovementMode;

	// Team A faces the net along -X (yaw 180). Moving toward -Y is moving to the player's right.
	struct FCase
	{
		EMovementMode Mode;
		FVec3 Velocity;
		double WantYaw;       // where the body ends up facing
		double WantDirection; // travel relative to the body
	};
	const double Run = Settings.RunClipSpeedMps;
	const FCase Cases[] = {
		{ EMovementMode::Adjust, Vec(0.0, -0.2, 0.0), 180.0, 90.0 },
		{ EMovementMode::Shuffle, Vec(0.0, -2.0, 0.0), 180.0, 90.0 },
		{ EMovementMode::Backpedal, Vec(2.0, 0.0, 0.0), 180.0, 180.0 },
		{ EMovementMode::Decel, Vec(0.0, -1.0, 0.0), 180.0, 90.0 },
		{ EMovementMode::Sprint, Vec(-Run, 0.0, 0.0), 180.0, 0.0 },
		{ EMovementMode::Crossover, Vec(0.0, -Run, 0.0), -90.0, 0.0 },
		{ EMovementMode::Run, Vec(0.0, Run, 0.0), 90.0, 0.0 },
		// Running back to the glass turns the body all the way round.
		{ EMovementMode::TurnAndRun, Vec(Run, 0.0, 0.0), 0.0, 0.0 },
	};
	for (const FCase& C : Cases)
	{
		CAPTURE(static_cast<int>(C.Mode));
		FPlayerAnimModel Model(&Settings, &Clips, &Lib);
		FPlayerAnimInput In;
		In.TeamSign = 1.0;
		In.bRallyLive = true;
		In.Position = Vec(6.0, 0.0, 0.0);
		// Start square to the net, then switch to the mode: the body turns at the data rate.
		Model.Step(In, Dt);
		In.Mode = C.Mode;
		In.Velocity = C.Velocity;
		const double Turn = std::fabs(C.WantYaw - 180.0) > 180.0 ? 360.0 - std::fabs(C.WantYaw - 180.0) : std::fabs(C.WantYaw - 180.0);
		const int TicksToTurn = static_cast<int>(std::ceil(Turn / (Settings.FacingTurnRateDegPerS * Dt)));
		if (TicksToTurn > 1)
		{
			const FPlayerAnimFrame& First = Model.Step(In, Dt);
			const double Moved = std::fabs(std::remainder(First.FacingYawDeg - 180.0, 360.0));
			CHECK(Moved == Approx(Settings.FacingTurnRateDegPerS * Dt).epsilon(0.01));
		}
		for (int I = 0; I < TicksToTurn + 2; ++I)
		{
			Model.Step(In, Dt);
		}
		const FPlayerAnimFrame& F = Model.GetFrame();
		CHECK(F.Mode == C.Mode);
		CHECK(std::remainder(F.FacingYawDeg - C.WantYaw, 360.0) == Approx(0.0).epsilon(1e-6));
		CHECK(std::remainder(F.DirectionDeg - C.WantDirection, 360.0) == Approx(0.0).epsilon(1e-6));
		CHECK(F.NetYawDeg == Approx(180.0));
	}

	// A swing squares the body up again, whatever the gait.
	FPlayerAnimModel Model(&Settings, &Clips, &Lib);
	FPlayerAnimInput In;
	In.TeamSign = 1.0;
	In.Mode = EMovementMode::Run;
	In.Velocity = Vec(0.0, Run, 0.0);
	for (int I = 0; I < 60; ++I)
	{
		Model.Step(In, Dt);
	}
	CHECK(Model.GetFrame().FacingYawDeg == Approx(90.0));
	In.Phase = ESwingPhase::Windup;
	for (int I = 0; I < 60; ++I)
	{
		In.PhaseTimeS = I * Dt;
		Model.Step(In, Dt);
	}
	CHECK(Model.GetFrame().FacingYawDeg == Approx(180.0));
}

TEST_CASE("Anim.Ready: the stance is taller at the net and lower at the back glass (N1)")
{
	const FAnimSettings Settings = LoadSettings();
	const FAnimClipLibrary Clips = LoadClips();
	const FShotLibrary Lib = LoadLibrary();
	FPlayerAnimModel Model(&Settings, &Clips, &Lib);
	FPlayerAnimInput In;
	In.TeamSign = 1.0;
	In.bRallyLive = true;
	// Net players stand taller with the racket up; back-court players stay low to dig balls off the glass.
	CHECK(Settings.ReadyCrouchAlphaBack > Settings.ReadyCrouchAlphaNet);
	In.Position = Vec(0.0, 0.0, 0.0);
	CHECK(Model.Step(In, Dt).CrouchAlpha == Approx(Settings.ReadyCrouchAlphaNet));
	In.Position = Vec(10.0, 0.0, 0.0);
	CHECK(Model.Step(In, Dt).CrouchAlpha == Approx(Settings.ReadyCrouchAlphaBack));
	// Halfway in between in the middle of the half, about 0.4 (movement doc 2.1).
	In.Position = Vec(5.0, 0.0, 0.0);
	const double Mid = Model.Step(In, Dt).CrouchAlpha;
	CHECK(Mid == Approx(0.5 * (Settings.ReadyCrouchAlphaNet + Settings.ReadyCrouchAlphaBack)));
	CHECK(Mid == Approx(0.4).epsilon(0.15));
}

TEST_CASE("Anim.Contact: the forward swing runs over ForwardS with no jump, contact only on the hit tick (N2)")
{
	for (const FShotCase& Case : SliceCases())
	{
		for (const double Offset : { -0.16, 0.0, 0.13 })
		{
			CAPTURE(std::string(Case.Name));
			CAPTURE(Offset);
			FAnimRig Rig;
			FPlayerShotController P(0, 1.0, &Rig.Player, &Rig.Lib, &Rig.Move, Case.Start, 7);
			Rig.Feed(Case.Feed);
			FAnimPlan Plan = Case.Plan;
			Plan.OffsetS = Offset;
			Rig.Play(P, Plan);
			REQUIRE(Rig.ContactTick >= 0);
			CheckContactSync(Rig, false);
			// The forward phase: at least ForwardS of it, and the clip moves on smoothly.
			int Forward = 0;
			double Biggest = 0.0;
			double Last = -1.0;
			const FTick& C = Rig.Contact();
			REQUIRE(C.Frame.Clip != nullptr);
			for (int I = 0; I <= Rig.ContactTick; ++I)
			{
				const FTick& T = Rig.Ticks[static_cast<std::size_t>(I)];
				if (T.Phase == ESwingPhase::Forward || I == Rig.ContactTick)
				{
					if (Last >= 0.0)
					{
						Biggest = std::max(Biggest, T.Frame.SwingProgress - Last);
					}
					Forward += T.Phase == ESwingPhase::Forward ? 1 : 0;
				}
				Last = T.Frame.SwingProgress;
			}
			const double ForwardS = Rig.Lib.Get(C.Frame.Clip->Shot).ForwardS;
			CHECK(Forward >= static_cast<int>(ForwardS / Dt) - 1);
			// No tick moves more than about twice an even share of the forward swing.
			CHECK(Biggest <= 2.0 * Dt / std::max(ForwardS, Dt) + 0.05);
		}
	}
}
