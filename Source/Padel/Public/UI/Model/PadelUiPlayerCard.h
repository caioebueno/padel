// What a player card shows (docs/ui/MENU_SPEC.md 6.6): the name, the profile line in
// words, four card stats averaged from Data/Players.json attributes, and the overall
// rating. The engine reads the ratings (FPadelShotData::LoadPlayer) and draws the card
// (UPadelPlayerCard); the numbers are worked out here so the native suite checks them.
//
// UI model: plain C++20, no Unreal headers. It sees PadelCore (FPlayerRatings), which is
// plain C++ too.

#pragma once

#include "Shots/PlayerRatings.h"

#include <string>
#include <utility>
#include <vector>

namespace Padel::Ui
{
	// A stat the card can't show (no Players.json row, or an attribute name it doesn't
	// know): drawn as a dash with an empty bar (spec 7.12).
	inline constexpr int UnknownCardStat = -1;

	// The roster's play style tag (FRosterPlayer::Style) as a card word (copy Card.Style.*).
	enum class EPlayStyle
	{
		Unknown,
		Wall,
		Net,
		Power,
		Touch,
		AllRound,
	};

	EPlayStyle PlayStyleFromTag(const char* Tag);

	// Spec 6.6 (UiTheme.json PlayerCardStats): each card label and the attributes it averages.
	using FCardStatGroups = std::vector<std::pair<std::string, std::vector<std::string>>>;

	struct FCardStat
	{
		std::string Label;
		int Value = UnknownCardStat;
	};

	struct FPlayerCardView
	{
		int Id = 0;
		int Team = 0;
		std::string FirstName;
		// The surname, or "Player {id}" when the roster doesn't know the id.
		std::string Surname;
		// False when Data/Players.json had no row: the card shows dashes, Start stays enabled.
		bool bHasRatings = false;
		bool bLeftSide = false;
		bool bLeftHanded = false;
		double HeightM = 0.0;
		EPlayStyle Style = EPlayStyle::Unknown;
		std::vector<FCardStat> Stats;
		int Overall = UnknownCardStat;
	};

	// The rounded mean of the named attributes, or UnknownCardStat if the list is empty or
	// names an attribute FPlayerRatings doesn't have.
	int CardStat(const Shots::FPlayerRatings& Ratings, const std::vector<std::string>& Attributes);

	// GDD 6.2's overall rating.
	// TODO(spec 13 Q1): GDD 6.2 weights the attributes by preferred side but gives no
	// numbers, and Data/Players.json has no OverallWeights block yet (W6, Agent 3's file).
	// Until it does, every attribute counts the same.
	int OverallRating(const Shots::FPlayerRatings& Ratings);

	// Ratings may be null (no Players.json row). The name, side and style come from the UI
	// roster; side and handedness from the ratings when there are any.
	FPlayerCardView MakePlayerCard(int Id, const Shots::FPlayerRatings* Ratings, const FCardStatGroups& Groups);

	// The Data/Players.json row for a roster id: "mikko_ventara" (first_last, lower case),
	// the same rows APadelMatchDirector::PlayerRows names. Empty for an unknown id.
	std::string PlayerRowName(int Id);

	// "1.86 m".
	std::string FormatHeight(double HeightM);

	// How full a 1-99 meter is: Value / 99, clamped; 0 for an unknown stat.
	float MeterFill(int Value);
}
