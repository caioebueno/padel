#include "UI/Model/PadelUiMatchSetup.h"

#include "UI/Model/PadelUiRoster.h"

#include <cctype>

namespace Padel::Ui
{
	void FMatchSetup::CycleDifficulty(int Step)
	{
		const int Count = 3;
		const int Next = ((static_cast<int>(Difficulty) + Step) % Count + Count) % Count;
		Difficulty = static_cast<EDifficulty>(Next);
	}

	void FMatchSetup::CycleLength()
	{
		SetsToWin = SetsToWin == 2 ? 1 : 2;
	}

	void FMatchSetup::ToggleGoldenPoint()
	{
		bGoldenPoint = !bGoldenPoint;
	}

	void FMatchSetup::CyclePlayer()
	{
		HumanPlayerId = HumanPlayerId == 1 ? 0 : 1;
	}

	std::string FMatchSetup::DifficultyLabel() const
	{
		switch (Difficulty)
		{
		case EDifficulty::Easy:
			return "Easy";
		case EDifficulty::Hard:
			return "Hard";
		case EDifficulty::Medium:
			break;
		}
		return "Medium";
	}

	std::string FMatchSetup::LengthLabel() const
	{
		return SetsToWin == 1 ? "1 set" : "Best of 3 sets";
	}

	std::string FMatchSetup::GoldenPointLabel() const
	{
		return bGoldenPoint ? "On" : "Off (advantage)";
	}

	std::string FMatchSetup::PlayerLabel() const
	{
		const FRosterPlayer* Player = FindPlayer(HumanPlayerId);
		if (!Player)
		{
			return DisplayName(HumanPlayerId);
		}
		std::string Side(Player->Side);
		for (char& C : Side)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return DisplayName(HumanPlayerId) + " (" + Side + " side)";
	}

	std::string FMatchSetup::DifficultyKey() const
	{
		std::string Key = DifficultyLabel();
		for (char& C : Key)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return Key;
	}
}
