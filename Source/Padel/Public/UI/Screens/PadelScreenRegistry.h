// Which widget class draws each screen. One line per screen in the .cpp, so the screen
// workstreams each change their own line and nothing else (spec 11.1).

#pragma once

#include "CoreMinimal.h"
#include "UI/PadelScreenTypes.h"

class UPadelScreenWidget;

namespace PadelScreenRegistry
{
	/** The class for a menu screen; the placeholder for one that isn't built yet. */
	TSubclassOf<UPadelScreenWidget> ScreenClass(EPadelScreen Screen);
}
