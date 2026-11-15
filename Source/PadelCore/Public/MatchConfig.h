// Match format options (GDD 3.1, 11). Rule choices, not tuning, so the regulation
// values are the defaults here.

#pragma once

#include "PadelTypes.h"

namespace Padel
{
	struct FMatchConfig
	{
		// 2 = best of 3 (the slice default), 1 = one set.
		int SetsToWin = 2;
		// GDD 3.1: golden point at 40-40, on by default. Off means advantage scoring.
		bool bGoldenPoint = true;
		int GamesPerSet = 6;
		int TiebreakPoints = 7;
		// Super tiebreak instead of the deciding set.
		bool bSuperTiebreakFinal = false;
		int SuperTiebreakPoints = 10;
		// Change ends every this many points in a tiebreak.
		int TiebreakChangeEvery = 6;
		ETeam FirstServingTeam = ETeam::A;

		static FMatchConfig BestOfThree() { return FMatchConfig(); }

		static FMatchConfig OneSet()
		{
			FMatchConfig Config;
			Config.SetsToWin = 1;
			return Config;
		}
	};
}
