#include "AI/AiTuningJson.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>

namespace Padel::AI::TuningJson
{
namespace
{
	// A value in a flat row: the tuning files only hold numbers and strings.
	struct FValue
	{
		bool bIsNumber = false;
		double Number = 0.0;
		std::string Text;
	};

	using FRow = std::map<std::string, FValue>;

	// A minimal reader for "[ { "Key": value, ... }, ... ]". Nested objects and arrays
	// aren't needed by the tuning files, so they are reported as errors.
	class FReader
	{
	public:
		explicit FReader(const std::string& InText) : Text(InText) {}

		bool ReadRows(std::vector<FRow>& OutRows, std::vector<std::string>& Errors)
		{
			SkipSpace();
			if (!Expect('['))
			{
				Errors.push_back("expected '[' at the start of the file");
				return false;
			}
			SkipSpace();
			if (Peek() == ']')
			{
				++Pos;
				return true;
			}
			while (true)
			{
				FRow Row;
				if (!ReadObject(Row, Errors))
				{
					return false;
				}
				OutRows.push_back(std::move(Row));
				SkipSpace();
				if (Peek() == ',')
				{
					++Pos;
					SkipSpace();
					continue;
				}
				if (Peek() == ']')
				{
					++Pos;
					return true;
				}
				Errors.push_back("expected ',' or ']' after a row at offset " + std::to_string(Pos));
				return false;
			}
		}

	private:
		const std::string& Text;
		size_t Pos = 0;

		char Peek() const { return Pos < Text.size() ? Text[Pos] : '\0'; }

		void SkipSpace()
		{
			while (Pos < Text.size() && std::isspace(static_cast<unsigned char>(Text[Pos])))
			{
				++Pos;
			}
		}

		bool Expect(char C)
		{
			if (Peek() != C)
			{
				return false;
			}
			++Pos;
			return true;
		}

		bool ReadString(std::string& Out)
		{
			if (!Expect('"'))
			{
				return false;
			}
			while (Pos < Text.size() && Text[Pos] != '"')
			{
				if (Text[Pos] == '\\' && Pos + 1 < Text.size())
				{
					++Pos;
				}
				Out.push_back(Text[Pos]);
				++Pos;
			}
			return Expect('"');
		}

		bool ReadValue(FValue& Out)
		{
			SkipSpace();
			if (Peek() == '"')
			{
				return ReadString(Out.Text);
			}
			const char* Start = Text.c_str() + Pos;
			char* End = nullptr;
			const double Number = std::strtod(Start, &End);
			if (End == Start)
			{
				return false;
			}
			Pos += static_cast<size_t>(End - Start);
			Out.bIsNumber = true;
			Out.Number = Number;
			return true;
		}

		bool ReadObject(FRow& Out, std::vector<std::string>& Errors)
		{
			if (!Expect('{'))
			{
				Errors.push_back("expected '{' at offset " + std::to_string(Pos));
				return false;
			}
			SkipSpace();
			if (Expect('}'))
			{
				return true;
			}
			while (true)
			{
				SkipSpace();
				std::string Key;
				if (!ReadString(Key))
				{
					Errors.push_back("expected a key at offset " + std::to_string(Pos));
					return false;
				}
				SkipSpace();
				if (!Expect(':'))
				{
					Errors.push_back("expected ':' after \"" + Key + "\"");
					return false;
				}
				FValue Value;
				if (!ReadValue(Value))
				{
					Errors.push_back("unsupported value for \"" + Key + "\" (numbers and strings only)");
					return false;
				}
				if (Out.count(Key) != 0)
				{
					Errors.push_back("duplicate key \"" + Key + "\"");
				}
				Out[Key] = Value;
				SkipSpace();
				if (Expect(','))
				{
					continue;
				}
				if (Expect('}'))
				{
					return true;
				}
				Errors.push_back("expected ',' or '}' at offset " + std::to_string(Pos));
				return false;
			}
		}
	};

	template <typename T>
	struct FField
	{
		const char* Key;
		double T::*DoubleMember;
		int T::*IntMember;
	};

	const std::vector<FField<FAiDifficulty>>& DifficultyFields()
	{
		static const std::vector<FField<FAiDifficulty>> Fields = {
			{ "ReadDelayS", &FAiDifficulty::ReadDelayS, nullptr },
			{ "TimingWeightPerfect", &FAiDifficulty::TimingWeightPerfect, nullptr },
			{ "TimingWeightGood", &FAiDifficulty::TimingWeightGood, nullptr },
			{ "TimingWeightEarlyLate", &FAiDifficulty::TimingWeightEarlyLate, nullptr },
			{ "TimingWeightMiss", &FAiDifficulty::TimingWeightMiss, nullptr },
			{ "SafeShotBias", &FAiDifficulty::SafeShotBias, nullptr },
			{ "TacticalWeight", &FAiDifficulty::TacticalWeight, nullptr },
			{ "AimScatterMult", &FAiDifficulty::AimScatterMult, nullptr },
			{ "AttributeOffset", nullptr, &FAiDifficulty::AttributeOffset },
			{ "PredictionNoiseMult", &FAiDifficulty::PredictionNoiseMult, nullptr },
		};
		return Fields;
	}

	const std::vector<FField<FAiTactics>>& TacticsFields()
	{
		static const std::vector<FField<FAiTactics>> Fields = {
			{ "NetDepthM", &FAiTactics::NetDepthM, nullptr },
			{ "BackDepthM", &FAiTactics::BackDepthM, nullptr },
			{ "PartnerSpacingM", &FAiTactics::PartnerSpacingM, nullptr },
			{ "BallShiftFactor", &FAiTactics::BallShiftFactor, nullptr },
			{ "MaxLateralShiftM", &FAiTactics::MaxLateralShiftM, nullptr },
			{ "TakeMiddleShiftM", &FAiTactics::TakeMiddleShiftM, nullptr },
			{ "HumanNetDepthM", &FAiTactics::HumanNetDepthM, nullptr },
			{ "HumanDepthHysteresisM", &FAiTactics::HumanDepthHysteresisM, nullptr },
			{ "DriveToNetMaxDepthM", &FAiTactics::DriveToNetMaxDepthM, nullptr },
			{ "LobAdvanceMinDepthM", &FAiTactics::LobAdvanceMinDepthM, nullptr },
			{ "AtNetDepthM", &FAiTactics::AtNetDepthM, nullptr },
			{ "HomeErrorMaxM", &FAiTactics::HomeErrorMaxM, nullptr },
			{ "CourtMarginM", &FAiTactics::CourtMarginM, nullptr },
			{ "MinPlayerDepthM", &FAiTactics::MinPlayerDepthM, nullptr },
			{ "ArriveRadiusM", &FAiTactics::ArriveRadiusM, nullptr },
			{ "SlowRadiusM", &FAiTactics::SlowRadiusM, nullptr },
			{ "SprintSlackS", &FAiTactics::SprintSlackS, nullptr },
			{ "SprintDistanceM", &FAiTactics::SprintDistanceM, nullptr },
			{ "SpeedAt1Mps", &FAiTactics::SpeedAt1Mps, nullptr },
			{ "SpeedAt99Mps", &FAiTactics::SpeedAt99Mps, nullptr },
			{ "AccelTimeAt1S", &FAiTactics::AccelTimeAt1S, nullptr },
			{ "AccelTimeAt99S", &FAiTactics::AccelTimeAt99S, nullptr },
			{ "PlanningSpeedFactor", &FAiTactics::PlanningSpeedFactor, nullptr },
			{ "ReachSideAt1M", &FAiTactics::ReachSideAt1M, nullptr },
			{ "ReachSideAt99M", &FAiTactics::ReachSideAt99M, nullptr },
			{ "OverheadReachAt1M", &FAiTactics::OverheadReachAt1M, nullptr },
			{ "OverheadReachAt99M", &FAiTactics::OverheadReachAt99M, nullptr },
			{ "JumpExtraAt1M", &FAiTactics::JumpExtraAt1M, nullptr },
			{ "JumpExtraAt99M", &FAiTactics::JumpExtraAt99M, nullptr },
			{ "AnticipationDelayScaleAt1", &FAiTactics::AnticipationDelayScaleAt1, nullptr },
			{ "AnticipationDelayScaleAt99", &FAiTactics::AnticipationDelayScaleAt99, nullptr },
			{ "PredictDtS", &FAiTactics::PredictDtS, nullptr },
			{ "PredictHorizonS", &FAiTactics::PredictHorizonS, nullptr },
			{ "SampleEverySteps", nullptr, &FAiTactics::SampleEverySteps },
			{ "WallNoiseAt1M", &FAiTactics::WallNoiseAt1M, nullptr },
			{ "WallNoiseAt99M", &FAiTactics::WallNoiseAt99M, nullptr },
			{ "GlassRereadDelayScale", &FAiTactics::GlassRereadDelayScale, nullptr },
			{ "MinContactHeightM", &FAiTactics::MinContactHeightM, nullptr },
			{ "IdealContactHeightM", &FAiTactics::IdealContactHeightM, nullptr },
			{ "OverheadMinHeightM", &FAiTactics::OverheadMinHeightM, nullptr },
			{ "ContactSideM", &FAiTactics::ContactSideM, nullptr },
			{ "ContactForwardM", &FAiTactics::ContactForwardM, nullptr },
			{ "InterceptWallMarginM", &FAiTactics::InterceptWallMarginM, nullptr },
			{ "ScoreHeightWeight", &FAiTactics::ScoreHeightWeight, nullptr },
			{ "ScoreSlackWeight", &FAiTactics::ScoreSlackWeight, nullptr },
			{ "SlackCapS", &FAiTactics::SlackCapS, nullptr },
			{ "ScoreTimeWeight", &FAiTactics::ScoreTimeWeight, nullptr },
			{ "VolleyAtNetBonus", &FAiTactics::VolleyAtNetBonus, nullptr },
			{ "OverheadBonus", &FAiTactics::OverheadBonus, nullptr },
			{ "SwingLeadS", &FAiTactics::SwingLeadS, nullptr },
			{ "MiddleBandM", &FAiTactics::MiddleBandM, nullptr },
			{ "BetterPlacedMarginS", &FAiTactics::BetterPlacedMarginS, nullptr },
			{ "ConfusionMax", &FAiTactics::ConfusionMax, nullptr },
			{ "SmashMaxDepthM", &FAiTactics::SmashMaxDepthM, nullptr },
			{ "SmashMinHeightM", &FAiTactics::SmashMinHeightM, nullptr },
			{ "SetSlackS", &FAiTactics::SetSlackS, nullptr },
			{ "PinnedDepthM", &FAiTactics::PinnedDepthM, nullptr },
			{ "ChiquitaMaxDepthM", &FAiTactics::ChiquitaMaxDepthM, nullptr },
			{ "LowBallHeightM", &FAiTactics::LowBallHeightM, nullptr },
			{ "WeightBandeja", &FAiTactics::WeightBandeja, nullptr },
			{ "WeightViboraLeft", &FAiTactics::WeightViboraLeft, nullptr },
			{ "WeightViboraRight", &FAiTactics::WeightViboraRight, nullptr },
			{ "WeightSmash", &FAiTactics::WeightSmash, nullptr },
			{ "WeightOverheadLobFromBack", &FAiTactics::WeightOverheadLobFromBack, nullptr },
			{ "WeightLobVsNet", &FAiTactics::WeightLobVsNet, nullptr },
			{ "WeightChiquitaVsNet", &FAiTactics::WeightChiquitaVsNet, nullptr },
			{ "WeightDriveVsNet", &FAiTactics::WeightDriveVsNet, nullptr },
			{ "WeightDriveBase", &FAiTactics::WeightDriveBase, nullptr },
			{ "WeightLobBase", &FAiTactics::WeightLobBase, nullptr },
			{ "WeightChiquitaBase", &FAiTactics::WeightChiquitaBase, nullptr },
			{ "WeightLobPinned", &FAiTactics::WeightLobPinned, nullptr },
			{ "PinnedDriveScale", &FAiTactics::PinnedDriveScale, nullptr },
			{ "WeightSplitDrive", &FAiTactics::WeightSplitDrive, nullptr },
			{ "WeightVolley", &FAiTactics::WeightVolley, nullptr },
			{ "WeightLobVolley", &FAiTactics::WeightLobVolley, nullptr },
			{ "SafeLobBonus", &FAiTactics::SafeLobBonus, nullptr },
			{ "KickServeChance", &FAiTactics::KickServeChance, nullptr },
			{ "LobDepthM", &FAiTactics::LobDepthM, nullptr },
			{ "LobSideM", &FAiTactics::LobSideM, nullptr },
			{ "ChiquitaDepthM", &FAiTactics::ChiquitaDepthM, nullptr },
			{ "DriveDeepDepthM", &FAiTactics::DriveDeepDepthM, nullptr },
			{ "DriveSideM", &FAiTactics::DriveSideM, nullptr },
			{ "VolleyDeepDepthM", &FAiTactics::VolleyDeepDepthM, nullptr },
			{ "FeetDepthOffsetM", &FAiTactics::FeetDepthOffsetM, nullptr },
			{ "BandejaDepthM", &FAiTactics::BandejaDepthM, nullptr },
			{ "BandejaSideM", &FAiTactics::BandejaSideM, nullptr },
			{ "ViboraDepthM", &FAiTactics::ViboraDepthM, nullptr },
			{ "ViboraSideM", &FAiTactics::ViboraSideM, nullptr },
			{ "SmashDepthM", &FAiTactics::SmashDepthM, nullptr },
			{ "ServeDepthM", &FAiTactics::ServeDepthM, nullptr },
			{ "ServeSideM", &FAiTactics::ServeSideM, nullptr },
			{ "TargetSideLimitM", &FAiTactics::TargetSideLimitM, nullptr },
			{ "TargetDepthMinM", &FAiTactics::TargetDepthMinM, nullptr },
			{ "TargetDepthMaxM", &FAiTactics::TargetDepthMaxM, nullptr },
			{ "AimScatterBaseM", &FAiTactics::AimScatterBaseM, nullptr },
			{ "ChargeDrive", &FAiTactics::ChargeDrive, nullptr },
			{ "ChargeVolley", &FAiTactics::ChargeVolley, nullptr },
			{ "ChargeLob", &FAiTactics::ChargeLob, nullptr },
			{ "ChargeBandeja", &FAiTactics::ChargeBandeja, nullptr },
			{ "ChargeVibora", &FAiTactics::ChargeVibora, nullptr },
			{ "ChargeSmash", &FAiTactics::ChargeSmash, nullptr },
			{ "ChargeChiquita", &FAiTactics::ChargeChiquita, nullptr },
			{ "ChargeServe", &FAiTactics::ChargeServe, nullptr },
			{ "WindowPerfectS", &FAiTactics::WindowPerfectS, nullptr },
			{ "WindowGoodS", &FAiTactics::WindowGoodS, nullptr },
			{ "WindowEarlyLateS", &FAiTactics::WindowEarlyLateS, nullptr },
			{ "WindowMissExtraS", &FAiTactics::WindowMissExtraS, nullptr },
			{ "ServeDelayS", &FAiTactics::ServeDelayS, nullptr },
			{ "PointGapS", &FAiTactics::PointGapS, nullptr },
			{ "WaistHeightFraction", &FAiTactics::WaistHeightFraction, nullptr },
			{ "ContactMatchS", &FAiTactics::ContactMatchS, nullptr },
			{ "NetClearanceM", &FAiTactics::NetClearanceM, nullptr },
			{ "NetRetargetDepthStepM", &FAiTactics::NetRetargetDepthStepM, nullptr },
			{ "NetRetargetSteps", nullptr, &FAiTactics::NetRetargetSteps },
			{ "NetRetargetChargeStep", &FAiTactics::NetRetargetChargeStep, nullptr },
		};
		return Fields;
	}

	template <typename T>
	void ApplyRow(const FRow& Row, const std::vector<FField<T>>& Fields, T& Out, const std::string& Where,
		std::vector<std::string>& Errors)
	{
		std::set<std::string> Known = { "Name" };
		for (const FField<T>& Field : Fields)
		{
			Known.insert(Field.Key);
			const auto It = Row.find(Field.Key);
			if (It == Row.end())
			{
				Errors.push_back(Where + ": missing \"" + Field.Key + "\"");
				continue;
			}
			if (!It->second.bIsNumber)
			{
				Errors.push_back(Where + ": \"" + Field.Key + "\" must be a number");
				continue;
			}
			if (Field.DoubleMember != nullptr)
			{
				Out.*Field.DoubleMember = It->second.Number;
			}
			else
			{
				const double Rounded = std::round(It->second.Number);
				if (Rounded != It->second.Number)
				{
					Errors.push_back(Where + ": \"" + Field.Key + "\" must be a whole number");
				}
				Out.*Field.IntMember = static_cast<int>(Rounded);
			}
		}
		for (const auto& Pair : Row)
		{
			if (Known.count(Pair.first) == 0)
			{
				Errors.push_back(Where + ": unknown key \"" + Pair.first + "\"");
			}
		}
	}

	std::string RowName(const FRow& Row)
	{
		const auto It = Row.find("Name");
		return It != Row.end() && !It->second.bIsNumber ? It->second.Text : std::string();
	}
}

FLoadResult ParseDifficulties(const std::string& Json, std::vector<FAiDifficulty>& OutRows)
{
	FLoadResult Result;
	std::vector<FRow> Rows;
	FReader Reader(Json);
	if (!Reader.ReadRows(Rows, Result.Errors))
	{
		return Result;
	}
	OutRows.clear();
	for (size_t Index = 0; Index < Rows.size(); ++Index)
	{
		FAiDifficulty Row;
		Row.Name = RowName(Rows[Index]);
		const std::string Where = Row.Name.empty() ? "row " + std::to_string(Index) : Row.Name;
		if (Row.Name.empty())
		{
			Result.Errors.push_back(Where + ": missing \"Name\"");
		}
		ApplyRow(Rows[Index], DifficultyFields(), Row, Where, Result.Errors);
		OutRows.push_back(Row);
	}
	Result.bOk = Result.Errors.empty();
	return Result;
}

FLoadResult ParseTactics(const std::string& Json, FAiTactics& OutTactics)
{
	FLoadResult Result;
	std::vector<FRow> Rows;
	FReader Reader(Json);
	if (!Reader.ReadRows(Rows, Result.Errors))
	{
		return Result;
	}
	if (Rows.size() != 1)
	{
		Result.Errors.push_back("AiTactics.json must hold exactly one row");
		return Result;
	}
	ApplyRow(Rows[0], TacticsFields(), OutTactics, "AiTactics", Result.Errors);
	Result.bOk = Result.Errors.empty();
	return Result;
}
}
