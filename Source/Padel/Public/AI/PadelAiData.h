// Loads the AI's tuning text files (Data/AiTactics.json, Data/Difficulty.json) for the
// game. Parsing lives in PadelCore (AI/AiTuningJson.h) so the native tests and the
// soak run read the same files the same way.

#pragma once

#include "CoreMinimal.h"
#include "AI/AiTuning.h"

#include <vector>

namespace Padel::AIData
{
	/** Reads Data/AiTactics.json from the project directory. Logs every problem and returns false on any. */
	PADEL_API bool LoadTactics(Padel::AI::FAiTactics& OutTactics);

	/** Reads every row of Data/Difficulty.json (Easy, Medium, Hard). */
	PADEL_API bool LoadDifficulties(std::vector<Padel::AI::FAiDifficulty>& OutRows);

	/** The row with this name, or null. */
	PADEL_API const Padel::AI::FAiDifficulty* FindDifficulty(const std::vector<Padel::AI::FAiDifficulty>& Rows, const FString& Name);
}
