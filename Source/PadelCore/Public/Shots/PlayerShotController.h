// One player's movement and swings, human or AI, engine-free (GDD 4, 5, 6.3, 7).
//
// Press a shot button: windup starts and charges. Release: the contact moment. The
// release is graded against the ideal moment, when the predicted ball comes closest
// to the ideal contact point. Early releases meet the ball at the ideal moment (the
// Forward phase), late ones at the release; holding through the window fires at the
// late edge. Contact builds an FShotContext and hands the resolved FShotIntent to the
// ball through IBallPort.

#pragma once

#include "Shots/ShotMath.h"

#include "Shots/BallPort.h"
#include "Shots/PlayerInputFrame.h"
#include "Shots/PlayerMotion.h"
#include "Shots/ShotContext.h"
#include "Shots/ShotLibrary.h"
#include "PadelRng.h"
#include "Shots/SwingPhase.h"

#include <cstdint>

namespace Padel::Shots
{
	enum class ESwingOutcome : std::uint8_t
	{
		None,
		// The ball was hit: see Intent.
		Hit,
		// Contact time came but the ball was out of reach or not playable.
		Whiff,
		// Released with no ball coming, or far too early: no contact at all.
		ShadowSwing,
	};

	// What happened this tick, for the game, the debug overlay and animation.
	struct FSwingEvents
	{
		bool bSwingStarted = false;
		bool bServeTossed = false;
		bool bSplitStep = false;
		ESwingOutcome Outcome = ESwingOutcome::None;
		FShotIntent Intent;
	};

	class PADELCORE_API FPlayerShotController
	{
	public:
		FPlayerShotController(int InPlayerId, double InTeamSign, const FPlayerRatings* InRatings,
			const FShotLibrary* InLibrary, const FMovementTuning* InMovement, const FVec3& InStart, std::uint64_t InSeed);

		FSwingEvents Tick(const FPlayerInputFrame& Input, double Dt, IBallPort& Ball);

		// Agent 4's entry point: the AI plays a shot through the same model as a human.
		// The swing starts so that Charge is reached at ideal + TimingOffsetS, when it releases.
		void RequestAiShot(EShotType ShotType, const FVec3& TargetM, double Charge, double TimingOffsetS);
		void CancelAiShot() { bAiPending = false; }

		// The match calls this when the other team hits, for the split-step (GDD 6.3).
		bool NotifyOpponentContact();

		// Serve (GDD 3.2): while waiting, the drive button tosses the ball; the next drive
		// press is the serve swing (RB held = kick).
		void SetAwaitingServe(bool bInAwaiting) { bAwaitingServe = bInAwaiting; bServing = false; }
		bool IsAwaitingServe() const { return bAwaitingServe; }
		void TossForServe(IBallPort& Ball);

		// Match context for the next contact.
		void SetPressurePoint(bool bIn) { bPressurePoint = bIn; }
		void SetNextIsReturn(bool bIn) { bNextIsReturn = bIn; }
		void SetWrongSide(bool bIn) { bWrongSide = bIn; }

		FPlayerMotion& GetMotion() { return Motion; }
		const FPlayerMotion& GetMotion() const { return Motion; }
		ESwingPhase GetPhase() const { return Phase; }
		double GetPhaseTimeS() const { return PhaseTimeS; }
		double GetCharge() const { return Charge; }
		EShotButton GetButton() const { return Button; }
		bool HasIdealMoment() const { return bHasIdeal; }
		// Seconds from now to the ideal contact moment (negative once it has passed).
		double TimeToIdealS() const { return IdealTimeS - NowS; }
		const FVec3& GetIdealBallPosition() const { return IdealBallPosition; }
		// The ideal contact point beside the player, on the given side (+1 right).
		FVec3 IdealContactPoint(double Side, double Height) const;
		double MaxContactHeight() const;
		double ReachM() const;
		int GetPlayerId() const { return PlayerId; }
		double GetTeamSign() const { return TeamSign; }

	private:
		void StartSwing(EShotButton InButton, bool bInAttack, bool bInTouch);
		void Release(FSwingEvents& Events, IBallPort& Ball);
		void Contact(FSwingEvents& Events, IBallPort& Ball);
		void EndSwing(double FollowThroughS, double RecoveryS);
		void UpdateIdeal(const IBallPort& Ball);
		void SetPhase(ESwingPhase InPhase) { Phase = InPhase; PhaseTimeS = 0.0; }
		// The shot a press would most likely play, for timing windows and swing lengths.
		const FShotParams& FamilyShot(bool bBallBounced) const;
		bool IsPlayable(const FBallView& View, int LastHitter) const;

		int PlayerId = 0;
		double TeamSign = 1.0;
		const FPlayerRatings* Ratings = nullptr;
		const FShotLibrary* Library = nullptr;
		FPlayerMotion Motion;
		FPadelRng Rng;

		double NowS = 0.0;
		FPlayerInputFrame Previous;

		ESwingPhase Phase = ESwingPhase::Idle;
		double PhaseTimeS = 0.0;
		double PhaseLengthS = 0.0;
		double RecoveryLengthS = 0.0;
		EShotButton Button = EShotButton::Drive;
		bool bAttack = false;
		bool bTouch = false;
		double PressTimeS = 0.0;
		double Charge = 0.0;
		double ContactTimeS = 0.0;
		double ReleaseOffsetS = 0.0;
		FVec2 ReleaseAim;

		bool bHasIdeal = false;
		double IdealTimeS = 0.0;
		FVec3 IdealBallPosition;
		bool bIdealBounced = false;
		bool bIdealOffGlass = false;

		bool bAiPending = false;
		bool bAiSwinging = false;
		EShotType AiShotType = EShotType::Drive;
		FVec3 AiTarget;
		double AiCharge = 0.0;
		double AiOffsetS = 0.0;

		bool bAwaitingServe = false;
		bool bServing = false;
		bool bPressurePoint = false;
		bool bNextIsReturn = false;
		bool bWrongSide = false;
	};
}
