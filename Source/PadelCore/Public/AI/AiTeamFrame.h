// Converts between court space (GDD 2, metres) and a team's own frame.
//
// A team is named by the sign of the half it stands on: +1 for +X (Team A at the
// start), -1 for -X. That keeps change of ends free of special cases.
// Team frame: X = metres to the player's right as they face the net, Y = depth,
// metres from the net toward their own back glass. Unreal is left-handed, so a team
// on half S faces -X*S and its right is -Y*S.

#pragma once

// Determinism pragmas (no FMA contraction) for the inline maths below; see PadelCoreApi.h.
#include "PadelCoreApi.h"

#include "AI/AiMath.h"

namespace Padel::AI::TeamFrame
{
	inline FAiVec2 ToLocal(int HalfSign, const FAiVec& Court)
	{
		return { -Court.Y * HalfSign, Court.X * HalfSign };
	}

	inline FAiVec ToCourt(int HalfSign, const FAiVec2& Local, double Height = 0.0)
	{
		return { Local.Y * HalfSign, -Local.X * HalfSign, Height };
	}

	// Depth of a court point for this team (negative on the other half).
	inline double Depth(int HalfSign, const FAiVec& Court)
	{
		return Court.X * HalfSign;
	}

	// A court-space ground direction as the stick vector: X right, Y toward the net.
	inline FAiVec2 MoveVector(int HalfSign, const FAiVec& CourtDir)
	{
		const FAiVec2 Local = ToLocal(HalfSign, CourtDir);
		return { Local.X, -Local.Y };
	}

	inline FAiVec CourtDirection(int HalfSign, const FAiVec2& Move)
	{
		return ToCourt(HalfSign, { Move.X, -Move.Y });
	}
}
