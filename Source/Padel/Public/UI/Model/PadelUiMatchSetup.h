// The Quick Match choices on the setup screen (GDD 11 mode 1: difficulty, match length,
// golden point on/off, which player the human controls).
//
// UI model: plain C++20, no Unreal headers.

#pragma once

#include <string>

namespace Padel::Ui
{
	enum class EDifficulty
	{
		Easy,
		Medium,
		Hard,
	};

	struct FMatchSetup
	{
		EDifficulty Difficulty = EDifficulty::Medium;
		// 2 = best of 3 (GDD 13 decision 5 default), 1 = one set.
		int SetsToWin = 2;
		bool bGoldenPoint = true;
		// Team A player the human controls. GDD 6.4: default the left side (player 1).
		int HumanPlayerId = 1;

		// Each setup row cycles through its values with left/right (Step = -1 or +1).
		void CycleDifficulty(int Step);
		void CycleLength();
		void ToggleGoldenPoint();
		void CyclePlayer();

		int PartnerId() const { return HumanPlayerId ^ 1; }

		std::string DifficultyLabel() const;
		// "Best of 3 sets" or "1 set".
		std::string LengthLabel() const;
		std::string GoldenPointLabel() const;
		// "Mikko Ventara (left side)".
		std::string PlayerLabel() const;
		// "easy", "medium", "hard": the row names in Data/Difficulty.json (Agent 4).
		std::string DifficultyKey() const;
	};
}
