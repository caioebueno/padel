// GDD 7: the stick at release becomes a target on the opponents' half. Up is deep,
// down is short, left and right are across, as seen from the hitter's own end (the
// camera is always behind the human's team, GDD 9). No input is a safe target in
// the middle.

#pragma once

#include "Shots/ShotMath.h"
#include "Shots/ShotParams.h"

namespace Padel::Shots
{
	struct PADELCORE_API FAimMapper
	{
		// Aim: X = right, Y = deep, each -1..1.
		static FVec3 Target(const FVec2& Aim, const FShotParams& Shot, double TeamSign);

		// GDD 3.2: diagonally into the box across from the server, aimed around the box centre.
		static FVec3 ServeTarget(const FVec2& Aim, const FShotParams& Shot, double TeamSign, double ServerY);

		static double Depth(const FVec2& Aim, const FShotParams& Shot);
	};
}
