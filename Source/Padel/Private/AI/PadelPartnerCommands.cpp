#include "AI/PadelPartnerCommands.h"

namespace Padel::AIInput
{
std::optional<Padel::AI::EPartnerCommand> CommandForAction(EPadelInputAction Action)
{
	using Padel::AI::EPartnerCommand;
	switch (Action)
	{
	case EPadelInputAction::CmdNet: return EPartnerCommand::GoNet;
	case EPadelInputAction::CmdBack: return EPartnerCommand::StayBack;
	case EPadelInputAction::CmdMiddle: return EPartnerCommand::TakeMiddle;
	case EPadelInputAction::CmdYours: return EPartnerCommand::Yours;
	default: return std::nullopt;
	}
}
}
