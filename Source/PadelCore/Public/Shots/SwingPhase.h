// Swing phases, the contract with animation (docs/decisions/0004-shot-system.md).

#pragma once

// Determinism pragmas (no FMA contraction) for the inline maths below; see PadelCoreApi.h.
#include "PadelCoreApi.h"

#include <cstdint>

namespace Padel::Shots
{
	enum class ESwingPhase : std::uint8_t
	{
		// No swing.
		Idle,
		// Button held: backswing, scrubbed by Charge (0..1 over FShotModel::ChargeTimeS).
		Windup,
		// Released early: the racket comes forward and meets the ball at the ideal moment.
		Forward,
		// After contact (or a whiff or shadow swing), FShotParams::FollowThroughS long.
		FollowThrough,
		// Back to the ready stance, FShotParams::RecoveryS long. No new swing until Idle.
		Recovery,
	};

	inline constexpr const char* SwingPhaseName(ESwingPhase Phase)
	{
		switch (Phase)
		{
		case ESwingPhase::Windup: return "Windup";
		case ESwingPhase::Forward: return "Forward";
		case ESwingPhase::FollowThrough: return "FollowThrough";
		case ESwingPhase::Recovery: return "Recovery";
		default: return "Idle";
		}
	}
}
