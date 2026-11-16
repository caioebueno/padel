#include "UI/Model/PadelUiQuickMatch.h"

namespace Padel::Ui
{
	FFocusMap SetupFocusMap()
	{
		// Spec 7.3: Difficulty, Format, Golden point, Start, wrapping top and bottom.
		FFocusMap Map = FFocusMap::VerticalList({SetupDifficultyId, SetupFormatId, SetupGoldenPointId, SetupStartId});
		// Principle 1: the defaults start with one press.
		Map.SetDefault(SetupStartId);
		return Map;
	}

	int DifficultyIndex(const FMatchSetup& Setup)
	{
		return static_cast<int>(Setup.Difficulty);
	}

	void SetDifficultyIndex(FMatchSetup& Setup, int Index)
	{
		if (Index >= 0 && Index < DifficultyCount)
		{
			Setup.Difficulty = static_cast<EDifficulty>(Index);
		}
	}

	int FormatIndex(const FMatchSetup& Setup)
	{
		return Setup.SetsToWin == 1 ? 0 : 1;
	}

	void SetFormatIndex(FMatchSetup& Setup, int Index)
	{
		if (Index == 0 || Index == 1)
		{
			Setup.SetsToWin = Index == 0 ? 1 : 2;
		}
	}

	std::array<int, 2> SetupPairOrder()
	{
		const FMatchSetup Defaults;
		return {Defaults.HumanPlayerId, Defaults.PartnerId()};
	}

	std::array<int, 2> SetupOpponentOrder()
	{
		return {2, 3};
	}

	std::array<int, 2> VersusPairOrder(int Team, const FMatchSetup& Setup)
	{
		if (Team == 0)
		{
			return {Setup.HumanPlayerId, Setup.PartnerId()};
		}
		return SetupOpponentOrder();
	}

	bool FVersusClock::TakeTimeout(bool bMatchReady)
	{
		if (bMatchReady || bStarted || bTimeoutShown || Elapsed < StartTimeout)
		{
			return false;
		}
		bTimeoutShown = true;
		return true;
	}
}
