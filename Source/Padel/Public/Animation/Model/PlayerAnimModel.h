// The player's animation state, engine-free (docs/decisions/0005-player-animation.md).
//
// Stepped on the simulation's fixed tick right after FPlayerShotController::Tick, it
// turns the swing phases (docs/decisions/0004-shot-system.md) and the movement into
// what the Animation Blueprint plays: a locomotion state with stride matching and a
// Motion Matching trajectory, a swing clip with an explicit playhead, the full-body
// weight, celebrations, and a procedural racket position for the placeholder body.
//
// Contact sync: every swing clip is driven by one "swing progress" P:
//   0..1 windup (scrubbed by Charge), 1..2 forward swing, 2 = the contact frame,
//   2..3 follow-through, then the layer fades out over the recovery.
// P is exactly 2 on the tick where the shot controller reports the Hit, Whiff or
// ShadowSwing, so the clip's contact frame is on screen when the ball leaves the racket.

#pragma once

#include "Animation/Model/PlayerAnimTuning.h"
#include "Shots/PlayerShotController.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Padel::Animation
{
	using Shots::FVec3;

	enum class ELocomotionState : std::uint8_t
	{
		// Between points: relaxed stance.
		Idle,
		// Rally live or a ball coming: knees bent, racket up.
		Ready,
		Run,
		Sprint,
		// GDD 6.3: the little hop before the opponent's contact.
		SplitStep,
	};

	const char* LocomotionStateName(ELocomotionState State);

	// What the model needs each tick. FromController fills it from the shot controller.
	struct FPlayerAnimInput
	{
		Shots::ESwingPhase Phase = Shots::ESwingPhase::Idle;
		double PhaseTimeS = 0.0;
		double Charge = 0.0;
		Shots::EShotButton Button = Shots::EShotButton::Drive;
		bool bAttack = false;
		bool bTouch = false;
		bool bServing = false;
		bool bHasIdeal = false;
		double TimeToIdealS = 0.0;
		// In the Forward phase, the time left to contact (GDD 5.1: release + ForwardS).
		double TimeToContactS = 0.0;
		bool bIdealBounced = true;
		FVec3 IdealBallPosition;
		// This tick's swing result; Intent is valid when Outcome is Hit.
		Shots::ESwingOutcome Outcome = Shots::ESwingOutcome::None;
		Shots::FShotIntent Intent;

		FVec3 Position;
		FVec3 Velocity;
		bool bSprinting = false;
		bool bSplitHopping = false;
		double TeamSign = 1.0;
		double ForehandSign = 1.0;
		// M2: how the player is moving (gait), from FPlayerMotion.
		EMovementMode Mode = EMovementMode::Adjust;
		// The ball is in play (the owner knows: practice ball live, or a rally on).
		bool bRallyLive = false;

		static FPlayerAnimInput FromController(const Shots::FPlayerShotController& Controller, const Shots::FSwingEvents& Events, bool bInRallyLive);
	};

	inline constexpr int MaxTrajectorySamples = 8;

	struct FPlayerAnimFrame
	{
		ELocomotionState Locomotion = ELocomotionState::Idle;
		double SpeedMps = 0.0;
		// Movement direction relative to where the body faces: 0 forward, +90 to the right.
		double DirectionDeg = 0.0;
		// M2's movement mode this tick (N1).
		EMovementMode Mode = EMovementMode::Adjust;
		// World yaw of the net direction for this player's team, degrees, Unreal convention.
		double NetYawDeg = 0.0;
		// N1 ready-stance depth: how much the crouched clips weigh against the upright ones
		// (Animation.json ReadyCrouchAlphaNet at the net to ReadyCrouchAlphaBack at the back).
		double CrouchAlpha = 0.0;
		// Play rate for the run or sprint cycle so the feet match the ground speed.
		double StridePlayRate = 1.0;
		// World yaw the body faces, degrees, Unreal convention (N1): square to the net, or
		// turned toward travel in Crossover, TurnAndRun and Run, at FacingTurnRateDegPerS.
		double FacingYawDeg = 0.0;

		// Swing layer. Clip is the one to show; PrevClip fades out under it.
		const FAnimClip* Clip = nullptr;
		double ClipTimeS = 0.0;
		const FAnimClip* PrevClip = nullptr;
		double PrevClipTimeS = 0.0;
		// Weight of Clip against PrevClip (1 = only Clip).
		double ClipAlpha = 1.0;
		// Weight of the whole swing layer over the locomotion.
		double SwingWeight = 0.0;
		// 1: the swing owns the legs too (planted). 0: upper body only, legs from locomotion.
		double FullBodyAlpha = 0.0;
		double SwingProgress = 0.0;
		ESwingSide Side = ESwingSide::Fore;
		ESwingVariant Variant = ESwingVariant::Perfect;
		// This tick shows the contact frame.
		bool bContactFrame = false;
		// Speed the feet would slide at if the swing owned the legs while moving.
		double FootSlideMps = 0.0;

		// Celebration after a point (empty asset when none).
		std::string CelebrationAsset;
		double CelebrationTimeS = 0.0;
		double CelebrationWeight = 0.0;

		// Placeholder racket face centre, world metres (simulation axes).
		FVec3 RacketPosition;

		// Motion Matching trajectory: world positions (metres) at FAnimSettings::TrajectoryTimesS.
		std::array<FVec3, MaxTrajectorySamples> Trajectory{};
		int TrajectoryCount = 0;
	};

	class FPlayerAnimModel
	{
	public:
		FPlayerAnimModel(const FAnimSettings* InSettings, const FAnimClipLibrary* InClips, const Shots::FShotLibrary* InShots);

		const FPlayerAnimFrame& Step(const FPlayerAnimInput& In, double Dt);
		const FPlayerAnimFrame& GetFrame() const { return Frame; }

		// A point ended: celebrate or drop the head once the swing is over. The point
		// index picks the clip, so replays of a match show the same ones (no RNG).
		void NotifyPointEnded(bool bWon, unsigned PointIndex);

	private:
		void StepLocomotion(const FPlayerAnimInput& In, double Dt);
		void StepSwing(const FPlayerAnimInput& In, double Dt);
		void StepCelebration(const FPlayerAnimInput& In, double Dt);
		void StepRacket(const FPlayerAnimInput& In);
		void StepTrajectory(const FPlayerAnimInput& In, double Dt);
		void SetClip(const FAnimClip* InClip);
		EShotType FamilyShot(const FPlayerAnimInput& In) const;
		ESwingSide SideOf(const FPlayerAnimInput& In, const FVec3& BallPosition) const;
		double FollowThroughS(EShotType Shot) const;
		static double ClipTime(const FAnimClip& Clip, double Progress);

		const FAnimSettings* Settings = nullptr;
		const FAnimClipLibrary* Clips = nullptr;
		const Shots::FShotLibrary* ShotLib = nullptr;
		FPlayerAnimFrame Frame;

		Shots::ESwingPhase LastPhase = Shots::ESwingPhase::Idle;
		bool bFirstStep = true;
		// Forward swing: progress at the release and the time left to the ideal moment.
		double ForwardFromP = 0.0;
		double ForwardLengthS = 0.0;
		double FollowLengthS = 0.0;
		EShotType PlayedShot = EShotType::Drive;
		// Contact key for the placeholder racket, in the player's frame.
		FPlayerFrameVec ContactKey;
		bool bHasContactKey = false;
		double CrossfadeLeftS = 0.0;

		bool bCelebrationPending = false;
		bool bCelebrating = false;
		double CelebrationLeftS = 0.0;
		std::string PendingCelebration;

		std::vector<FVec3> History;
	};
}
