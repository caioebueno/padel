#include "UI/Model/PadelUiCodes.h"

#include <array>
#include <cctype>
#include <utility>

namespace Padel::Ui
{
	namespace
	{
		using FLabel = std::pair<std::string_view, std::string_view>;

		// GDD 3.3 table, then the special cases in GDD 3.3 and Agent 2's engine notes.
		constexpr std::array<FLabel, 15> ReasonLabels{{
			{"DOUBLE_BOUNCE", "Double bounce"},
			{"INTO_NET", "Into the net"},
			{"WALL_BEFORE_BOUNCE", "Glass on the fly"},
			{"OUT_DIRECT", "Out"},
			{"OWN_SIDE", "Stayed on own side"},
			{"BODY_HIT", "Hit a player"},
			{"DOUBLE_HIT", "Double hit"},
			{"NET_TOUCH", "Touched the net"},
			{"CROSSED", "Crossed the net"},
			{"RACKET_THROWN", "Racket thrown"},
			{"DOUBLE_FAULT", "Double fault"},
			{"VOLLEYED_SERVE", "Volleyed the serve"},
			{"OUT_OF_COURT_WINNER", "Out of the court"},
			{"CAME_BACK_OVER", "Came back over"},
			{"STUCK_IN_FENCE", "Stuck in the fence"},
		}};

		// The hitter's shot did the work: the opponents couldn't play it.
		constexpr std::array<std::string_view, 5> WinnerReasons{
			"DOUBLE_BOUNCE", "OUT_OF_COURT_WINNER", "BODY_HIT", "CAME_BACK_OVER", "STUCK_IN_FENCE"};

		constexpr std::array<FLabel, 5> TimingLabels{{
			{"PERFECT", "Perfect"}, {"GOOD", "Good"}, {"EARLY", "Early"}, {"LATE", "Late"}, {"MISS", "Miss"},
		}};

		constexpr std::array<FLabel, 11> ShotLabels{{
			{"DRIVE", "Drive"}, {"VOLLEY", "Volley"}, {"LOB", "Lob"}, {"BANDEJA", "Bandeja"},
			{"VIBORA", "V\xC3\xAD" "bora"}, {"SMASH", "Smash"}, {"CHIQUITA", "Chiquita"},
			{"SERVE", "Serve"}, {"SERVE_FLAT", "Serve"}, {"SERVE_KICK", "Kick serve"}, {"MISHIT", "Mishit"},
		}};

		template <std::size_t N>
		std::string Lookup(const std::array<FLabel, N>& Table, std::string_view Code)
		{
			const std::string Key = NormaliseCode(Code);
			for (const FLabel& Entry : Table)
			{
				if (Entry.first == Key)
				{
					return std::string(Entry.second);
				}
			}
			return SentenceCase(Code);
		}
	}

	std::string NormaliseCode(std::string_view Code)
	{
		std::string Out(Code);
		for (char& C : Out)
		{
			C = C == ' ' ? '_' : static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
		}
		return Out;
	}

	std::string SentenceCase(std::string_view Code)
	{
		std::string Out;
		Out.reserve(Code.size());
		for (const char C : Code)
		{
			const unsigned char U = static_cast<unsigned char>(C);
			if (C == '_')
			{
				Out.push_back(' ');
			}
			else if (Out.empty())
			{
				Out.push_back(static_cast<char>(std::toupper(U)));
			}
			else
			{
				Out.push_back(static_cast<char>(std::tolower(U)));
			}
		}
		return Out;
	}

	std::string ReasonLabel(std::string_view Reason)
	{
		return Lookup(ReasonLabels, Reason);
	}

	bool IsWinnerReason(std::string_view Reason)
	{
		const std::string Key = NormaliseCode(Reason);
		for (const std::string_view Winner : WinnerReasons)
		{
			if (Winner == Key)
			{
				return true;
			}
		}
		return false;
	}

	std::string ServeFaultLabel(std::string_view Fault)
	{
		if (Fault.empty())
		{
			return "Fault";
		}
		std::string Detail = SentenceCase(Fault);
		Detail[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(Detail[0])));
		return "Fault: " + Detail;
	}

	std::string TimingLabel(std::string_view Grade)
	{
		return Lookup(TimingLabels, Grade);
	}

	std::string ShotLabel(std::string_view Shot)
	{
		return Lookup(ShotLabels, Shot);
	}

	int TeamOfPlayer(int PlayerId)
	{
		if (PlayerId < 0 || PlayerId > 3)
		{
			return -1;
		}
		return PlayerId < 2 ? TeamA : TeamB;
	}

	int OtherTeam(int Team)
	{
		return Team == TeamA ? TeamB : TeamA;
	}
}
