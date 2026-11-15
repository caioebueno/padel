// One player's profile and ratings (GDD 6.1), engine-free. Filled from Data/Players.json
// by the Padel module (FPlayerRow has the same field names).

#pragma once

#include "Shots/ShotMath.h"

#include <cstdint>
#include <string_view>

namespace Padel::Shots
{
	enum class EHandedness : std::uint8_t { Right, Left };
	// RIGHT is the "drive" side, LEFT the "revés" side (GDD 6.1).
	enum class ECourtSide : std::uint8_t { Right, Left };

	enum class ERating : std::uint8_t
	{
		// Technical: the wrong-side penalty (GDD 6.2) applies to these.
		Drive, Backhand, Volley, Lob, Overhead, Bandeja, Vibora, WallPlay, Touch, Serve, ReturnSkill,
		// Physical
		Speed, Acceleration, Agility, Power, Reach, Jump, Stamina,
		// Mental
		Anticipation, Positioning, Composure, Consistency, Teamwork,
		Count,
	};

	inline constexpr bool IsTechnical(ERating Rating) { return Rating <= ERating::ReturnSkill; }

	// Names as used in Data/*.json ("Drive", "WallPlay", ...). Count when unknown.
	PADELCORE_API ERating RatingFromName(std::string_view Name);
	PADELCORE_API const char* RatingName(ERating Rating);

	struct PADELCORE_API FPlayerRatings
	{
		EHandedness Handedness = EHandedness::Right;
		ECourtSide PreferredSide = ECourtSide::Right;
		double HeightM = 1.80;

		// 1-99. The defaults are a mid-level placeholder, not tuning: real players come from Data/Players.json.
		int Drive = 50, Backhand = 50, Volley = 50, Lob = 50, Overhead = 50, Bandeja = 50, Vibora = 50,
			WallPlay = 50, Touch = 50, Serve = 50, ReturnSkill = 50;
		int Speed = 50, Acceleration = 50, Agility = 50, Power = 50, Reach = 50, Jump = 50, Stamina = 50;
		int Anticipation = 50, Positioning = 50, Composure = 50, Consistency = 50, Teamwork = 50;

		int Get(ERating Rating) const;
		void Set(ERating Rating, int Value);

		// GDD 5.3: rating / 99, with the GDD 6.2 wrong-side multiplier on technical ratings.
		double Unit(ERating Rating, double WrongSideMult = 1.0) const;

		// +1 when the forehand is on the player's right.
		double ForehandSign() const { return Handedness == EHandedness::Right ? 1.0 : -1.0; }
	};
}
