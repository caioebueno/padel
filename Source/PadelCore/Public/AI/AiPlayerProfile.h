// The ratings the AI reads for one player (GDD 6.1), plus what they mean for
// planning. The Padel-side adapter copies them from Padel::Shots::FPlayerRatings
// (same field names); tests build them directly.

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiTuning.h"
#include "AI/AiTypes.h"

namespace Padel::AI
{
	struct PADELCORE_API FAiPlayerProfile
	{
		int Speed = 50;
		int Acceleration = 50;
		int Reach = 50;
		int Jump = 50;
		int WallPlay = 50;
		int Anticipation = 50;
		int Positioning = 50;
		int Teamwork = 50;
		int ReturnSkill = 50;
		ESide PreferredSide = ESide::Right;
		EHand Handedness = EHand::Right;

		// GDD 8: the difficulty's attribute scale, clamped to the 1..99 range.
		FAiPlayerProfile WithOffset(int Offset) const;

		double TopSpeed(const FAiTactics& T) const;
		double AccelTime(const FAiTactics& T) const;
		double SideReach(const FAiTactics& T) const;
		// Highest contact point, jumping (GDD 6.1 Reach and Jump).
		double MaxContactHeight(const FAiTactics& T) const;
		// GDD 8: Wall Play adds noise to the predicted rebound.
		double WallNoise(const FAiTactics& T, const FAiDifficulty& D) const;
		// GDD 6.1 Anticipation: how soon the player reacts after the opponent's contact.
		double ReadDelay(const FAiTactics& T, const FAiDifficulty& D) const;

		// The side of the court their forehand faces, in the team frame (+1 right).
		double ForehandSign() const { return Handedness == EHand::Right ? 1.0 : -1.0; }
	};
}
