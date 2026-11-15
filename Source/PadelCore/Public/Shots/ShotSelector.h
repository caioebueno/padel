// Picks the shot from the button, modifiers and context (GDD 4, 7).
//
// Height fallbacks keep every button useful: an overhead press on a low ball plays
// the groundstroke, a lob press above the shoulder plays a bandeja. The heights are
// each shot's contact band in Data/Shots.json.

#pragma once

#include "Shots/ShotMath.h"

#include "Shots/ShotContext.h"
#include "Shots/ShotLibrary.h"

namespace Padel::Shots
{
	struct FShotChoice
	{
		EShotType ShotType = EShotType::Drive;
		// Variant error, e.g. the backhand overhead (GDD 4).
		double ExtraErrorMult = 1.0;
	};

	struct PADELCORE_API FShotSelector
	{
		static FShotChoice Select(const FShotContext& Context, const FShotLibrary& Library);
	};
}
