// GDD 7 partner commands: which input action gives which command to the AI partner.
// The player controller binds IA_CmdNet / IA_CmdBack / IA_CmdMiddle / IA_CmdYours and
// passes the command to the team's FAiTeamController::GiveCommand.

#pragma once

#include "CoreMinimal.h"
#include "AI/AiTypes.h"
#include "Gameplay/PadelInputConfig.h"

#include <optional>

namespace Padel::AIInput
{
	/** The partner command for a D-pad command action, or empty for any other action. */
	PADEL_API std::optional<Padel::AI::EPartnerCommand> CommandForAction(EPadelInputAction Action);
}
