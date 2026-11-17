// One tick of a player's input, sampled by the game (stick, keys) or produced by the
// AI. Edges are found by comparing frames, so the same frames replay the same swings.

#pragma once

#include "Shots/ShotMath.h"

namespace Padel::Shots
{
	struct FPlayerInputFrame
	{
		// Team-relative: X = right, Y = toward the net (up on the stick), length 0..1.
		FVec2 Move;
		bool bSprint = false;
		// How far the move is going when the caller knows it (the AI, A3), so the gait chosen
		// is the one planned; below 0 for the stick (FPlayerMotion::Step).
		double PlannedDistanceM = -1.0;
		// Held state of each shot button this tick.
		bool bDrive = false;
		bool bLob = false;
		bool bOverhead = false;
		bool bModAttack = false;
		bool bModTouch = false;
	};
}
