#include "Data/PadelShotData.h"

#include "Data/PadelBallMachineFeedRow.h"
#include "Data/PadelMovementRow.h"
#include "Data/PadelPlayerRow.h"
#include "Data/PadelPracticeBallRow.h"
#include "Data/PadelShotModelRow.h"
#include "Data/PadelShotRow.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PadelLog.h"
#include "UObject/Package.h"

namespace PadelShotDataDetail
{
	UDataTable* LoadTable(const TCHAR* File, UScriptStruct* RowStruct, TArray<FString>& Errors)
	{
		const FString Path = FPaths::Combine(FPadelShotData::DataDirectory(), File);
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			Errors.Add(FString::Printf(TEXT("Cannot read %s"), *Path));
			return nullptr;
		}
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = RowStruct;
		for (const FString& Problem : Table->CreateTableFromJSONString(Json))
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), File, *Problem));
		}
		return Table;
	}

	template <typename RowType>
	const RowType* DefaultRow(UDataTable* Table, const TCHAR* File, TArray<FString>& Errors)
	{
		const RowType* Row = Table ? Table->FindRow<RowType>(FName(TEXT("default")), File, false) : nullptr;
		if (!Row)
		{
			Errors.Add(FString::Printf(TEXT("%s has no \"default\" row"), File));
		}
		return Row;
	}

	void AddCoreErrors(const std::vector<std::string>& Core, const TCHAR* File, TArray<FString>& Errors)
	{
		for (const std::string& Problem : Core)
		{
			Errors.Add(FString::Printf(TEXT("%s: %s"), File, UTF8_TO_TCHAR(Problem.c_str())));
		}
	}
}

FString FPadelShotData::DataDirectory()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"));
}

FPadelShotData FPadelShotData::Load()
{
	using namespace PadelShotDataDetail;
	using namespace Padel::Shots;
	FPadelShotData Out;

	if (UDataTable* Shots = LoadTable(TEXT("Shots.json"), FPadelShotRow::StaticStruct(), Out.Errors))
	{
		for (int32 Index = 0; Index < ShotTypeCount; ++Index)
		{
			const Padel::EShotType Type = static_cast<Padel::EShotType>(Index);
			if (Type == Padel::EShotType::Mishit)
			{
				continue;
			}
			if (const FPadelShotRow* Row = Shots->FindRow<FPadelShotRow>(FName(UTF8_TO_TCHAR(Padel::ShotTypeName(Type))), TEXT("Shots.json"), false))
			{
				Out.Library.SetShot(Row->ToCore(Type));
			}
		}
	}
	if (const FPadelShotModelRow* Row = DefaultRow<FPadelShotModelRow>(LoadTable(TEXT("ShotModel.json"), FPadelShotModelRow::StaticStruct(), Out.Errors), TEXT("ShotModel.json"), Out.Errors))
	{
		Out.Library.Model = Row->ToCore();
	}
	AddCoreErrors(Out.Library.Validate(), TEXT("Shots.json/ShotModel.json"), Out.Errors);

	if (const FPadelMovementRow* Row = DefaultRow<FPadelMovementRow>(LoadTable(TEXT("Movement.json"), FPadelMovementRow::StaticStruct(), Out.Errors), TEXT("Movement.json"), Out.Errors))
	{
		Out.Movement = Row->ToCore();
		AddCoreErrors(Out.Movement.Validate(), TEXT("Movement.json"), Out.Errors);
	}
	if (const FPadelPracticeBallRow* Row = DefaultRow<FPadelPracticeBallRow>(LoadTable(TEXT("PracticeBall.json"), FPadelPracticeBallRow::StaticStruct(), Out.Errors), TEXT("PracticeBall.json"), Out.Errors))
	{
		Out.PracticeBall = Row->ToCore();
		AddCoreErrors(Out.PracticeBall.Validate(), TEXT("PracticeBall.json"), Out.Errors);
	}
	if (UDataTable* Feeds = LoadTable(TEXT("BallMachine.json"), FPadelBallMachineFeedRow::StaticStruct(), Out.Errors))
	{
		Feeds->ForeachRow<FPadelBallMachineFeedRow>(TEXT("BallMachine.json"), [&Out](const FName& Key, const FPadelBallMachineFeedRow& Row)
		{
			Out.Feeds.push_back(Row.ToCore(Key));
		});
	}

	for (const FString& Error : Out.Errors)
	{
		UE_LOG(LogPadel, Error, TEXT("Shot data: %s"), *Error);
	}
	Out.bOk = Out.Errors.IsEmpty();
	return Out;
}

bool FPadelShotData::LoadPlayer(FName RowName, Padel::Shots::FPlayerRatings& OutRatings, FString* OutDisplayName)
{
	TArray<FString> Errors;
	UDataTable* Table = PadelShotDataDetail::LoadTable(TEXT("Players.json"), FPadelPlayerRow::StaticStruct(), Errors);
	const FPadelPlayerRow* Row = Table ? Table->FindRow<FPadelPlayerRow>(RowName, TEXT("Players.json"), false) : nullptr;
	if (!Row)
	{
		UE_LOG(LogPadel, Error, TEXT("Players.json has no row %s"), *RowName.ToString());
		return false;
	}
	OutRatings = Row->ToCore();
	if (OutDisplayName)
	{
		*OutDisplayName = Row->DisplayName;
	}
	return true;
}
