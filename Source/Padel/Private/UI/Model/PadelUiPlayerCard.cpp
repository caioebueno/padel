#include "UI/Model/PadelUiPlayerCard.h"

#include "UI/Model/PadelUiRoster.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Padel::Ui
{
	namespace
	{
		// Players.json ratings run 1-99 (GDD 6.1), so a meter is full at 99.
		constexpr float PlayerCardRatingMax = 99.0f;

		int PlayerCardRoundedMean(int Sum, int Count)
		{
			return static_cast<int>(std::lround(static_cast<double>(Sum) / static_cast<double>(Count)));
		}

		std::string PlayerCardLower(const char* Text)
		{
			std::string Out(Text);
			for (char& C : Out)
			{
				C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
			}
			return Out;
		}
	}

	EPlayStyle PlayStyleFromTag(const char* Tag)
	{
		if (!Tag)
		{
			return EPlayStyle::Unknown;
		}
		const std::string Lower = PlayerCardLower(Tag);
		if (Lower == "wall")
		{
			return EPlayStyle::Wall;
		}
		if (Lower == "net")
		{
			return EPlayStyle::Net;
		}
		if (Lower == "power")
		{
			return EPlayStyle::Power;
		}
		if (Lower == "touch")
		{
			return EPlayStyle::Touch;
		}
		if (Lower == "all-round" || Lower == "allround")
		{
			return EPlayStyle::AllRound;
		}
		return EPlayStyle::Unknown;
	}

	int CardStat(const Shots::FPlayerRatings& Ratings, const std::vector<std::string>& Attributes)
	{
		if (Attributes.empty())
		{
			return UnknownCardStat;
		}
		int Sum = 0;
		for (const std::string& Name : Attributes)
		{
			const Shots::ERating Rating = Shots::RatingFromName(Name);
			if (Rating == Shots::ERating::Count)
			{
				return UnknownCardStat;
			}
			Sum += Ratings.Get(Rating);
		}
		return PlayerCardRoundedMean(Sum, static_cast<int>(Attributes.size()));
	}

	int OverallRating(const Shots::FPlayerRatings& Ratings)
	{
		// Equal weights until spec 13 Q1 is decided (see the header).
		const int Count = static_cast<int>(Shots::ERating::Count);
		int Sum = 0;
		for (int Index = 0; Index < Count; ++Index)
		{
			Sum += Ratings.Get(static_cast<Shots::ERating>(Index));
		}
		return PlayerCardRoundedMean(Sum, Count);
	}

	FPlayerCardView MakePlayerCard(int Id, const Shots::FPlayerRatings* Ratings, const FCardStatGroups& Groups)
	{
		FPlayerCardView Card;
		Card.Id = Id;
		Card.Team = Id / 2;  // 0, 1 are Team A; 2, 3 Team B (PadelUiRoster.h)
		const FRosterPlayer* Player = FindPlayer(Id);
		if (Player)
		{
			Card.FirstName = Player->First;
			Card.Surname = Player->Last;
			Card.bLeftSide = std::strcmp(Player->Side, "Left") == 0;
			Card.Style = PlayStyleFromTag(Player->Style);
		}
		else
		{
			// Spec 7.12: "Player {id}" in the surname's place, no first name.
			Card.Surname = DisplayName(Id);
		}

		Card.bHasRatings = Ratings != nullptr;
		if (Ratings)
		{
			Card.bLeftSide = Ratings->PreferredSide == Shots::ECourtSide::Left;
			Card.bLeftHanded = Ratings->Handedness == Shots::EHandedness::Left;
			Card.HeightM = Ratings->HeightM;
			Card.Overall = OverallRating(*Ratings);
		}
		for (const auto& [Label, Attributes] : Groups)
		{
			Card.Stats.push_back({Label, Ratings ? CardStat(*Ratings, Attributes) : UnknownCardStat});
		}
		return Card;
	}

	std::string PlayerRowName(int Id)
	{
		const FRosterPlayer* Player = FindPlayer(Id);
		if (!Player)
		{
			return std::string();
		}
		return PlayerCardLower(Player->First) + "_" + PlayerCardLower(Player->Last);
	}

	std::string FormatHeight(double HeightM)
	{
		char Buffer[16];
		std::snprintf(Buffer, sizeof(Buffer), "%.2f m", HeightM);
		return Buffer;
	}

	float MeterFill(int Value)
	{
		if (Value == UnknownCardStat || Value <= 0)
		{
			return 0.0f;
		}
		const float Fill = static_cast<float>(Value) / PlayerCardRatingMax;
		return Fill > 1.0f ? 1.0f : Fill;
	}
}
