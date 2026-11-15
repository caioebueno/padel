#include "AI/PadelAiData.h"

#include "AI/AiTuningJson.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"

#include <string>

namespace Padel::AIData
{
namespace
{
	bool ReadDataFile(const TCHAR* Relative, std::string& OutText)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), Relative);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogPadel, Error, TEXT("AI data: can't read %s"), *Path);
			return false;
		}
		OutText = TCHAR_TO_UTF8(*Text);
		return true;
	}

	bool Report(const TCHAR* File, const Padel::AI::TuningJson::FLoadResult& Result)
	{
		for (const std::string& Error : Result.Errors)
		{
			UE_LOG(LogPadel, Error, TEXT("AI data %s: %s"), File, UTF8_TO_TCHAR(Error.c_str()));
		}
		return Result.bOk;
	}
}

bool LoadTactics(Padel::AI::FAiTactics& OutTactics)
{
	std::string Text;
	return ReadDataFile(TEXT("AiTactics.json"), Text)
		&& Report(TEXT("AiTactics.json"), Padel::AI::TuningJson::ParseTactics(Text, OutTactics));
}

bool LoadDifficulties(std::vector<Padel::AI::FAiDifficulty>& OutRows)
{
	std::string Text;
	return ReadDataFile(TEXT("Difficulty.json"), Text)
		&& Report(TEXT("Difficulty.json"), Padel::AI::TuningJson::ParseDifficulties(Text, OutRows));
}

const Padel::AI::FAiDifficulty* FindDifficulty(const std::vector<Padel::AI::FAiDifficulty>& Rows, const FString& Name)
{
	const std::string Wanted = TCHAR_TO_UTF8(*Name);
	for (const Padel::AI::FAiDifficulty& Row : Rows)
	{
		if (Row.Name == Wanted)
		{
			return &Row;
		}
	}
	return nullptr;
}
}
