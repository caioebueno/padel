// What the Pause screen shows (docs/ui/MENU_SPEC.md 7.5), worked out without the engine:
// the score line in its header and which context the focused item explains.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include "UI/Model/PadelUiScore.h"

#include <string>
#include <string_view>
#include <vector>

namespace Padel::Ui
{
	// The numbers between the two team names, oldest first: each finished set, then the
	// current games and points, with en dashes: {"6–4", "3–2", "30–15"}. During advantage
	// the side without it reads 40 ("AD–40"); in a tie-break the points are plain counts.
	std::vector<std::string> PauseScoreParts(const FScoreView& Score);

	// What the context panel explains for the focused item (spec 7.5).
	enum class EPauseContext
	{
		// Resume, Restart: the match rules and the two pairs.
		Rules,
		Controls,
		Settings,
		// "The match will not be saved."
		Quit,
	};

	// From a Pause element id ("Pause.Quit"). Unknown ids show the rules.
	EPauseContext PauseContextFor(std::string_view ElementId);
}
