// Loads the AI's real tuning files (Data/Difficulty.json, Data/AiTactics.json) for
// the native tests, so tests run against the numbers the game ships with.
#pragma once

#include "AI/AiPlayerProfile.h"
#include "AI/AiTuningJson.h"
#include "CourtSpec.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Padel::AI::TestData
{
	inline std::string RepoPath(const std::string& Relative)
	{
		// This file is Tests/Native/Support/AiTestData.h.
		std::string Here = __FILE__;
		for (int Up = 0; Up < 4; ++Up)
		{
			const size_t Slash = Here.find_last_of("/\\");
			Here = Slash == std::string::npos ? std::string(".") : Here.substr(0, Slash);
		}
		return Here + "/" + Relative;
	}

	inline std::string ReadText(const std::string& Relative)
	{
		std::ifstream File(RepoPath(Relative), std::ios::binary);
		if (!File)
		{
			throw std::runtime_error("can't open " + RepoPath(Relative));
		}
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		return Buffer.str();
	}

	inline const FAiTactics& Tactics()
	{
		static const FAiTactics Loaded = [] {
			FAiTactics T;
			const TuningJson::FLoadResult Result = TuningJson::ParseTactics(ReadText("Data/AiTactics.json"), T);
			if (!Result.bOk)
			{
				throw std::runtime_error("Data/AiTactics.json: " + (Result.Errors.empty() ? std::string("?") : Result.Errors[0]));
			}
			return T;
		}();
		return Loaded;
	}

	inline const FAiDifficulty& Difficulty(const char* Name)
	{
		static const std::vector<FAiDifficulty> Rows = [] {
			std::vector<FAiDifficulty> Out;
			const TuningJson::FLoadResult Result = TuningJson::ParseDifficulties(ReadText("Data/Difficulty.json"), Out);
			if (!Result.bOk)
			{
				throw std::runtime_error("Data/Difficulty.json: " + (Result.Errors.empty() ? std::string("?") : Result.Errors[0]));
			}
			return Out;
		}();
		for (const FAiDifficulty& Row : Rows)
		{
			if (Row.Name == Name)
			{
				return Row;
			}
		}
		throw std::runtime_error(std::string("no difficulty row ") + Name);
	}

	// A typical slice-roster player, around 78 overall (GDD 6.4).
	inline FAiPlayerProfile Player(ESide Side, EHand Hand = EHand::Right)
	{
		FAiPlayerProfile P;
		P.Speed = 76;
		P.Acceleration = 78;
		P.Reach = 75;
		P.Jump = 70;
		P.WallPlay = 78;
		P.Anticipation = 77;
		P.Positioning = 79;
		P.Teamwork = 78;
		P.ReturnSkill = 76;
		P.PreferredSide = Side;
		P.Handedness = Hand;
		return P;
	}
}
