// Display names for the four invented slice players (GDD 6.4: "Agent 7 and the asset
// agent design their names"). Each surname was web-searched for the GDD 1 originality
// check and matched no padel player, club, tour or racket brand.
//
// Only what the menus and HUD show. Ratings belong in Data/Players.json (Agent 3/4);
// when FPlayerRow lands with a display name, read it from there and drop this table.
//
// UI model: plain C++20, no Unreal headers (see PadelUiCodes.h).

#pragma once

#include <string>

namespace Padel::Ui
{
	struct FRosterPlayer
	{
		int Id;
		const char* First;
		const char* Last;
		const char* Side;   // GDD 6.1 preferred side
		const char* Style;  // GDD 6.1 play style tag
	};

	// Players 0, 1 are Team A; 2, 3 Team B (Agent 2's convention).
	const FRosterPlayer* FindPlayer(int Id);

	std::string TeamName(int Team);
	// Score bug label: "ARUNDE / VENTARA".
	std::string PairLabel(int Team);
	// "Mikko Ventara", or "Player 7" for an unknown id.
	std::string DisplayName(int Id);
	std::string Surname(int Id);
}
