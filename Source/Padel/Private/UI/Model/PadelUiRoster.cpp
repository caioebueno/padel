#include "UI/Model/PadelUiRoster.h"

#include <array>
#include <cctype>

namespace Padel::Ui
{
	namespace
	{
		// GDD 6.4: Team A is a right-side wall defender and a left-side power player;
		// Team B an all-round right-side player and a left-side touch player.
		constexpr std::array<FRosterPlayer, 4> Players{{
			{0, "Selva", "Arunde", "Right", "Wall"},
			{1, "Mikko", "Ventara", "Left", "Power"},
			{2, "Ilan", "Corvisa", "Right", "All-round"},
			{3, "Ruben", "Halvaro", "Left", "Touch"},
		}};

		constexpr std::array<const char*, 2> TeamNames{"Lagoon", "Ember"};

		std::string Upper(const char* Text)
		{
			std::string Out(Text);
			for (char& C : Out)
			{
				C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
			}
			return Out;
		}
	}

	const FRosterPlayer* FindPlayer(int Id)
	{
		for (const FRosterPlayer& Player : Players)
		{
			if (Player.Id == Id)
			{
				return &Player;
			}
		}
		return nullptr;
	}

	std::string TeamName(int Team)
	{
		return (Team == 0 || Team == 1) ? TeamNames[static_cast<std::size_t>(Team)] : "";
	}

	std::string PairLabel(int Team)
	{
		const FRosterPlayer* First = FindPlayer(Team * 2);
		const FRosterPlayer* Second = FindPlayer(Team * 2 + 1);
		if (!First || !Second)
		{
			return "";
		}
		return Upper(First->Last) + " / " + Upper(Second->Last);
	}

	std::string DisplayName(int Id)
	{
		if (const FRosterPlayer* Player = FindPlayer(Id))
		{
			return std::string(Player->First) + " " + Player->Last;
		}
		return "Player " + std::to_string(Id);
	}

	std::string Surname(int Id)
	{
		if (const FRosterPlayer* Player = FindPlayer(Id))
		{
			return Player->Last;
		}
		return "Player " + std::to_string(Id);
	}
}
