#include "UI/Model/PadelUiScore.h"

namespace Padel::Ui
{
	std::vector<int> SetColumns(const FScoreView& View, int Team)
	{
		std::vector<int> Out;
		Out.reserve(View.Sets.size() + 1);
		for (const std::array<int, 2>& Set : View.Sets)
		{
			Out.push_back(Set[Team]);
		}
		if (!View.bMatchOver)
		{
			Out.push_back(View.Games[Team]);
		}
		return Out;
	}

	bool IsGoldenPointNow(const FScoreView& View)
	{
		return View.bGoldenPoint && !View.bInTiebreak && View.Points[0] == "40" && View.Points[1] == "40";
	}

	std::string PressureTag(const FScoreView& View)
	{
		if (View.bMatchOver)
		{
			return "";
		}
		if (View.bMatchPoint)
		{
			return "MATCH POINT";
		}
		if (View.bSetPoint)
		{
			return "SET POINT";
		}
		if (View.bBreakPoint)
		{
			return "BREAK POINT";
		}
		if (IsGoldenPointNow(View))
		{
			return GoldenPointTag;
		}
		if (View.bInTiebreak)
		{
			return TiebreakTag;
		}
		return "";
	}

	std::string SummaryLine(const FScoreView& View)
	{
		std::string Out = FinalLine(View.Sets);
		if (View.bMatchOver)
		{
			return Out;
		}
		if (!Out.empty())
		{
			Out += ' ';
		}
		Out += std::to_string(View.Games[0]) + "-" + std::to_string(View.Games[1]);
		Out += ' ' + View.Points[0] + "-" + View.Points[1];
		return Out;
	}

	std::string FinalLine(const std::vector<std::array<int, 2>>& Sets)
	{
		std::string Out;
		for (const std::array<int, 2>& Set : Sets)
		{
			if (!Out.empty())
			{
				Out += ' ';
			}
			Out += std::to_string(Set[0]) + "-" + std::to_string(Set[1]);
		}
		return Out;
	}

	std::string PointLabelFromCount(int Count)
	{
		static const char* const Labels[] = {"0", "15", "30", "40"};
		if (Count < 0)
		{
			return Labels[0];
		}
		return Labels[Count > 3 ? 3 : Count];
	}
}
