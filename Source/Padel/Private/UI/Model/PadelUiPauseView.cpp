#include "UI/Model/PadelUiPauseView.h"

namespace Padel::Ui
{
	namespace
	{
		// U+2013, the en dash the spec's score lines use.
		constexpr const char* PauseScoreDash = "\xE2\x80\x93";

		std::string PausePointLabel(const std::string& Label)
		{
			// The side without advantage shows "" in the score bug; spelled out it's 40.
			return Label.empty() ? std::string("40") : Label;
		}
	}

	std::vector<std::string> PauseScoreParts(const FScoreView& Score)
	{
		std::vector<std::string> Parts;
		for (const std::array<int, 2>& Set : Score.Sets)
		{
			Parts.push_back(std::to_string(Set[0]) + PauseScoreDash + std::to_string(Set[1]));
		}
		if (Score.bMatchOver)
		{
			return Parts;
		}
		Parts.push_back(std::to_string(Score.Games[0]) + PauseScoreDash + std::to_string(Score.Games[1]));
		Parts.push_back(PausePointLabel(Score.Points[0]) + PauseScoreDash + PausePointLabel(Score.Points[1]));
		return Parts;
	}

	EPauseContext PauseContextFor(std::string_view ElementId)
	{
		if (ElementId == "Pause.Controls")
		{
			return EPauseContext::Controls;
		}
		if (ElementId == "Pause.Settings")
		{
			return EPauseContext::Settings;
		}
		if (ElementId == "Pause.Quit")
		{
			return EPauseContext::Quit;
		}
		return EPauseContext::Rules;
	}
}
