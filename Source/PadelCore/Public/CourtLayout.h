// The placeholder court as a list of boxes, derived from CourtSpec.h. APadelCourt turns
// each box into a mesh component; the native tests ray-cast against the same list, so
// the geometry the game uses is the geometry CI checks.
//
// Plain C++ (no Unreal headers). Units are metres on the axes described in CourtSpec.h.

#pragma once

#include "CourtSpec.h"

#include <optional>
#include <string>
#include <vector>

namespace Padel::Court
{
	struct FVec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	enum class EZone : int
	{
		Court,
		Runoff,
		Ceiling,
	};

	// Placeholder colour roles; the Unreal side maps them to real colours.
	enum class EColorRole : int
	{
		Turf,
		Runoff,
		Line,
		Frame,
		Net,
		Hidden,
	};

	struct FCourtBox
	{
		std::string Name;
		FVec3 Centre;
		FVec3 Size;
		// Surface the ball would hit; None for visual-only pieces (lines, frames, tape).
		ESurface Surface = ESurface::None;
		EZone Zone = EZone::Court;
		bool bCollides = false;
		bool bVisible = true;
		EColorRole Color = EColorRole::Frame;
	};

	struct FRayHit
	{
		const FCourtBox* Box = nullptr;
		ESurface Surface = ESurface::None;
		// Fraction along the segment, 0 at Start, 1 at End.
		double Time = 0.0;
		FVec3 Point;
	};

	// Every box of the placeholder court. Built once and cached.
	const std::vector<FCourtBox>& BuildCourtLayout();

	// First colliding box hit by the segment Start -> End, if any. Starting inside a
	// box does not count as a hit on that box.
	std::optional<FRayHit> RaycastLayout(const std::vector<FCourtBox>& Layout, const FVec3& Start, const FVec3& End);
}
