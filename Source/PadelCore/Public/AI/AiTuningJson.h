// Reads the AI's tuning files. They use Unreal's DataTable JSON layout (an array of
// flat row objects with a "Name" key), so the same text imports into DataTables and
// loads here without the engine, for the native tests and the soak run.

#pragma once

#include "PadelCoreApi.h"

#include "AI/AiTuning.h"

#include <string>
#include <vector>

namespace Padel::AI::TuningJson
{
	struct FLoadResult
	{
		bool bOk = false;
		// Human-readable problems: parse errors, unknown keys, missing keys.
		std::vector<std::string> Errors;
	};

	// Every field must be present exactly once, and no unknown keys are allowed, so the
	// data and the struct can't drift apart silently.
	PADELCORE_API FLoadResult ParseDifficulties(const std::string& Json, std::vector<FAiDifficulty>& OutRows);
	PADELCORE_API FLoadResult ParseTactics(const std::string& Json, FAiTactics& OutTactics);
}
