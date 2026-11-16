// GDD 8 pair tactics: the formation, the home positions it gives each player, the
// partner commands (GDD 7) and who of the two takes the ball.
//
// Slot 0 is the pair's right-side player and slot 1 the left-side player, as they
// face the net. Positions here are in the team frame (AiTeamFrame.h).

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiInterceptPlanner.h"
#include "AI/AiRandom.h"

#include <array>
#include <optional>

namespace Padel::AI
{
	inline constexpr int RightSlot = 0;
	inline constexpr int LeftSlot = 1;

	struct FTakerQuery
	{
		std::array<std::optional<FInterceptPlan>, 2> Plans;
		std::array<FAiVec2, 2> PlayerLocal;
		std::array<FAiPlayerProfile, 2> Profiles;
		// Local X of the interception point the pair is deciding on.
		double BallLocalX = 0.0;
		// Teamwork of the pair: the lower of the two (GDD 6.1, partner coverage).
		int Teamwork = 50;
	};

	class PADELCORE_API FAiPairTactics
	{
	public:
		explicit FAiPairTactics(const FAiTactics& InTactics);

		EFormation GetFormation() const { return Formation; }
		void SetFormation(EFormation InFormation) { Formation = InFormation; }

		// GDD 8: both at the net after a good attacking shot or a deep lob; both back
		// after a lob over them.
		void OnOwnShot(EAiShot Shot, double ContactDepth, double TargetDepth);
		void OnOpponentShot(EAiShot Shot);
		// A human partner sets the pair's depth; the AI moves with them. Hysteresis
		// stops the AI flickering when the human hovers on the boundary.
		void FollowHuman(double HumanDepth);

		void SetCommand(int Slot, EPartnerCommand Command);
		EPartnerCommand GetCommand(int Slot) const { return Commands[Slot]; }
		// GDD 8: a command lasts until the end of the point.
		void ClearCommands();
		// "Yours" covers the next ball only: cleared once the pair has played it.
		void ConsumeYours();

		// Home positions for both slots, from the formation, the ball and any commands.
		std::array<FAiVec2, 2> HomePositions(const FAiVec2& BallLocal) const;
		// Side-to-side distance between the partners for a ball at this local X.
		double SpacingFor(double BallLocalX) const;

		// GDD 8: the player whose side the ball is on takes it; down the middle the
		// forehand of the left-side player by default, unless the partner is clearly
		// better placed. Teamwork decides how cleanly that works. Exactly one slot is
		// returned, so a pair never plays the ball twice.
		int ChooseTaker(const FTakerQuery& Query, FPadelRng& Rng) const;

	private:
		const FAiTactics& Tactics;
		EFormation Formation = EFormation::BothBack;
		std::array<EPartnerCommand, 2> Commands = { EPartnerCommand::None, EPartnerCommand::None };
	};
}
