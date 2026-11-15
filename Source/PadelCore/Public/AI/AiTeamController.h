// Runs the AI for one pair (GDD 8): reads the ball after the opponent's contact,
// predicts it, decides who takes it, moves both players as a pair and picks the
// shot. One slot may be the human, whose partner then follows their depth and
// their commands (GDD 7, 8).
//
// The match drives it with events and one Tick per fixed step. It owns its seeded
// RNG, so the same seed and the same events give the same decisions.

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiBallPredictor.h"
#include "AI/AiPairTactics.h"
#include "AI/AiShotChooser.h"

#include <array>
#include <optional>

namespace Padel::AI
{
	// What the controller sees each tick, in metres.
	struct FAiWorldView
	{
		double Time = 0.0;
		FAiBall Ball;
		// GDD 3.2: the serve must bounce before it is returned.
		bool bMustBounce = false;
		std::array<FAiVec, 2> OwnPositions;
		std::array<FAiVec, 2> OwnVelocities;
		std::array<FAiVec, 2> OpponentPositions;
	};

	struct FAiTeamSetup
	{
		int HalfSign = 1;
		// Slot 0 plays the right side, slot 1 the left side.
		std::array<FAiPlayerProfile, 2> Profiles;
		// -1 when both are AI.
		int HumanSlot = -1;
		uint64_t Seed = 0;
	};

	class PADELCORE_API FAiTeamController
	{
	public:
		FAiTeamController(const FAiTeamSetup& Setup, const FAiTactics& InTactics, const FAiDifficulty& InDifficulty,
			const IAiBallModel& InModel);

		int GetHalfSign() const { return HalfSign; }
		// GDD 3.1 change of ends.
		void SetHalfSign(int InHalfSign) { HalfSign = InHalfSign; }
		bool IsAi(int Slot) const { return Slot != HumanSlot; }
		const FAiPlayerProfile& GetProfile(int Slot) const { return Profiles[Slot]; }
		const FAiPairTactics& GetPair() const { return Pair; }
		// The slot planned to take the incoming ball, or -1.
		int GetTaker() const { return Taker; }
		const std::optional<FInterceptPlan>& GetPlan(int Slot) const { return Plans[Slot]; }

		// --- Events from the match ---
		void OnPointStarted(bool bServing);
		void OnPointEnded();
		void OnBallHit(double Time, int HitterHalfSign, EAiShot Shot, const FAiVec& Contact, const FAiVec& Target);
		void OnBallContact(double Time, Court::ESurface Surface);

		// GDD 7 partner commands, for the human's partner. Returns true when the
		// partner takes the command, so the game can play the acknowledgement.
		bool GiveCommand(EPartnerCommand Command);

		// GDD 3.1 golden point: the AI pair picks its receiver. Returns a slot.
		int ChooseGoldenPointReceiver() const;

		FAiShotDecision ChooseServe(const FAiVec& ServerPosition);

		// One decision per AI slot; empty for the human's slot.
		std::array<std::optional<FAiIntent>, 2> Tick(const FAiWorldView& View);

	private:
		void Read(const FAiWorldView& View);
		FAiIntent MoveTo(int Slot, const FAiVec& From, const FAiVec& Target, bool bChasing, double Slack) const;
		double TeamReadDelay() const;

		int HalfSign = 1;
		int HumanSlot = -1;
		std::array<FAiPlayerProfile, 2> Profiles;
		const FAiTactics& Tactics;
		const FAiDifficulty& Difficulty;
		FAiBallPredictor Predictor;
		FAiPairTactics Pair;
		FPadelRng Rng;

		// Absolute time of the next read of the incoming ball, or negative for none.
		double ReadAt = -1.0;
		double PlanTime = 0.0;
		bool bIncoming = false;
		EAiShot LastOpponentShot = EAiShot::Drive;
		bool bShotSent = false;
		int Taker = -1;
		std::array<std::optional<FInterceptPlan>, 2> Plans;
		std::array<FAiVec2, 2> HomeError;
	};
}
